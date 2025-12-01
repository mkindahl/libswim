#ifndef SWIM_EVENT_H_
#define SWIM_EVENT_H_

#include <stdbool.h>

#include <uuid/uuid.h>

typedef enum EventType {
  EVENT_TYPE_PING,
  EVENT_TYPE_ACK,
} EventType;

struct EventHeader {
  EventType type;
  uuid_t uuid;
  struct timespec time;
};

struct PingEvent {
  struct EventHeader hdr;
};

struct AckEvent {
  struct EventHeader hdr;
};

struct Event {
  union {
    struct EventHeader hdr;
    struct PingEvent ping;
    struct AckEvent ack;
  };
};

typedef struct Event Event;

bool swim_event_string(Event* event, char* buf, size_t);

#endif /* SWIM_EVENT_H_ */
