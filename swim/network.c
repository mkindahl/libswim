#include "network.h"

#include <assert.h>
#include <memory.h>
#include <netdb.h>
#include <stdlib.h>

#include <sys/param.h>

#include "swim/debug.h"
#include "swim/defs.h"
#include "swim/event.h"
#include "swim/utils.h"

static void cleanup_free(void *ptr) {
  void **p = (void **)ptr;
  free(*p);
}

/*
 * Helper function to attach gossip to an event.
 */
static void swim_fill_gossip(SWIM *swim, Event *event, int count) {
  assert(event->gossip_count >= count);
  if (swim->view_size < SWIM_MAXGOSSIP) {
    for (int i = 0; i < swim->view_size; ++i)
      swim_node_copy(&event->gossip[i], &swim->view[i].info);
  } else {
    for (int i = 0; i < count; ++i)
      swim_node_copy(&event->gossip[i],
                     &swim->view[rand() % swim->view_size].info);
  }
}

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
  char buf[NI_MAXHOST + NI_MAXSERV + 1];
  char uuidbuf[40];

  uuid_unparse(event->hdr.uuid, uuidbuf);
  LOG("node %s addr %s (%lu bytes): <- %s", uuidbuf,
      addr2str_r(addr, addrlen, buf, sizeof(buf)), event->hdr.event_size,
      swim_event_print(event));
  return sendto(swim->sockfd, event, event->hdr.event_size, 0, addr, addrlen);
}

/*
 * Helper function to send ACK.
 */
ssize_t swim_send_ack(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  char addrbuf[NI_MAXHOST + NI_MAXSERV + 1];
  const int gossip_count = MIN(swim->view_size, SWIM_MAXGOSSIP);
  __attribute__((cleanup(cleanup_free))) Event *event =
      swim_event_create(swim->uuid, EVENT_TYPE_ACK, gossip_count);

  TRACE("Sending ACK with gossip of size %d to %s", gossip_count,
        addr2str_r(addr, addrlen, addrbuf, sizeof(addrbuf)));

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}

/*
 * Helper function to send PING.
 */
ssize_t swim_send_ping(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  char buf[128] = {0};
  const int gossip_count = MIN(swim->view_size, SWIM_MAXGOSSIP);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim->uuid, EVENT_TYPE_PING, gossip_count);

  TRACE("sending ping to %s", addr2str_r(addr, addrlen, buf, sizeof(buf)));

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}

const char *swim_getaddr_r(struct sockaddr *addr, socklen_t addrlen, char *buf,
                           size_t buflen) {
  char host[NI_MAXHOST], service[NI_MAXSERV];
  if (getnameinfo(addr, addrlen, host, sizeof(host), service, sizeof(service),
                  NI_NUMERICSERV) == 1)
    return NULL;

  snprintf(buf, buflen, "%s:%s", host, service);

  return buf;
}
