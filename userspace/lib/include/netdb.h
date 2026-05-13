#ifndef SRVROS_POSIX_NETDB_H
#define SRVROS_POSIX_NETDB_H

#include <sys/socket.h>

#define AI_PASSIVE 0x01
#define EAI_FAIL -4
#define EAI_MEMORY -10
#define EAI_NONAME -2
#define EAI_SERVICE -8

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

int getaddrinfo(const char *node,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res);
void freeaddrinfo(struct addrinfo *res);
const char *gai_strerror(int errcode);

#endif
