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

#include "swim/network.h"
#include "swim/swim.h"

#ifdef SWIM_TRACING
#define SWIM_OPTSTRING "dvl:"
#define SWIM_OPTUSAGE "[ -d ] "
#else
#define SWIM_OPTSTRING "vl:"
#define SWIM_OPTUSAGE ""
#endif

#define STR(X) EXPAND(X)
#define EXPAND(X) #X

/*
 * Print usage and exit.
 */
static void print_usage(const char *program_name) {
  fprintf(stderr,
          "usage: %s " SWIM_OPTUSAGE
          " [ -v ] [ -l PORT ] [ HOSTNAME [ PORT ] ]\n",
          program_name);
  exit(EXIT_FAILURE);
}

static SwimAddress parse_options(int argc, char *argv[]) {
  int opt, err;
  struct addrinfo *result, hints;
  SwimAddress options = {.addrlen = 0};
  char *service = STR(SWIM_DEFAULT_PORTNO);
  char *hostname = NULL;

  while ((opt = getopt(argc, argv, SWIM_OPTSTRING)) != -1) {
    switch (opt) {
#ifdef SWIM_TRACING
      case 'd':
        swim_tracing_on = true;
        break;
#endif
      case 'v':
        swim_verbose = true;
        break;
      case 'l':
        swim_listen_port = atoi(optarg);
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
  SwimAddress address = parse_options(argc, argv);

  swim_server_init(&swim, &address);
  swim_server_start(&swim);
  swim_server_wait(&swim);
}
