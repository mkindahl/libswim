/*
 * Module: Encoding
 *
 * Module that handles encoding and decoding data types for transfer
 * over the network.
 *
 * We are using little-endian format for integers since it is more
 * efficient to encode and decode on a majority of platforms. It is
 * also easier to decode if you want to "downcast" to a type with
 * smaller range.
 */

#ifndef SWIM_ENCODING_H_
#define SWIM_ENCODING_H_

#include <assert.h>
#include <endian.h> /* Using it for now */
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <arpa/inet.h>
#include <sys/socket.h>

#include "swim/event.h"

/*
 * Encoder and decoder for primitive types.
 *
 * We are sticking to types that support POSIX.1-2008 for now, since
 * endian(3) is not supported on non-libc platforms.
 */

#define SWIM_MAKE_ENCODER(TYPE, ENCODE)                              \
  static inline ssize_t swim_encode_##TYPE(void* buf, size_t buflen, \
                                           TYPE##_t val) {           \
    assert(buflen >= sizeof(TYPE##_t));                              \
    *(TYPE##_t*)buf = (TYPE##_t)ENCODE(val);                         \
    return sizeof(TYPE##_t);                                         \
  }                                                                  \
  extern int _unused_variable

#define SWIM_MAKE_DECODER(TYPE, DECODE)                              \
  static inline ssize_t swim_decode_##TYPE(void* buf, size_t buflen, \
                                           TYPE##_t* val) {          \
    assert(buflen >= sizeof(TYPE##_t));                              \
    *val = (TYPE##_t)DECODE(*(TYPE##_t*)buf);                        \
    return sizeof(TYPE##_t);                                         \
  }                                                                  \
  extern int _unused_variable

SWIM_MAKE_DECODER(int16, ntohs);
SWIM_MAKE_DECODER(int32, ntohl);
SWIM_MAKE_DECODER(int64, be64toh);
SWIM_MAKE_DECODER(uint16, ntohs);
SWIM_MAKE_DECODER(uint32, ntohl);
SWIM_MAKE_DECODER(uint64, be64toh);
SWIM_MAKE_ENCODER(int16, htons);
SWIM_MAKE_ENCODER(int32, htonl);
SWIM_MAKE_ENCODER(int64, htobe64);
SWIM_MAKE_ENCODER(uint16, htons);
SWIM_MAKE_ENCODER(uint32, htonl);
SWIM_MAKE_ENCODER(uint64, htobe64);

static inline ssize_t swim_encode_fixlen(unsigned char* buf, size_t buflen,
                                         const void* data, size_t bytes) {
  assert(buflen >= bytes);
  memcpy(buf, data, bytes);
  return bytes;
}

static inline ssize_t swim_decode_fixlen(unsigned char* buf, size_t buflen,
                                         void* data, size_t bytes) {
  assert(buflen >= bytes);
  memcpy(data, buf, bytes);
  return bytes;
}

extern ssize_t swim_encode_sockaddr(unsigned char* buf, size_t buflen,
                                    struct sockaddr* addr, socklen_t addrlen);
extern ssize_t swim_decode_sockaddr(unsigned char* buf, size_t buflen,
                                    struct sockaddr* addr, socklen_t* addrlen);
extern ssize_t swim_encode_event(unsigned char* buf, size_t buflen,
                                 Event* event);
extern ssize_t swim_decode_event(unsigned char* buf, size_t buflen,
                                 Event* event);
extern ssize_t swim_encode_varlen(unsigned char* buf, size_t buflen,
                                  const void* data, int16_t length);
extern ssize_t swim_decode_varlen(unsigned char* buf, size_t buflen, void* data,
                                  int16_t* length);
extern ssize_t swim_encode_uuid(unsigned char* buf, size_t buflen, uuid_t uuid);
extern ssize_t swim_decode_uuid(unsigned char* buf, size_t buflen, uuid_t uuid);
extern ssize_t swim_encode_event_header(unsigned char* buf, size_t buflen,
                                        EventHeader* header);
extern ssize_t swim_decode_event_header(unsigned char* buf, size_t buflen,
                                        EventHeader* header);
extern ssize_t swim_encode_gossip(unsigned char* buf, size_t buflen,
                                  NodeInfo* gossip, uint16_t gossip_count);
extern ssize_t swim_decode_gossip(unsigned char* buf, size_t buflen,
                                  NodeInfo* gossip, uint16_t* gossip_count);

#endif /* SWIM_ENCODING_H_ */
