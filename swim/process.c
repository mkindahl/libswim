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
#include "swim/swim.h"

static void swim_process_ping(SWIM *swim, Event *event, struct sockaddr *addr,
                              socklen_t addrlen) {
  struct PingEvent *ping = &event->body.ping;
  TRACE("sending ping");
  size_t bytes =
      swim_send_packet(swim, event, sizeof(struct AckEvent), addr, addrlen);
}

static void swim_process_ack(SWIM *swim, Event *event, struct sockaddr *addr,
                             socklen_t addrlen) {
  struct AckEvent *ack = &event->body.ack;
  TRACE("processing ack");
}

struct EventInfo {
  const char *name;
  void (*callback)(SWIM *swim, Event *event, struct sockaddr *addr,
                   socklen_t addrlen);
};

static struct EventInfo swim_event_processing[] = {
    [EVENT_TYPE_PING] =
        {
            .name = "PING",
            .callback = swim_process_ping,
        },
    [EVENT_TYPE_ACK] =
        {
            .name = "ACK",
            .callback = swim_process_ack,
        },
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

  (*info->callback)(swim, event, addr, addrlen);
}
