#include "encoding.h"
#include "test.h"

#include <inttypes.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "swim/defs.h"
#include "swim/event.h"

#define MAKE_TEST(TYPE, PR)                          \
  static void test_encoding_##TYPE(TYPE##_t value) { \
    TYPE##_t actual;                                 \
    unsigned char buf[sizeof(TYPE##_t)];             \
    swim_encode_##TYPE(buf, sizeof(buf), value);     \
    swim_decode_##TYPE(buf, sizeof(buf), &actual);   \
    EXPECT_EQ_INT(actual, value, PR);                \
  }                                                  \
  extern int _undefined_variable

MAKE_TEST(uint32, PRIu32);
MAKE_TEST(uint16, PRIu16);

#define LENGTH_OF(V) ((int)(sizeof(V) / sizeof((V)[0])))

static void test_encoding_uuid(uuid_t uuid) {
  uuid_t actual;
  unsigned char buf[sizeof(uuid_t)];

  swim_encode_uuid(buf, sizeof(buf), uuid);
  swim_decode_uuid(buf, sizeof(buf), actual);
  EXPECT_TRUE(uuid_compare(uuid, actual) == 0);
}

static void test_encoding_fixlen(const char* data, size_t bytes) {
  char actual[bytes];
  unsigned char buf[128];

  swim_encode_fixlen(buf, sizeof(buf), data, bytes);
  swim_decode_fixlen(buf, sizeof(buf), actual, bytes);
  EXPECT_EQ_STR(data, actual, bytes);
}

static void test_encoding_varlen(const char* data, int16_t bytes) {
  char actual[128];
  int16_t length;
  unsigned char buf[128];

  swim_encode_varlen(buf, sizeof(buf), data, bytes);
  swim_decode_varlen(buf, sizeof(buf), actual, &length);
  EXPECT_EQ_INT(bytes, length, PRIi16);
  EXPECT_EQ_STR((const char*)data, actual, length);
}

static void test_encoding_header(EventHeader* header) {
  unsigned char buf[SWIM_MAX_PACKET_SIZE];
  EventHeader actual;

  swim_encode_event_header(buf, sizeof(buf), header);
  swim_decode_event_header(buf, sizeof(buf), &actual);
  EXPECT_EQ_INT(header->type, actual.type, PRId32);
  EXPECT_EQ_UUID(header->uuid, actual.uuid);
  EXPECT_EQ_INT(header->time, actual.time, PRId64);
}

static void test_encoding_sockaddr(struct sockaddr* addr, socklen_t addrlen) {
  unsigned char buf[SWIM_MAX_PACKET_SIZE];
  struct sockaddr_storage actual;
  socklen_t actual_len;
  struct sockaddr_in* paddr;

  swim_encode_sockaddr(buf, sizeof(buf), addr, addrlen);
  swim_decode_sockaddr(buf, sizeof(buf), (struct sockaddr*)&actual,
                       &actual_len);
  EXPECT_EQ_INT(addrlen, actual_len, PRIi32);
  EXPECT_EQ_INT(addr->sa_family, actual.ss_family, PRIu16);
  switch (addr->sa_family) {
    case AF_INET:
      paddr = (struct sockaddr_in*)&actual;
      EXPECT_EQ_INT(paddr->sin_port, paddr->sin_port, PRIu16);
      EXPECT_EQ_INT(paddr->sin_addr.s_addr, paddr->sin_addr.s_addr, PRIu32);
      break;
    default:
      FAILURE("not covered");
      break;
  }
}

int main(void) {
  const char* uuid_strings[] = {
      "d79b445d-dda9-4556-96b9-c42bc00ffde7",
      "a609e73c-cb49-4b5e-977c-a0c28bd17f6a",
      "ecaed8ba-140f-432a-9bc2-c128c2914769",
      "174aa260-e44b-4925-a974-5651465ca210",
      "26de9634-025c-4fd5-9d9a-3a2a5dd35ebd",
      "e10ba08d-6853-46b7-8363-c11f03ad95fc",
      "c0fc7074-b50e-4481-8b22-f5ef3615c6b2",
      "2b707ae5-e280-4060-8349-a11386db91fe",
      "ca6a667d-3b21-4c8d-8dc0-02a63fe80d3a",
      "2f6f1dba-6e9f-456c-be89-dd0558ad08f4",
  };

  do {
    uint32_t values[] = {1234567890UL, 0UL};
    for (size_t i = 0; i < LENGTH_OF(values); ++i)
      test_encoding_uint32(values[i]);
  } while (0);

  do {
    uint16_t values[] = {12345UL, 0UL};
    for (size_t i = 0; i < LENGTH_OF(values); ++i)
      test_encoding_uint16(values[i]);
  } while (0);

  do {
    for (size_t i = 0; i < LENGTH_OF(uuid_strings); ++i) {
      uuid_t uuid;
      uuid_parse(uuid_strings[i], uuid);
      test_encoding_uuid(uuid);
    }
  } while (0);

  do {
    const char* values[] = {
        "",
        "1",
        "d79b445d-dda9-96b9-c42bc00ffde7",
        "a609e73c-cb49-4b5e-977c-a0c28bd17f6a",
        "ecaed8ba-140f-432a-9bc2-c128c2914769",
        "174aa260-e44b-4925-a974-5651465ca210",
        "26de9634-025c-4fd5-9d9a-3a2a5dd35ebd",
        "e10ba08d-6853-46b7-8363-c11f03ad95fc",
        "c0fc7074-b50e-8b22-f5ef3615c6b2",
        "2b707ae5-e280-4060-8349-a11386db91fe",
        "ca6a667d-3b21-4c8d-8dc0-02a63fe80d3a",
        "2f6f1dba-6e9f-dd0558ad08f4",
    };
    for (size_t i = 0; i < LENGTH_OF(values); ++i) {
      test_encoding_varlen(values[i], strlen(values[i]));
      test_encoding_fixlen(values[i], strlen(values[i]));
    }
  } while (0);

  do {
    EventHeader headers[] = {
        {.type = EVENT_TYPE_PING, .time = 1766383853},
        {.type = EVENT_TYPE_ACK, .time = 1766373853},
        {.type = EVENT_TYPE_JOIN, .time = 1766393853},
        {.type = EVENT_TYPE_LEAVE, .time = 1766483853},
        {.type = EVENT_TYPE_PING_REQ, .time = 1766283853},
    };

    for (size_t i = 0; i < LENGTH_OF(headers); ++i) {
      uuid_parse(uuid_strings[i], headers[i].uuid);
      test_encoding_header(&headers[i]);
    }
  } while (0);

  do {
    struct {
      const char* addr;
      int port;
    } addr_str[] = {
        {.addr = "8.8.8.8", .port = 1234},
        {.addr = "localhost", .port = 4321},
    };

    for (int i = 0; i < LENGTH_OF(addr_str); ++i) {
      struct sockaddr_in addr = {.sin_family = AF_INET,
                                 .sin_port = htons(addr_str[i].port)};
      inet_aton(addr_str[i].addr, &addr.sin_addr);
      test_encoding_sockaddr((struct sockaddr*)&addr, sizeof(addr));
    }

  } while (0);

  exit(exit_code);
}
