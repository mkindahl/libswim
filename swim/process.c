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

#include <sys/socket.h>

#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"

/*
 * Process the reception of a PING message.
 *
 * We just respond that we are alive.
 */
static void swim_process_ping(SWIM *swim,
                              __attribute__((__unused__)) Event *event,
                              struct sockaddr *addr, socklen_t addrlen) {
  swim_send_ack(swim, addr, addrlen);
}

static void swim_process_ack(__attribute__((__unused__)) SWIM *swim,
                             __attribute__((__unused__)) Event *event,
                             __attribute__((__unused__)) struct sockaddr *addr,
                             __attribute__((__unused__)) socklen_t addrlen) {}

static void swim_process_join(SWIM *swim, Event *event, struct sockaddr *addr,
                              socklen_t addrlen) {
  struct JoinEvent *join = &event->join;
  Instance instance = {
      .last_seen = event->hdr.time,
      .status = SWIM_STATUS_ALIVE,
  };

  uuid_copy(instance.uuid, join->joining_uuid);
  swim_state_add_instance(swim, &instance);

  swim_send_ack(swim, addr, addrlen);
}

static void swim_process_leave(SWIM *swim, Event *event, struct sockaddr *addr,
                               socklen_t addrlen) {
  struct LeaveEvent *leave = &event->leave;

  swim_state_del_instance(swim, leave->leaving_uuid);
  swim_send_ack(swim, addr, addrlen);
}

struct EventInfo {
  const char *name;
  void (*callback)(SWIM *swim, Event *event, struct sockaddr *addr,
                   socklen_t addrlen);
};

static struct EventInfo swim_event_processing[] = {
    [EVENT_TYPE_PING] = {.name = "PING", .callback = swim_process_ping},
    [EVENT_TYPE_ACK] = {.name = "ACK", .callback = swim_process_ack},
    [EVENT_TYPE_JOIN] = {.name = "JOIN", .callback = swim_process_join},
    [EVENT_TYPE_LEAVE] = {.name = "LEAVE", .callback = swim_process_leave},
};

void swim_process_event(SWIM *swim, Event *event, size_t bytes,
                        struct sockaddr *addr, socklen_t addrlen) {
  char host[NI_MAXHOST], service[NI_MAXSERV];
  struct EventInfo *info = &swim_event_processing[event->hdr.type];
  int s = getnameinfo(addr, addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                      NI_NUMERICSERV);
  if (s != 0) {
    perror("getnameinfo");
    exit(EXIT_FAILURE);
  }

  TRACE("Got %s of %zd bytes from %s:%s", info->name, bytes, host, service);

  /* We notice that we have seen the sending server as well, so that we do not
   * need to ping it unnecessarily. */
  swim_state_update_time(swim, &event->hdr.uuid, &event->hdr.time);
  (*info->callback)(swim, event, addr, addrlen);
}
