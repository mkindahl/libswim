#include "swim/event.h"

#include <memory.h>
#include <stdio.h>

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
    default:
      return false;
  }
  uuid_unparse(event->hdr.uuid, ptr);
  return true;
}
