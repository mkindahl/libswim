#include "state.h"

#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <netinet/in.h>
#include <sys/socket.h>

#include "swim/logging.h"
#include "swim/network.h"
#include "swim/node.h"
#include "swim/utils.h"

static const char *status_name[] = {
    [SWIM_STATUS_UNKNOWN] = "UNKNOWN",
    [SWIM_STATUS_ALIVE] = "ALIVE",
    [SWIM_STATUS_SUSPECT] = "SUSPECT",
    [SWIM_STATUS_DEAD] = "DEAD",
};

const char *swim_status_name(Status status) {
  return status_name[status];
}

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
  memset(swim, 0, sizeof(SWIM));

  int fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;

  struct timeval timeout = {.tv_sec = 10, .tv_usec = 0};
  int err = setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
  if (err < 0) {
    perror("setsockopt");
    close(fd);
    return false;
  }

  swim->addr.sin_family = AF_INET;
  swim->addr.sin_addr.s_addr = htonl(INADDR_ANY);
  swim->addr.sin_port = htons(port);

  ssize_t res = bind(fd, (struct sockaddr *)&swim->addr, sizeof(swim->addr));
  if (res < 0) {
    close(fd);
    return false;
  }

  swim->view_capacity = 32; /* Cannot be zero, since it is doubled each time */
  swim->view = calloc(swim->view_capacity, sizeof(NodeState));
  swim->sockfd = fd;

  time(&swim->last_heartbeat);
  uuid_generate(swim->uuid);

#ifdef SWIM_TRACING
  char ubuf[SWIM_UUID_STR_LEN];
  char abuf[NI_MAXHOST + NI_MAXSERV + 2];
  TRACE("initialized server with UUID %s to listen on %s",
        swim_uuid_str_r(swim->uuid, ubuf, sizeof(ubuf)),
        swim_addr_str_r((struct sockaddr *)&swim->addr,
                        sizeof(swim->addr),
                        abuf,
                        sizeof(abuf)));
#endif

  return true;
}

NodeState *swim_state_suspect(SWIM *swim, uuid_t uuid, struct sockaddr *addr,
                              socklen_t addrlen) {
  NodeState *node = swim_state_get_node(swim, uuid);

  if (node) {
    node->info.status = SWIM_STATUS_SUSPECT;

    node->witness.addrlen = addrlen;
    memcpy(&node->witness.addr, addr, addrlen);
  }
  return node;
}

/*
 * Function: swim_state_notice
 *
 * Notice that node has sent a message and update status accordingly.
 *
 * If the instance is in the view and it is not marked dead, we mark
 * it as seen and alive. In this case we assume that the address is
 * already set so we will not change it.
 *
 * If there is no node, we ignore it. It can have been removed from
 * the view for different reasons, but we are not tracking
 * it for now.
 *
 * If the node exists and is dead, we just ignore it. Hence dead nodes
 * will be cleaned up once they are old enough.
 */
NodeState *swim_state_notice(SWIM *swim, uuid_t uuid, time_t time,
                             uint32_t incarnation) {
  NodeState *node = swim_state_get_node(swim, uuid);

  if (node) {
    assert(node->info.last_seen > 0 &&
           node->info.status != SWIM_STATUS_UNKNOWN);

    if (incarnation > node->info.incarnation) {
      node->info.incarnation = incarnation;
      node->info.status = SWIM_STATUS_ALIVE;
      node->info.last_seen = time;
    } else if (incarnation == node->info.incarnation) {
      switch (node->info.status) {
        case SWIM_STATUS_UNKNOWN:
        case SWIM_STATUS_SUSPECT:
          node->info.status = SWIM_STATUS_ALIVE;
          node->info.last_seen = time;
          break;

        case SWIM_STATUS_ALIVE:
          node->info.last_seen = time;
          break;

        case SWIM_STATUS_DEAD:
          break;
      }
    }
    /* incarnation < stored: stale, ignore */
  }
  return node;
}

/*
 * Merge node info into the state.
 *
 * This is used to merge the status when processing gossip.
 */

void swim_state_merge(SWIM *swim, NodeInfo *info) {
  if (uuid_compare(info->uuid, swim->uuid) == 0)
    return;

  NodeState *node = swim_state_get_node(swim, info->uuid);

  if (node == NULL) {
    swim_state_add(swim, info);
  } else {
    if (info->incarnation > node->info.incarnation) {
      /* Newer incarnation: accept unconditionally */
      node->info.incarnation = info->incarnation;
      node->info.last_seen = info->last_seen;
      node->info.status = info->status;
      swim_node_reset(node);
    } else if (info->incarnation == node->info.incarnation) {
      /* Same incarnation: status precedence (Alive < Suspect < Dead) */
      switch (node->info.status) {
        case SWIM_STATUS_ALIVE:
        case SWIM_STATUS_SUSPECT:
          if (info->status > node->info.status) {
            node->info.status = info->status;
            node->info.last_seen = info->last_seen;
            swim_node_reset(node);
          } else if (info->status == node->info.status &&
                     node->info.last_seen < info->last_seen) {
            node->info.last_seen = info->last_seen;
          }
          break;

        case SWIM_STATUS_DEAD:
        case SWIM_STATUS_UNKNOWN:
          swim_node_reset(node);
          break;
        default:
          break;
      }
    }
    /* info->incarnation < stored: stale, ignore */
  }
}

/*
 * Add a node to the state view.
 *
 * We do not change the status of the node if it already exists.
 *
 * Note:
 *
 *    How do we deal with nodes that are declared dead but we get
 *    gossip about them?
 *
 *    We should probably ignore the gossip and require the node to
 *    generate a new UUID and re-join the cluster.
 */
NodeState *swim_state_add(SWIM *swim, NodeInfo *info) {
  /* Pre-conditions. */
  assert(uuid_compare(info->uuid, swim->uuid) != 0);
  assert(swim_state_get_node(swim, info->uuid) == NULL);

#ifdef SWIM_TRACING
  char ubuf[SWIM_UUID_STR_LEN];
  char abuf[NI_MAXHOST + NI_MAXSERV + 2];
  TRACE("node %s addr %s addrlen %d",
        swim_uuid_str_r(info->uuid, ubuf, sizeof(ubuf)),
        swim_addr_str_r(
            (struct sockaddr *)&info->addr, info->addrlen, abuf, sizeof(abuf)),
        info->addrlen);
#endif

  if (swim->view_size >= swim->view_capacity) {
    NodeState *new_view =
        realloc(swim->view, 2 * swim->view_capacity * sizeof(NodeState));
    if (new_view == NULL) {
      fprintf(stderr,
              "failed to grow view from %d to %d\n",
              swim->view_capacity,
              2 * swim->view_capacity);
      return NULL;
    }
    swim->view = new_view;
    swim->view_capacity *= 2;
  }

  NodeState *node = &swim->view[swim->view_size++];
  node->info = *info;

  qsort(swim->view, swim->view_size, sizeof(NodeState), node_compare);
  swim_probe_invalidate(swim);

  assert(node != NULL); /* Post-condition */

  return node;
}

/*
 * Delete an node from the view by marking it dead.
 *
 * If an node want to re-join, it need to create a new UUID and
 * issue a new join message.
 *
 * In theory it is possible to add an old UUID if it has been cleaned
 * up from the view, but since it might not have been cleaned up from
 * all nodes, this might lead to strange behaviour.
 */
NodeState *swim_state_del(SWIM *swim, uuid_t uuid) {
  /* We ignore deleting the node itself */
  if (uuid_compare(uuid, swim->uuid) == 0)
    return NULL;

  NodeState *node = swim_state_get_node(swim, uuid);
  if (node != NULL) {
    node->info.status = SWIM_STATUS_DEAD;
    swim_probe_invalidate(swim);
  }
  return node;
}

NodeState *swim_state_get_node(SWIM *swim, uuid_t uuid) {
  return bsearch(
      uuid, swim->view, swim->view_size, sizeof(NodeState), node_compare);
}

static swim_state_print_fn print_callback = NULL;
static void *print_callback_userdata = NULL;

void swim_state_set_print_callback(swim_state_print_fn cb, void *userdata) {
  print_callback = cb;
  print_callback_userdata = userdata;
}

static const char *time_as_string(time_t time, char *buf, size_t bufsize) {
  struct tm t;

  if (localtime_r(&time, &t) == NULL)
    return NULL;

  strftime(buf, bufsize, "%F %T", &t);

  return buf;
}

void swim_state_print(SWIM *swim) {
  if (print_callback) {
    print_callback(swim, print_callback_userdata);
    return;
  }

  char ubuf[SWIM_UUID_STR_LEN];
  fprintf(
      stderr, "UUID: %s\n", swim_uuid_str_r(swim->uuid, ubuf, sizeof(ubuf)));
  fprintf(stderr,
          "%-40s %-26s %-10s %-*s %-*s\n",
          "UUID",
          "LAST_SEEN",
          "STATUS",
          INET_ADDRSTRLEN + 5,
          "ADDRESS",
          INET_ADDRSTRLEN + 5,
          "WITNESS");
  for (int i = 0; i < swim->view_size; ++i) {
    NodeState *node = &swim->view[i];

    if (node) {
      char buf[128];
      char abuf[NI_MAXHOST + NI_MAXSERV + 2];
      const char *address = swim_addr_str_r((struct sockaddr *)&node->info.addr,
                                            node->info.addrlen,
                                            abuf,
                                            sizeof(abuf));

      if (address) {
        char wbuf[NI_MAXHOST + NI_MAXSERV + 2];
        const char *witness = "";

        if (node->witness.addrlen > 0)
          witness = swim_addr_str_r((struct sockaddr *)&node->witness.addr,
                                    node->witness.addrlen,
                                    wbuf,
                                    sizeof(wbuf));

        fprintf(stderr,
                "%-40s %-26s %-10s %-*s %-*s\n",
                swim_uuid_str_r(node->info.uuid, ubuf, sizeof(ubuf)),
                time_as_string(node->info.last_seen, buf, sizeof(buf)),
                status_name[node->info.status],
                INET_ADDRSTRLEN + 5,
                address,
                INET_ADDRSTRLEN + 5,
                witness);
      } else {
        fprintf(stderr,
                "%-40s %-20s %-10s\n",
                swim_uuid_str_r(node->info.uuid, ubuf, sizeof(ubuf)),
                time_as_string(node->info.last_seen, buf, sizeof(buf)),
                status_name[node->info.status]);
      }
    }
  }
}

/*
 * Shuffle the probe schedule using Fisher-Yates.
 *
 * Fills the order array with indices 0..view_size-1 and shuffles
 * them. Resets the position to the start of the new round.
 */
void swim_probe_shuffle(SWIM *swim) {
  int n = swim->view_size;

  if (n == 0) {
    swim->probe.size = 0;
    swim->probe.pos = 0;
    return;
  }

  int *order = realloc(swim->probe.order, n * sizeof(int));
  if (order == NULL)
    return;

  for (int i = 0; i < n; i++)
    order[i] = i;

  for (int i = n - 1; i > 0; i--) {
    int j = rand() % (i + 1);
    int tmp = order[i];
    order[i] = order[j];
    order[j] = tmp;
  }

  swim->probe.order = order;
  swim->probe.size = n;
  swim->probe.pos = 0;
}

/*
 * Return the next probe target index, or -1 if no probeable node exists.
 *
 * Reshuffles automatically when the current round is exhausted or the
 * view has changed. Skips dead nodes.
 */
int swim_probe_next(SWIM *swim) {
  if (swim->view_size == 0)
    return -1;

  if (swim->probe.pos >= swim->probe.size ||
      swim->probe.size != swim->view_size)
    swim_probe_shuffle(swim);

  while (swim->probe.pos < swim->probe.size) {
    int idx = swim->probe.order[swim->probe.pos++];
    if (idx < swim->view_size &&
        swim->view[idx].info.status != SWIM_STATUS_DEAD)
      return idx;
  }

  return -1;
}

/*
 * Invalidate the probe schedule, forcing a reshuffle on next probe.
 */
void swim_probe_invalidate(SWIM *swim) {
  swim->probe.size = 0;
}
