#include "node.h"

#include <string.h>

#include <uuid/uuid.h>

void swim_node_init(NodeInfo* info, uuid_t uuid, struct sockaddr* addr,
                    socklen_t addrlen) {
  memset(info, 0, sizeof(*info));

  info->addrlen = addrlen;

  memcpy(&info->addr, addr, addrlen);
  uuid_copy(info->uuid, uuid);
}

void swim_node_copy(NodeInfo* dst, NodeInfo* src) {
  dst->addrlen = src->addrlen;
  dst->status = src->status;

  uuid_copy(dst->uuid, src->uuid);
  memcpy(&dst->addr, &src->addr, src->addrlen);
  dst->last_seen = src->last_seen;
}

bool swim_node_has_witness(NodeState* node) {
  return (node->witness.addrlen > 0);
}

void swim_node_reset(NodeState* node) {
  node->ping_time = 0;
  node->suspect_time = 0;
  memset(&node->witness, 0, sizeof(node->witness));
}

void swim_node_reset_witness(NodeState* node) {
  memset(&node->witness, 0, sizeof(node->witness));
}
