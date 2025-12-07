#include "state.h"

#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <sys/socket.h>

#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/utils.h"

static const char *status_name[] = {
    [SWIM_STATUS_UNKNOWN] = "UNKNOWN",
    [SWIM_STATUS_ALIVE] = "ALIVE",
    [SWIM_STATUS_SUSPECT] = "SUSPECT",
    [SWIM_STATUS_DEAD] = "DEAD",
};

/*
 * Compare two nodes or a key and an node.
 *
 * This can be used with both bsearch() and qsort() since the first
 * element of the Node structure is the UUID field.
 */
static int node_compare(const void *pkey, const void *pnode) {
  const uuid_t *key = (const uuid_t *)pkey;
  const NodeState *node = pnode;
  return uuid_compare(*key, node->info.uuid);
}

bool swim_state_init(SWIM *swim, uint16_t port) {
  struct sockaddr_in serveraddr;
  char addrbuf[NI_MAXHOST + NI_MAXSERV + 1], uuid_buf[40];
  int err, fd;
  ssize_t res;
  struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;

  err = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

  if (err < 0) {
    perror("setsockopt");
    exit(EXIT_FAILURE);
  }

  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(port);

  res = bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if (res < 0)
    return false;

  swim->view_capacity = 32; /* Cannot be zero, since it is doubled each time */
  swim->view_size = 0;
  swim->view = calloc(swim->view_capacity, sizeof(NodeState));
  swim->addr = serveraddr;
  swim->sockfd = fd;

  uuid_generate(swim->uuid);

  uuid_unparse(swim->uuid, uuid_buf);
  TRACE("initialized server with UUID %s to listen on %s", uuid_buf,
        addr2str_r((struct sockaddr *)&serveraddr, sizeof(serveraddr), addrbuf,
                   sizeof(addrbuf)));

  return true;
}

void swim_state_update_time(SWIM *swim, uuid_t uuid, time_t time) {
  NodeState *node = swim_state_get(swim, uuid);
  if (node != NULL && node->info.last_seen <= time)
    node->info.last_seen = time;
}

/*
 * Add or update a node to the state view.
 *
 * If the node is already in the view, we just update the timestamp
 * and mark it as alive.
 *
 * Note:
 *
 *    How do we deal with nodes that are declared dead but we get
 *    gossip about them?
 *
 *    We should probably ignore the gossip and require the node to
 *    generate a new UUID and re-join the cluster.
 */
void swim_state_add(SWIM *swim, NodeInfo *info) {
  NodeState *existing;

  /* We ignore adding the node itself, if that is passed for some
     reason */
  if (uuid_compare(info->uuid, swim->uuid) == 0)
    return;

  if ((existing = swim_state_get(swim, info->uuid))) {
    memcpy(&existing->info.last_seen, &info->last_seen,
           sizeof(existing->info.last_seen));
    existing->info.status = SWIM_STATUS_ALIVE;
  } else {
    NodeState *node;

    if (swim->view_size >= swim->view_capacity) {
      swim->view = realloc(swim->view, 2 * swim->view_capacity);
      swim->view_capacity *= 2;
    }

    node = &swim->view[swim->view_size++];
    node->info = *info;

    qsort(swim->view, swim->view_size, sizeof(NodeState), node_compare);
  }
}

void swim_state_del(SWIM *swim, uuid_t uuid) {
  /* We ignore deleting the node itself, if that is passed for some
     reason */
  if (uuid_compare(uuid, swim->uuid) == 0)
    return;

  swim_state_set_status(swim, uuid, SWIM_STATUS_DEAD);
}

void swim_state_set_status(SWIM *swim, uuid_t uuid, Status status) {
  NodeState *node = swim_state_get(swim, uuid);
  if (node != NULL)
    node->info.status = status;
}

NodeState *swim_state_get(SWIM *swim, uuid_t uuid) {
  return bsearch(uuid, swim->view, swim->view_size, sizeof(NodeState),
                 node_compare);
}

static const char *time_as_string(time_t time, char *buf, size_t bufsize) {
  struct tm t;

  if (localtime_r(&time, &t) == NULL)
    return NULL;

  strftime(buf, bufsize, "%F %T", &t);

  return buf;
}

void swim_state_print(SWIM *swim) {
  char uuid_buf[40], host[NI_MAXHOST], service[NI_MAXSERV];

  TRACE("view_size: %d, view_capacity: %d", swim->view_size,
        swim->view_capacity);

  fprintf(stderr, "%-40s %-26s %-10s %-20s %-10s\n", "UUID", "TIME", "STATUS",
          "ADDRESS", "PORT");
  for (int i = 0; i < swim->view_size; ++i) {
    NodeState *node = &swim->view[i];

    if (node) {
      char buf[128];
      int err =
          getnameinfo((struct sockaddr *)&node->info.addr, node->info.addrlen,
                      host, NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);

      uuid_unparse(node->info.uuid, uuid_buf);
      if (err == 0) {
        fprintf(stderr, "%-40s %-26s %-10s %-20s %-10s\n", uuid_buf,
                time_as_string(node->info.last_seen, buf, sizeof(buf)),
                status_name[node->info.status], host, service);
      } else {
        fprintf(stderr, "%-40s %-20s %-10s\n", uuid_buf,
                time_as_string(node->info.last_seen, buf, sizeof(buf)),
                status_name[node->info.status]);
      }
    }
  }
}
