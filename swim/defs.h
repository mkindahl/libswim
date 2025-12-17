#ifndef SWIM_DEFS_H_
#define SWIM_DEFS_H_

#define SWIM_DEFAULT_PORTNO 7946  /* Default port number */
#define SWIM_MAXPACKET 2048       /* Max UDP packet size */
#define SWIM_MAXLEAVE 5           /* Maximum number of leave events sent */
#define SWIM_MAX_GOSSIP_SIZE 10   /* Max number of nodes in gossip */
#define SWIM_MAXPINGREQ 5         /* Max number of indirect pings done */
#define SWIM_HEARTBEAT_INTERVAL 1 /* Interval between heartbeats in seconds */

#endif /* SWIM_DEFS_H_ */
