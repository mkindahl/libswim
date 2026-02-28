#ifndef SWIM_TEST_H_
#define SWIM_TEST_H_

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <uuid/uuid.h>

static int exit_code = EXIT_SUCCESS;
static int test_count = 0;

#define FAILURE(FMT, ...)                         \
  do {                                            \
    exit_code = EXIT_FAILURE;                     \
    printf("FAILED (" FMT ")!\n", ##__VA_ARGS__); \
  } while (0)

#define EXPECT_EQ_INT(VALUE, EXPECT, PR)                        \
  do {                                                          \
    printf("%s %d...", __func__, ++test_count);                 \
    if ((VALUE) == (EXPECT))                                    \
      printf("ok\n");                                           \
    else                                                        \
      FAILURE("expected %" PR ", got %" PR, (EXPECT), (VALUE)); \
  } while (0)

#define EXPECT_EQ_STR(VALUE, EXPECT, BYTES)              \
  do {                                                   \
    printf("%s %d...", __func__, ++test_count);          \
    if (memcmp((VALUE), (EXPECT), (BYTES)) == 0)         \
      printf("ok\n");                                    \
    else                                                 \
      FAILURE("expected %s, got %s", (EXPECT), (VALUE)); \
  } while (0)

#define EXPECT_EQ_UUID(VALUE, EXPECT)                                        \
  do {                                                                       \
    printf("%s %d...", __func__, ++test_count);                              \
    if (uuid_compare((VALUE), (EXPECT)) == 0)                                \
      printf("ok\n");                                                        \
    else {                                                                   \
      exit_code = EXIT_FAILURE;                                              \
      char expect_buf[40], value_buf[40];                                    \
      uuid_unparse((EXPECT), expect_buf);                                    \
      uuid_unparse((VALUE), value_buf);                                      \
      printf("FAILED (expected '%s', got '%s') !\n", expect_buf, value_buf); \
    }                                                                        \
  } while (0)

#define EXPECT_TRUE(EXPR)                       \
  do {                                          \
    printf("%s %d...", __func__, ++test_count); \
    if ((EXPR))                                 \
      printf("ok\n");                           \
    else {                                      \
      exit_code = EXIT_FAILURE;                 \
      printf("FAILED!\n");                      \
    }                                           \
  } while (0)

#define EXPECT_NULL(EXPR)                       \
  do {                                          \
    printf("%s %d...", __func__, ++test_count); \
    if ((EXPR) == NULL)                         \
      printf("ok\n");                           \
    else {                                      \
      exit_code = EXIT_FAILURE;                 \
      printf("FAILED (expected NULL)!\n");      \
    }                                           \
  } while (0)

#define EXPECT_NOT_NULL(EXPR)                   \
  do {                                          \
    printf("%s %d...", __func__, ++test_count); \
    if ((EXPR) != NULL)                         \
      printf("ok\n");                           \
    else {                                      \
      exit_code = EXIT_FAILURE;                 \
      printf("FAILED (expected non-NULL)!\n");  \
    }                                           \
  } while (0)

#endif /* SWIM_TEST_H_ */
