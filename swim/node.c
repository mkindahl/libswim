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
  memcpy(&dst->last_seen, &src->last_seen, sizeof(src->last_seen));
}
