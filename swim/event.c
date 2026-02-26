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
  return snprintf(buf, buflen, "ACK(%s)", swim_uuid_str(event->ack.ack_uuid));
}

static int swim_print_ping_req(Event* event, char* buf, size_t buflen) {
  return snprintf(buf, buflen, "PING_REQ(%s)",
                  swim_uuid_str(event->ping_req.ping_req_uuid));
}

static int swim_print_join(Event* event, char* buf, size_t buflen) {
  if (event->join.join_addrlen > 0)
    return snprintf(buf, buflen, "JOIN(%s, %s)",
                    swim_uuid_str(event->join.join_uuid),
                    swim_addr_str((struct sockaddr*)&event->join.join_addr,
                                  event->join.join_addrlen));
  else
    return snprintf(buf, buflen, "JOIN(%s)",
                    swim_uuid_str(event->join.join_uuid));
}

static int swim_print_leave(Event* event, char* buf, size_t buflen) {
  return snprintf(buf, buflen, "LEAVE(%s)",
                  swim_uuid_str(event->leave.leave_uuid));
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

/*
 * Note: gossip_count is always bounded by SWIM_MAX_GOSSIP_SIZE at all
 * call sites, so the size calculation below cannot overflow in practice.
 * If new callers are added, they must ensure gossip_count is non-negative
 * and small enough to avoid wrapping event_size.
 */
Event* swim_event_create(uuid_t uuid, EventType type, int gossip_count) {
  const size_t event_size = sizeof(Event) + gossip_count * sizeof(NodeInfo);

  assert(gossip_count >= 0 && gossip_count <= SWIM_MAX_GOSSIP_SIZE);
  Event* event;

  assert(event_size < SWIM_MAX_PACKET_SIZE);

  event = malloc(event_size);
  if (event == NULL)
    return NULL;

  memset(event, 0, event_size);

  swim_event_init(event, uuid, type);
  event->gossip_count = gossip_count;

  return event;
}

void swim_event_init(Event* event, uuid_t uuid, EventType type) {
  memset(event, 0, sizeof(Event));
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
