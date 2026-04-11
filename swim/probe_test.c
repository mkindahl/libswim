// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <stdlib.h>
#include <string.h>

#include "swim/node.h"
#include "swim/state.h"

int __wrap_rand(void) {
  return (int)mock();
}

/*
 * Setup function for the test suite.
 */
static int setup(void **state) {
  SWIM *swim = calloc(1, sizeof(SWIM));
  assert_non_null(swim);
  swim->sockfd = -1;
  swim->view_capacity = 16;
  swim->view = calloc(swim->view_capacity, sizeof(NodeState));
  swim->probe.order = NULL;
  swim->probe.pos = 0;
  swim->probe.size = 0;
  uuid_generate(swim->uuid);
  time(&swim->last_heartbeat);
  *state = swim;
  return 0;
}

/**
 * Teardown function for the test suite.
 */
static int teardown(void **state) {
  SWIM *swim = *state;
  free(swim->probe.order);
  free(swim->view);
  free(swim);
  return 0;
}

/**
 * Add an alive node to the SWIM instance.
 */
static void add_alive_node(SWIM *swim, uuid_t uuid) {
  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9000),
      .sin_addr.s_addr = htonl(0x7f000001),
  };
  NodeInfo info;
  swim_node_init(&info, uuid, (struct sockaddr *)&addr, sizeof(addr));
  info.status = SWIM_STATUS_ALIVE;
  info.last_seen = 1000;
  info.incarnation = 1;
  swim_state_add(swim, &info);
}

/*
 * Queue will_return values for a Fisher-Yates shuffle of n elements.
 * The identity permutation is produced by having rand() return i for each
 * step (i = n-1 down to 1), so rand() % (i+1) == i, which is a no-op swap.
 */
static void mock_shuffle_identity(int n) {
  for (int i = n - 1; i > 0; i--)
    will_return(__wrap_rand, i);
}

/*
 * Queue will_return values for a Fisher-Yates shuffle of n elements
 * where rand() always returns 0, producing a rotation-like permutation.
 * For n=5 this gives [1, 2, 3, 4, 0].
 */
static void mock_shuffle_rotate(int n) {
  for (int i = n - 1; i > 0; i--)
    will_return(__wrap_rand, 0);
}

/* After shuffle, order contains each index in 0..view_size-1 exactly once. */
static void test_shuffle_permutation_valid(void **state) {
  SWIM *swim = *state;

  const int N = 5;
  uuid_t ids[5];
  for (int i = 0; i < N; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  mock_shuffle_identity(N);
  swim_probe_shuffle(swim);

  assert_int_equal(swim->probe.size, N);
  assert_int_equal(swim->probe.pos, 0);

  /* Identity shuffle: order should be [0, 1, 2, 3, 4] */
  for (int i = 0; i < N; i++)
    assert_int_equal(swim->probe.order[i], i);
}

/* Calling probe_next N times returns every live node exactly once. */
static void test_full_round_coverage(void **state) {
  SWIM *swim = *state;

  const int N = 5;
  uuid_t ids[5];
  for (int i = 0; i < N; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  /* First probe_next triggers a shuffle */
  mock_shuffle_identity(N);

  int seen[5] = {0};
  for (int i = 0; i < N; i++) {
    int idx = swim_probe_next(swim);
    assert_in_range(idx, 0, N - 1);
    seen[idx]++;
  }

  for (int i = 0; i < N; i++)
    assert_int_equal(seen[i], 1);
}

/* After exhausting a round, the next call reshuffles and starts a new round. */
static void test_auto_reshuffle_after_round(void **state) {
  SWIM *swim = *state;

  const int N = 3;
  uuid_t ids[3];
  for (int i = 0; i < N; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  /* First round: identity shuffle */
  mock_shuffle_identity(N);
  for (int i = 0; i < N; i++)
    swim_probe_next(swim);

  assert_int_equal(swim->probe.pos, N);

  /*
   * Next call triggers reshuffle. With mock_shuffle_rotate(3), the
   * Fisher-Yates produces [1, 2, 0], so first element is 1.
   */
  mock_shuffle_rotate(N);
  int idx = swim_probe_next(swim);
  assert_int_equal(idx, 1);
  assert_int_equal(swim->probe.pos, 1);

  /* Complete the round and verify full coverage */
  int seen[3] = {0};
  seen[idx]++;
  for (int i = 1; i < N; i++) {
    idx = swim_probe_next(swim);
    assert_in_range(idx, 0, N - 1);
    seen[idx]++;
  }

  for (int i = 0; i < N; i++)
    assert_int_equal(seen[i], 1);
}

/* Adding a node mid-round forces a reshuffle with new view size. */
static void test_view_change_invalidates(void **state) {
  SWIM *swim = *state;

  uuid_t ids[4];
  for (int i = 0; i < 3; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  /* First probe_next triggers shuffle with N=3 */
  mock_shuffle_identity(3);
  swim_probe_next(swim);
  assert_int_equal(swim->probe.size, 3);

  /* Add a 4th node — this invalidates the schedule */
  uuid_generate(ids[3]);
  add_alive_node(swim, ids[3]);
  assert_int_equal(swim->probe.size, 0);

  /* Next probe should reshuffle with N=4 */
  mock_shuffle_identity(4);
  int idx = swim_probe_next(swim);
  assert_in_range(idx, 0, 3);
  assert_int_equal(swim->probe.size, 4);
}

/* Dead nodes are skipped by probe_next. */
static void test_dead_nodes_skipped(void **state) {
  SWIM *swim = *state;

  const int N = 4;
  uuid_t ids[4];
  for (int i = 0; i < N; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  /* Mark two nodes dead */
  swim->view[0].info.status = SWIM_STATUS_DEAD;
  swim->view[2].info.status = SWIM_STATUS_DEAD;

  /* Identity shuffle so order is [0, 1, 2, 3] */
  mock_shuffle_identity(N);
  swim_probe_shuffle(swim);

  /*
   * Walk through the round. With identity order [0,1,2,3] and nodes 0,2
   * dead, only nodes 1 and 3 should be returned.
   */
  int count = 0;
  while (swim->probe.pos < swim->probe.size) {
    int idx = swim_probe_next(swim);
    if (idx >= 0) {
      assert_true(swim->view[idx].info.status != SWIM_STATUS_DEAD);
      count++;
    }
  }
  assert_int_equal(count, 2);
}

/*
 * Two consecutive full rounds: for every node, the gap between its
 * last probe in round 1 and first probe in round 2 is at most 2N-1.
 */
static void test_probe_gap_bound(void **state) {
  SWIM *swim = *state;

  const int N = 5;
  uuid_t ids[5];
  for (int i = 0; i < N; i++) {
    uuid_generate(ids[i]);
    add_alive_node(swim, ids[i]);
  }

  /* Round 1: identity [0,1,2,3,4] */
  mock_shuffle_identity(N);

  int last_in_round1[5] = {0};
  int first_in_round2[5];
  memset(first_in_round2, -1, sizeof(first_in_round2));

  for (int step = 0; step < N; step++) {
    int idx = swim_probe_next(swim);
    assert_in_range(idx, 0, N - 1);
    last_in_round1[idx] = step;
  }

  /* Round 2: reverse [4,3,2,1,0] */
  mock_shuffle_rotate(N);

  for (int step = 0; step < N; step++) {
    int idx = swim_probe_next(swim);
    assert_in_range(idx, 0, N - 1);
    if (first_in_round2[idx] == -1)
      first_in_round2[idx] = N + step;
  }

  /* Check the gap for each node: first_in_round2 - last_in_round1 <= 2N-1 */
  for (int i = 0; i < N; i++) {
    assert_true(first_in_round2[i] != -1);
    int gap = first_in_round2[i] - last_in_round1[i];
    assert_in_range(gap, 1, 2 * N - 1);
  }
}

/* probe_next returns -1 on empty view. */
static void test_empty_view(void **state) {
  SWIM *swim = *state;

  int idx = swim_probe_next(swim);
  assert_int_equal(idx, -1);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test_setup_teardown(
          test_shuffle_permutation_valid, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_full_round_coverage, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_auto_reshuffle_after_round, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_view_change_invalidates, setup, teardown),
      cmocka_unit_test_setup_teardown(test_dead_nodes_skipped, setup, teardown),
      cmocka_unit_test_setup_teardown(test_probe_gap_bound, setup, teardown),
      cmocka_unit_test_setup_teardown(test_empty_view, setup, teardown),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
