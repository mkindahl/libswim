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

#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/utils.h"

/*
 * Process all gossip piggybacked on another event.
 */
static void swim_process_gossip(SWIM *swim, NodeInfo *gossip, int count) {
  for (int i = 0; i < count; ++i) {
    NodeInfo *info = &gossip[i];
    assert(info->last_seen > 0 && info->status != SWIM_STATUS_UNKNOWN);

    /*
     * If node is declared dead and see this gossip, it will exit for
     * now.
     *
     * TODO(mkindahl): Generate a new UUID and re-join the cluster.
     */
    switch (info->status) {
      case SWIM_STATUS_DEAD:
        if (uuid_compare(info->uuid, swim->uuid) == 0) {
          char buf[40];
          uuid_unparse(swim->uuid, buf);
          fprintf(stderr, "Node %s was declared dead, exiting\n", buf);
          exit(EXIT_FAILURE);
        }

      case SWIM_STATUS_ALIVE:
      case SWIM_STATUS_SUSPECT:
      case SWIM_STATUS_UNKNOWN:
        break;
    }

    swim_state_merge(swim, info);
  }
}

/*
 * Process the reception of a PING message.
 *
 * We just respond that we are alive.
 */
static void swim_process_ping(SWIM *swim, Event *event,
                              __attribute__((__unused__)) struct sockaddr *addr,
                              __attribute__((__unused__)) socklen_t addrlen) {
  NodeInfo sender = {.status = SWIM_STATUS_ALIVE};

  swim_send_ack(swim, addr, addrlen);

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  time(&sender.last_seen);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
  swim_process_gossip(swim, event->gossip, event->gossip_count);
}

static void swim_process_ack(SWIM *swim, Event *event,
                             __attribute__((__unused__)) struct sockaddr *addr,
                             __attribute__((__unused__)) socklen_t addrlen) {
  NodeInfo sender = {.status = SWIM_STATUS_ALIVE};

  memcpy(&sender.addr, addr, addrlen);
  uuid_copy(sender.uuid, event->hdr.uuid);
  time(&sender.last_seen);
  sender.addrlen = addrlen;

  swim_state_add(swim, &sender);
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
   * We skip adding the node if we have already added it. This is
   * needed since we can receive a rumor that the node has joined as a
   * result of the forwarding above
   */
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

  swim_send_ack(swim, addr, addrlen);
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
  swim_send_ack(swim, addr, addrlen);
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
    [EVENT_TYPE_ACK] = swim_process_ack,
    [EVENT_TYPE_JOIN] = swim_process_join,
    [EVENT_TYPE_LEAVE] = swim_process_leave,
};

void swim_process_event(SWIM *swim, Event *event, size_t bytes,
                        struct sockaddr *addr, socklen_t addrlen) {
  char buf[NI_MAXHOST + NI_MAXSERV + 1];
  char uuidbuf[40];
  process_callback_t *callback = swim_event_processing[event->hdr.type];

  uuid_unparse(event->hdr.uuid, uuidbuf);
  LOG("node %s addr %s (%lu bytes) -> %s", uuidbuf,
      addr2str_r(addr, addrlen, buf, sizeof(buf)), bytes,
      swim_event_print(event));

  /* We notice that we have seen the sending server as well, so that we do not
   * need to ping it unnecessarily. */
  swim_state_update_time(swim, event->hdr.uuid, event->hdr.time);
  (*callback)(swim, event, addr, addrlen);
}
