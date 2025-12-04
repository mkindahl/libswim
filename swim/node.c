#include "node.h"

#include <memory.h>

#include <uuid/uuid.h>

void swim_nodeinfo_copy(NodeInfo* dst, NodeInfo* src) {
  dst->addrlen = src->addrlen;
  dst->status = src->status;

  uuid_copy(dst->uuid, src->uuid);
  memcpy(&dst->addr, &src->addr, src->addrlen);
  memcpy(&dst->last_seen, &src->last_seen, sizeof(src->last_seen));
}
