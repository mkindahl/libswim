#ifndef SWIM_UTILS_H_
#define SWIM_UTILS_H_

#include <stdlib.h>

#include <sys/socket.h>

#include <uuid/uuid.h>

extern const char *swim_addr_str(struct sockaddr *addr, socklen_t addrlen);
extern const char *swim_addr_str_r(struct sockaddr *addr, socklen_t addrlen,
                                   char *buf, size_t buflen);
extern const char *swim_uuid_str(uuid_t uuid);

#endif /* SWIM_UTILS_H_ */
