#include "swim/network.h"

#include "swim/event.h"

ssize_t swim_recv_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return recvfrom(swim->sockfd, buf, buflen, 0, addr, &addrlen);
}

ssize_t swim_send_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return sendto(swim->sockfd, buf, buflen, 0, addr, addrlen);
}

/*
 * Helper function to send ACK.
 */
void swim_send_ack(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  Event response;

  swim_event_init(swim->uuid, &response, EVENT_TYPE_ACK);
  swim_send_packet(swim, &response, sizeof(response), addr, addrlen);
}

/*
 * Helper function to send PING.
 */
void swim_send_ping(SWIM *swim, struct sockaddr *addr, socklen_t addrlen) {
  Event response;

  swim_event_init(swim->uuid, &response, EVENT_TYPE_PING);
  swim_send_packet(swim, &response, sizeof(response), addr, addrlen);
}
