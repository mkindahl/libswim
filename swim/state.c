#include "state.h"

#include <netdb.h>
#include <string.h>

#include <sys/socket.h>

#include "swim/debug.h"

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
  } else {
    TRACE("initialized server to listen on %s:%s", host, service);

    swim->addr.ip4 = serveraddr;
    swim->sockfd = fd;

    return true;
  }
}
