#ifndef SWIM_UTILS_H_
#define SWIM_UTILS_H_

#include <stdlib.h>

#include <sys/socket.h>

#include <uuid/uuid.h>

#define SWIM_UUID_STR_LEN 37 /* 36 hex chars + null */

extern const char *swim_addr_str_r(struct sockaddr *addr, socklen_t addrlen,
                                   char *buf, size_t buflen);
extern const char *swim_uuid_str_r(uuid_t uuid, char *buf, size_t buflen);

#endif /* SWIM_UTILS_H_ */
