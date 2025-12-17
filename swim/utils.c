#include "utils.h"

#include <netdb.h>
#include <stdio.h>

#include <sys/socket.h>

const char *swim_addr_str_r(struct sockaddr *addr, socklen_t addrlen, char *buf,
                            size_t buflen) {
  if (addrlen > 0) {
    char host[NI_MAXHOST], service[NI_MAXSERV];
    if (getnameinfo(addr, addrlen, host, sizeof(host), service, sizeof(service),
                    NI_NUMERICSERV) == 1)
      return NULL;

    snprintf(buf, buflen, "%s:%s", host, service);
  }

  return buf;
}

const char *swim_addr_str(struct sockaddr *addr, socklen_t addrlen) {
  static char buf[NI_MAXHOST + NI_MAXSERV + 2];
  return swim_addr_str_r(addr, addrlen, buf, sizeof(buf));
}

extern const char *swim_uuid_str(uuid_t uuid) {
  static char buf[40];
  uuid_unparse(uuid, buf);
  return buf;
}
