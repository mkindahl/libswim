#ifndef SWIM_PROCESS_H_
#define SWIM_PROCESS_H_

#include "swim/event.h"
#include "swim/swim.h"

extern void swim_process_event(SWIM *swim, Event *event, size_t bytes,
                               struct sockaddr *addr, socklen_t addrlen);

#endif /* SWIM_PROCESS_H_ */
