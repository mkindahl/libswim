#include "logging.h"

#include <stdarg.h>
#include <stdio.h>

bool swim_verbose = false;

#ifdef SWIM_TRACING
bool swim_tracing_on = false;
#endif

static swim_log_sink_fn log_sink = NULL;
static void *log_sink_userdata = NULL;

void swim_set_log_sink(swim_log_sink_fn sink, void *userdata) {
  log_sink = sink;
  log_sink_userdata = userdata;
}

void swim_log_write(const char *fmt, ...) {
  va_list ap;
  char buf[1024];

  va_start(ap, fmt);
  vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  if (log_sink) {
    log_sink(buf, log_sink_userdata);
  } else {
    fprintf(stderr, "%s\n", buf);
  }
}
