/* Real IPv4 UDP broadcast option and packet-level test traffic. */
#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <srvros/syscall_numbers.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("glib-so-broadcast: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)
#define PORT 47771

static int option(int fd, int expected) {
    int value = -99; socklen_t length = sizeof(value);
    return getsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, &length) == 0 &&
           value == expected && length == sizeof(value);
}
static struct sockaddr_in destination(const char *text) {
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET; address.sin_port = htons(PORT);
    if (inet_pton(AF_INET, text, &address.sin_addr) != 1) address.sin_family = AF_UNSPEC;
    return address;
}
static int send_named(int fd, const struct sockaddr_in *address, const char *text, int flags) {
    size_t n = strlen(text);
    return sendto(fd, text, n, flags, (const struct sockaddr *)address, sizeof(*address)) == (ssize_t)n;
}

static ssize_t send_nonblocking_message(int fd, struct sockaddr_in *address, char *text) {
    struct iovec data = {.iov_base = text, .iov_len = strlen(text)};
    struct msghdr message = {.msg_name = address, .msg_namelen = sizeof(*address),
                            .msg_iov = &data, .msg_iovlen = 1};
    return sendmsg(fd, &message, MSG_DONTWAIT);
}

int main(void) {
    _Static_assert(SO_BROADCAST == 6, "Linux-compatible option value");
    struct sockaddr_in limited = destination("255.255.255.255");
    struct sockaddr_in directed = destination("10.0.2.255");
    struct sockaddr_in unicast = destination("10.0.2.3");
    CHECK("addresses", limited.sin_family == AF_INET && directed.sin_family == AF_INET && unicast.sin_family == AF_INET);
    int fd = socket(AF_INET, SOCK_DGRAM, 0), independent = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK("sockets", fd >= 0 && independent >= 0);
    int alias = dup(fd);
    CHECK("dup", alias >= 0 && option(fd, 0) && option(alias, 0) && option(independent, 0));
    int output = 0x12345678; socklen_t length = sizeof(output) - 1;
    errno = 0;
    CHECK("short-get", getsockopt(fd, SOL_SOCKET, SO_BROADCAST, &output, &length) == -1 &&
          errno == EINVAL && output == 0x12345678 && length == sizeof(output) - 1);
    int value = 1;
    errno = 0;
    CHECK("short-set", setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value) - 1) == -1 &&
          errno == EINVAL && option(fd, 0));
    errno = 0;
    CHECK("disabled-sendto", sendto(fd, "forbidden-disabled", 18, 0,
          (struct sockaddr *)&limited, sizeof(limited)) == -1 && errno == EACCES);
    errno = 0;
    CHECK("disabled-directed", send_nonblocking_message(fd, &directed, "forbidden-directed") == -1 && errno == EACCES);
    struct sockaddr_in local = {0}; socklen_t local_len = sizeof(local);
    CHECK("no-autobind", getsockname(fd, (struct sockaddr *)&local, &local_len) == 0 && ntohs(local.sin_port) == 0);
    CHECK("connect", connect(fd, (struct sockaddr *)&limited, sizeof(limited)) == 0);
    errno = 0;
    CHECK("connected-disabled", send(fd, "forbidden-connected", 19, 0) == -1 && errno == EACCES);
    value = -7;
    CHECK("enable-normalized", setsockopt(alias, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == 0 &&
          option(fd, 1) && option(alias, 1) && option(independent, 0));
    CHECK("limited", send_named(fd, &limited, "broadcast-limited", 0));
    CHECK("directed", send_nonblocking_message(fd, &directed, "broadcast-directed") ==
          (ssize_t)(sizeof("broadcast-directed") - 1));
    CHECK("connected", send(fd, "broadcast-connected", sizeof("broadcast-connected") - 1, 0) ==
          (ssize_t)(sizeof("broadcast-connected") - 1));
    value = 0;
    CHECK("reset", setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == 0 && option(alias, 0));
    errno = 0;
    CHECK("connected-reset", send(alias, "forbidden-reset", 15, 0) == -1 && errno == EACCES);
    CHECK("unicast-default", option(independent, 0) && send_named(independent, &unicast, "broadcast-unicast", 0));
    int unix_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK("unix-socket", unix_fd >= 0);
    errno = 0;
    CHECK("unix-option", setsockopt(unix_fd, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == -1 && errno == ENOPROTOOPT);
    CHECK("unix-close", close(unix_fd) == 0);

    long raw = srv_net_udp_open();
    CHECK("raw-open", raw >= 0);
    int raw_alias = dup((int)raw);
    CHECK("raw-dup", raw_alias >= 0 && option((int)raw, 0) && option(raw_alias, 0));
    CHECK("raw-invalid-option", srv_syscall5(SYS_NET_UDP_BROADCAST, SRV_NET_BROADCAST_SET,
          raw, 2, 0, 0) == SRV_ERR_INVAL && option((int)raw, 0));
    CHECK("raw-denied", srv_net_udp_sendto((int)raw, UINT32_C(0xffffffff), PORT,
          "forbidden-native", sizeof("forbidden-native") - 1) == SRV_ERR_ACCES);
    uint32_t raw_ip = 0; uint16_t raw_port = 1;
    CHECK("raw-no-autobind", srv_net_sockname((int)raw, &raw_ip, &raw_port) == 0 && raw_port == 0);
    value = 1;
    CHECK("raw-enable-alias", setsockopt(raw_alias, SOL_SOCKET, SO_BROADCAST, &value, sizeof(value)) == 0 && option((int)raw, 1));
    CHECK("raw-send", srv_net_udp_sendto((int)raw, UINT32_C(0xffffffff), PORT,
          "broadcast-native", sizeof("broadcast-native") - 1) == (long)(sizeof("broadcast-native") - 1));
    CHECK("raw-close", close(raw_alias) == 0 && srv_close((int)raw) == 0);
    long fresh = srv_net_udp_open();
    CHECK("fresh-default", fresh >= 0 && option((int)fresh, 0) && srv_close((int)fresh) == 0);
    CHECK("close", close(alias) == 0 && close(fd) == 0 && close(independent) == 0);
    printf("glib-so-broadcast: ok\n");
    return 0;
}
