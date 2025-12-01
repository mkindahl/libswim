#ifndef LIBSWIM_SWIM_H_
#define LIBSWIM_SWIM_H_

#include "event.h"

#include <netinet/in.h>

struct SWIM {
  int sockfd;
  union {
    struct sockaddr_in ip4;
    struct sockaddr_in6 ip6;
  } addr;
};

typedef struct SWIM SWIM;

#endif /* LIBSWIM_SWIM_H_ */
