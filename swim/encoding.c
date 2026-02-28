#include "encoding.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>

#include "swim/defs.h"
#include "swim/event.h"

ssize_t swim_encode_varlen(unsigned char* buf, size_t buflen, const void* data,
                           int16_t length) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  assert(buflen >= (size_t)length);
  assert(length < INT16_MAX);

  ptr += swim_encode_int16(ptr, end - ptr, length);
  ptr += swim_encode_fixlen(ptr, end - ptr, data, length);

  return ptr - buf;
}

ssize_t swim_decode_varlen(unsigned char* buf, size_t buflen, void* data,
                           int16_t* length) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_int16(ptr, end - ptr, length);
  assert(*length >= 0 && (size_t)*length <= buflen);
  ptr += swim_decode_fixlen(ptr, end - ptr, data, *length);

  return ptr - buf;
}

ssize_t swim_encode_uuid(unsigned char* buf, size_t buflen, uuid_t uuid) {
  return swim_encode_fixlen(buf, buflen, uuid, sizeof(uuid_t));
}

ssize_t swim_decode_uuid(unsigned char* buf, size_t buflen, uuid_t uuid) {
  return swim_decode_fixlen(buf, buflen, uuid, sizeof(uuid_t));
}

ssize_t swim_encode_sockaddr(unsigned char* buf, size_t buflen,
                             struct sockaddr* addr, socklen_t addrlen) {
  return swim_encode_varlen(buf, buflen, addr, addrlen);
}

ssize_t swim_decode_sockaddr(unsigned char* buf, size_t buflen,
                             struct sockaddr* addr, socklen_t* addrlen) {
  int16_t bytes;
  ssize_t result = swim_decode_varlen(buf, buflen, addr, &bytes);
  *addrlen = bytes;
  return result;
}

ssize_t swim_encode_event_header(unsigned char* buf, size_t buflen,
                                 EventHeader* header) {
  uint64_t* plen;
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uint16(ptr, end - ptr, SWIM_PROTOCOL_VERSION);
  ptr += swim_encode_uint16(ptr, end - ptr, header->type);

  /* Write a placeholder for the size and save a pointer to the
   * length. We need to fill it in last */
  plen = (uint64_t*)ptr;
  ptr += swim_encode_uint32(ptr, end - ptr, 0);
  ptr += swim_encode_int64(ptr, end - ptr, header->time);
  ptr += swim_encode_uuid(ptr, end - ptr, header->uuid);
  ptr += swim_encode_uint32(ptr, end - ptr, header->incarnation);

  *plen = ptr - buf;

  return *plen;
}

ssize_t swim_decode_event_header(unsigned char* buf, size_t buflen,
                                 EventHeader* header) {
  uint32_t event_size;
  uint16_t protocol_version;
  uint16_t type;
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uint16(ptr, end - ptr, &protocol_version);
  ptr += swim_decode_uint16(ptr, end - ptr, &type);
  header->type = type;
  ptr += swim_decode_uint32(ptr, end - ptr, &event_size);
  ptr += swim_decode_int64(ptr, end - ptr, &header->time);
  ptr += swim_decode_uuid(ptr, end - ptr, header->uuid);
  ptr += swim_decode_uint32(ptr, end - ptr, &header->incarnation);

  return ptr - buf;
}

static ssize_t swim_encode_event_ack(unsigned char* buf, size_t buflen,
                                     struct AckEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uuid(ptr, end - ptr, event->ack_uuid);

  return ptr - buf;
}

static ssize_t swim_decode_event_ack(unsigned char* buf, size_t buflen,
                                     struct AckEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uuid(ptr, end - ptr, event->ack_uuid);

  return ptr - buf;
}

static ssize_t swim_encode_event_join(unsigned char* buf, size_t buflen,
                                      struct JoinEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uuid(ptr, end - ptr, event->join_uuid);
  ptr += swim_encode_sockaddr(
      ptr, end - ptr, (struct sockaddr*)&event->join_addr, event->join_addrlen);

  return ptr - buf;
}

static ssize_t swim_decode_event_join(unsigned char* buf, size_t buflen,
                                      struct JoinEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uuid(ptr, end - ptr, event->join_uuid);
  ptr +=
      swim_decode_sockaddr(ptr, end - ptr, (struct sockaddr*)&event->join_addr,
                           &event->join_addrlen);
  return ptr - buf;
}

static ssize_t swim_encode_event_leave(unsigned char* buf, size_t buflen,
                                       struct LeaveEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uuid(ptr, end - ptr, event->leave_uuid);

  return ptr - buf;
}

static ssize_t swim_decode_event_leave(unsigned char* buf, size_t buflen,
                                       struct LeaveEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uuid(ptr, end - ptr, event->leave_uuid);

  return ptr - buf;
}

static ssize_t swim_encode_event_ping_req(unsigned char* buf, size_t buflen,
                                          struct PingReqEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uuid(ptr, end - ptr, event->ping_req_uuid);

  return ptr - buf;
}

static ssize_t swim_decode_event_ping_req(unsigned char* buf, size_t buflen,
                                          struct PingReqEvent* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uuid(ptr, end - ptr, event->ping_req_uuid);

  return ptr - buf;
}

ssize_t swim_encode_event(unsigned char* buf, size_t buflen, Event* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_event_header(ptr, end - ptr, &event->hdr);

  switch (event->hdr.type) {
    /* These events have no extra information */
    case EVENT_TYPE_PING:
      break;

    case EVENT_TYPE_ACK:
      ptr += swim_encode_event_ack(ptr, end - ptr, &event->ack);
      break;

    case EVENT_TYPE_JOIN:
      ptr += swim_encode_event_join(ptr, end - ptr, &event->join);
      break;

    case EVENT_TYPE_LEAVE:
      ptr += swim_encode_event_leave(ptr, end - ptr, &event->leave);
      break;

    case EVENT_TYPE_PING_REQ:
      ptr += swim_encode_event_ping_req(ptr, end - ptr, &event->ping_req);
      break;

    default:
      return -1;
  }

  ptr += swim_encode_gossip(ptr, end - ptr, event->gossip, event->gossip_count);

  return ptr - buf;
}

ssize_t swim_encode_info(unsigned char* buf, size_t buflen, NodeInfo* info) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_fixlen(ptr, end - ptr, info->uuid, sizeof(uuid_t));
  ptr += swim_encode_int64(ptr, end - ptr, info->last_seen);
  ptr += swim_encode_uint16(ptr, end - ptr, info->status);
  ptr += swim_encode_uint32(ptr, end - ptr, info->incarnation);
  ptr += swim_encode_sockaddr(ptr, end - ptr, (struct sockaddr*)&info->addr,
                              info->addrlen);

  return ptr - buf;
}

ssize_t swim_decode_info(unsigned char* buf, size_t buflen, NodeInfo* info) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;
  uint16_t status;

  ptr += swim_decode_fixlen(ptr, end - ptr, info->uuid, sizeof(uuid_t));
  ptr += swim_decode_int64(ptr, end - ptr, &info->last_seen);
  ptr += swim_decode_uint16(ptr, end - ptr, &status);
  ptr += swim_decode_uint32(ptr, end - ptr, &info->incarnation);
  ptr += swim_decode_sockaddr(ptr, end - ptr, (struct sockaddr*)&info->addr,
                              &info->addrlen);

  info->status = status;

  return ptr - buf;
}

ssize_t swim_encode_gossip(unsigned char* buf, size_t buflen, NodeInfo* gossip,
                           uint16_t gossip_count) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_encode_uint16(ptr, end - ptr, gossip_count);

  for (int i = 0; i < gossip_count; ++i)
    ptr += swim_encode_info(ptr, end - ptr, &gossip[i]);
  return ptr - buf;
}

ssize_t swim_decode_gossip(unsigned char* buf, size_t buflen, NodeInfo* gossip,
                           uint16_t* gossip_count, uint16_t gossip_capacity) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;

  ptr += swim_decode_uint16(ptr, end - ptr, gossip_count);

  if (*gossip_count > gossip_capacity) {
    fprintf(stderr, "gossip_count %u exceeds capacity %u, dropping message\n",
            *gossip_count, gossip_capacity);
    return -1;
  }

  for (int i = 0; i < *gossip_count; ++i)
    ptr += swim_decode_info(ptr, end - ptr, &gossip[i]);
  return ptr - buf;
}

ssize_t swim_decode_event(unsigned char* buf, size_t buflen, Event* event) {
  unsigned char* ptr = buf;
  unsigned char* end = buf + buflen;
  ptr += swim_decode_event_header(ptr, end - ptr, &event->hdr);

  switch (event->hdr.type) {
    /* These events have no extra information */
    case EVENT_TYPE_PING:
      break;

    case EVENT_TYPE_ACK:
      ptr += swim_decode_event_ack(ptr, end - ptr, &event->ack);
      break;

    case EVENT_TYPE_JOIN:
      ptr += swim_decode_event_join(ptr, end - ptr, &event->join);
      break;

    case EVENT_TYPE_LEAVE:
      ptr += swim_decode_event_leave(ptr, end - ptr, &event->leave);
      break;

    case EVENT_TYPE_PING_REQ:
      ptr += swim_decode_event_ping_req(ptr, end - ptr, &event->ping_req);
      break;

    default:
      return -1;
  }

  uint16_t gossip_capacity =
      (buflen - (ptr - buf) - sizeof(uint16_t)) / sizeof(NodeInfo);
  ssize_t gossip_bytes = swim_decode_gossip(
      ptr, end - ptr, event->gossip, &event->gossip_count, gossip_capacity);
  if (gossip_bytes < 0)
    return -1;
  ptr += gossip_bytes;

  return ptr - buf;
}
