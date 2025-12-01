#include "swim/network.h"

ssize_t swim_recv_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return recvfrom(swim->sockfd, buf, buflen, 0, addr, &addrlen);
}

ssize_t swim_send_packet(SWIM *swim, void *buf, size_t buflen,
                         struct sockaddr *addr, socklen_t addrlen) {
  return sendto(swim->sockfd, buf, buflen, 0, addr, addrlen);
}
