/* IPv6 option declarations and refusal, plus unaffected IPv4 TTL state. */
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-ipv6-unicast-hops: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int ttl_is(int fd, int expected) {
    int value = -1;
    socklen_t length = sizeof(value);
    return getsockopt(fd, IPPROTO_IP, IP_TTL, &value, &length) == 0 &&
        length == sizeof(value) && value == expected;
}

int main(void) {
    _Static_assert(IPV6_UNICAST_HOPS == 16, "Linux-compatible option value");
    errno = 0;
    CHECK("ipv6-stream", socket(AF_INET6, SOCK_STREAM, IPPROTO_TCP) == -1 && errno == EAFNOSUPPORT);
    errno = 0;
    CHECK("ipv6-datagram", socket(AF_INET6, SOCK_DGRAM, IPPROTO_UDP) == -1 && errno == EAFNOSUPPORT);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    CHECK("ipv4-socket", fd >= 0 && ttl_is(fd, 64));
    int ttl = 37;
    CHECK("ttl-set", setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 && ttl_is(fd, 37));
    int hops = 19;
    errno = 0;
    CHECK("ipv6-set-refused", setsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &hops, sizeof(hops)) == -1 &&
          errno == ENOSYS && hops == 19 && ttl_is(fd, 37));
    int output = 0x12345678;
    socklen_t length = sizeof(output);
    errno = 0;
    CHECK("ipv6-get-refused", getsockopt(fd, IPPROTO_IPV6, IPV6_UNICAST_HOPS, &output, &length) == -1 &&
          errno == ENOSYS && output == 0x12345678 && length == sizeof(output) && ttl_is(fd, 37));
    ttl = -1;
    CHECK("ttl-reset", setsockopt(fd, IPPROTO_IP, IP_TTL, &ttl, sizeof(ttl)) == 0 && ttl_is(fd, 64));
    CHECK("close", close(fd) == 0);
    puts("glib-ipv6-unicast-hops: ok");
    return 0;
}
