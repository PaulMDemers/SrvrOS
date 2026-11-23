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

#define IP_TTL 2
/* Public option identifiers also let callers handle unavailable features. */
#define IP_MULTICAST_LOOP 34
/* Source compatibility for callers that handle unsupported IPv6 sockets. */
#define IPV6_UNICAST_HOPS 16
#define IPV6_MULTICAST_LOOP 19

#define INADDR_ANY 0x00000000u
#define INADDR_LOOPBACK 0x7f000001u
#define IN_MULTICAST(address) (((uint32_t)(address) & 0xf0000000u) == 0xe0000000u)
#define IN6ADDR_ANY_INIT {{0}}
#define IN6ADDR_LOOPBACK_INIT {{0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 1}}
#define INET_ADDRSTRLEN 16
#define INET6_ADDRSTRLEN 46

/* IPv4 classifiers take host-order integers. IPv6 classifiers read the
 * network-order bytes and evaluate the caller's pointer only once. */
static inline int __srvros_in6_is_zero(const unsigned char *address, unsigned count) {
    for (unsigned i = 0; i < count; ++i) {
        if (address[i] != 0) return 0;
    }
    return 1;
}

static inline int __srvros_in6_is_loopback(const unsigned char *address) {
    return __srvros_in6_is_zero(address, 15) && address[15] == 1;
}

static inline int __srvros_in6_has_prefix(const unsigned char *address,
                                        unsigned first, unsigned mask, unsigned value) {
    return address[0] == first && (address[1] & mask) == value;
}

#define IN6_IS_ADDR_UNSPECIFIED(address) __srvros_in6_is_zero((const unsigned char *)(address), 16)
#define IN6_IS_ADDR_LOOPBACK(address) __srvros_in6_is_loopback((const unsigned char *)(address))
#define IN6_IS_ADDR_LINKLOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xfe, 0xc0, 0x80)
#define IN6_IS_ADDR_SITELOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xfe, 0xc0, 0xc0)
#define IN6_IS_ADDR_MULTICAST(address) (((const unsigned char *)(address))[0] == 0xff)
#define IN6_IS_ADDR_MC_NODELOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xff, 0x0f, 1)
#define IN6_IS_ADDR_MC_LINKLOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xff, 0x0f, 2)
#define IN6_IS_ADDR_MC_SITELOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xff, 0x0f, 5)
#define IN6_IS_ADDR_MC_ORGLOCAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xff, 0x0f, 8)
#define IN6_IS_ADDR_MC_GLOBAL(address) \
    __srvros_in6_has_prefix((const unsigned char *)(address), 0xff, 0x0f, 14)

#ifdef __cplusplus
extern "C" {
#endif
extern const struct in6_addr in6addr_any;
extern const struct in6_addr in6addr_loopback;
#ifdef __cplusplus
}
#endif

uint16_t htons(uint16_t value);
uint16_t ntohs(uint16_t value);
uint32_t htonl(uint32_t value);
uint32_t ntohl(uint32_t value);

#endif
