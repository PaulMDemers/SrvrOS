#ifndef SRVROS_POSIX_NET_IF_H
#define SRVROS_POSIX_NET_IF_H

#include <stddef.h>

#define IF_NAMESIZE 16

#define IFF_UP 0x1
#define IFF_BROADCAST 0x2
#define IFF_LOOPBACK 0x8
#define IFF_RUNNING 0x40

unsigned int if_nametoindex(const char *ifname);
char *if_indextoname(unsigned int ifindex, char *ifname);

#endif
