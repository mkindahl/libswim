#ifndef SWIM_DEBUG_H_
#define SWIM_DEBUG_H_

#include "swim/config.h"

#ifdef SWIM_TRACING

#include <stdio.h>

#define TRACE(MSG, ...)                                        \
  do {                                                         \
    fprintf(stderr, "%s: " MSG "\n", __func__, ##__VA_ARGS__); \
  } while (0)

#endif

#endif /* SWIM_DEBUG_H_ */
