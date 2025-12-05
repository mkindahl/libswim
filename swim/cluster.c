#include "cluster.h"

#include <string.h>

#include <sys/param.h>

#include "swim/defs.h"
#include "swim/event.h"
#include "swim/network.h"

/*
 * Join a cluster contacting an existing node.
 *
 * This will send a join request to the provided address. It does not
 * wait for an ACK since the main server loop will deal with any
 * incoming messages, including an ACK, and process it accordingly.
 */
void swim_cluster_join(SWIM* swim, struct sockaddr* addr, socklen_t addrlen) {
  Event event;

  swim_event_init(&event, swim->uuid, EVENT_TYPE_JOIN, sizeof(event));

  uuid_copy(event.join.join_uuid, swim->uuid);
  memcpy(&event.join.join_addr, &swim->addr, sizeof(swim->addr));
  event.join.join_addrlen = sizeof(swim->addr);

  swim_send_event(swim, &event, addr, addrlen);
}

/*
 * Send a leave request to the cluster.
 *
 * This will mark the server as definitely dead by sending a leave
 * event to some other nodes.
 *
 * In the event that all leave requests are lost, the cluster will
 * notice that the node does not respond any more and eventually
 * remove it from the cluster.
 */
void swim_cluster_leave(SWIM* swim) {
  const int pos = rand() % swim->view_size;
  Event event;

  if (swim->view_size == 0)
    return;

  swim_event_init(&event, swim->uuid, EVENT_TYPE_LEAVE, sizeof(event));
  for (int i = 0; i < MIN(swim->view_size, SWIM_MAXLEAVE); ++i) {
    NodeState* node = &swim->view[(pos + i) % swim->view_size];
    swim_send_event(swim, &event, (struct sockaddr*)&node->info.addr,
                    node->info.addrlen);
  }
}
