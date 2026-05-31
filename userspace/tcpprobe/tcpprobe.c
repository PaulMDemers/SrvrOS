#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include <srvros/sys.h>

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

static void dump_net_state(void) {
    struct srv_net_status_info status;
    if (srv_net_status_info(&status) == 0) {
        printf("tcpprobe: status tcp=%llu tx=%llu rx=%llu arp=%llu ip=%llu packets=%llu\n",
            (unsigned long long)status.tcp_connection_count,
            (unsigned long long)status.tx_frames,
            (unsigned long long)status.rx_frames,
            (unsigned long long)status.arp_packets,
            (unsigned long long)status.ipv4_packets,
            (unsigned long long)status.tcp_packets);
    }
    for (uint64_t i = 0; i < 16; i++) {
        struct srv_net_info info;
        if (srv_net_list(i, &info) < 0) {
            continue;
        }
        if (info.kind != SRV_NET_KIND_TCP_CONNECTION) {
            continue;
        }
        printf("tcpprobe: conn id=%llu state=%s flags=0x%x ",
            (unsigned long long)info.id,
            info.state_name,
            (unsigned)info.flags);
        print_ip(info.local_ip);
        printf(":%u -> ", (unsigned)info.local_port);
        print_ip(info.remote_ip);
        printf(":%u tx=%llu win=%u err=%u\n",
            (unsigned)info.remote_port,
            (unsigned long long)info.tx_outstanding,
            (unsigned)info.peer_window,
            (unsigned)info.error);
    }
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = send(fd, buffer + offset, length - offset, 0);
        if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
            struct pollfd pfd = { .fd = fd, .events = POLLOUT, .revents = 0 };
            if (poll(&pfd, 1, 3000) <= 0) {
                return -1;
            }
            continue;
        }
        if (count <= 0) {
            return -1;
        }
        offset += (size_t)count;
    }
    return 0;
}

static int connect_nonblocking(int fd, const struct sockaddr *addr, socklen_t addrlen) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags < 0 || fcntl(fd, F_SETFL, flags | O_NONBLOCK) < 0) {
        printf("tcpprobe: fcntl errno=%d\n", errno);
        return -1;
    }

    int result = connect(fd, addr, addrlen);
    if (result == 0) {
        printf("tcpprobe: connect immediate\n");
        return 0;
    }
    if (errno != EINPROGRESS) {
        printf("tcpprobe: connect errno=%d\n", errno);
        return -1;
    }
    printf("tcpprobe: connect in-progress\n");
    dump_net_state();

    struct pollfd pfd = { .fd = fd, .events = POLLOUT | POLLERR | POLLHUP, .revents = 0 };
    for (int i = 0; i < 8; i++) {
        errno = 0;
        pfd.revents = 0;
        result = poll(&pfd, 1, 1000);
        printf("tcpprobe: poll[%d] result=%d revents=0x%x errno=%d\n", i, result, pfd.revents, errno);
        dump_net_state();
        if (result > 0 && (pfd.revents & (POLLOUT | POLLERR | POLLHUP)) != 0) {
            break;
        }
    }
    if (result <= 0 || (pfd.revents & (POLLOUT | POLLERR | POLLHUP)) == 0) {
        errno = ETIMEDOUT;
        return -1;
    }

    int error = -1;
    socklen_t error_len = sizeof(error);
    if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &error_len) < 0) {
        printf("tcpprobe: soerror getsockopt errno=%d\n", errno);
        return -1;
    }
    printf("tcpprobe: soerror=%d\n", error);
    if (error != 0) {
        errno = error;
        return -1;
    }
    return 0;
}

int main(int argc, char **argv) {
    const char *host = argc > 1 ? argv[1] : "10.0.2.2";
    const char *port = argc > 2 ? argv[2] : "18090";
    const char *message = argc > 3 ? argv[3] : "guest-ok\n";

    printf("tcpprobe: start host=%s port=%s\n", host, port);

    struct addrinfo hints;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    struct addrinfo *info = 0;
    int gai = getaddrinfo(host, port, &hints, &info);
    if (gai != 0) {
        printf("tcpprobe: getaddrinfo %s\n", gai_strerror(gai));
        return 1;
    }

    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        printf("tcpprobe: socket errno=%d\n", errno);
        freeaddrinfo(info);
        return 1;
    }

    if (connect_nonblocking(fd, info->ai_addr, info->ai_addrlen) < 0) {
        printf("tcpprobe: connect failed errno=%d\n", errno);
        close(fd);
        freeaddrinfo(info);
        return 1;
    }
    freeaddrinfo(info);

    printf("tcpprobe: connected\n");
    if (write_all(fd, message, strlen(message)) < 0) {
        printf("tcpprobe: send failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    printf("tcpprobe: sent\n");

    char buffer[128];
    ssize_t count = recv(fd, buffer, sizeof(buffer) - 1, 0);
    if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
        struct pollfd pfd = { .fd = fd, .events = POLLIN | POLLERR | POLLHUP, .revents = 0 };
        int result = poll(&pfd, 1, 3000);
        printf("tcpprobe: read poll result=%d revents=0x%x errno=%d\n", result, pfd.revents, errno);
        count = recv(fd, buffer, sizeof(buffer) - 1, 0);
    }
    if (count < 0) {
        printf("tcpprobe: recv failed errno=%d\n", errno);
        close(fd);
        return 1;
    }
    buffer[count] = '\0';
    printf("tcpprobe: recv %zd %s\n", count, buffer);
    close(fd);
    printf("tcpprobe: ok\n");
    return 0;
}
