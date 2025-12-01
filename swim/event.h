#ifndef SWIM_EVENT_H_
#define SWIM_EVENT_H_

#include <stdbool.h>

#include <uuid/uuid.h>

/*
 * Type of an event.
 */
typedef enum EventType {
  EVENT_TYPE_PING,
  EVENT_TYPE_ACK,
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
  struct EventHeader hdr;
};

struct AckEvent {
  struct EventHeader hdr;
};

typedef struct Instance {
  uuid_t uuid;
  struct timespec last_seen;
  Status status;
} Instance;

typedef struct Event {
  union {
    struct EventHeader hdr;
    struct PingEvent ping;
    struct AckEvent ack;
  };
  size_t gossip_count;
  Instance gossip_instances[];
} Event;

bool swim_event_string(Event* event, char* buf, size_t);

#endif /* SWIM_EVENT_H_ */
