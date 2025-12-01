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
#include "swim/swim.h"

#include <arpa/inet.h>
#include <netinet/in.h>

#define BUFSIZE 1024

void swim_process_ping(SWIM *swim, Event *event, struct sockaddr *addr,
                       socklen_t addrlen) {
  struct PingEvent *ping = &event->body.ping;
  TRACE("sending ping");
  size_t bytes =
      swim_send_packet(swim, event, sizeof(struct AckEvent), addr, addrlen);
}

void swim_process_ack(SWIM *swim, Event *event, struct sockaddr *addr,
                      socklen_t addrlen) {
  struct AckEvent *ack = &event->body.ack;
  TRACE("processing ack");
}

struct EventInfo {
  const char *name;
  void (*callback)(SWIM *swim, Event *event, struct sockaddr *addr,
                   socklen_t addrlen);
};

static struct EventInfo event_type_name[] = {
    [EVENT_TYPE_PING] =
        {
            .name = "PING",
            .callback = swim_process_ping,
        },
    [EVENT_TYPE_ACK] =
        {
            .name = "ACK",
            .callback = swim_process_ack,
        },
};

bool swim_state_init(SWIM *swim, uint16_t port) {
  struct sockaddr_in serveraddr;
  char host[NI_MAXHOST], service[NI_MAXSERV];
  int err, fd;
  ssize_t res;

  fd = socket(AF_INET, SOCK_DGRAM, 0);
  if (fd < 0)
    return false;

  memset(&serveraddr, 0, sizeof(serveraddr));
  serveraddr.sin_family = AF_INET;
  serveraddr.sin_addr.s_addr = htonl(INADDR_ANY);
  serveraddr.sin_port = htons(port);

  res = bind(fd, (struct sockaddr *)&serveraddr, sizeof(serveraddr));
  if (res < 0)
    return false;

  err = getnameinfo((struct sockaddr *)&serveraddr, sizeof(serveraddr), host,
                    NI_MAXHOST, service, NI_MAXSERV, NI_NUMERICSERV);
  if (err != 0) {
    TRACE("getnameinfo: %s", gai_strerror(err));
    return false;
  } else {
    TRACE("initialized server to listen on %s:%s", host, service);

    swim->addr.ip4 = serveraddr;
    swim->sockfd = fd;

    return true;
  }
}

void swim_event_process(SWIM *swim, Event *event, size_t bytes,
                        struct sockaddr *addr, socklen_t addrlen) {
  char host[NI_MAXHOST], service[NI_MAXSERV];
  struct EventInfo *info = &event_type_name[event->hdr.type];
  int s = getnameinfo(addr, addrlen, host, NI_MAXHOST, service, NI_MAXSERV,
                      NI_NUMERICSERV);
  if (s != 0)
    fprintf(stderr, "getnameinfo: %s\n", gai_strerror(s));

  TRACE("Got %s of %zd bytes from %s:%s", info->name, bytes, host, service);

  (*info->callback)(swim, event, addr, addrlen);
}

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

    swim_event_process(&swim, event, bytes, (struct sockaddr *)&clientaddr,
                       sizeof(clientaddr));
  }
}
