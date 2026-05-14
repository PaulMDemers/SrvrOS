#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <srvros/sys.h>

#define POSIX_SOCKET_MAX 8
#define POSIX_SOCKET_BASE 1000

struct posix_socket {
    int used;
    int domain;
    int type;
    int protocol;
    uint16_t port;
    int listener_fd;
    uint64_t fd_flags;
};

static struct posix_socket sockets[POSIX_SOCKET_MAX];

static struct posix_socket *socket_at(int fd) {
    if (fd < POSIX_SOCKET_BASE || fd >= POSIX_SOCKET_BASE + POSIX_SOCKET_MAX) {
        return 0;
    }
    struct posix_socket *socket = &sockets[fd - POSIX_SOCKET_BASE];
    return socket->used ? socket : 0;
}

int __posix_socket_is_pseudo(int fd) {
    return socket_at(fd) != 0;
}

int __posix_socket_poll_fd(int fd) {
    struct posix_socket *socket = socket_at(fd);
    if (socket == 0) {
        return fd;
    }
    return socket->listener_fd;
}

int __posix_socket_fcntl(int fd, int command, uint64_t flags) {
    struct posix_socket *socket = socket_at(fd);
    if (socket == 0) {
        errno = EBADF;
        return -1;
    }
    if (command == SRV_F_GETFL) {
        if (socket->listener_fd >= 0) {
            long result = srv_fcntl(socket->listener_fd, command, 0);
            if (result >= 0) {
                socket->fd_flags = (uint64_t)result;
            }
        }
        return (int)socket->fd_flags;
    }
    if (command == SRV_F_SETFL) {
        socket->fd_flags = flags;
        if (socket->listener_fd < 0) {
            return 0;
        }
    } else if (socket->listener_fd < 0) {
        errno = EBADF;
        return -1;
    }
    long result = srv_fcntl(socket->listener_fd, command, flags);
    if (result < 0) {
        errno = EBADF;
        return -1;
    }
    return (int)result;
}

int __posix_socket_close(int fd) {
    struct posix_socket *socket = socket_at(fd);
    if (socket == 0) {
        errno = EBADF;
        return -1;
    }
    if (socket->listener_fd >= 0) {
        (void)srv_close(socket->listener_fd);
    }
    memset(socket, 0, sizeof(*socket));
    socket->listener_fd = -1;
    return 0;
}

int socket(int domain, int type, int protocol) {
    if (domain != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    if (type != SOCK_STREAM) {
        errno = EPROTONOSUPPORT;
        return -1;
    }
    for (int i = 0; i < POSIX_SOCKET_MAX; i++) {
        if (!sockets[i].used) {
            sockets[i].used = 1;
            sockets[i].domain = domain;
            sockets[i].type = type;
            sockets[i].protocol = protocol;
            sockets[i].port = 0;
            sockets[i].listener_fd = -1;
            sockets[i].fd_flags = 0;
            return POSIX_SOCKET_BASE + i;
        }
    }
    errno = EMFILE;
    return -1;
}

int bind(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    struct posix_socket *socket = socket_at(fd);
    if (socket == 0 || addr == 0 || addrlen < sizeof(struct sockaddr_in)) {
        errno = EINVAL;
        return -1;
    }
    const struct sockaddr_in *in = (const struct sockaddr_in *)addr;
    if (in->sin_family != AF_INET) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    socket->port = ntohs(in->sin_port);
    return 0;
}

int listen(int fd, int backlog) {
    struct posix_socket *socket = socket_at(fd);
    (void)backlog;
    if (socket == 0 || socket->port == 0) {
        errno = EINVAL;
        return -1;
    }
    if (socket->listener_fd >= 0) {
        return 0;
    }
    long listener = srv_net_listen(socket->port);
    if (listener < 0) {
        errno = EADDRINUSE;
        return -1;
    }
    socket->listener_fd = (int)listener;
    if (socket->fd_flags != 0 && srv_fcntl(socket->listener_fd, SRV_F_SETFL, socket->fd_flags) < 0) {
        (void)srv_close(socket->listener_fd);
        socket->listener_fd = -1;
        errno = EBADF;
        return -1;
    }
    return 0;
}

int accept(int fd, struct sockaddr *addr, socklen_t *addrlen) {
    struct posix_socket *socket = socket_at(fd);
    uint64_t length = 0;
    if (socket == 0 || socket->listener_fd < 0) {
        errno = EBADF;
        return -1;
    }
    long connection = srv_net_accept(socket->listener_fd, 0, 0, &length);
    if (connection < 0) {
        if (connection == SRV_ERR_AGAIN) {
            errno = EAGAIN;
            return -1;
        }
        errno = EIO;
        return -1;
    }
    if (addr != 0 && addrlen != 0 && *addrlen >= sizeof(struct sockaddr_in)) {
        struct sockaddr_in *in = (struct sockaddr_in *)addr;
        memset(in, 0, sizeof(*in));
        in->sin_family = AF_INET;
        *addrlen = sizeof(*in);
    }
    return (int)connection;
}

int connect(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    (void)fd;
    (void)addr;
    (void)addrlen;
    errno = ENOSYS;
    return -1;
}

ssize_t send(int fd, const void *buffer, size_t length, int flags) {
    (void)flags;
    return write(fd, buffer, length);
}

ssize_t recv(int fd, void *buffer, size_t length, int flags) {
    (void)flags;
    return read(fd, buffer, length);
}

int setsockopt(int fd, int level, int option_name, const void *option_value, socklen_t option_len) {
    (void)fd;
    (void)level;
    (void)option_name;
    (void)option_value;
    (void)option_len;
    return 0;
}

uint16_t htons(uint16_t value) {
    return (uint16_t)((value << 8) | (value >> 8));
}

uint16_t ntohs(uint16_t value) {
    return htons(value);
}

uint32_t htonl(uint32_t value) {
    return ((value & 0x000000ffu) << 24) |
        ((value & 0x0000ff00u) << 8) |
        ((value & 0x00ff0000u) >> 8) |
        ((value & 0xff000000u) >> 24);
}

uint32_t ntohl(uint32_t value) {
    return htonl(value);
}

static int parse_decimal_octet(const char **text, uint32_t *octet) {
    uint32_t value = 0;
    int digits = 0;
    while (**text >= '0' && **text <= '9') {
        value = value * 10 + (uint32_t)(**text - '0');
        if (value > 255) {
            return 0;
        }
        (*text)++;
        digits++;
    }
    *octet = value;
    return digits > 0;
}

int inet_pton(int af, const char *src, void *dst) {
    if (af != AF_INET || src == 0 || dst == 0) {
        errno = EAFNOSUPPORT;
        return -1;
    }
    uint32_t parts[4];
    const char *p = src;
    for (int i = 0; i < 4; i++) {
        if (!parse_decimal_octet(&p, &parts[i])) {
            return 0;
        }
        if (i < 3) {
            if (*p != '.') {
                return 0;
            }
            p++;
        }
    }
    if (*p != '\0') {
        return 0;
    }
    ((struct in_addr *)dst)->s_addr =
        (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
    return 1;
}

static char *write_u8(char *out, uint8_t value) {
    char digits[3];
    int count = 0;
    if (value == 0) {
        *out++ = '0';
        return out;
    }
    while (value > 0) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        *out++ = digits[--count];
    }
    return out;
}

const char *inet_ntop(int af, const void *src, char *dst, socklen_t size) {
    if (af != AF_INET || src == 0 || dst == 0 || size < INET_ADDRSTRLEN) {
        errno = EINVAL;
        return 0;
    }
    uint32_t ip = ((const struct in_addr *)src)->s_addr;
    char *out = dst;
    out = write_u8(out, (uint8_t)(ip >> 24));
    *out++ = '.';
    out = write_u8(out, (uint8_t)(ip >> 16));
    *out++ = '.';
    out = write_u8(out, (uint8_t)(ip >> 8));
    *out++ = '.';
    out = write_u8(out, (uint8_t)ip);
    *out = '\0';
    return dst;
}

static int parse_service(const char *service, uint16_t *port) {
    long value = 0;
    if (service == 0 || service[0] == '\0') {
        *port = 0;
        return 1;
    }
    for (const char *p = service; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10 + (*p - '0');
        if (value > 65535) {
            return 0;
        }
    }
    *port = (uint16_t)value;
    return 1;
}

int getaddrinfo(const char *node,
    const char *service,
    const struct addrinfo *hints,
    struct addrinfo **res) {
    uint16_t port = 0;
    struct in_addr address;
    if (res == 0 || !parse_service(service, &port)) {
        return EAI_SERVICE;
    }
    *res = 0;
    if (node == 0 || node[0] == '\0') {
        address.s_addr = 0;
    } else if (inet_pton(AF_INET, node, &address) != 1) {
        uint32_t resolved = 0;
        if (srv_net_dns(node, &resolved) < 0) {
            return EAI_NONAME;
        }
        address.s_addr = resolved;
    }

    struct addrinfo *info = calloc(1, sizeof(*info));
    struct sockaddr_in *addr = calloc(1, sizeof(*addr));
    if (info == 0 || addr == 0) {
        free(info);
        free(addr);
        return EAI_MEMORY;
    }
    addr->sin_family = AF_INET;
    addr->sin_port = htons(port);
    addr->sin_addr = address;
    info->ai_family = AF_INET;
    info->ai_socktype = hints != 0 && hints->ai_socktype != 0 ? hints->ai_socktype : SOCK_STREAM;
    info->ai_protocol = hints != 0 && hints->ai_protocol != 0 ? hints->ai_protocol : IPPROTO_TCP;
    info->ai_addrlen = sizeof(*addr);
    info->ai_addr = (struct sockaddr *)addr;
    *res = info;
    return 0;
}

void freeaddrinfo(struct addrinfo *res) {
    while (res != 0) {
        struct addrinfo *next = res->ai_next;
        free(res->ai_addr);
        free(res);
        res = next;
    }
}

const char *gai_strerror(int errcode) {
    switch (errcode) {
    case 0:
        return "ok";
    case EAI_NONAME:
        return "name not found";
    case EAI_MEMORY:
        return "out of memory";
    case EAI_SERVICE:
        return "bad service";
    default:
        return "resolver failure";
    }
}
