// clang-format off
#include <stdarg.h>
#include <stddef.h>
#include <setjmp.h>
#include <cmocka.h>
// clang-format on

#include "encoding.h"

#include <inttypes.h>
#include <string.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "swim/defs.h"
#include "swim/event.h"

#define LENGTH_OF(V) ((int)(sizeof(V) / sizeof((V)[0])))

/* --- uint32 encoding --- */

static void test_encoding_uint32(void **state) {
  (void)state; /* to avoid unused parameter warning */
  uint32_t values[] = {1234567890UL, 0UL};
  for (int i = 0; i < LENGTH_OF(values); i++) {
    uint32_t actual;
    unsigned char buf[sizeof(uint32_t)];
    swim_encode_uint32(buf, sizeof(buf), values[i]);
    swim_decode_uint32(buf, sizeof(buf), &actual);
    assert_int_equal(actual, values[i]);
  }
}

/* --- uint16 encoding --- */

static void test_encoding_uint16(void **state) {
  (void)state; /* to avoid unused parameter warning */
  uint16_t values[] = {12345, 0};
  for (int i = 0; i < LENGTH_OF(values); i++) {
    uint16_t actual;
    unsigned char buf[sizeof(uint16_t)];
    swim_encode_uint16(buf, sizeof(buf), values[i]);
    swim_decode_uint16(buf, sizeof(buf), &actual);
    assert_int_equal(actual, values[i]);
  }
}

/* --- UUID encoding --- */

static const char *uuid_strings[] = {
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

static void test_encoding_uuid(void **state) {
  (void)state; /* to avoid unused parameter warning */
  for (int i = 0; i < LENGTH_OF(uuid_strings); i++) {
    uuid_t uuid, actual;
    unsigned char buf[sizeof(uuid_t)];
    uuid_parse(uuid_strings[i], uuid);
    swim_encode_uuid(buf, sizeof(buf), uuid);
    swim_decode_uuid(buf, sizeof(buf), actual);
    assert_int_equal(uuid_compare(uuid, actual), 0);
  }
}

/* --- variable-length encoding --- */

static const char *string_values[] = {
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

static void test_encoding_varlen(void **state) {
  (void)state; /* to avoid unused parameter warning */
  for (int i = 0; i < LENGTH_OF(string_values); i++) {
    int16_t bytes = strlen(string_values[i]);
    char actual[128];
    int16_t length;
    unsigned char buf[128];

    swim_encode_varlen(buf, sizeof(buf), string_values[i], bytes);
    swim_decode_varlen(buf, sizeof(buf), actual, &length);
    assert_int_equal(bytes, length);
    assert_memory_equal(string_values[i], actual, length);
  }
}

/* --- fixed-length encoding --- */

static void test_encoding_fixlen(void **state) {
  (void)state; /* to avoid unused parameter warning */
  for (int i = 0; i < LENGTH_OF(string_values); i++) {
    size_t bytes = strlen(string_values[i]);
    char actual[128];
    unsigned char buf[128];

    swim_encode_fixlen(buf, sizeof(buf), string_values[i], bytes);
    swim_decode_fixlen(buf, sizeof(buf), actual, bytes);
    assert_memory_equal(string_values[i], actual, bytes);
  }
}

/* --- event header encoding --- */

static void test_encoding_header(void **state) {
  (void)state; /* to avoid unused parameter warning */
  EventHeader headers[] = {
      {.type = EVENT_TYPE_PING, .time = 1766383853},
      {.type = EVENT_TYPE_ACK, .time = 1766373853},
      {.type = EVENT_TYPE_JOIN, .time = 1766393853},
      {.type = EVENT_TYPE_LEAVE, .time = 1766483853},
      {.type = EVENT_TYPE_PING_REQ, .time = 1766283853},
  };

  for (int i = 0; i < LENGTH_OF(headers); i++) {
    unsigned char buf[SWIM_MAX_PACKET_SIZE];
    EventHeader actual;

    uuid_parse(uuid_strings[i], headers[i].uuid);
    swim_encode_event_header(buf, sizeof(buf), &headers[i]);
    swim_decode_event_header(buf, sizeof(buf), &actual);
    assert_int_equal(headers[i].type, actual.type);
    assert_int_equal(uuid_compare(headers[i].uuid, actual.uuid), 0);
    assert_int_equal(headers[i].time, actual.time);
  }
}

/* --- sockaddr encoding --- */

static void test_encoding_sockaddr(void **state) {
  (void)state; /* to avoid unused parameter warning */
  struct {
    const char *addr;
    int port;
  } addr_str[] = {
      {.addr = "8.8.8.8", .port = 1234},
      {.addr = "127.0.0.1", .port = 4321},
  };

  for (int i = 0; i < LENGTH_OF(addr_str); i++) {
    struct sockaddr_in addr = {.sin_family = AF_INET,
                               .sin_port = htons(addr_str[i].port)};
    inet_aton(addr_str[i].addr, &addr.sin_addr);

    unsigned char buf[SWIM_MAX_PACKET_SIZE];
    struct sockaddr_storage actual;
    socklen_t actual_len;

    swim_encode_sockaddr(
        buf, sizeof(buf), (struct sockaddr *)&addr, sizeof(addr));
    swim_decode_sockaddr(
        buf, sizeof(buf), (struct sockaddr *)&actual, &actual_len);
    assert_int_equal(sizeof(addr), actual_len);
    assert_int_equal(addr.sin_family, actual.ss_family);

    struct sockaddr_in *pactual = (struct sockaddr_in *)&actual;
    assert_int_equal(addr.sin_port, pactual->sin_port);
    assert_int_equal(addr.sin_addr.s_addr, pactual->sin_addr.s_addr);
  }
}

int main(void) {
  const struct CMUnitTest tests[] = {
      cmocka_unit_test(test_encoding_uint32),
      cmocka_unit_test(test_encoding_uint16),
      cmocka_unit_test(test_encoding_uuid),
      cmocka_unit_test(test_encoding_varlen),
      cmocka_unit_test(test_encoding_fixlen),
      cmocka_unit_test(test_encoding_header),
      cmocka_unit_test(test_encoding_sockaddr),
  };
  return cmocka_run_group_tests(tests, NULL, NULL);
}
