#include "cluster.h"

#include <sys/param.h>

#include "swim/debug.h"
#include "swim/defs.h"
#include "swim/event.h"
#include "swim/network.h"

/*
 * Join a cluster contacting an existing node.
 *
 * This will send a join request to the provided address. It does not
 * wait for an ACK since the main server loop will deal with any
 * incoming messages, including an ACK, and process it accordingly.
 *
 * We mark the address of ourselves as unknown since this will be
 * figured out by the receiving node.
 */
void swim_cluster_join(SWIM* swim, struct sockaddr* addr, socklen_t addrlen) {
  Event event;

  swim_event_init(&event, swim->uuid, EVENT_TYPE_JOIN);
  uuid_copy(event.join.join_uuid, swim->uuid);
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
  if (swim->view_size == 0)
    return;

  int pos = rand() % swim->view_size;

  Event event;
  swim_event_init(&event, swim->uuid, EVENT_TYPE_LEAVE);
  for (int i = 0; i < MIN(swim->view_size, SWIM_MAXLEAVE); ++i) {
    NodeState* node = &swim->view[(pos + i) % swim->view_size];
    swim_send_event(swim, &event, (struct sockaddr*)&node->info.addr,
                    node->info.addrlen);
  }
}

/*
 * Perform a cluster heartbeat.
 *
 * Cluster heartbeats are done on a regular basis and can send out a
 * heartbeat event (this is just a PING event) to other nodes but also
 * update the status of all nodes in the view.
 */
void swim_cluster_heartbeat(SWIM* swim) {
  time_t now;

  time(&now);

  /*
   * We might have been processing an event before the next heartbeat,
   * so in that case we just skip the heartbeat processing since
   * nothing is expected to have changed.
   */
  if (swim->view_size > 0 &&
      swim->last_heartbeat + SWIM_HEARTBEAT_INTERVAL < now) {
    NodeState* target = &swim->view[rand() % swim->view_size];

    swim->last_heartbeat = now;

    swim_send_ping(swim, (struct sockaddr*)&target->info.addr,
                   target->info.addrlen);

    /*
     * Update the state of all nodes in the view. We only do this if
     * we have had a heartbeat since there is no point in running this
     * loop unless the time has increased by at least one heartbeat
     * period.
     */
    for (int i = 0; i < swim->view_size; ++i) {
      NodeState* candidate = &swim->view[i];
      switch (candidate->info.status) {
        case SWIM_STATUS_ALIVE:
          if (candidate->info.last_seen <= now - 60) {
            TRACE("node was last seen %ld, which is before %ld",
                  candidate->info.last_seen, now - 60);
            swim_cluster_suspect_dead(swim, candidate);
          }
          break;

        case SWIM_STATUS_SUSPECT:
          if (candidate->info.last_seen <= now - 120) {
            TRACE("witness time %ld, which is before %ld",
                  candidate->info.last_seen, now - 120);
            swim_cluster_declare_dead(swim, candidate);
          }
          break;

        case SWIM_STATUS_DEAD:
        case SWIM_STATUS_UNKNOWN:
          break;

        default:
          fprintf(stderr, "unknown status of node: %d\n",
                  candidate->info.status);
          break;
      }
    }
  }
}

/*
 * Handle a suspected dead node.
 *
 * Ping a node in the cluster indirectly.
 *
 * If there are no nodes in the view, we will ping nobody. Otherwise,
 * we will send out either one ping request to each node, or at most
 * SWIM_MAXPINGREQ ping requests to randomly selected nodes.
 */
void swim_cluster_suspect_dead(SWIM* swim, NodeState* target) {
  /* Should not be possible, since we have a node as parameter */
  if (swim->view_size == 0)
    return;

  int start = rand() % swim->view_size;

  /*
   * We update the target node here since we already suspect it is
   * dead. This will filter it out in the loop below and not send a
   * ping request to the suspected dead node.
   */
  target->info.status = SWIM_STATUS_SUSPECT;

  for (int sent = MIN(swim->view_size, SWIM_MAXPINGREQ); sent > start; --sent) {
    NodeState* node = &swim->view[(start + sent) % swim->view_size];
    if (node->info.status == SWIM_STATUS_ALIVE)
      swim_send_ping_req(swim, target->info.uuid,
                         (struct sockaddr*)&node->info.addr,
                         node->info.addrlen);
  }
}

/*
 * Declare a node as dead.
 *
 * Right now we just mark it dead and let anti-entropy deal with
 * propagating it. For better efficiency, we need to send out a rumor
 * that the node is dead, especially if there is a witness.
 *
 * Note that a node declared dead can never refute it. The node need
 * to create a new UUID and join the cluster again.
 */
void swim_cluster_declare_dead(__attribute__((unused)) SWIM* swim,
                               NodeState* target) {
  target->info.status = SWIM_STATUS_DEAD;
}

/*
 * Refute that a node is dead.
 *
 * This means setting the status to alive and removing any witness
 * that says otherwise.
 */
void swim_cluster_refuted_dead(__attribute__((unused)) SWIM* swim,
                               NodeState* node) {
  node->info.status = SWIM_STATUS_ALIVE;
  swim_node_reset_witness(node);
}
