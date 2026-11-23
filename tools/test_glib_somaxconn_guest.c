/* Real AF_UNIX backlog admission and drain on the public socket interface. */
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <srvros/sys.h>
#include <srvros/syscall_numbers.h>

#define PATH "/glib-somaxconn-test"
#define CHECK(phase, condition) do { if (!(condition)) { \
    printf("glib-somaxconn: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
    return 1; \
} } while (0)

static int test_limit(int backlog, unsigned limit) {
    struct sockaddr_un address = {0};
    address.sun_family = AF_UNIX;
    memcpy(address.sun_path, PATH, sizeof(PATH));
    int listener = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
    CHECK("socket", listener >= 0);
    CHECK("bind", bind(listener, (struct sockaddr *)&address, sizeof(address)) == 0);
    CHECK("listen", listen(listener, backlog) == 0);
    int clients[SOMAXCONN];
    for (unsigned i = 0; i < limit; ++i) {
        long client = srv_unix_connect(PATH);
        CHECK("admit", client >= 0);
        clients[i] = (int)client;
        char byte = (char)('A' + i);
        CHECK("write", srv_write(clients[i], &byte, 1) == 1);
    }
    CHECK("full", srv_unix_connect(PATH) == SRV_ERR_AGAIN);
    if (limit == 1) {
        int extra = socket(AF_UNIX, SOCK_STREAM | SOCK_NONBLOCK, 0);
        CHECK("posix-client", extra >= 0);
        errno = 0;
        CHECK("posix-full", connect(extra, (struct sockaddr *)&address, sizeof(address)) == -1 && errno == EAGAIN);
        CHECK("posix-close", close(extra) == 0);
    }
    int accepted = accept(listener, NULL, NULL);
    CHECK("accept-first", accepted >= 0);
    char byte = 0;
    CHECK("read-first", read(accepted, &byte, 1) == 1 &&
          byte >= 'A' && (unsigned)(byte - 'A') < limit);
    unsigned first = (unsigned)(byte - 'A');
    unsigned seen = 1u << first;
    CHECK("close-first", close(accepted) == 0 && srv_close(clients[first]) == 0);
    long retry = srv_unix_connect(PATH);
    CHECK("retry-after-drain", retry >= 0);
    byte = 'z';
    CHECK("write-retry", srv_write((int)retry, &byte, 1) == 1);
    CHECK("full-after-retry", srv_unix_connect(PATH) == SRV_ERR_AGAIN);
    for (unsigned i = 0; i < limit; ++i) {
        accepted = accept(listener, NULL, NULL);
        CHECK("accept-drain", accepted >= 0);
        byte = 0;
        CHECK("read-drain", read(accepted, &byte, 1) == 1 &&
              (byte == 'z' || (byte >= 'A' && (unsigned)(byte - 'A') < limit)));
        unsigned index = byte == 'z' ? limit : (unsigned)(byte - 'A');
        CHECK("unique-peer", (seen & (1u << index)) == 0);
        seen |= 1u << index;
        CHECK("close-drain", close(accepted) == 0 &&
              srv_close(index == limit ? (int)retry : clients[index]) == 0);
    }
    CHECK("all-peers", seen == (1u << (limit + 1)) - 1u);
    errno = 0;
    CHECK("empty", accept(listener, NULL, NULL) == -1 && (errno == EAGAIN || errno == EWOULDBLOCK));
    CHECK("unlink", srv_unix_unlink(PATH) == 0);
    CHECK("listener-close", close(listener) == 0);
    return 0;
}

int main(void) {
    CHECK("constant", SOMAXCONN == 16 && SRV_UNIX_LISTEN_BACKLOG_MAX == 16);
    CHECK("zero", test_limit(0, 1) == 0);
    CHECK("negative", test_limit(-1, 1) == 0);
    CHECK("three", test_limit(3, 3) == 0);
    CHECK("gio-default", test_limit(10, 10) == 0);
    CHECK("maximum", test_limit(SOMAXCONN, 16) == 0);
    CHECK("clamped", test_limit(64, 16) == 0);
    printf("glib-somaxconn: ok\n");
    return 0;
}
