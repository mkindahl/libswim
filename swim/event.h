#ifndef SWIM_EVENT_H_
#define SWIM_EVENT_H_

#include <stdbool.h>

#include <sys/socket.h>

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

/*
 * Status of an instance.
 *
 * Instances can be alive, suspected dead, or declared dead.
 *
 * State can also be unknown, which is mostly relevent for servers
 * that are newly added to the cluster.
 */
typedef enum Status {
  SWIM_STATUS_UNKNOWN,
  SWIM_STATUS_ALIVE,
  SWIM_STATUS_SUSPECT,
  SWIM_STATUS_DEAD,
} Status;

typedef struct EventHeader {
  EventType type;
  uuid_t uuid;
  struct timespec time;
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
 * Instance data that we send out to the cluster.
 */
typedef struct InstanceData {
  uuid_t uuid;
  struct timespec last_seen;
  Status status;
} InstanceData;

typedef struct Event {
  union {
    EventHeader hdr;
    struct PingEvent ping;
    struct AckEvent ack;
    struct JoinEvent join;
    struct LeaveEvent leave;
  };
  size_t gossip_count;
  InstanceData instances_gossip[];
} Event;

bool swim_event_string(Event* event, char* buf, size_t);
void swim_event_init(uuid_t uuid, Event* event, EventType type);

#endif /* SWIM_EVENT_H_ */
