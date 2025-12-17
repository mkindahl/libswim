#include "event.h"

#include <assert.h>
#include <memory.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>

#include "swim/defs.h"
#include "swim/utils.h"

typedef int print_callback_t(Event* event, char* buf, size_t buflen);

static int swim_print_ping(__attribute__((unused)) Event* event, char* buf,
                           size_t buflen) {
  return snprintf(buf, buflen, "PING");
}

static int swim_print_ack(Event* event, char* buf, size_t buflen) {
  char uuid_buf[40];

  uuid_unparse(event->ack.ack_uuid, uuid_buf);
  return snprintf(buf, buflen, "ACK(%s)", uuid_buf);
}

static int swim_print_ping_req(Event* event, char* buf, size_t buflen) {
  char uuid_buf[40];

  uuid_unparse(event->ping_req.ping_req_uuid, uuid_buf);
  return snprintf(buf, buflen, "PING_REQ(%s)", uuid_buf);
}

static int swim_print_join(Event* event, char* buf, size_t buflen) {
  char uuidbuf[40];

  uuid_unparse(event->join.join_uuid, uuidbuf);
  if (event->join.join_addrlen > 0)
    return snprintf(buf, buflen, "JOIN(%s, %s)", uuidbuf,
                    swim_addr_str((struct sockaddr*)&event->join.join_addr,
                                  event->join.join_addrlen));
  else
    return snprintf(buf, buflen, "JOIN(%s)", uuidbuf);
}

static int swim_print_leave(Event* event, char* buf, size_t buflen) {
  char uuidbuf[40];
  uuid_unparse(event->leave.leave_uuid, uuidbuf);
  return snprintf(buf, buflen, "LEAVE(%s)", uuidbuf);
}

/*
 * Event dispatch structure with information about all events.
 */
static print_callback_t* swim_event[] = {
    [EVENT_TYPE_PING] = swim_print_ping,
    [EVENT_TYPE_PING_REQ] = swim_print_ping_req,
    [EVENT_TYPE_ACK] = swim_print_ack,
    [EVENT_TYPE_JOIN] = swim_print_join,
    [EVENT_TYPE_LEAVE] = swim_print_leave,
};

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
  event->gossip_count = gossip_count;

  return event;
}

void swim_event_init(Event* event, uuid_t uuid, EventType type, size_t size) {
  assert(size < UINT32_MAX);
  memset(event, 0, sizeof(Event));
  event->hdr.version = 1;
  event->hdr.event_size = size;
  event->hdr.type = type;
  time(&event->hdr.time);
  uuid_copy(event->hdr.uuid, uuid);
}

const char* swim_event_print(Event* event) {
  static char buf[128] = {0};
  print_callback_t* print = swim_event[event->hdr.type];
  (void)(*print)(event, buf, sizeof(buf));
  return buf;
}
