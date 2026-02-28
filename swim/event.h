#ifndef SWIM_EVENT_H_
#define SWIM_EVENT_H_

#include <stdbool.h>
#include <stdint.h>

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
  EVENT_TYPE_PING_REQ,

  /* Rumor events */
  EVENT_TYPE_JOIN = 16,
  EVENT_TYPE_LEAVE,
  MAX_EVENT_TYPE
} EventType;

/*
 * Struct: EventHeader
 */
typedef struct EventHeader {
  EventType type;          /* Type of the event */
  uuid_t uuid;             /* UUID of the event sender */
  time_t time;             /* Time when the event was sent */
  uint32_t incarnation;    /* Incarnation number of the sender */
} EventHeader;

struct PingEvent {
  EventHeader hdr;
};

struct PingReqEvent {
  EventHeader hdr;
  uuid_t ping_req_uuid;
};

/*
 * Struct: Acknowledge event
 *
 * The acknowledge event can be related to an arbitrary node and can
 * either be the response to a PING event or a PING_REQ event.
 */
struct AckEvent {
  EventHeader hdr;
  uuid_t ack_uuid;
};

/*
 * Join event.
 *
 * The node sending out the join event usually do not know it's
 * address, but this is filled in before forwarding the join event to
 * the rest of the cluster.
 */
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
    struct PingReqEvent ping_req;
    struct AckEvent ack;
    struct JoinEvent join;
    struct LeaveEvent leave;
  };

  uint16_t gossip_count;
  NodeInfo gossip[];
} Event;

typedef struct SWIM SWIM;

extern bool swim_event_string(Event* event, char* buf, size_t);
extern void swim_event_init(Event* event, SWIM* swim, EventType type);
extern Event* swim_event_create(SWIM* swim, EventType type, int gossip_count);
extern const char* swim_event_print(Event* event);

#endif /* SWIM_EVENT_H_ */
