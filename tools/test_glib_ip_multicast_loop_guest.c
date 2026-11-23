/* IPv4 multicast-loop option refusal with native UDP controls. */
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
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
    ttl = -1; broadcast = 0;
    CHECK("reset-controls", setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 &&
          setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) == 0 &&
          get_int(fd, IPPROTO_IP, IP_TTL, 64) && get_int(fd, SOL_SOCKET, SO_BROADCAST, 0));
    CHECK("close", close(fd) == 0);
    puts("glib-ip-multicast-loop: ok");
    return 0;
}
