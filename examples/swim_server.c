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

int main(int argc, char **argv) {
  SWIM swim;
  char buf[BUFSIZE];
  struct sockaddr_storage clientaddr;

  swim_state_init(&swim, SWIM_DEFAULT_PORTNO);

  while (1) {
    Event *event = (Event *)buf;
    size_t bytes;
    const char *hostaddrp;

    bytes =
        swim_recv_packet(&swim, buf, sizeof(buf),
                         (struct sockaddr *)&clientaddr, sizeof(clientaddr));
    hostaddrp = inet_ntoa(clientaddr.sin_addr);
    if (hostaddrp == NULL) {
      perror("inet_ntoa");
      exit(1);
    }

    swim_process_event(&swim, event, bytes, (struct sockaddr *)&clientaddr,
                       sizeof(clientaddr));
  }
}
