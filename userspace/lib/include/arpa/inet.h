#ifndef SRVROS_POSIX_ARPA_INET_H
#define SRVROS_POSIX_ARPA_INET_H

#include <netinet/in.h>

#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

#ifdef __cplusplus
extern "C" {
#endif

/* Parse IPv4 numbers-and-dots notation. A null output validates only. */
int inet_aton(const char *src, struct in_addr *dst);
int inet_pton(int af, const char *src, void *dst);
const char *inet_ntop(int af, const void *src, char *dst, socklen_t size);

#ifdef __cplusplus
}
#endif

#endif
