#include "utils.h"

#include <netdb.h>
#include <stdio.h>

#include <sys/socket.h>

const char *addr2str_r(struct sockaddr *addr, socklen_t addrlen, char *buf,
                       size_t buflen) {
  if (addrlen > 0) {
    char host[NI_MAXHOST], service[NI_MAXSERV];
    int err = getnameinfo(addr, addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                          NI_NUMERICSERV);
    if (err != 0) {
      perror("getnameinfo");
      exit(EXIT_FAILURE);
    }

    snprintf(buf, buflen, "%s:%s", host, service);
  }

  return buf;
}
