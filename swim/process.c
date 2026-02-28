/*
 * Module to process events.
 *
 * Processing events can involve both changing state of the system as
 * well as sending out one or more other events.
 */

#include "process.h"

#include <assert.h>
#include <netdb.h>
#include <string.h>

#include <sys/socket.h>
#include <sys/time.h>

#include "swim/cluster.h"
#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/utils.h"

/*
 * Process all gossip piggybacked on another event.
 */
static void swim_process_gossip(SWIM *swim, NodeInfo *gossip, int count) {
  char ubuf[SWIM_UUID_STR_LEN];
  TRACE("self uuid %s", swim_uuid_str_r(swim->uuid, ubuf, sizeof(ubuf)));

  for (int i = 0; i < count; ++i) {
    NodeInfo *info = &gossip[i];

    assert(info->last_seen > 0 && info->status != SWIM_STATUS_UNKNOWN);

    TRACE("uuid %s status %s",
          swim_uuid_str_r(info->uuid, ubuf, sizeof(ubuf)),
          swim_status_name(info->status));

    /*
     * If node is declared dead and see this gossip, it will exit for
     * now.
     *
     * TODO(mkindahl): Generate a new UUID and re-join the cluster.
     */
    if (uuid_compare(info->uuid, swim->uuid) == 0) {
      switch (info->status) {
        case SWIM_STATUS_DEAD:
          fprintf(stderr, "Node %s declared dead, exiting\n",
                  swim_uuid_str_r(info->uuid, ubuf, sizeof(ubuf)));
          exit(EXIT_FAILURE);

        case SWIM_STATUS_SUSPECT:
          TRACE("self suspected, incrementing incarnation to %u",
                swim->incarnation + 1);
          swim->incarnation++;
          continue;

        case SWIM_STATUS_ALIVE:
        case SWIM_STATUS_UNKNOWN:
          continue;
      }
    }

    swim_state_merge(swim, info);
  }
}

/*
 * Process the reception of a PING message.
 *
 * We just respond that we are alive.
 */
static void swim_process_ping(SWIM *swim, Event *event, struct sockaddr *addr,
                              socklen_t addrlen) {
  swim_send_ack(swim, swim->uuid, addr, addrlen);
  swim_process_gossip(swim, event->gossip, event->gossip_count);
}

/*
 * Process the reception of a PING_REQ message.
 *
 * We pick the requested server and record the sending server as a
 * witness. Any ack received from the pinged node will be sent back to
 * that witness.
 */
static void swim_process_ping_req(SWIM *swim, Event *event,
                                  struct sockaddr *addr, socklen_t addrlen) {
  NodeState *node =
      swim_state_suspect(swim, event->ping_req.ping_req_uuid, addr, addrlen);

  if (node)
    swim_send_ping(swim, (struct sockaddr *)&node->info.addr,
                   node->info.addrlen);
}

/*
 * Process the reception of an ACK message.
 *
 * If we are receiving an ACK to a PING_REQ, we need to send an ACK
 * back to the PING_REQ sender, which is recorded as the witness.
 */
static void swim_process_ack(SWIM *swim, Event *event,
                             __attribute__((unused)) struct sockaddr *addr,
                             __attribute__((unused)) socklen_t addrlen) {
  NodeState *node;

  /*
   * This is a strange situation, we shouldn't be able to get an ack
   * for ourselves, but there is nothing to do for us here.
   */
  if (uuid_compare(event->ack.ack_uuid, swim->uuid) == 0) {
    TRACE("Got ack for myself. Weird.");
    return;
  }

  node = swim_state_get_node(swim, event->ack.ack_uuid);
  if (node == NULL) {
    NodeInfo info;

    /*
     * If the node did not exist, we add it with the sender address
     * and mark it as alive and seen. This is necessary since the ACK
     * can be in response to a JOIN request, and in that case, it need
     * to be added to the view.
     */
    swim_node_init(&info, event->ack.ack_uuid, addr, addrlen);
    info.last_seen = event->hdr.time;

    node = swim_state_add(swim, &info);
  }

  switch (node->info.status) {
      /*
       * If the sender node exists, is marked suspect, and there is a
       * witness, then it needs to be notified that the node is alive.
       */
    case SWIM_STATUS_SUSPECT:
      if (swim_node_has_witness(node)) {
        swim_send_ack(swim, event->ack.ack_uuid,
                      (struct sockaddr *)&node->witness.addr,
                      node->witness.addrlen);
        swim_node_reset_witness(node);
      }
      swim_cluster_refuted_dead(swim, node);
      break;

    case SWIM_STATUS_UNKNOWN:
      swim_cluster_refuted_dead(swim, node);
      break;

    case SWIM_STATUS_DEAD:
    case SWIM_STATUS_ALIVE:
      break;
  }

  swim_process_gossip(swim, event->gossip, event->gossip_count);
}

/*
 * Process the reception of an JOIN message.
 *
 * When a request to join the cluster is made, we update the cluster
 * information with the new server and send an ACK message to the
 * sender containing partial cluster gossip.
 *
 * The sender can ignore the ACK message if they want and will get
 * gossip information messages later.
 */
static void swim_process_join(SWIM *swim, Event *event, struct sockaddr *addr,
                              socklen_t addrlen) {
  struct JoinEvent *join = &event->join;
  NodeInfo info;

  /*
   * We skip adding the node if we have already added it or if this is
   * an attempt to join the node itself.
   *
   * This is needed since we can receive a rumor that the node has
   * joined as a result of forwarding the join event.
   */
  if (uuid_compare(swim->uuid, join->join_uuid) == 0)
    return;

  if (swim_state_get_node(swim, join->join_uuid) != NULL)
    return;

  /* Fill in the address if it was not set by the sender */
  if (event->join.join_addrlen == 0) {
    event->join.join_addrlen = addrlen;
    memcpy(&event->join.join_addr, addr, addrlen);
  }

  swim_node_init(&info, join->join_uuid, (struct sockaddr *)&join->join_addr,
                 join->join_addrlen);
  info.status = SWIM_STATUS_ALIVE;
  info.last_seen = event->hdr.time;

  assert(info.last_seen > 0 && info.status != SWIM_STATUS_UNKNOWN);

  swim_state_add(swim, &info);

  for (int i = 0; i < swim->view_size; ++i) {
    NodeState *node = &swim->view[i];
    swim_send_event(swim, event, (struct sockaddr *)&node->info.addr,
                    swim->view[i].info.addrlen);
  }

  swim_send_ack(swim, swim->uuid, addr, addrlen);
}

/*
 * Process the reception of an LEAVE message.
 *
 * When a request to leave the cluster is made, we mark the server and
 * declared dead and send an ack message (with gossip) to the server.
 *
 * The sender can ignore the ACK message if they want, but will not
 * receive any other gossip messages unless the LEAVE message was
 * lost.
 */
static void swim_process_leave(SWIM *swim, Event *event, struct sockaddr *addr,
                               socklen_t addrlen) {
  struct LeaveEvent *leave = &event->leave;

  swim_state_del(swim, leave->leave_uuid);
  swim_send_ack(swim, swim->uuid, addr, addrlen);
}

/*
 * Event callback type.
 *
 * The callback is passed the SWIM state, the event being received,
 * and the address of the sender as seen by recvfrom().
 */
typedef void process_callback_t(SWIM *swim, Event *event, struct sockaddr *addr,
                                socklen_t addrlen);

/*
 * Event dispatch structure with information about all events.
 */
static process_callback_t *swim_event_processing[] = {
    [EVENT_TYPE_PING] = swim_process_ping,
    [EVENT_TYPE_PING_REQ] = swim_process_ping_req,
    [EVENT_TYPE_ACK] = swim_process_ack,
    [EVENT_TYPE_JOIN] = swim_process_join,
    [EVENT_TYPE_LEAVE] = swim_process_leave,
};

void swim_process_event(SWIM *swim, Event *event, struct sockaddr *addr,
                        socklen_t addrlen) {
  process_callback_t *callback = swim_event_processing[event->hdr.type];

  {
    char ubuf[SWIM_UUID_STR_LEN];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    LOG("=== %s from node %s addr %s", swim_event_print(event),
        swim_uuid_str_r(event->hdr.uuid, ubuf, sizeof(ubuf)),
        swim_addr_str_r(addr, addrlen, abuf, sizeof(abuf)));
  }

  /*
   * We need to notice the sender since the "public" address is there
   * and we need to add the sender to the view if it is not already
   * there.
   */
  swim_state_notice(swim, event->hdr.uuid, event->hdr.time,
                    event->hdr.incarnation);
  (*callback)(swim, event, addr, addrlen);
}
