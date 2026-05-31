#ifndef SRVROS_POSIX_NETINET_IN_H
#define SRVROS_POSIX_NETINET_IN_H

#include <stdint.h>
#include <sys/socket.h>

typedef uint32_t in_addr_t;
typedef uint16_t in_port_t;

struct in_addr {
    in_addr_t s_addr;
};

struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in {
    sa_family_t sin_family;
    in_port_t sin_port;
    struct in_addr sin_addr;
    unsigned char sin_zero[8];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};

extern const struct in6_addr in6addr_any;

#define INADDR_ANY 0x00000000u
#define INADDR_LOOPBACK 0x7f000001u
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46
#define IN6_IS_ADDR_LINKLOCAL(address) \
    ((((const unsigned char *)(address))[0] == 0xfe) && ((((const unsigned char *)(address))[1] & 0xc0) == 0x80))

uint16_t htons(uint16_t value);
uint16_t ntohs(uint16_t value);
uint32_t htonl(uint32_t value);
uint32_t ntohl(uint32_t value);

#endif
