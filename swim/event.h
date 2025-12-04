#ifndef SWIM_EVENT_H_
#define SWIM_EVENT_H_

#include <stdbool.h>

#include <sys/socket.h>

#include "swim/node.h"

#include <uuid/uuid.h>

/*
 * Type of an event.
 *
 * This includes all events that can change state, not only messages
 * received. We do not distinguish between notification-style messages
 * (such as JOIN and LEAVE) and cluster events (such as PING and ACK).
 */
typedef enum EventType {
  EVENT_TYPE_PING,
  EVENT_TYPE_ACK,
  EVENT_TYPE_JOIN,
  EVENT_TYPE_LEAVE,
} EventType;

typedef struct EventHeader {
  size_t event_size;
  EventType type;
  uuid_t uuid;
  struct timeval time;
  int gossip_count;
} EventHeader;

struct PingEvent {
  EventHeader hdr;
};

struct AckEvent {
  EventHeader hdr;
};

struct JoinEvent {
  EventHeader hdr;
  uuid_t join_uuid;
  socklen_t join_addrlen;
  struct sockaddr_storage join_addr;
};

struct LeaveEvent {
  EventHeader hdr;
  uuid_t leave_uuid;
};

/*
 * Event with gossip.
 *
 * Caveat:
 *
 *   Since the minimum MTU for a UDP packet is quite small (508 bytes,
 *   if you do not count the IP header), we should not attach gossip
 *   data and rather have separate messages for gossip.
 */
typedef struct Event {
  union {
    EventHeader hdr;
    struct PingEvent ping;
    struct AckEvent ack;
    struct JoinEvent join;
    struct LeaveEvent leave;
  };

  NodeInfo gossip[];
} Event;

extern bool swim_event_string(Event* event, char* buf, size_t);
extern void swim_event_init(Event* event, uuid_t uuid, EventType type,
                            size_t size);
extern Event* swim_event_create(uuid_t uuid, EventType type,
                                size_t gossip_count);

#endif /* SWIM_EVENT_H_ */
