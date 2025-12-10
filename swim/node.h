#ifndef SWIM_NODE_H_
#define SWIM_NODE_H_

#include <sys/socket.h>
#include <sys/time.h>

#include <uuid/uuid.h>

/*
 * Status of a node.
 *
 * Nodes can be alive, suspected dead, or declared dead.
 *
 * State can also be unknown, which is mostly relevent for servers
 * that are newly added to the cluster, or uninitialized memory.
 */
typedef enum Status {
  SWIM_STATUS_UNKNOWN,
  SWIM_STATUS_ALIVE,
  SWIM_STATUS_SUSPECT,
  SWIM_STATUS_DEAD,
} Status;

/*
 * Instance data that we send out to the cluster.
 */
typedef struct NodeInfo {
  uuid_t uuid;
  time_t last_seen;
  Status status;
  struct sockaddr_storage addr;
  socklen_t addrlen;
} NodeInfo;

extern void swim_node_init(NodeInfo* node, uuid_t uuid, struct sockaddr* addr,
                           socklen_t addrlen);
extern void swim_node_copy(NodeInfo* dst, NodeInfo* src);

#endif /* SWIM_NODE_H_ */
