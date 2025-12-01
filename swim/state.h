#ifndef SWIM_STATE_H_
#define SWIM_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include <netinet/in.h>

struct SWIM {
  int sockfd;
  union {
    struct sockaddr_in ip4;
    struct sockaddr_in6 ip6;
  } addr;
};

typedef struct SWIM SWIM;

extern bool swim_state_init(SWIM *swim, uint16_t port);

#endif /* SWIM_STATE_H_ */
