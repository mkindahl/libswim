#ifndef SWIM_SERVER_H_
#define SWIM_SERVER_H_

#include "swim/network.h"
#include "swim/state.h"

extern int swim_listen_port;
extern bool swim_tracing_on;
extern bool swim_verbose;

extern void swim_server_init(SWIM *swim, const SwimAddress *address);
extern const char *swim_server_start(SWIM *swim);
extern const char *swim_server_stop(SWIM *swim);
extern const char *swim_server_wait(SWIM *swim);
extern bool swim_server_running(SWIM *swim);
extern void swim_server_loop(SWIM *swim);

#endif /* SWIM_SERVER_H_ */
