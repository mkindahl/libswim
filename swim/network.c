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
  if (swim->view_size < SWIM_MAX_GOSSIP_SIZE) {
    for (int i = 0; i < swim->view_size; ++i) {
      swim_node_copy(&event->gossip[i], &swim->view[i].info);
      TRACE("uuid %s state %s", swim_uuid_str(event->gossip[i].uuid),
            swim_status_name(event->gossip[i].status));
    }
  } else {
    for (int i = 0; i < count; ++i) {
      swim_node_copy(&event->gossip[i],
                     &swim->view[rand() % swim->view_size].info);
      TRACE("uuid %s state %s", swim_uuid_str(event->gossip[i].uuid),
            swim_status_name(event->gossip[i].status));
    }
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
  LOG("node %s addr %s (%u bytes): <- %s", swim_uuid_str(event->hdr.uuid),
      swim_addr_str(addr, addrlen), event->hdr.event_size,
      swim_event_print(event));
  return sendto(swim->sockfd, event, event->hdr.event_size, 0, addr, addrlen);
}

/*
 * Helper function to send ACK.
 */
ssize_t swim_send_ack(SWIM *swim, uuid_t uuid, struct sockaddr *addr,
                      socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAX_GOSSIP_SIZE);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim->uuid, EVENT_TYPE_ACK, gossip_count);

  uuid_copy(event->ack.ack_uuid, uuid);

  TRACE("from %s gossip of size %d to %s", swim_uuid_str(swim->uuid),
        gossip_count, swim_addr_str(addr, addrlen));

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}

/*
 * Helper function to send PING.
 */
ssize_t swim_send_ping(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAX_GOSSIP_SIZE);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim->uuid, EVENT_TYPE_PING, gossip_count);

  TRACE("sending ping to %s", swim_addr_str(addr, addrlen));

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}

/*
 * Helper function to send PING_REQ.
 */
ssize_t swim_send_ping_req(SWIM *swim, uuid_t target_uuid,
                           struct sockaddr *addr, socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAX_GOSSIP_SIZE);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim->uuid, EVENT_TYPE_PING_REQ, gossip_count);

  uuid_copy(event->ping_req.ping_req_uuid, target_uuid);

  TRACE("sending ping_req for node %s to %s", swim_uuid_str(target_uuid),
        swim_addr_str(addr, addrlen));

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}
