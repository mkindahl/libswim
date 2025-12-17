#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/types.h>

#include "swim/cluster.h"
#include "swim/debug.h"
#include "swim/defs.h"
#include "swim/event.h"
#include "swim/network.h"
#include "swim/process.h"

#define STR(X) EXPAND(X)
#define EXPAND(X) #X

struct options {
  int listen_port;
  struct sockaddr_storage addr;
  socklen_t addrlen;
};

static void server_loop(SWIM *swim);

/*
 * Print usage and exit.
 */
static void print_usage(const char *program_name) {
  fprintf(stderr, "usage: %s [ -v ] [ -l PORT ] [ HOSTNAME [ PORT ] ]\n",
          program_name);
  exit(EXIT_FAILURE);
}

static struct options parse_options(int argc, char *argv[]) {
  int opt, err;
  struct addrinfo *result, hints;
  struct options options = {.listen_port = SWIM_DEFAULT_PORTNO};
  char *service = STR(SWIM_DEFAULT_PORTNO);
  char *hostname = NULL;

  while ((opt = getopt(argc, argv, "dvl:")) != -1) {
    switch (opt) {
      case 'd':
        tracing_on = true;
        break;
      case 'v':
        verbose = true;
        break;
      case 'l':
        options.listen_port = atoi(optarg);
        break;
      default:
        print_usage(argv[0]);
    }
  }

  if (optind < argc)
    hostname = argv[optind++];

  if (optind < argc)
    service = argv[optind++];

  if (hostname != NULL) {
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = 0;
    hints.ai_flags = 0;

    err = getaddrinfo(hostname, service, &hints, &result);
    if (err != 0) {
      fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(err));
      exit(EXIT_FAILURE);
    }

    memcpy(&options.addr, result->ai_addr, result->ai_addrlen);
    options.addrlen = result->ai_addrlen;

    freeaddrinfo(result);
  }

  return options;
}

int main(int argc, char *argv[]) {
  SWIM swim;
  struct options options;

  options = parse_options(argc, argv);

  swim_state_init(&swim, options.listen_port);

  if (options.addrlen > 0)
    swim_cluster_join(&swim, (struct sockaddr *)&options.addr, options.addrlen);

  server_loop(&swim);
}

static void server_loop(SWIM *swim) {
  ssize_t bytes;
  char buf[SWIM_MAXPACKET];
  struct sockaddr_storage addr_storage;

  while (true) {
    struct sockaddr *addr = (struct sockaddr *)&addr_storage;
    socklen_t addrlen = sizeof(addr_storage);
    Event *event = (Event *)buf;

    bytes = swim_recv_packet(swim, buf, sizeof(buf), addr, addrlen);
    if (bytes > 0) {
      swim_process_event(swim, event, bytes, addr, addrlen);
    } else if (swim->view_size > 0) {
      swim_cluster_heartbeat(swim);
    }

    swim_state_print(swim);
  }
}
