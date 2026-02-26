#include "utils.h"

#include <netdb.h>
#include <stdio.h>

#include <sys/socket.h>

const char *swim_addr_str_r(struct sockaddr *addr, socklen_t addrlen, char *buf,
                            size_t buflen) {
  if (addrlen > 0) {
    char host[NI_MAXHOST], service[NI_MAXSERV];
    if (getnameinfo(addr, addrlen, host, sizeof(host), service, sizeof(service),
                    NI_NUMERICSERV) != 0)
      return NULL;

    snprintf(buf, buflen, "%s:%s", host, service);
  }

  return buf;
}

const char *swim_uuid_str_r(uuid_t uuid, char *buf, size_t buflen) {
  if (buflen < SWIM_UUID_STR_LEN)
    return NULL;
  uuid_unparse(uuid, buf);
  return buf;
}

