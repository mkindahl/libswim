#include "server.h"

#include <sys/time.h>

#include "swim/cluster.h"
#include "swim/defs.h"
#include "swim/network.h"
#include "swim/process.h"

int swim_listen_port = SWIM_DEFAULT_PORTNO;

void swim_server_loop(SWIM *swim) {
  char buf[SWIM_MAX_PACKET_SIZE];
  struct sockaddr_storage addr_storage;
  time_t last_printout = 0;

  while (swim_server_running(swim)) {
    ssize_t bytes;
    struct sockaddr *addr = (struct sockaddr *)&addr_storage;
    socklen_t addrlen = sizeof(addr_storage);
    Event *event = (Event *)buf;
    time_t next_printout;

    bytes = swim_recv_event(swim, event, addr, &addrlen);
    if (bytes > 0)
      swim_process_event(swim, event, addr, addrlen);

    swim_cluster_heartbeat(swim);
    if (last_printout + 5 < time(&next_printout)) {
      swim_state_print(swim);
      last_printout = next_printout;
    }
  }
}
