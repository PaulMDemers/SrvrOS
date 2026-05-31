#ifndef SRVROS_POSIX_NETDB_H
#define SRVROS_POSIX_NETDB_H

#include <sys/socket.h>

#define AI_PASSIVE 0x01
#define AI_ADDRCONFIG 0x20
#define EAI_FAIL -4
#define EAI_MEMORY -10
#define EAI_NONAME -2
#define EAI_SERVICE -8
#define NI_MAXHOST 1025
#define NI_MAXSERV 32
#define NI_NUMERICHOST 0x01
#define NI_NUMERICSERV 0x02
#define NI_NOFQDN 0x04
#define NI_NAMEREQD 0x08
#define NI_DGRAM 0x10

struct addrinfo {
    int ai_flags;
    int ai_family;
    int ai_socktype;
    int ai_protocol;
    socklen_t ai_addrlen;
    struct sockaddr *ai_addr;
    char *ai_canonname;
    struct addrinfo *ai_next;
};

struct hostent {
    char *h_name;
    char **h_aliases;
    int h_addrtype;
    int h_length;
    char **h_addr_list;
};

struct servent {
    char *s_name;
    char **s_aliases;
    int s_port;
    char *s_proto;
};

int getaddrinfo(const char *node,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);
int getnameinfo(const struct sockaddr *addr,
    socklen_t addrlen,
    char *host,
    socklen_t hostlen,
    char *serv,
    socklen_t servlen,
    int flags);
struct servent *getservbyname(const char *name, const char *proto);
int getservbyport_r(int port,
    const char *proto,
    struct servent *result_buf,
    char *buf,
    size_t buflen,
    struct servent **result);

#endif
