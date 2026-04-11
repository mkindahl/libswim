#include "network.h"

#include <assert.h>
#include <memory.h>
#include <netdb.h>
#include <stdlib.h>

#include <sys/param.h>

#include "swim/defs.h"
#include "swim/encoding.h"
#include "swim/event.h"
#include "swim/logging.h"
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
#ifdef SWIM_TRACING
      char ubuf[SWIM_UUID_STR_LEN];
      TRACE("uuid %s state %s",
            swim_uuid_str_r(event->gossip[i].uuid, ubuf, sizeof(ubuf)),
            swim_status_name(event->gossip[i].status));
#endif
    }
  } else {
    for (int i = 0; i < count; ++i) {
      swim_node_copy(&event->gossip[i],
                     &swim->view[rand() % swim->view_size].info);
#ifdef SWIM_TRACING
      char ubuf[SWIM_UUID_STR_LEN];
      TRACE("uuid %s state %s",
            swim_uuid_str_r(event->gossip[i].uuid, ubuf, sizeof(ubuf)),
            swim_status_name(event->gossip[i].status));
#endif
    }
  }
}

/*
 * Receive an event from the network.
 */
ssize_t swim_recv_event(SWIM *swim, Event *event, struct sockaddr *addr,
                        socklen_t *addrlen) {
  unsigned char buf[SWIM_MAX_PACKET_SIZE];
  ssize_t bytes, result;

  bytes = recvfrom(swim->sockfd, buf, sizeof(buf), 0, addr, addrlen);
  if (bytes <= 0)
    return bytes;

  result = swim_decode_event(buf, sizeof(buf), event);
  if (result < 0)
    return result;

  {
    char ubuf[SWIM_UUID_STR_LEN];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    LOG("<-- %s (%ld bytes) node %s addr %s",
        swim_event_print(event),
        bytes,
        swim_uuid_str_r(event->hdr.uuid, ubuf, sizeof(ubuf)),
        swim_addr_str_r(addr, *addrlen, abuf, sizeof(abuf)));
  }

  return result;
}

/*
 * Helper function for sending an event.
 */
ssize_t swim_send_event(SWIM *swim, Event *event, struct sockaddr *addr,
                        socklen_t addrlen) {
  unsigned char buf[SWIM_MAX_PACKET_SIZE];
  ssize_t bytes = swim_encode_event(buf, sizeof(buf), event);

  {
    char ubuf[SWIM_UUID_STR_LEN];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    LOG("--> %s (%ld bytes) node %s addr %s",
        swim_event_print(event),
        bytes,
        swim_uuid_str_r(event->hdr.uuid, ubuf, sizeof(ubuf)),
        swim_addr_str_r(addr, addrlen, abuf, sizeof(abuf)));
  }

  return sendto(swim->sockfd, buf, bytes, 0, addr, addrlen);
}

/*
 * Helper function to send ACK.
 */
ssize_t swim_send_ack(SWIM *swim, uuid_t uuid, struct sockaddr *addr,
                      socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAX_GOSSIP_SIZE);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim, EVENT_TYPE_ACK, gossip_count);

  uuid_copy(event->ack.ack_uuid, uuid);

#ifdef SWIM_TRACING
  {
    char ubuf[SWIM_UUID_STR_LEN];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    TRACE("from %s gossip of size %d to %s",
          swim_uuid_str_r(swim->uuid, ubuf, sizeof(ubuf)),
          gossip_count,
          swim_addr_str_r(addr, addrlen, abuf, sizeof(abuf)));
  }
#endif

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}

/*
 * Helper function to send PING.
 */
ssize_t swim_send_ping(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  const int gossip_count = MIN(swim->view_size, SWIM_MAX_GOSSIP_SIZE);
  Event *event __attribute__((cleanup(cleanup_free))) =
      swim_event_create(swim, EVENT_TYPE_PING, gossip_count);

#ifdef SWIM_TRACING
  {
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    TRACE("sending ping to %s",
          swim_addr_str_r(addr, addrlen, abuf, sizeof(abuf)));
  }
#endif

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
      swim_event_create(swim, EVENT_TYPE_PING_REQ, gossip_count);

  uuid_copy(event->ping_req.ping_req_uuid, target_uuid);

#ifdef SWIM_TRACING
  {
    char ubuf[SWIM_UUID_STR_LEN];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    TRACE("sending ping_req for node %s to %s",
          swim_uuid_str_r(target_uuid, ubuf, sizeof(ubuf)),
          swim_addr_str_r(addr, addrlen, abuf, sizeof(abuf)));
  }
#endif

  swim_fill_gossip(swim, event, gossip_count);

  return swim_send_event(swim, event, addr, addrlen);
}
