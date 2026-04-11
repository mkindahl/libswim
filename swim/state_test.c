// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include <stdlib.h>

#include "swim/node.h"
#include "swim/state.h"

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

static int teardown(void **state) {
  SWIM *swim = *state;
  free(swim->probe.order);
  free(swim->view);
  free(swim);
  return 0;
}

static void make_node(NodeInfo *info, uuid_t uuid, Status status,
                      uint32_t incarnation, time_t last_seen) {
  struct sockaddr_in addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9000),
      .sin_addr.s_addr = htonl(0x7f000001),
  };
  swim_node_init(info, uuid, (struct sockaddr *)&addr, sizeof(addr));
  info->status = status;
  info->incarnation = incarnation;
  info->last_seen = last_seen;
}

/* --- swim_state_add tests --- */

static void test_add_new_node(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);

  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);

  NodeState *added = swim_state_add(swim, &info);
  assert_non_null(added);
  assert_int_equal(swim->view_size, 1);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.status, SWIM_STATUS_ALIVE);
  assert_int_equal(found->info.incarnation, 1);
  assert_int_equal((int)found->info.last_seen, 1000);
}

static void test_add_multiple_sorted(void **state) {
  SWIM *swim = *state;

  uuid_t ids[3];
  for (int i = 0; i < 3; i++) {
    uuid_generate(ids[i]);
    NodeInfo info;
    make_node(&info, ids[i], SWIM_STATUS_ALIVE, 1, 1000 + i);
    swim_state_add(swim, &info);
  }

  assert_int_equal(swim->view_size, 3);

  for (int i = 0; i < 3; i++) {
    NodeState *found = swim_state_get_node(swim, ids[i]);
    assert_non_null(found);
  }
}

/* --- swim_state_del tests --- */

static void test_del_marks_dead(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  NodeState *deleted = swim_state_del(swim, id);
  assert_non_null(deleted);
  assert_int_equal(deleted->info.status, SWIM_STATUS_DEAD);
}

static void test_del_self_ignored(void **state) {
  SWIM *swim = *state;

  NodeState *result = swim_state_del(swim, swim->uuid);
  assert_null(result);
  assert_int_equal(swim->view_size, 0);
}

static void test_del_missing_returns_null(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);

  NodeState *result = swim_state_del(swim, id);
  assert_null(result);
}

/* --- swim_state_suspect tests --- */

static void test_suspect_sets_status(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  struct sockaddr_in witness_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9001),
      .sin_addr.s_addr = htonl(0x7f000002),
  };

  NodeState *result = swim_state_suspect(
      swim, id, (struct sockaddr *)&witness_addr, sizeof(witness_addr));
  assert_non_null(result);
  assert_int_equal(result->info.status, SWIM_STATUS_SUSPECT);
}

static void test_suspect_records_witness(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  struct sockaddr_in witness_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9001),
      .sin_addr.s_addr = htonl(0x7f000002),
  };

  NodeState *result = swim_state_suspect(
      swim, id, (struct sockaddr *)&witness_addr, sizeof(witness_addr));
  assert_true(swim_node_has_witness(result));
}

/* --- swim_state_notice tests --- */

static void test_notice_alive_updates_time(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  NodeState *result = swim_state_notice(swim, id, 2000, 1);
  assert_non_null(result);
  assert_int_equal((int)result->info.last_seen, 2000);
  assert_int_equal(result->info.status, SWIM_STATUS_ALIVE);
}

static void test_notice_suspect_to_alive(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(swim, &info);

  NodeState *result = swim_state_notice(swim, id, 2000, 1);
  assert_non_null(result);
  assert_int_equal(result->info.status, SWIM_STATUS_ALIVE);
}

static void test_notice_dead_ignored(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_DEAD, 1, 1000);
  swim_state_add(swim, &info);

  NodeState *result = swim_state_notice(swim, id, 2000, 1);
  assert_non_null(result);
  assert_int_equal(result->info.status, SWIM_STATUS_DEAD);
  assert_int_equal((int)result->info.last_seen, 1000);
}

static void test_notice_higher_incarnation(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(swim, &info);

  NodeState *result = swim_state_notice(swim, id, 2000, 5);
  assert_non_null(result);
  assert_int_equal(result->info.status, SWIM_STATUS_ALIVE);
  assert_int_equal(result->info.incarnation, 5);
  assert_int_equal((int)result->info.last_seen, 2000);
}

static void test_notice_stale_incarnation_ignored(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 5, 1000);
  swim_state_add(swim, &info);

  NodeState *result = swim_state_notice(swim, id, 2000, 3);
  assert_non_null(result);
  assert_int_equal(result->info.incarnation, 5);
  assert_int_equal((int)result->info.last_seen, 1000);
  assert_int_equal(result->info.status, SWIM_STATUS_ALIVE);
}

static void test_notice_missing_returns_null(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);

  NodeState *result = swim_state_notice(swim, id, 2000, 1);
  assert_null(result);
}

/* --- swim_state_merge tests --- */

static void test_merge_new_node_added(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);

  swim_state_merge(swim, &info);

  assert_int_equal(swim->view_size, 1);
  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.status, SWIM_STATUS_ALIVE);
}

static void test_merge_higher_incarnation_updates(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 2, 2000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.incarnation, 2);
  assert_int_equal(found->info.status, SWIM_STATUS_SUSPECT);
  assert_int_equal((int)found->info.last_seen, 2000);
}

static void test_merge_same_incarnation_newer_time(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 1, 2000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal((int)found->info.last_seen, 2000);
  assert_int_equal(found->info.status, SWIM_STATUS_SUSPECT);
}

static void test_merge_same_incarnation_older_time_ignored(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal((int)found->info.last_seen, 1000);
  assert_int_equal(found->info.status, SWIM_STATUS_SUSPECT);
}

static void test_merge_same_incarnation_alive_no_override_suspect(
    void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.status, SWIM_STATUS_SUSPECT);
  assert_int_equal((int)found->info.last_seen, 1000);
}

static void test_merge_same_incarnation_same_status_newer_time(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.status, SWIM_STATUS_ALIVE);
  assert_int_equal((int)found->info.last_seen, 2000);
}

static void test_merge_stale_incarnation_ignored(void **state) {
  SWIM *swim = *state;

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 5, 1000);
  swim_state_add(swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 3, 2000);
  swim_state_merge(swim, &gossip);

  NodeState *found = swim_state_get_node(swim, id);
  assert_non_null(found);
  assert_int_equal(found->info.incarnation, 5);
  assert_int_equal(found->info.status, SWIM_STATUS_ALIVE);
  assert_int_equal((int)found->info.last_seen, 1000);
}

static void test_merge_self_ignored(void **state) {
  SWIM *swim = *state;

  NodeInfo info;
  make_node(&info, swim->uuid, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_merge(swim, &info);

  assert_int_equal(swim->view_size, 0);
}

int main(void) {
  const struct CMUnitTest tests[] = {
      /* swim_state_add */
      cmocka_unit_test_setup_teardown(test_add_new_node, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_add_multiple_sorted, setup, teardown),

      /* swim_state_del */
      cmocka_unit_test_setup_teardown(test_del_marks_dead, setup, teardown),
      cmocka_unit_test_setup_teardown(test_del_self_ignored, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_del_missing_returns_null, setup, teardown),

      /* swim_state_suspect */
      cmocka_unit_test_setup_teardown(
          test_suspect_sets_status, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_suspect_records_witness, setup, teardown),

      /* swim_state_notice */
      cmocka_unit_test_setup_teardown(
          test_notice_alive_updates_time, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_notice_suspect_to_alive, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_notice_dead_ignored, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_notice_higher_incarnation, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_notice_stale_incarnation_ignored, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_notice_missing_returns_null, setup, teardown),

      /* swim_state_merge */
      cmocka_unit_test_setup_teardown(
          test_merge_new_node_added, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_higher_incarnation_updates, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_same_incarnation_newer_time, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_same_incarnation_older_time_ignored, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_same_incarnation_alive_no_override_suspect,
          setup,
          teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_same_incarnation_same_status_newer_time, setup, teardown),
      cmocka_unit_test_setup_teardown(
          test_merge_stale_incarnation_ignored, setup, teardown),
      cmocka_unit_test_setup_teardown(test_merge_self_ignored, setup, teardown),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
