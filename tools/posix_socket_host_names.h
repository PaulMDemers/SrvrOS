/* Included guest libc definitions must not interpose on host libc. ELF linkers
 * otherwise retain unrelated public entry points and require unstubbed syscalls.
 * Include after host headers and existing per-fixture remaps. */
#ifndef SRVROS_POSIX_SOCKET_HOST_NAMES_H
#define SRVROS_POSIX_SOCKET_HOST_NAMES_H
#ifndef socket
#define socket audit_socket
#endif
#ifndef socketpair
#define socketpair audit_socketpair
#endif
#ifndef bind
#define bind audit_bind
#endif
#ifndef listen
#define listen audit_listen
#endif
#ifndef connect
#define connect audit_connect
#endif
#ifndef accept
#define accept audit_accept
#endif
#ifndef accept4
#define accept4 audit_accept4
#endif
#ifndef getsockname
#define getsockname audit_getsockname
#endif
#ifndef getpeername
#define getpeername audit_getpeername
#endif
#ifndef shutdown
#define shutdown audit_shutdown
#endif
#ifndef send
#define send audit_send
#endif
#ifndef recv
#define recv audit_recv
#endif
#ifndef sendto
#define sendto audit_sendto
#endif
#ifndef recvfrom
#define recvfrom audit_recvfrom
#endif
#ifndef sendmsg
#define sendmsg audit_sendmsg
#endif
#ifndef recvmsg
#define recvmsg audit_recvmsg
#endif
#ifndef setsockopt
#define setsockopt audit_setsockopt
#endif
#ifndef getsockopt
#define getsockopt audit_getsockopt
#endif
#ifndef htons
#define htons audit_htons
#endif
#ifndef ntohs
#define ntohs audit_ntohs
#endif
#ifndef htonl
#define htonl audit_htonl
#endif
#ifndef ntohl
#define ntohl audit_ntohl
#endif
#ifndef inet_pton
#define inet_pton audit_inet_pton
#endif
#ifndef inet_ntop
#define inet_ntop audit_inet_ntop
#endif
#ifndef getaddrinfo
#define getaddrinfo audit_getaddrinfo
#endif
#ifndef freeaddrinfo
#define freeaddrinfo audit_freeaddrinfo
#endif
#ifndef gai_strerror
#define gai_strerror audit_gai_strerror
#endif
#ifndef getnameinfo
#define getnameinfo audit_getnameinfo
#endif
#ifndef getservbyport_r
#define getservbyport_r audit_getservbyport_r
#endif
#ifndef if_indextoname
#define if_indextoname audit_if_indextoname
#endif
#ifndef in6addr_any
#define in6addr_any audit_in6addr_any
#endif
uint16_t htons(uint16_t value);
uint16_t ntohs(uint16_t value);
uint32_t htonl(uint32_t value);
uint32_t ntohl(uint32_t value);
#endif
