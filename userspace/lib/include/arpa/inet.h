#ifndef SRVROS_POSIX_ARPA_INET_H
#define SRVROS_POSIX_ARPA_INET_H

#include <netinet/in.h>

#define INET_ADDRSTRLEN 16

int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

#endif
