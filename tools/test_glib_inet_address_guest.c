/* Public SDK address helpers and the numeric-only resolver contract. */
#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, x) do { if (!(x)) { \
    printf("glib-inet-address: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

static unsigned calls;
static const struct in6_addr *once(const struct in6_addr *address) {
    ++calls;
    return address;
}
#define ONCE(phase, expression) do { calls = 0; int result = !!(expression); \
    CHECK(phase, result && calls == 1); } while (0)

static int numeric(int family) {
    struct addrinfo hints = {0};
    struct addrinfo *result = NULL;
    hints.ai_family = family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_NUMERICHOST;
    CHECK("numeric-resolve", getaddrinfo("192.0.2.123", "443", &hints, &result) == 0);
    CHECK("numeric-shape", result && !result->ai_next && result->ai_family == AF_INET &&
          result->ai_socktype == SOCK_STREAM && result->ai_protocol == IPPROTO_TCP &&
          result->ai_addr && result->ai_addrlen == sizeof(struct sockaddr_in));
    const struct sockaddr_in *address = (const struct sockaddr_in *)result->ai_addr;
    CHECK("numeric-address", address->sin_family == AF_INET && ntohs(address->sin_port) == 443 &&
          ntohl(address->sin_addr.s_addr) == UINT32_C(0xc000027b));
    freeaddrinfo(result);
    return 0;
}

int main(void) {
    const struct in6_addr any = IN6ADDR_ANY_INIT;
    const struct in6_addr loop = IN6ADDR_LOOPBACK_INIT;
    const unsigned char any_bytes[16] = {0};
    const unsigned char loop_bytes[16] = { [15] = 1 };
    CHECK("initializers", memcmp(&any, any_bytes, 16) == 0 && memcmp(&loop, loop_bytes, 16) == 0 &&
          memcmp(&in6addr_any, any_bytes, 16) == 0 && memcmp(&in6addr_loopback, loop_bytes, 16) == 0);
    ONCE("unspecified-once", IN6_IS_ADDR_UNSPECIFIED(once(&any)));
    ONCE("loopback-once", IN6_IS_ADDR_LOOPBACK(once(&loop)));
    CHECK("any-loop-distinct", !IN6_IS_ADDR_LOOPBACK(&any) && !IN6_IS_ADDR_UNSPECIFIED(&loop));
    struct in6_addr link = {{0xfe, 0x80}}, link_last = {{0xfe, 0xbf}};
    struct in6_addr site = {{0xfe, 0xc0}}, site_last = {{0xfe, 0xff}};
    struct in6_addr other = {{0xfe, 0x7f}};
    ONCE("linklocal-once", IN6_IS_ADDR_LINKLOCAL(once(&link)));
    ONCE("sitelocal-once", IN6_IS_ADDR_SITELOCAL(once(&site)));
    CHECK("local-prefixes", IN6_IS_ADDR_LINKLOCAL(&link_last) && !IN6_IS_ADDR_LINKLOCAL(&site) &&
          IN6_IS_ADDR_SITELOCAL(&site_last) && !IN6_IS_ADDR_SITELOCAL(&link) &&
          !IN6_IS_ADDR_LINKLOCAL(&other) && !IN6_IS_ADDR_SITELOCAL(&other));
    struct in6_addr node = {{0xff, 0x01}}, ml = {{0xff, 0x12}};
    struct in6_addr ms = {{0xff, 0x05}}, mo = {{0xff, 0x08}}, mg = {{0xff, 0x0e}};
    struct in6_addr unknown = {{0xff, 0x03}};
    ONCE("multicast-once", IN6_IS_ADDR_MULTICAST(once(&node)));
    ONCE("node-once", IN6_IS_ADDR_MC_NODELOCAL(once(&node)));
    ONCE("mc-link-once", IN6_IS_ADDR_MC_LINKLOCAL(once(&ml)));
    ONCE("mc-site-once", IN6_IS_ADDR_MC_SITELOCAL(once(&ms)));
    ONCE("mc-org-once", IN6_IS_ADDR_MC_ORGLOCAL(once(&mo)));
    ONCE("mc-global-once", IN6_IS_ADDR_MC_GLOBAL(once(&mg)));
    CHECK("multicast-scopes", !IN6_IS_ADDR_MC_LINKLOCAL(&node) &&
          !IN6_IS_ADDR_MC_NODELOCAL(&ml) && !IN6_IS_ADDR_MC_SITELOCAL(&mo) &&
          !IN6_IS_ADDR_MC_ORGLOCAL(&mg) && !IN6_IS_ADDR_MC_GLOBAL(&unknown) &&
          !IN6_IS_ADDR_MULTICAST(&link));
    struct in_addr v4;
    CHECK("v4-low", inet_pton(AF_INET, "224.0.0.0", &v4) == 1 && IN_MULTICAST(ntohl(v4.s_addr)));
    CHECK("v4-high", inet_pton(AF_INET, "239.255.255.255", &v4) == 1 && IN_MULTICAST(ntohl(v4.s_addr)));
    CHECK("v4-before", inet_pton(AF_INET, "223.255.255.255", &v4) == 1 && !IN_MULTICAST(ntohl(v4.s_addr)));
    CHECK("v4-after", inet_pton(AF_INET, "240.0.0.0", &v4) == 1 && !IN_MULTICAST(ntohl(v4.s_addr)));
    unsigned i = 0; const uint32_t values[] = { UINT32_C(0xe0000001), 0 };
    CHECK("v4-once", IN_MULTICAST(values[i++]) && i == 1);
    CHECK("numeric-inet", numeric(AF_INET) == 0);
    CHECK("numeric-unspec", numeric(AF_UNSPEC) == 0);
    struct addrinfo hints = {0}, *result = NULL;
    hints.ai_family = AF_INET; hints.ai_flags = AI_NUMERICHOST;
    CHECK("invalid-numeric", getaddrinfo("not-a-numeric-host.invalid", "80", &hints, &result) == EAI_NONAME && result == NULL);
    CHECK("empty-numeric", getaddrinfo("", "80", &hints, &result) == EAI_NONAME && result == NULL);
    hints.ai_family = AF_INET6;
    CHECK("ipv6-family", getaddrinfo("::1", "80", &hints, &result) == EAI_FAMILY && result == NULL);
    printf("glib-inet-address: ok\n");
    return 0;
}
