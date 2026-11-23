/* IP_TTL over real UDP/TCP and descriptor aliases; external peer validates wire TTL. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <srvros/sys.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-ip-ttl: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int get_ttl(int fd, int expected) {
    int value = -99;
    socklen_t length = sizeof(value);
    return getsockopt(fd, IPPROTO_IP, IP_TTL, &value, &length) == 0 &&
        length == sizeof(value) && value == expected;
}

static int set_ttl(int fd, int value) {
    return setsockopt(fd, IPPROTO_IP, IP_TTL, &value, sizeof(value));
}

static struct sockaddr_in peer(unsigned port) {
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    (void)inet_pton(AF_INET, "10.0.2.3", &address.sin_addr);
    return address;
}

static int udp_send(int fd, const char *text) {
    struct sockaddr_in address = peer(47770);
    size_t n = strlen(text);
    return sendto(fd, text, n, 0, (const struct sockaddr *)&address, sizeof(address)) == (ssize_t)n;
}

int main(void) {
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK("udp-open", udp >= 0 && get_ttl(udp, 64));
    CHECK("udp-default", udp_send(udp, "udp-default"));
    CHECK("udp-set", set_ttl(udp, 37) == 0 && get_ttl(udp, 37));
    int alias = dup(udp);
    CHECK("udp-alias", alias >= 0 && get_ttl(alias, 37));
    CHECK("udp-changed", udp_send(alias, "udp-changed"));
    const int invalid[] = {0, 256, -2};
    for (unsigned i = 0; i < sizeof(invalid)/sizeof(invalid[0]); ++i) {
        errno = 0;
        CHECK("udp-invalid", set_ttl(udp, invalid[i]) == -1 && errno == EINVAL && get_ttl(alias, 37));
    }
    int value = 7;
    errno = 0;
    CHECK("udp-short", setsockopt(udp, IPPROTO_IP, IP_TTL, &value, sizeof(value)-1) == -1 &&
          errno == EINVAL && get_ttl(udp, 37));
    int isolated = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK("udp-isolated-open", isolated >= 0 && get_ttl(isolated, 64));
    CHECK("udp-isolated", udp_send(isolated, "udp-isolated"));
    CHECK("udp-reset", set_ttl(alias, -1) == 0 && get_ttl(udp, 64) && udp_send(udp, "udp-reset"));
    CHECK("udp-close", close(alias) == 0 && close(udp) == 0);

    long raw = srv_net_udp_open();
    CHECK("raw-open", raw >= 0 && get_ttl((int)raw, 64));
    int raw_alias = dup((int)raw);
    CHECK("raw-alias", raw_alias >= 0 && set_ttl(raw_alias, 53) == 0 && get_ttl((int)raw, 53));
    CHECK("raw-wide-rejected", srv_syscall5(SYS_NET_IPV4_TTL, SRV_NET_TTL_SET, raw,
          (long)(UINT64_C(0x100000000) + 53), 0, 0) == SRV_ERR_INVAL && get_ttl((int)raw, 53));
    static const char raw_data[] = "udp-raw";
    CHECK("raw-send", srv_net_udp_sendto((int)raw, UINT32_C(0x0a000203), 47770,
          raw_data, sizeof(raw_data)-1) == (long)(sizeof(raw_data)-1));
    CHECK("raw-close", close(raw_alias) == 0 && close((int)raw) == 0);

    int tcp = socket(AF_INET, SOCK_STREAM, 0);
    CHECK("tcp-open", tcp >= 0 && get_ttl(tcp, 64) && set_ttl(tcp, 41) == 0);
    struct sockaddr_in destination = peer(47771);
    CHECK("tcp-connect", connect(tcp, (struct sockaddr *)&destination, sizeof(destination)) == 0);
    CHECK("tcp-connected-ttl", get_ttl(tcp, 41) && set_ttl(tcp, 43) == 0 && get_ttl(tcp, 43));
    CHECK("tcp-out", send(tcp, "tcp-out", 7, 0) == 7 && close(tcp) == 0);

    int listener = socket(AF_INET, SOCK_STREAM, 0);
    CHECK("listener-open", listener >= 0 && set_ttl(listener, 47) == 0);
    struct sockaddr_in local = {0};
    local.sin_family = AF_INET; local.sin_addr.s_addr = htonl(INADDR_ANY); local.sin_port = htons(47772);
    CHECK("listener-bind", bind(listener, (struct sockaddr *)&local, sizeof(local)) == 0);
    CHECK("listener-listen", listen(listener, 4) == 0 && get_ttl(listener, 47));
    long legacy_listener = srv_net_listen(47772);
    CHECK("listener-legacy-alias", legacy_listener >= 0 && get_ttl((int)legacy_listener, 47) &&
          close((int)legacy_listener) == 0);
    int competing = socket(AF_INET, SOCK_STREAM, 0);
    CHECK("listener-competing-bind", competing >= 0 && set_ttl(competing, 55) == 0 &&
          bind(competing, (struct sockaddr *)&local, sizeof(local)) == 0);
    errno = 0;
    CHECK("listener-conflict", listen(competing, 4) == -1 && errno == EADDRINUSE &&
          get_ttl(listener, 47) && close(competing) == 0);
    CHECK("listener-ready", udp_send(isolated, "tcp-listener-ready"));
    struct pollfd ready = {.fd = listener, .events = POLLIN};
    CHECK("listener-poll", poll(&ready, 1, 10000) == 1 && (ready.revents & POLLIN));
    int accepted = accept(listener, NULL, NULL);
    CHECK("accept", accepted >= 0 && get_ttl(accepted, 47));
    CHECK("tcp-inherited", send(accepted, "tcp-inherited", 13, 0) == 13);
    CHECK("tcp-accepted-set", set_ttl(accepted, 49) == 0 && get_ttl(accepted, 49) && get_ttl(listener, 47));
    CHECK("tcp-accepted", send(accepted, "tcp-accepted", 12, 0) == 12);
    CHECK("tcp-close", close(accepted) == 0 && close(listener) == 0 && close(isolated) == 0);

    int local_pair[2];
    CHECK("unix-pair", socketpair(AF_UNIX, SOCK_STREAM, 0, local_pair) == 0);
    errno = 0;
    CHECK("unix-refused", set_ttl(local_pair[0], 42) == -1 && errno == ENOPROTOOPT);
    CHECK("unix-close", close(local_pair[0]) == 0 && close(local_pair[1]) == 0);
    puts("glib-ip-ttl: ok");
    return 0;
}
