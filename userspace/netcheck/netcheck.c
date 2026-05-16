#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

static void print_ip(uint32_t ip) {
    printf("%u.%u.%u.%u",
        (unsigned)((ip >> 24) & 0xff),
        (unsigned)((ip >> 16) & 0xff),
        (unsigned)((ip >> 8) & 0xff),
        (unsigned)(ip & 0xff));
}

static int fail(const char *step) {
    printf("netcheck: fail %s\n", step);
    return 1;
}

static int write_all(int fd, const char *buffer, size_t length) {
    size_t offset = 0;
    while (offset < length) {
        ssize_t count = send(fd, buffer + offset, length - offset, 0);
        if (count <= 0) {
            return -1;
        }
        offset += (size_t)count;
    }
    return 0;
}

static int check_dhcp(struct srv_net_status_info *status) {
    long ip = srv_net_dhcp();
    if (ip < 0) {
        return fail("dhcp");
    }
    if (srv_net_status_info(status) < 0 || !status->initialized || status->local_ip == 0) {
        return fail("status");
    }
    printf("netcheck: ip ");
    print_ip(status->local_ip);
    printf(" dns ");
    print_ip(status->dns_ip);
    printf(" router ");
    print_ip(status->router_ip);
    printf("\n");
    return 0;
}

static int check_dns(void) {
    uint32_t ip = 0;
    if (srv_net_dns("linguicityworld.app", &ip) < 0 || ip == 0) {
        return fail("dns");
    }
    printf("netcheck: dns linguicityworld.app ");
    print_ip(ip);
    printf("\n");
    return 0;
}

static int check_ping(uint32_t router_ip) {
    uint64_t elapsed = 0;
    if (router_ip == 0) {
        router_ip = ((uint32_t)10 << 24) | ((uint32_t)0 << 16) | ((uint32_t)2 << 8) | 2;
    }
    if (srv_net_ping(router_ip, 77, 100, &elapsed) < 0) {
        return fail("ping");
    }
    printf("netcheck: ping ");
    print_ip(router_ip);
    printf(" %llut\n", (unsigned long long)elapsed);
    return 0;
}

static int check_udp(uint32_t local_ip) {
    int server_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    int client_fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (server_fd < 0 || client_fd < 0) {
        if (server_fd >= 0) {
            close(server_fd);
        }
        if (client_fd >= 0) {
            close(client_fd);
        }
        return fail("udp-socket");
    }

    struct sockaddr_in server;
    memset(&server, 0, sizeof(server));
    server.sin_family = AF_INET;
    server.sin_port = htons(7105);
    server.sin_addr.s_addr = INADDR_ANY;
    if (bind(server_fd, (struct sockaddr *)&server, sizeof(server)) < 0) {
        close(server_fd);
        close(client_fd);
        return fail("udp-bind");
    }

    server.sin_addr.s_addr = local_ip;
    const char *message = "netcheck-udp";
    if (sendto(client_fd, message, strlen(message), 0, (const struct sockaddr *)&server, sizeof(server)) !=
        (ssize_t)strlen(message)) {
        close(server_fd);
        close(client_fd);
        return fail("udp-send");
    }

    char buffer[64];
    struct sockaddr_in peer;
    socklen_t peer_len = sizeof(peer);
    ssize_t count = recvfrom(server_fd, buffer, sizeof(buffer), 0, (struct sockaddr *)&peer, &peer_len);
    if (count != (ssize_t)strlen(message) || memcmp(buffer, message, strlen(message)) != 0) {
        close(server_fd);
        close(client_fd);
        return fail("udp-recv");
    }
    if (sendto(server_fd, buffer, (size_t)count, 0, (const struct sockaddr *)&peer, peer_len) != count) {
        close(server_fd);
        close(client_fd);
        return fail("udp-reply");
    }
    count = recvfrom(client_fd, buffer, sizeof(buffer), 0, 0, 0);
    close(server_fd);
    close(client_fd);
    if (count != (ssize_t)strlen(message) || memcmp(buffer, message, strlen(message)) != 0) {
        return fail("udp-echo");
    }
    printf("netcheck: udp ok\n");
    return 0;
}

static int check_tcp_http(void) {
    struct addrinfo hints;
    struct addrinfo *info = 0;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    int gai = getaddrinfo("example.com", "80", &hints, &info);
    if (gai != 0) {
        printf("netcheck: getaddrinfo %s\n", gai_strerror(gai));
        return 1;
    }

    int fd = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (fd < 0) {
        freeaddrinfo(info);
        return fail("tcp-socket");
    }
    if (connect(fd, info->ai_addr, info->ai_addrlen) < 0) {
        close(fd);
        freeaddrinfo(info);
        return fail("tcp-connect");
    }
    freeaddrinfo(info);

    const char *request = "GET / HTTP/1.0\r\nHost: example.com\r\nConnection: close\r\n\r\n";
    if (write_all(fd, request, strlen(request)) < 0) {
        close(fd);
        return fail("tcp-send");
    }

    char buffer[256];
    ssize_t count = recv(fd, buffer, sizeof(buffer) - 1, 0);
    close(fd);
    if (count <= 0) {
        return fail("tcp-recv");
    }
    buffer[count] = '\0';
    if (strstr(buffer, "HTTP/") == 0) {
        return fail("tcp-http");
    }
    printf("netcheck: tcp http ok\n");
    return 0;
}

int main(void) {
    struct srv_net_status_info status;
    memset(&status, 0, sizeof(status));

    if (check_dhcp(&status) != 0 ||
        check_dns() != 0 ||
        check_ping(status.router_ip) != 0 ||
        check_udp(status.local_ip) != 0 ||
        check_tcp_http() != 0) {
        return 1;
    }

    printf("netcheck: ok\n");
    return 0;
}
