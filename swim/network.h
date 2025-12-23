#ifndef SWIM_NETWORK_H_
#define SWIM_NETWORK_H_

#include <stdlib.h>

#include <sys/socket.h>

#include "swim/event.h"
#include "swim/state.h"

typedef struct SwimAddress {
  struct sockaddr_storage addr;
  socklen_t addrlen;
} SwimAddress;

extern ssize_t swim_recv_event(SWIM *swim, Event *event, struct sockaddr *addr,
                               socklen_t *addrlen);
extern ssize_t swim_send_event(SWIM *swim, Event *event, struct sockaddr *addr,
                               socklen_t addrlen);
extern ssize_t swim_send_ack(SWIM *swim, uuid_t uuid, struct sockaddr *addr,
                             socklen_t addrlen);
extern ssize_t swim_send_ping(SWIM *swim, struct sockaddr *addr,
                              socklen_t addrlen);
extern ssize_t swim_send_ping_req(SWIM *swim, uuid_t target_uuid,
                                  struct sockaddr *addr, socklen_t addrlen);

#endif /* SWIM_NETWORK_H_ */
