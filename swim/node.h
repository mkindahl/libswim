#ifndef SWIM_NODE_H_
#define SWIM_NODE_H_

#include <stdbool.h>
#include <stdint.h>

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
  uint32_t incarnation;
  Status status;
  struct sockaddr_storage addr;
  socklen_t addrlen;
} NodeInfo;

/*
 * Node state data.
 *
 * This stores additional data beyond the normal node info that are
 * present in the events.
 */
typedef struct NodeState {
  NodeInfo info;
  time_t ping_time;
  time_t suspect_time;

  /*
   * Witness for a node suspected dead.
   *
   * This is set when a PING_REQ is sent and if an ACK is received for
   * a node that is suspected dead and has a witness, a response is
   * sent to that witness. The time the node was suspected dead is
   * also recorded and used to decide when a node should be declared
   * dead.
   *
   * Note that this can currently not handle multiple PING_REQ for the
   * same node from different witnesses and we rely on gossip to deal
   * with this.  To handle this properly, we need a list of witnesses
   * and send a request to all of them, but we do not do this right now.
   *
   * Also note that the witness is only used to decide if any other
   * server needs to know about an ack. If a node is suspect for too
   * long, it should be declared dead regardless of whether there is a
   * witness or not.
   */
  struct {
    struct sockaddr_storage addr;
    socklen_t addrlen;
  } witness;
} NodeState;

extern void swim_node_init(NodeInfo* node, uuid_t uuid, struct sockaddr* addr,
                           socklen_t addrlen);
extern void swim_node_copy(NodeInfo* dst, NodeInfo* src);
extern bool swim_node_has_witness(NodeState* node);
extern void swim_node_reset_witness(NodeState* node);
extern void swim_node_reset(NodeState* node);

#endif /* SWIM_NODE_H_ */
