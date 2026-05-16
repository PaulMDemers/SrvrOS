#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void usage(void) {
    puts("usage: udpecho server [port] | udpecho client host [port] [message] | udpecho self");
}

static uint16_t parse_port(const char *text, uint16_t fallback) {
    unsigned value = 0;
    if (text == 0 || text[0] == '\0') {
        return fallback;
    }
    for (const char *p = text; *p != '\0'; p++) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        value = value * 10u + (unsigned)(*p - '0');
        if (value > 65535u) {
            return 0;
        }
    }
    return (uint16_t)value;
}

static int server(uint16_t port) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("udpecho: socket");
        return 1;
    }

    struct sockaddr_in local;
    memset(&local, 0, sizeof(local));
    local.sin_family = AF_INET;
    local.sin_port = htons(port);
    local.sin_addr.s_addr = INADDR_ANY;
    if (bind(fd, (struct sockaddr *)&local, sizeof(local)) < 0) {
        perror("udpecho: bind");
        close(fd);
        return 1;
    }

    char buffer[512];
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    ssize_t count = recvfrom(fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
    if (count < 0) {
        perror("udpecho: recvfrom");
        close(fd);
        return 1;
    }
    if (sendto(fd, buffer, (size_t)count, 0, (const struct sockaddr *)&peer, peer_len) != count) {
        perror("udpecho: sendto");
        close(fd);
        return 1;
    }

    printf("udpecho: served %ld bytes\n", (long)count);
    close(fd);
    return 0;
}

static int client(const char *host, uint16_t port, const char *message) {
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        perror("udpecho: socket");
        return 1;
    }

    struct sockaddr_in remote;
    memset(&remote, 0, sizeof(remote));
    remote.sin_family = AF_INET;
    remote.sin_port = htons(port);
    if (inet_pton(AF_INET, host, &remote.sin_addr) != 1) {
        printf("udpecho: bad host\n");
        close(fd);
        return 2;
    }

    size_t length = strlen(message);
    if (sendto(fd, message, length, 0, (const struct sockaddr *)&remote, sizeof(remote)) != (ssize_t)length) {
        perror("udpecho: sendto");
        close(fd);
        return 1;
    }

    char buffer[512];
    ssize_t count = recvfrom(fd, buffer, sizeof(buffer) - 1, 0, 0, 0);
    if (count < 0) {
        perror("udpecho: recvfrom");
        close(fd);
        return 1;
    }
    buffer[count] = '\0';
    printf("udpecho: reply %s\n", buffer);
    close(fd);
    return strcmp(buffer, message) == 0 ? 0 : 1;
}

static int selftest(void) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd < 0 || client_fd < 0) {
        perror("udpecho: socket");
        if (server_fd >= 0) {
            close(server_fd);
        }
        if (client_fd >= 0) {
            close(client_fd);
        }
        return 1;
    }

    int reuse = 1;
    int type = 0;
    socklen_t type_len = sizeof(type);
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse)) < 0 ||
        getsockopt(client_fd, SOL_SOCKET, SO_TYPE, &type, &type_len) < 0 ||
        type != SOCK_DGRAM) {
        perror("udpecho: sockopt");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(7001);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("udpecho: bind");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    server_addr.sin_addr.s_addr = 0x0a00020fu;
    char buffer[512];
    if (recvfrom(client_fd, buffer, sizeof(buffer), MSG_DONTWAIT, 0, 0) >= 0 || errno != EAGAIN) {
        printf("udpecho: nonblock failed\n");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    const char *message = "hello-udp";
    if (sendto(client_fd,
            message,
            strlen(message),
            0,
            (const struct sockaddr *)&server_addr,
            sizeof(server_addr)) != (ssize_t)strlen(message)) {
        perror("udpecho: sendto");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    ssize_t count = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
    if (count < 0) {
        perror("udpecho: recvfrom");
        close(server_fd);
        close(client_fd);
        return 1;
    }
    if (sendto(server_fd, buffer, (size_t)count, 0, (const struct sockaddr *)&peer, peer_len) != count) {
        perror("udpecho: sendto");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    count = recvfrom(client_fd, buffer, sizeof(buffer) - 1, 0, 0, 0);
    if (count < 0) {
        perror("udpecho: recvfrom");
        close(server_fd);
        close(client_fd);
        return 1;
    }
    buffer[count] = '\0';
    printf("udpecho: self %s\n", buffer);

    if (sendto(client_fd, "", 0, 0, (const struct sockaddr *)&server_addr, sizeof(server_addr)) != 0) {
        perror("udpecho: zero-send");
        close(server_fd);
        close(client_fd);
        return 1;
    }
    peer_len = sizeof(peer);
    count = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
    if (count != 0) {
        perror("udpecho: zero-recv");
        close(server_fd);
        close(client_fd);
        return 1;
    }
    printf("udpecho: zero ok\n");

    if (connect(client_fd, (const struct sockaddr *)&server_addr, sizeof(server_addr)) < 0 ||
        shutdown(client_fd, SHUT_WR) < 0 ||
        send(client_fd, "x", 1, MSG_NOSIGNAL) >= 0 ||
        errno != EPIPE) {
        printf("udpecho: shutdown failed\n");
        close(server_fd);
        close(client_fd);
        return 1;
    }

    close(server_fd);
    close(client_fd);
    return strcmp(buffer, message) == 0 ? 0 : 1;
}

int main(int argc, char **argv) {
    if (argc < 2) {
        usage();
        return 2;
    }

    if (strcmp(argv[1], "server") == 0) {
        uint16_t port = parse_port(argc > 2 ? argv[2] : 0, 7000);
        if (port == 0) {
            usage();
            return 2;
        }
        return server(port);
    }

    if (strcmp(argv[1], "client") == 0) {
        if (argc < 3) {
            usage();
            return 2;
        }
        uint16_t port = parse_port(argc > 3 ? argv[3] : 0, 7000);
        if (port == 0) {
            usage();
            return 2;
        }
        return client(argv[2], port, argc > 4 ? argv[4] : "srvros-udp");
    }

    if (strcmp(argv[1], "self") == 0) {
        return selftest();
    }

    usage();
    return 2;
}
