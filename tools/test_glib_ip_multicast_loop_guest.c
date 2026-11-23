/* IPv4 multicast-loop option refusal with native UDP controls. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("glib-ip-multicast-loop: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

static int get_int(int fd, int level, int name, int expected) {
    int value = -99; socklen_t length = sizeof(value);
    return getsockopt(fd, level, name, &value, &length) == 0 &&
           length == sizeof(value) && value == expected;
}

int main(void) {
    _Static_assert(IP_MULTICAST_LOOP == 34, "Linux-compatible option value");
    _Static_assert(IP_MULTICAST_TTL == 33, "Linux-compatible IPv4 multicast TTL");
    _Static_assert(IPV6_MULTICAST_HOPS == 18, "Linux-compatible IPv6 multicast hops");
    _Static_assert(IP_ADD_MEMBERSHIP == 35 && IP_DROP_MEMBERSHIP == 36, "IPv4 membership options");
    _Static_assert(IPV6_JOIN_GROUP == 20 && IPV6_LEAVE_GROUP == 21, "IPv6 membership options");
    _Static_assert(sizeof(struct ip_mreq) == 8 && _Alignof(struct ip_mreq) == 4 &&
                   offsetof(struct ip_mreq, imr_multiaddr) == 0 &&
                   offsetof(struct ip_mreq, imr_interface) == 4, "IPv4 membership layout");
    _Static_assert(sizeof(struct ipv6_mreq) == 20 && _Alignof(struct ipv6_mreq) == 4 &&
                   offsetof(struct ipv6_mreq, ipv6mr_multiaddr) == 0 &&
                   offsetof(struct ipv6_mreq, ipv6mr_interface) == 16, "IPv6 membership layout");
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    CHECK("socket", fd >= 0);
    CHECK("default-controls", get_int(fd, IPPROTO_IP, IP_TTL, 64) &&
          get_int(fd, SOL_SOCKET, SO_BROADCAST, 0));
    int ttl = 37, broadcast = 1;
    CHECK("set-controls", setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 &&
          setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0);
    const struct { int level, name; } unsupported[] = {
        { IPPROTO_IP, IP_MULTICAST_LOOP },
        { IPPROTO_IP, IP_MULTICAST_TTL },
        { IPPROTO_IPV6, IPV6_MULTICAST_HOPS },
    };
    for (unsigned i = 0; i < sizeof(unsupported) / sizeof(unsupported[0]); ++i) {
        for (int value = 0; value <= 1; ++value) {
            int input = value;
            errno = 0;
            CHECK("set-refused", setsockopt(fd, unsupported[i].level, unsupported[i].name,
                  &input, sizeof(input)) == -1 && errno == ENOSYS && input == value);
            int output = 0x12345678; socklen_t length = sizeof(output);
            errno = 0;
            CHECK("get-refused", getsockopt(fd, unsupported[i].level, unsupported[i].name,
                  &output, &length) == -1 && errno == ENOSYS && output == 0x12345678 &&
                  length == sizeof(output));
            CHECK("controls-preserved", get_int(fd, IPPROTO_IP, IP_TTL, 37) &&
                  get_int(fd, SOL_SOCKET, SO_BROADCAST, 1));
        }
    }
    struct ip_mreq ipv4 = {.imr_multiaddr = {.s_addr = htonl(0xef010203u)},
                           .imr_interface = {.s_addr = INADDR_ANY}};
    const unsigned char ipv4_bytes[8] = { 239, 1, 2, 3, 0, 0, 0, 0 };
    struct ipv6_mreq ipv6 = {.ipv6mr_multiaddr = {.s6_addr = {0xff, 0x02, [15] = 1}},
                             .ipv6mr_interface = 0};
    const unsigned char ipv6_bytes[20] = { 0xff, 0x02, [15] = 1 };
    CHECK("membership-bytes", memcmp(&ipv4, ipv4_bytes, sizeof(ipv4)) == 0 &&
          memcmp(&ipv6, ipv6_bytes, sizeof(ipv6)) == 0);
    const int v4_options[] = { IP_ADD_MEMBERSHIP, IP_DROP_MEMBERSHIP };
    const int v6_options[] = { IPV6_JOIN_GROUP, IPV6_LEAVE_GROUP };
    for (unsigned i = 0; i < 2; ++i) {
        errno = 0;
        CHECK("ipv4-membership-refused", setsockopt(fd, IPPROTO_IP, v4_options[i], &ipv4, sizeof(ipv4)) == -1 &&
              errno == ENOSYS && memcmp(&ipv4, ipv4_bytes, sizeof(ipv4)) == 0);
        errno = 0;
        CHECK("ipv6-membership-refused", setsockopt(fd, IPPROTO_IPV6, v6_options[i], &ipv6, sizeof(ipv6)) == -1 &&
              errno == ENOSYS && memcmp(&ipv6, ipv6_bytes, sizeof(ipv6)) == 0);
        CHECK("membership-controls-preserved", get_int(fd, IPPROTO_IP, IP_TTL, 37) &&
              get_int(fd, SOL_SOCKET, SO_BROADCAST, 1));
    }
    ttl = -1; broadcast = 0;
    CHECK("reset-controls", setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 &&
          setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0 &&
          get_int(fd, IPPROTO_IP, IP_TTL, 64) && get_int(fd, SOL_SOCKET, SO_BROADCAST, 0));
    CHECK("close", close(fd) == 0);
    puts("glib-ip-multicast-loop: ok");
    return 0;
}
