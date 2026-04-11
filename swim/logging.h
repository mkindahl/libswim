#ifndef SWIM_LOGGING_H_
#define SWIM_LOGGING_H_

#include <stdbool.h>
#include <stdio.h>

#include "swim/config.h"

extern bool swim_verbose;

typedef void (*swim_log_sink_fn)(const char *msg, void *userdata);

extern void swim_set_log_sink(swim_log_sink_fn sink, void *userdata);
extern void swim_log_write(const char *fmt, ...);

#define LOG(MSG, ...)                                                \
  do {                                                               \
    if (swim_verbose) {                                              \
      char timestr[200];                                             \
      time_t now = time(NULL);                                       \
      if (strftime(timestr, sizeof(timestr), "%c", localtime(&now))) \
        swim_log_write("%s " MSG, timestr, ##__VA_ARGS__);           \
    }                                                                \
  } while (0)

#ifdef SWIM_TRACING

extern bool swim_tracing_on;

#define TRACE(MSG, ...)                                    \
  do {                                                     \
    if (swim_tracing_on)                                   \
      swim_log_write("%s: " MSG, __func__, ##__VA_ARGS__); \
  } while (0)

#else

#define TRACE(MSG, ...) \
  do {                  \
  } while (0)

#endif

#endif /* SWIM_LOGGING_H_ */
