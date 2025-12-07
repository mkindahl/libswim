#include "swim/event.h"

#include <assert.h>
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

#include "swim/defs.h"

bool swim_event_decode(Event* event, const char* buf, size_t buflen) {
  memcpy(event, buf, buflen);
  return true;
}

bool swim_event_string(Event* event, char* buf, size_t buflen) {
  char* ptr = buf;
  memset(buf, 0, buflen);
  switch (event->hdr.type) {
    case EVENT_TYPE_PING:
      ptr += sprintf(ptr, "ping from ");
      break;
    case EVENT_TYPE_ACK:
      ptr += sprintf(ptr, "ack from ");
      break;
    case EVENT_TYPE_JOIN:
      ptr += sprintf(ptr, "join from ");
      break;
    case EVENT_TYPE_LEAVE:
      ptr += sprintf(ptr, "leave from ");
      break;
    default:
      return false;
  }
  uuid_unparse(event->hdr.uuid, ptr);
  return true;
}

Event* swim_event_create(uuid_t uuid, EventType type, size_t gossip_count) {
  const size_t event_size = sizeof(Event) + gossip_count * sizeof(NodeInfo);
  Event* event = malloc(event_size);

  assert(event_size < SWIM_MAXPACKET);

  memset(event, 0, event_size);

  swim_event_init(event, uuid, type, event_size);
  event->hdr.gossip_count = gossip_count;

  return event;
}

void swim_event_init(Event* event, uuid_t uuid, EventType type, size_t size) {
  event->hdr.type = type;
  event->hdr.event_size = size;
  time(&event->hdr.time);
  uuid_copy(event->hdr.uuid, uuid);
}
