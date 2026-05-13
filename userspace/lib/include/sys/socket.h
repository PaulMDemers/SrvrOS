#ifndef SRVROS_POSIX_SYS_SOCKET_H
#define SRVROS_POSIX_SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#define AF_INET 2
#define PF_INET AF_INET
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define SOL_SOCKET 1
#define SO_REUSEADDR 2

typedef uint16_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

int socket(int domain, int type, int protocol);
int bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
ssize_t send(int fd, const void *buffer, size_t length, int flags);
ssize_t recv(int fd, void *buffer, size_t length, int flags);
int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len);

#endif
