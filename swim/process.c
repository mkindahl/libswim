/*
 * Module to process events.
 *
 * Processing events can involve both changing state of the system as
 * well as sending out one or more other events.
 */

#include "process.h"

#include <assert.h>
#include <netdb.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/time.h>

#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/utils.h"

/*
 * Process the reception of a PING message.
 *
 * We just respond that we are alive.
 */
static void swim_process_ping(SWIM *swim, Event *event,
                              __attribute__((__unused__)) struct sockaddr *addr,
                              __attribute__((__unused__)) socklen_t addrlen) {
  NodeInfo sender = {.status = SWIM_STATUS_ALIVE};

  swim_send_ack(swim, addr, addrlen);

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  time(&sender.last_seen);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
  for (int i = 0; i < event->gossip_count; ++i)
    swim_state_add(swim, &event->gossip[i]);
}

static void swim_process_ack(SWIM *swim, Event *event,
                             __attribute__((__unused__)) struct sockaddr *addr,
                             __attribute__((__unused__)) socklen_t addrlen) {
  NodeInfo sender = {.status = SWIM_STATUS_ALIVE};

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  time(&sender.last_seen);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
  for (int i = 0; i < event->gossip_count; ++i)
    swim_state_add(swim, &event->gossip[i]);
}

/*
 * Process the reception of an JOIN message.
 *
 * When a request to join the cluster is made, we update the cluster
 * information with the new server and send an ACK message to the
 * sender containing partial cluster gossip.
 *
 * The sender can ignore the ACK message if they want and will get
 * gossip information messages later.
 */
static void swim_process_join(SWIM *swim, Event *event, struct sockaddr *addr,
                              socklen_t addrlen) {
  struct JoinEvent *join = &event->join;
  NodeInfo info;

  /*
   * We skip adding the node if we have already added it. This is
   * needed since we can receive a rumor that the node has joined as a
   * result of the forwarding above
   */
  if (swim_state_get(swim, join->join_uuid) != NULL)
    return;

  /* Fill in the address if it was not set by the sender */
  if (event->join.join_addrlen == 0) {
    event->join.join_addrlen = addrlen;
    memcpy(&event->join.join_addr, addr, addrlen);
  }

  swim_node_init(&info, join->join_uuid, (struct sockaddr *)&join->join_addr,
                 join->join_addrlen);
  info.status = SWIM_STATUS_ALIVE;
  info.last_seen = event->hdr.time;

  assert(info.last_seen > 0 && info.status != SWIM_STATUS_UNKNOWN);

  swim_state_add(swim, &info);

  for (int i = 0; i < swim->view_size; ++i) {
    NodeState *node = &swim->view[i];
    swim_send_event(swim, event, (struct sockaddr *)&node->info.addr,
                    swim->view[i].info.addrlen);
  }

  swim_send_ack(swim, addr, addrlen);
}

/*
 * Process the reception of an LEAVE message.
 *
 * When a request to leave the cluster is made, we mark the server and
 * declared dead and send an ack message (with gossip) to the server.
 *
 * The sender can ignore the ACK message if they want, but will not
 * receive any other gossip messages unless the LEAVE message was
 * lost.
 */
static void swim_process_leave(SWIM *swim, Event *event, struct sockaddr *addr,
                               socklen_t addrlen) {
  struct LeaveEvent *leave = &event->leave;

  swim_state_del(swim, leave->leave_uuid);
  swim_send_ack(swim, addr, addrlen);
}

typedef struct EventInfo {
  const char *name;
  void (*callback)(SWIM *swim, Event *event, struct sockaddr *addr,
                   socklen_t addrlen);
} EventInfo;

static struct EventInfo swim_event_processing[] = {
    [EVENT_TYPE_PING] = {.name = "PING", .callback = swim_process_ping},
    [EVENT_TYPE_ACK] = {.name = "ACK", .callback = swim_process_ack},
    [EVENT_TYPE_JOIN] = {.name = "JOIN", .callback = swim_process_join},
    [EVENT_TYPE_LEAVE] = {.name = "LEAVE", .callback = swim_process_leave},
};

void swim_process_event(SWIM *swim, Event *event, size_t bytes,
                        struct sockaddr *addr, socklen_t addrlen) {
  char buf[NI_MAXHOST + NI_MAXSERV + 1];
  EventInfo *info = &swim_event_processing[event->hdr.type];

  TRACE("Got %s of %zd bytes from %s", info->name, bytes,
        addr2str_r(addr, addrlen, buf, sizeof(buf)));

  /* We notice that we have seen the sending server as well, so that we do not
   * need to ping it unnecessarily. */
  swim_state_update_time(swim, event->hdr.uuid, event->hdr.time);
  (*info->callback)(swim, event, addr, addrlen);
}
