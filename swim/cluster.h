#ifndef SWIM_CLUSTER_H_
#define SWIM_CLUSTER_H_

#include <sys/socket.h>

#include "swim/state.h"

extern void swim_cluster_join(SWIM* swim, struct sockaddr* addr,
                              socklen_t addrlen);
extern void swim_cluster_leave(SWIM* swim);

#endif /* SWIM_CLUSTER_H_ */
