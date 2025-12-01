#include <stdlib.h>

#include <sys/socket.h>

#include "swim/state.h"

extern ssize_t swim_send_packet(SWIM *swim, void *buf, size_t buflen,
                                struct sockaddr *addr, socklen_t addrlen);
extern ssize_t swim_recv_packet(SWIM *swim, void *buf, size_t buflen,
                                struct sockaddr *addr, socklen_t addrlen);
