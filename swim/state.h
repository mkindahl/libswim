#ifndef SWIM_STATE_H_
#define SWIM_STATE_H_

#include <stdbool.h>
#include <stdint.h>

#include "swim/event.h"

#include <netinet/in.h>

/*
 * Instance state data.
 *
 * This stores additional data beyond the normal instance data that
 * are present in the events. In particular, address of the instance
 * as seen when receiving an event from a joining instance and
 * outstanding ping requests.
 */
typedef struct InstanceState {
  InstanceData base;
} InstanceState;

/*
 * SWIM state including current view.
 *
 * The view does not include the node instance itself since that is
 * unnecessary to track.
 */
typedef struct SWIM {
  int sockfd;
  struct sockaddr_in addr;
  uuid_t uuid;
  int view_capacity;
  int view_size;
  InstanceState* view;
} SWIM;

extern bool swim_state_init(SWIM* swim, uint16_t port);
extern void swim_state_add(SWIM* swim, InstanceData* instance);
extern void swim_state_del(SWIM* swim, uuid_t);
extern void swim_state_print(SWIM* swim);
extern InstanceState* swim_state_get(SWIM* swim, uuid_t uuid);
extern void swim_state_update_time(SWIM* swim, uuid_t uuid,
                                   struct timespec* time);
extern void swim_state_set_status(SWIM* swim, uuid_t uuid, Status status);

#endif /* SWIM_STATE_H_ */
