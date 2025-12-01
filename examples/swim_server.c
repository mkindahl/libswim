#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include "swim/debug.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/process.h"
#include "swim/swim.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFSIZE 1024

int main(__attribute__((__unused__)) int argc,
         __attribute__((__unused__)) char **argv) {
  SWIM swim;
  char buf[BUFSIZE];
  struct sockaddr_storage addr_storage;
  struct sockaddr *addr = (struct sockaddr *)&addr_storage;
  socklen_t addrlen = sizeof(addr_storage);

  swim_state_init(&swim, SWIM_DEFAULT_PORTNO);

  while (1) {
    Event *event = (Event *)buf;
    size_t bytes = swim_recv_packet(&swim, buf, sizeof(buf), addr, addrlen);
    swim_process_event(&swim, event, bytes, addr, addrlen);
    swim_state_print(&swim);
  }
}
