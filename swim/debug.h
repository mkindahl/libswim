#ifndef SWIM_DEBUG_H_
#define SWIM_DEBUG_H_

#include <stdbool.h>
#include <stdio.h>

#include "swim/config.h"

#ifdef SWIM_TRACING

extern bool tracing_on;
extern bool verbose;

#define LOG(MSG, ...)                                                \
  do {                                                               \
    if (verbose) {                                                   \
      char timestr[200];                                             \
      time_t now = time(NULL);                                       \
      if (strftime(timestr, sizeof(timestr), "%c", localtime(&now))) \
        fprintf(stderr, "%s " MSG "\n", timestr, ##__VA_ARGS__);     \
    }                                                                \
  } while (0)

#define TRACE(MSG, ...)                                          \
  do {                                                           \
    if (tracing_on)                                              \
      fprintf(stderr, "%s: " MSG "\n", __func__, ##__VA_ARGS__); \
  } while (0)

#endif

#endif /* SWIM_DEBUG_H_ */
