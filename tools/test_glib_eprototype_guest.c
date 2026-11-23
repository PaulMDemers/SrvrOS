/* SDK errno declaration and unchanged socket error behavior. */
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("glib-eprototype: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

int main(void) {
    const char *expected = "protocol wrong type for socket";
    CHECK("errno-value", EPROTOTYPE == 91 && EPROTOTYPE != EPROTONOSUPPORT &&
          EPROTOTYPE != EAFNOSUPPORT && EPROTOTYPE != EINVAL);
    errno = EBUSY;
    CHECK("strerror", strcmp(strerror(EPROTOTYPE), expected) == 0 && errno == EBUSY);
    char text[64];
    errno = EBUSY;
    CHECK("strerror-r", strerror_r(EPROTOTYPE, text, sizeof(text)) == 0 &&
          strcmp(text, expected) == 0 && errno == EBUSY);
    char short_text[5] = {0};
    errno = EBUSY;
    CHECK("strerror-r-short", strerror_r(EPROTOTYPE, short_text, sizeof(short_text)) == ERANGE &&
          strcmp(short_text, "prot") == 0 && errno == EBUSY);

    int fd = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
    CHECK("stream", fd >= 0);
    int flags = fcntl(fd, F_GETFL), fdflags = fcntl(fd, F_GETFD);
    CHECK("stream-flags", flags >= 0 && (flags & O_NONBLOCK) &&
          fdflags >= 0 && (fdflags & FD_CLOEXEC));
    CHECK("stream-close", close(fd) == 0);
    errno = 0;
    CHECK("named-seqpacket", socket(AF_UNIX, SOCK_SEQPACKET, 0) == -1 && errno == EPROTONOSUPPORT);
    errno = 0;
    CHECK("tcp-dgram", socket(AF_INET, SOCK_DGRAM, IPPROTO_TCP) == -1 && errno == EPROTONOSUPPORT);
    errno = 0;
    CHECK("udp-stream", socket(AF_INET, SOCK_STREAM, IPPROTO_UDP) == -1 && errno == EPROTONOSUPPORT);
    errno = 0;
    CHECK("ipv6", socket(AF_INET6, SOCK_STREAM, 0) == -1 && errno == EAFNOSUPPORT);
    printf("glib-eprototype: ok\n");
    return 0;
}
