#include "state.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>

#include "swim/debug.h"
#include "swim/event.h"

static const char *status_name[] = {
    [SWIM_STATUS_UNKNOWN] = "UNKNOWN",
    [SWIM_STATUS_ALIVE] = "ALIVE",
    [SWIM_STATUS_SUSPECT] = "SUSPECT",
    [SWIM_STATUS_DEAD] = "DEAD",
};

/*
 * Check if a new timestamp is after an old timestamp.
 */
static bool timespec_after(struct timespec *old, struct timespec *new) {
  if (old->tv_sec == new->tv_sec)
    return new->tv_nsec > old->tv_nsec;
  else
    return new->tv_sec > old->tv_sec;
}

/*
 * Compare two instances or a key and an instance.
 *
 * This can be used with both bsearch() and qsort() since the first
 * element of the Instance structure is the UUID field.
 */
static int instance_compare(const void *pkey, const void *pinstance) {
  const uuid_t *key = (const uuid_t *)pkey;
  const InstanceState *instance = pinstance;
  return uuid_compare(*key, instance->base.uuid);
}

bool swim_state_init(SWIM *swim, uint16_t port) {
  struct sockaddr_in serveraddr;
  char host[NI_MAXHOST], service[NI_MAXSERV];
  int err, fd;
  ssize_t res;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;

  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(port);

  res = bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if (res < 0)
    return false;

  err = getnameinfo((struct sockaddr *)&serveraddr, sizeof(serveraddr), host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
  if (err != 0) {
    TRACE("getnameinfo: %s", gai_strerror(err));
    return false;
  }

  TRACE("initialized server to listen on %s:%s", host, service);

  swim->view_capacity = 32; /* Cannot be zero, since it is doubled each time */
  swim->view_size = 0;
  swim->view = calloc(swim->view_capacity, sizeof(InstanceState));
  swim->addr = serveraddr;
  swim->sockfd = fd;

  uuid_generate(swim->uuid);

  return true;
}

void swim_state_update_time(SWIM *swim, uuid_t uuid, struct timespec *time) {
  InstanceState *instance = swim_state_get(swim, uuid);
  if (instance != NULL && timespec_after(&instance->base.last_seen, time))
    memcpy(&instance->base.last_seen, time, sizeof(instance->base.last_seen));
}

void swim_state_add(SWIM *swim, InstanceData *instance, struct sockaddr *addr,
                    socklen_t addrlen) {
  InstanceState *instance_state;

  if (swim->view_size >= swim->view_capacity) {
    swim->view = realloc(swim->view, 2 * swim->view_capacity);
    swim->view_capacity *= 2;
  }

  instance_state = &swim->view[swim->view_size++];

  instance_state->base = *instance;
  instance_state->addrlen = addrlen;
  memcpy(&instance_state->addr, addr, addrlen);
  qsort(swim->view, swim->view_size, sizeof(InstanceState), instance_compare);
}

void swim_state_del(SWIM *swim, uuid_t uuid) {
  swim_state_set_status(swim, uuid, SWIM_STATUS_DEAD);
}

void swim_state_set_status(SWIM *swim, uuid_t uuid, Status status) {
  InstanceState *instance = bsearch(uuid, swim->view, swim->view_size,
                                    sizeof(InstanceState), instance_compare);
  if (instance != NULL)
    instance->base.status = status;
}

InstanceState *swim_state_get(SWIM *swim, uuid_t uuid) {
  return bsearch(uuid, swim->view, swim->view_size, sizeof(InstanceState),
                 instance_compare);
}

static const char *timespec_string(struct timespec *ts, char *buf) {
  char *ptr = buf;
  struct tm t;

  if (localtime_r(&(ts->tv_sec), &t) == NULL)
    return NULL;

  ptr += strftime(buf, sizeof(buf) - (ptr - buf), "%F %T", &t);
  ptr += snprintf(ptr, sizeof(buf) - (ptr - buf), ".%09ld", ts->tv_nsec);

  return buf;
}

void swim_state_print(SWIM *swim) {
  char uuid_buf[40], host[NI_MAXHOST], service[NI_MAXSERV];

  TRACE("view_size: %lu, view_capacity: %lu", swim->view_size,
        swim->view_capacity);

  fprintf(stderr, "%-40s %-20s %-10s %-20s %-10s\n", "UUID", "TIME", "STATUS",
          "ADDRESS", "PORT");
  for (size_t i = 0; i < swim->view_size; ++i) {
    InstanceState *instance = &swim->view[i];

    if (instance) {
      char buf[128];
      int err =
          getnameinfo((struct sockaddr *)&instance->addr, instance->addrlen,
                      host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);

      uuid_unparse(instance->base.uuid, uuid_buf);
      if (err == 0) {
        fprintf(stderr, "%-40s %-20s %-10s %-20s %-10s\n", uuid_buf,
                timespec_string(&instance->base.last_seen, buf),
                status_name[instance->base.status], host, service);
      } else {
        fprintf(stderr, "%-40s %-20s %-10s\n", uuid_buf,
                timespec_string(&instance->base.last_seen, buf),
                status_name[instance->base.status]);
      }
    }
  }
}
