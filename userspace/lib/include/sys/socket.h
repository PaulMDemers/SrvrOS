#ifndef SRVROS_POSIX_SYS_SOCKET_H
#define SRVROS_POSIX_SYS_SOCKET_H

#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/uio.h>

#define AF_INET 2
#define AF_UNIX 1
#define AF_LOCAL AF_UNIX
#define PF_INET AF_INET
#define PF_UNIX AF_UNIX
#define PF_LOCAL AF_LOCAL
#define SOCK_STREAM 1
#define SOCK_DGRAM 2
#define SOCK_NONBLOCK 0x0800
#define SOCK_CLOEXEC 0x80000
#define IPPROTO_TCP 6
#define IPPROTO_UDP 17
#define SOL_SOCKET 1
#define SO_REUSEADDR 2
#define SO_ERROR 4
#define SO_TYPE 3
#define SO_ACCEPTCONN 30
#define SO_RCVBUF 8
#define SO_SNDBUF 7
#define SO_KEEPALIVE 9
#define SO_LINGER 13

#define MSG_PEEK 0x02
#define MSG_DONTWAIT 0x40
#define MSG_CMSG_CLOEXEC 0x40000000
#define MSG_NOSIGNAL 0x4000

#define SHUT_RD 0
#define SHUT_WR 1
#define SHUT_RDWR 2

typedef uint16_t sa_family_t;

struct sockaddr {
    sa_family_t sa_family;
    char sa_data[14];
};

struct linger {
    int l_onoff;
    int l_linger;
};

struct msghdr {
    void *msg_name;
    socklen_t msg_namelen;
    struct iovec *msg_iov;
    size_t msg_iovlen;
    void *msg_control;
    size_t msg_controllen;
    int msg_flags;
};

int socket(int domain, int type, int protocol);
int socketpair(int domain, int type, int protocol, int fds[2]);
int bind(int fd, const struct sockaddr *addr, socklen_t addrlen);
int listen(int fd, int backlog);
int accept(int fd, struct sockaddr *addr, socklen_t *addrlen);
int accept4(int fd, struct sockaddr *addr, socklen_t *addrlen, int flags);
int connect(int fd, const struct sockaddr *addr, socklen_t addrlen);
int getsockname(int fd, struct sockaddr *addr, socklen_t *addrlen);
int getpeername(int fd, struct sockaddr *addr, socklen_t *addrlen);
int shutdown(int fd, int how);
ssize_t send(int fd, const void *buffer, size_t length, int flags);
ssize_t recv(int fd, void *buffer, size_t length, int flags);
ssize_t sendto(int fd,
    const void *buffer,
    size_t length,
    int flags,
    const struct sockaddr *dest_addr,
    socklen_t addrlen);
ssize_t recvfrom(int fd,
    void *buffer,
    size_t length,
    int flags,
    struct sockaddr *src_addr,
    socklen_t *addrlen);
ssize_t sendmsg(int fd, const struct msghdr *message, int flags);
ssize_t recvmsg(int fd, struct msghdr *message, int flags);
int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len);
int getsockopt(int fd, int level, int option_name, void *option_value, socklen_t *option_len);

#endif
