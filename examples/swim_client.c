#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sys/socket.h>
#include <sys/types.h>

#include "swim/event.h"
#include "swim/swim.h"

#include <arpa/inet.h>
#include <bits/time.h>
#include <netinet/in.h>

#define BUFSIZE 1024

#define STR(X) EXPAND(X)
#define EXPAND(X) #X

#define TRACE(MSG, ...)                                        \
  do {                                                         \
    fprintf(stderr, "%s: " MSG "\n", __func__, ##__VA_ARGS__); \
  } while (0)

int main(int argc, char **argv) {
  Event event;
  int sockfd, err;
  ssize_t bytes;
  struct sockaddr_storage serveraddr;
  socklen_t serverlen = sizeof(serveraddr);
  char hostname[NI_MAXHOST];
  char service[NI_MAXSERV];
  char buf[BUFSIZE];
  char str[BUFSIZE];
  struct addrinfo hints;
  struct addrinfo *result;

  /* check command line arguments */
  if (argc < 3) {
    fprintf(stderr, "usage: %s <hostname> <event>\n", argv[0]);
    exit(0);
  }

  memset(&hints, 0, sizeof(hints));
  hints.ai_family = AF_INET;       /* Allow IPv4 */
  hints.ai_socktype = SOCK_DGRAM;  /* Datagram socket */
  hints.ai_flags = AI_NUMERICSERV; /* Numeric server */
  hints.ai_protocol = 0;           /* Any protocol */

  err = getaddrinfo(argv[1], STR(SWIM_DEFAULT_PORTNO), &hints, &result);
  if (err != 0) {
    fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
    exit(EXIT_FAILURE);
  }

  if (getnameinfo(result->ai_addr, result->ai_addrlen, hostname,
                  sizeof(hostname), service, sizeof(service), NI_NAMEREQD)) {
    perror("getnameinfo");
    exit(EXIT_FAILURE);
  } else {
    TRACE("getaddrinfo returned host %s service %s", hostname, service);
  }

  sockfd = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
  if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
  }

  memcpy(&serveraddr, result->ai_addr, result->ai_addrlen);

  freeaddrinfo(result);

  memset(&event, 0, sizeof(Event));

  if (strcmp(argv[2], "ping") == 0) {
    event.hdr.type = EVENT_TYPE_PING;
  } else if (strcmp(argv[2], "ack") == 0) {
    event.hdr.type = EVENT_TYPE_ACK;
  } else if (strcmp(argv[2], "join") == 0) {
    event.hdr.type = EVENT_TYPE_JOIN;
    if (argc > 3)
      uuid_parse(argv[3], event.join.join_uuid);
    else {
      fprintf(stderr, "need a UUID\n");
      exit(EXIT_FAILURE);
    }
  } else if (strcmp(argv[2], "leave") == 0) {
    event.hdr.type = EVENT_TYPE_LEAVE;
    if (argc > 3)
      uuid_parse(argv[3], event.leave.leave_uuid);
    else {
      fprintf(stderr, "need a UUID\n");
      exit(EXIT_FAILURE);
    }
  } else {
    fprintf(stderr, "ERROR, no such host as %s\n", hostname);
    exit(EXIT_FAILURE);
  }

  clock_gettime(CLOCK_REALTIME, &event.hdr.time);

  uuid_generate(event.hdr.uuid);

  bytes = sendto(sockfd, &event, sizeof(event), 0,
                 (struct sockaddr *)&serveraddr, serverlen);
  if (bytes < 0) {
    perror("sendto");
    exit(EXIT_FAILURE);
  }

  /* Wait and read the server's reply if we sent a ping */
  if (event.hdr.type == EVENT_TYPE_PING || event.hdr.type == EVENT_TYPE_JOIN ||
      event.hdr.type == EVENT_TYPE_LEAVE) {
    bytes = recv(sockfd, buf, sizeof(buf), 0);
    if (bytes < 0) {
      perror("recv");
      exit(EXIT_FAILURE);
    }

    if (swim_event_string((Event *)buf, str, sizeof(str)))
      printf("received %lu bytes: %s\n", bytes, str);
  }

  exit(EXIT_SUCCESS);
}
