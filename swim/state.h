#ifndef SWIM_STATE_H_
#define SWIM_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include <netinet/in.h>

#include "swim/node.h"

/*
 * SWIM state including current view.
 *
 * The view does not include the node itself since that is unnecessary
 * to track.
 *
 * The server state is opaque and is dependent on the server
 * implementation. See the corresponding server implementation file.
 */
typedef struct SWIM {
  int sockfd;
  time_t last_heartbeat;
  struct sockaddr_in addr;
  uuid_t uuid;
  uint32_t incarnation;
  int view_capacity;
  int view_size;
  NodeState* view;
  void* server;
} SWIM;

extern NodeState* swim_state_add(SWIM* swim, NodeInfo* info);
extern NodeState* swim_state_get_node(SWIM* swim, uuid_t uuid);
extern NodeState* swim_state_suspect(SWIM* swim, uuid_t uuid,
                                     struct sockaddr* addr, socklen_t addrlen);
extern bool swim_state_init(SWIM* swim, uint16_t port);
extern NodeState* swim_state_del(SWIM* swim, uuid_t);
extern NodeState* swim_state_notice(SWIM* swim, uuid_t uuid, time_t time,
                                    uint32_t incarnation);
extern void swim_state_print(SWIM* swim);
extern void swim_state_set_status(SWIM* swim, uuid_t uuid, Status status);
extern void swim_state_merge(SWIM* swim, NodeInfo* info);
extern const char* swim_status_name(Status status);

#endif /* SWIM_STATE_H_ */
