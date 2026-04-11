#include <curses.h>
#include <netdb.h>
#include <signal.h>
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

#include "swim/logging.h"
#include "swim/network.h"
#include "swim/node.h"
#include "swim/swim.h"
#include "swim/utils.h"

#ifdef SWIM_TRACING
#define SWIM_OPTSTRING "dvl:"
#define SWIM_OPTUSAGE "[ -d ] "
#else
#define SWIM_OPTSTRING "vl:"
#define SWIM_OPTUSAGE ""
#endif

#define STR(X) EXPAND(X)
#define EXPAND(X) #X

#define LOG_RING_SIZE 256
#define LOG_LINE_MAX 256

static SWIM swim;
static WINDOW *status_win;
static WINDOW *log_win;
static char log_ring[LOG_RING_SIZE][LOG_LINE_MAX];
static int log_head = 0;
static int log_count = 0;

static void tui_cleanup(void) {
  endwin();
}

static void create_windows(void) {
  int rows, cols;
  int status_height;

  getmaxyx(stdscr, rows, cols);
  status_height = rows * 2 / 5;
  if (status_height < 4)
    status_height = 4;

  if (status_win)
    delwin(status_win);
  if (log_win)
    delwin(log_win);

  status_win = newwin(status_height, cols, 0, 0);
  log_win = newwin(rows - status_height, cols, status_height, 0);

  scrollok(log_win, TRUE);
}

static void render_log(void) {
  int rows, cols;

  getmaxyx(log_win, rows, cols);
  (void)cols;
  werase(log_win);

  box(log_win, 0, 0);
  mvwprintw(log_win, 0, 2, " Log ");

  int visible = rows - 2;
  int start = 0;
  int count = log_count;

  if (count > visible) {
    start = count - visible;
    count = visible;
  }

  for (int i = 0; i < count; i++) {
    int idx =
        (log_head - log_count + start + i + LOG_RING_SIZE) % LOG_RING_SIZE;
    mvwprintw(log_win, 1 + i, 1, "%.*s", cols - 2, log_ring[idx]);
  }

  wrefresh(log_win);
}

static void tui_log_sink(const char *msg, void *userdata) {
  (void)userdata;

  strncpy(log_ring[log_head], msg, LOG_LINE_MAX - 1);
  log_ring[log_head][LOG_LINE_MAX - 1] = '\0';
  log_head = (log_head + 1) % LOG_RING_SIZE;
  if (log_count < LOG_RING_SIZE)
    log_count++;

  render_log();
}

static void tui_state_print(SWIM *s, void *userdata) {
  int rows, cols;
  char ubuf[SWIM_UUID_STR_LEN];

  (void)userdata;

  getmaxyx(status_win, rows, cols);
  (void)cols;
  werase(status_win);

  box(status_win, 0, 0);
  mvwprintw(status_win, 0, 2, " Nodes ");

  mvwprintw(status_win,
            1,
            1,
            "Self: %s",
            swim_uuid_str_r(s->uuid, ubuf, sizeof(ubuf)));

  mvwprintw(status_win,
            2,
            1,
            "%-36s %-19s %-8s %-21s %-21s",
            "UUID",
            "LAST_SEEN",
            "STATUS",
            "ADDRESS",
            "WITNESS");

  for (int i = 0; i < s->view_size && i < rows - 4; i++) {
    NodeState *node = &s->view[i];
    char tbuf[20];
    char abuf[NI_MAXHOST + NI_MAXSERV + 2];
    struct tm t;

    const char *timestr = "";
    if (localtime_r(&node->info.last_seen, &t)) {
      strftime(tbuf, sizeof(tbuf), "%F %T", &t);
      timestr = tbuf;
    }

    const char *address = swim_addr_str_r((struct sockaddr *)&node->info.addr,
                                          node->info.addrlen,
                                          abuf,
                                          sizeof(abuf));
    if (!address)
      address = "";

    char wbuf[NI_MAXHOST + NI_MAXSERV + 2];
    const char *witness = "";
    if (node->witness.addrlen > 0)
      witness = swim_addr_str_r((struct sockaddr *)&node->witness.addr,
                                node->witness.addrlen,
                                wbuf,
                                sizeof(wbuf));

    mvwprintw(status_win,
              3 + i,
              1,
              "%-36s %-19s %-8s %-21s %-21s",
              swim_uuid_str_r(node->info.uuid, ubuf, sizeof(ubuf)),
              timestr,
              swim_status_name(node->info.status),
              address,
              witness);
  }

  if (s->view_size >= rows - 4) {
    mvwprintw(status_win, rows - 1, 1, "(%d more)", s->view_size - (rows - 4));
  }

  wrefresh(status_win);
}

static void handle_sigint(int sig) {
  (void)sig;
  swim_server_stop(&swim);
}

static void handle_sigwinch(int sig) {
  (void)sig;
  endwin();
  refresh();
  create_windows();
  tui_state_print(&swim, NULL);
  render_log();
}

static void print_usage(const char *program_name) {
  endwin();
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
  SwimAddress address = parse_options(argc, argv);

  initscr();
  cbreak();
  noecho();
  curs_set(0);
  atexit(tui_cleanup);

  create_windows();

  swim_set_log_sink(tui_log_sink, NULL);
  swim_state_set_print_callback(tui_state_print, NULL);

  signal(SIGINT, handle_sigint);
  signal(SIGWINCH, handle_sigwinch);

  swim_server_init(&swim, &address);
  tui_state_print(&swim, NULL);
  render_log();

  swim_server_start(&swim);
  swim_server_wait(&swim);
}
