#ifndef SWIM_STATE_H_
#define SWIM_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "swim/event.h"

#include <netinet/in.h>

typedef struct SWIM {
  int sockfd;
  union {
    struct sockaddr_in ip4;
    struct sockaddr_in6 ip6;
  } addr;

  uuid_t uuid;

  size_t view_capacity;
  size_t view_size;
  Instance* view;
} SWIM;

extern bool swim_state_init(SWIM* swim, uint16_t port);
extern void swim_state_print(SWIM* swim);
extern void swim_state_update_time(SWIM* swim, uuid_t* uuid,
                                   struct timespec* time);
extern void swim_state_add_instance(SWIM* swim, Instance* instance);
extern void swim_state_del_instance(SWIM* swim, uuid_t);

#endif /* SWIM_STATE_H_ */
