/*
 * Module to process events.
 *
 * Processing events can involve both changing state of the system as
 * well as sending out one or more other events.
 */

#include "process.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
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
  InstanceData sender = {.status = SWIM_STATUS_ALIVE};

  swim_send_ack(swim, addr, addrlen);

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  gettimeofday(&sender.last_seen, NULL);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
  for (int i = 0; i < event->hdr.gossip_count; ++i)
    swim_state_add(swim, &event->gossip_instances[i]);
}

static void swim_process_ack(SWIM *swim, Event *event,
                             __attribute__((__unused__)) struct sockaddr *addr,
                             __attribute__((__unused__)) socklen_t addrlen) {
  InstanceData sender = {.status = SWIM_STATUS_ALIVE};

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  gettimeofday(&sender.last_seen, NULL);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
  for (int i = 0; i < event->hdr.gossip_count; ++i)
    swim_state_add(swim, &event->gossip_instances[i]);
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
  InstanceData instance = {
      .last_seen = event->hdr.time,
      .status = SWIM_STATUS_ALIVE,
  };

  memcpy(&instance.addr, addr, addrlen);
  instance.addrlen = addrlen;
  uuid_copy(instance.uuid, join->join_uuid);

  swim_state_add(swim, &instance);
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
  swim_state_update_time(swim, event->hdr.uuid, &event->hdr.time);
  (*info->callback)(swim, event, addr, addrlen);
}
