#include "swim/network.h"

#include <netdb.h>
#include <stdio.h>

#include <sys/param.h>

#include "swim/defs.h"
#include "swim/event.h"
#include "swim/utils.h"

ssize_t swim_recv_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return recvfrom(swim->sockfd, buf, buflen, 0, addr, &addrlen);
}

ssize_t swim_send_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return sendto(swim->sockfd, buf, buflen, 0, addr, addrlen);
}

/*
 * Helper function for sending an event.
 */

ssize_t swim_send_event(SWIM *swim, Event *event, struct sockaddr *addr,
                        socklen_t addrlen) {
  return swim_send_packet(swim, event, event->hdr.event_size, addr, addrlen);
}

/*
 * Helper function to send ACK.
 */
ssize_t swim_send_ack(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAXGOSSIP);
  Event *event = swim_event_create(swim->uuid, EVENT_TYPE_ACK, gossip_count);

  if (swim->view_size < SWIM_MAXGOSSIP) {
    for (int i = 0; i < swim->view_size; ++i)
      event->gossip_instances[i] = swim->view[i].base;
  } else {
    for (int i = 0; i < gossip_count; ++i)
      event->gossip_instances[i] = swim->view[rand() % swim->view_size].base;
  }

  return swim_send_packet(swim, event, event->hdr.event_size, addr, addrlen);
}

/*
 * Helper function to send PING.
 */
ssize_t swim_send_ping(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAXGOSSIP);
  Event *event = swim_event_create(swim->uuid, EVENT_TYPE_PING, gossip_count);

  if (swim->view_size < SWIM_MAXGOSSIP) {
    for (int i = 0; i < swim->view_size; ++i)
      event->gossip_instances[i] = swim->view[i].base;
  } else {
    for (int i = 0; i < gossip_count; ++i)
      event->gossip_instances[i] = swim->view[rand() % swim->view_size].base;
  }

  return swim_send_packet(swim, &event, event->hdr.event_size, addr, addrlen);
}
