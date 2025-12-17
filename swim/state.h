#ifndef SWIM_STATE_H_
#define SWIM_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include <netinet/in.h>

#include "swim/event.h"

/*
 * Node state data.
 *
 * This stores additional data beyond the normal node data that are
 * present in the events. In particular, address of the node as
 * seen when receiving an event from a joining node.
 */
typedef struct NodeState {
  NodeInfo info;
} NodeState;

/*
 * SWIM state including current view.
 *
 * The view does not include the node itself since that is unnecessary
 * to track.
 */
typedef struct SWIM {
  int sockfd;
  struct sockaddr_in addr;
  uuid_t uuid;
  int view_capacity;
  int view_size;
  NodeState* view;
} SWIM;

extern void swim_state_add(SWIM* swim, NodeInfo* info);
extern NodeState* swim_state_get_node(SWIM* swim, uuid_t uuid);
extern bool swim_state_init(SWIM* swim, uint16_t port);
extern void swim_state_del(SWIM* swim, uuid_t);
extern void swim_state_print(SWIM* swim);
extern void swim_state_set_status(SWIM* swim, uuid_t uuid, Status status);
extern void swim_state_update_time(SWIM* swim, uuid_t uuid, time_t time);
extern void swim_state_merge(SWIM* swim, NodeInfo* info);
extern const char* swim_status_name(Status status);

#endif /* SWIM_STATE_H_ */
