/*
 * PThread version of server implementation.
 *
 * This contains the POSIX Thread version of the server.
 */

#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "swim/cluster.h"
#include "swim/network.h"
#include "swim/server.h"
#include "swim/state.h"

struct PThreadServerState {
  pthread_t thread;
  bool stopped;
  SwimAddress address;
};

static void *pthread_server_loop(void *arg) {
  SWIM *swim = arg;
  struct PThreadServerState *state = swim->server;
  SwimAddress *address = &state->address;

  if (address->addrlen > 0)
    swim_cluster_join(swim, (struct sockaddr *)&address->addr,
                      address->addrlen);

  swim_server_loop(swim);
  return NULL;
}

void swim_server_init(SWIM *swim,
                      __attribute__((unused)) const SwimAddress *address) {
  struct PThreadServerState *state = malloc(sizeof(struct PThreadServerState));
  if (state == NULL)
    return;

  memset(state, 0, sizeof(*state));
  state->address = *address;

  swim_state_init(swim, swim_listen_port);
  swim->server = state;
}

const char *swim_server_start(SWIM *swim) {
  struct PThreadServerState *state = swim->server;
  if (pthread_create(&state->thread, NULL, pthread_server_loop, swim) != 0)
    return strerror(errno);
  return NULL;
}

const char *swim_server_stop(SWIM *swim) {
  struct PThreadServerState *state = swim->server;
  state->stopped = true;
  return NULL;
}

const char *swim_server_wait(SWIM *swim) {
  struct PThreadServerState *state = swim->server;

  pthread_join(state->thread, NULL);
  return NULL;
}

bool swim_server_running(SWIM *swim) {
  struct PThreadServerState *state = swim->server;
  return !state->stopped;
}
