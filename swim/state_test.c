#include "state.h"
#include "test.h"

#include "swim/node.h"

static void swim_test_init(SWIM *swim) {
  memset(swim, 0, sizeof(*swim));
  swim->sockfd = -1;
  swim->view_capacity = 16;
  swim->view = calloc(swim->view_capacity, sizeof(NodeState));
  uuid_generate(swim->uuid);
  time(&swim->last_heartbeat);
}

static void swim_test_cleanup(SWIM *swim) { free(swim->view); }

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

static void test_add_new_node(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);

  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);

  NodeState *added = swim_state_add(&swim, &info);
  EXPECT_NOT_NULL(added);
  EXPECT_EQ_INT(swim.view_size, 1, "d");

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_ALIVE, "d");
  EXPECT_EQ_INT(found->info.incarnation, 1, "u");
  EXPECT_EQ_INT((int)found->info.last_seen, 1000, "d");

  swim_test_cleanup(&swim);
}

static void test_add_multiple_sorted(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t ids[3];
  for (int i = 0; i < 3; i++) {
    uuid_generate(ids[i]);
    NodeInfo info;
    make_node(&info, ids[i], SWIM_STATUS_ALIVE, 1, 1000 + i);
    swim_state_add(&swim, &info);
  }

  EXPECT_EQ_INT(swim.view_size, 3, "d");

  for (int i = 0; i < 3; i++) {
    NodeState *found = swim_state_get_node(&swim, ids[i]);
    EXPECT_NOT_NULL(found);
  }

  swim_test_cleanup(&swim);
}

/* --- swim_state_del tests --- */

static void test_del_marks_dead(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  NodeState *deleted = swim_state_del(&swim, id);
  EXPECT_NOT_NULL(deleted);
  EXPECT_EQ_INT(deleted->info.status, SWIM_STATUS_DEAD, "d");

  swim_test_cleanup(&swim);
}

static void test_del_self_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  NodeState *result = swim_state_del(&swim, swim.uuid);
  EXPECT_NULL(result);
  EXPECT_EQ_INT(swim.view_size, 0, "d");

  swim_test_cleanup(&swim);
}

static void test_del_missing_returns_null(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);

  NodeState *result = swim_state_del(&swim, id);
  EXPECT_NULL(result);

  swim_test_cleanup(&swim);
}

/* --- swim_state_suspect tests --- */

static void test_suspect_sets_status(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  struct sockaddr_in witness_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9001),
      .sin_addr.s_addr = htonl(0x7f000002),
  };

  NodeState *result = swim_state_suspect(
      &swim, id, (struct sockaddr *)&witness_addr, sizeof(witness_addr));
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_SUSPECT, "d");

  swim_test_cleanup(&swim);
}

static void test_suspect_records_witness(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  struct sockaddr_in witness_addr = {
      .sin_family = AF_INET,
      .sin_port = htons(9001),
      .sin_addr.s_addr = htonl(0x7f000002),
  };

  NodeState *result = swim_state_suspect(
      &swim, id, (struct sockaddr *)&witness_addr, sizeof(witness_addr));
  EXPECT_TRUE(swim_node_has_witness(result));

  swim_test_cleanup(&swim);
}

/* --- swim_state_notice tests --- */

static void test_notice_alive_updates_time(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  NodeState *result = swim_state_notice(&swim, id, 2000, 1);
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT((int)result->info.last_seen, 2000, "d");
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_ALIVE, "d");

  swim_test_cleanup(&swim);
}

static void test_notice_suspect_to_alive(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(&swim, &info);

  NodeState *result = swim_state_notice(&swim, id, 2000, 1);
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_ALIVE, "d");

  swim_test_cleanup(&swim);
}

static void test_notice_dead_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_DEAD, 1, 1000);
  swim_state_add(&swim, &info);

  NodeState *result = swim_state_notice(&swim, id, 2000, 1);
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_DEAD, "d");
  EXPECT_EQ_INT((int)result->info.last_seen, 1000, "d");

  swim_test_cleanup(&swim);
}

static void test_notice_higher_incarnation(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(&swim, &info);

  NodeState *result = swim_state_notice(&swim, id, 2000, 5);
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_ALIVE, "d");
  EXPECT_EQ_INT(result->info.incarnation, 5, "u");
  EXPECT_EQ_INT((int)result->info.last_seen, 2000, "d");

  swim_test_cleanup(&swim);
}

static void test_notice_stale_incarnation_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 5, 1000);
  swim_state_add(&swim, &info);

  NodeState *result = swim_state_notice(&swim, id, 2000, 3);
  EXPECT_NOT_NULL(result);
  EXPECT_EQ_INT(result->info.incarnation, 5, "u");
  EXPECT_EQ_INT((int)result->info.last_seen, 1000, "d");
  EXPECT_EQ_INT(result->info.status, SWIM_STATUS_ALIVE, "d");

  swim_test_cleanup(&swim);
}

static void test_notice_missing_returns_null(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);

  NodeState *result = swim_state_notice(&swim, id, 2000, 1);
  EXPECT_NULL(result);

  swim_test_cleanup(&swim);
}

/* --- swim_state_merge tests --- */

static void test_merge_new_node_added(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);

  swim_state_merge(&swim, &info);

  EXPECT_EQ_INT(swim.view_size, 1, "d");
  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_ALIVE, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_higher_incarnation_updates(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 2, 2000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.incarnation, 2, "u");
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_SUSPECT, "d");
  EXPECT_EQ_INT((int)found->info.last_seen, 2000, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_same_incarnation_newer_time(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 1, 2000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT((int)found->info.last_seen, 2000, "d");
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_SUSPECT, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_same_incarnation_older_time_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT((int)found->info.last_seen, 1000, "d");
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_SUSPECT, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_same_incarnation_alive_no_override_suspect(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_SUSPECT, 1, 1000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_SUSPECT, "d");
  EXPECT_EQ_INT((int)found->info.last_seen, 1000, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_same_incarnation_same_status_newer_time(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_ALIVE, 1, 2000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_ALIVE, "d");
  EXPECT_EQ_INT((int)found->info.last_seen, 2000, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_stale_incarnation_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  uuid_t id;
  uuid_generate(id);
  NodeInfo info;
  make_node(&info, id, SWIM_STATUS_ALIVE, 5, 1000);
  swim_state_add(&swim, &info);

  NodeInfo gossip;
  make_node(&gossip, id, SWIM_STATUS_SUSPECT, 3, 2000);
  swim_state_merge(&swim, &gossip);

  NodeState *found = swim_state_get_node(&swim, id);
  EXPECT_NOT_NULL(found);
  EXPECT_EQ_INT(found->info.incarnation, 5, "u");
  EXPECT_EQ_INT(found->info.status, SWIM_STATUS_ALIVE, "d");
  EXPECT_EQ_INT((int)found->info.last_seen, 1000, "d");

  swim_test_cleanup(&swim);
}

static void test_merge_self_ignored(void) {
  SWIM swim;
  swim_test_init(&swim);

  NodeInfo info;
  make_node(&info, swim.uuid, SWIM_STATUS_ALIVE, 1, 1000);
  swim_state_merge(&swim, &info);

  EXPECT_EQ_INT(swim.view_size, 0, "d");

  swim_test_cleanup(&swim);
}

int main(void) {
  /* swim_state_add */
  test_add_new_node();
  test_add_multiple_sorted();

  /* swim_state_del */
  test_del_marks_dead();
  test_del_self_ignored();
  test_del_missing_returns_null();

  /* swim_state_suspect */
  test_suspect_sets_status();
  test_suspect_records_witness();

  /* swim_state_notice */
  test_notice_alive_updates_time();
  test_notice_suspect_to_alive();
  test_notice_dead_ignored();
  test_notice_higher_incarnation();
  test_notice_stale_incarnation_ignored();
  test_notice_missing_returns_null();

  /* swim_state_merge */
  test_merge_new_node_added();
  test_merge_higher_incarnation_updates();
  test_merge_same_incarnation_newer_time();
  test_merge_same_incarnation_older_time_ignored();
  test_merge_same_incarnation_alive_no_override_suspect();
  test_merge_same_incarnation_same_status_newer_time();
  test_merge_stale_incarnation_ignored();
  test_merge_self_ignored();

  exit(exit_code);
}
