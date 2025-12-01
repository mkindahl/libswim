#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>

#include <sys/socket.h>

static const char* show_family(int val) {
  const char* symbols[] = {
      .AF_INET = "AF_INET",
      .AF_INET6 = "AF_INET6",
  };
}

const char* show_address(struct sockaddr* addr, socklen_t addrlen, char* buf,
                         size_t buflen) {
  char hostname[NI_MAXHOST], service[NI_MAXSERV];

  if (getnameinfo(addr, addrlen, hostname, sizeof(hostname), service,
                  sizeof(service), NI_NAMEREQD))
    return NULL;
  snprintf(buf, buflen, "family %s host %s service %s",
           show_family(addr->sa_family), hostname, service);

  return buf;
}
