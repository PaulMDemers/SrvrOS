#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(condition) do { if (!(condition)) { \
    fprintf(stderr, "glib-fionread: line %d, errno %d\n", __LINE__, errno); \
    return 1; \
} } while (0)

static int available(int fd, int expected) {
    int count = -1;
    return ioctl(fd, FIONREAD, &count) == 0 && count == expected;
}

int main(void) {
    char buffer[8];
    int pair[2];
    CHECK(pipe(pair) == 0 && available(pair[0], 0));
    CHECK(write(pair[1], "abcdef", 6) == 6 && available(pair[0], 6));
    int alias = dup(pair[0]);
    CHECK(alias >= 0 && available(alias, 6));
    CHECK(ioctl(alias, FIONREAD, (void *)0) == -1 && errno == EFAULT);
    CHECK(read(pair[0], buffer, 2) == 2 && available(alias, 4));
    CHECK(close(pair[1]) == 0 && available(alias, 4));
    CHECK(read(alias, buffer, sizeof(buffer)) == 4 && available(pair[0], 0));
    CHECK(close(alias) == 0 && close(pair[0]) == 0);

    CHECK(socketpair(AF_UNIX, SOCK_SEQPACKET, 0, pair) == 0);
    CHECK(send(pair[0], "abc", 3, 0) == 3 && send(pair[0], "defgh", 5, 0) == 5);
    CHECK(available(pair[1], 3) && available(pair[1], 3));
    CHECK(recv(pair[1], buffer, 2, 0) == 2 && available(pair[1], 5));
    CHECK(recv(pair[1], buffer, sizeof(buffer), 0) == 5 && available(pair[1], 0));
    CHECK(close(pair[0]) == 0 && close(pair[1]) == 0);

    const char path[] = "/fionread-guest";
    struct sockaddr_un un = {.sun_family = AF_UNIX};
    memcpy(un.sun_path, path, sizeof(path));
    int listener = socket(AF_UNIX, SOCK_STREAM, 0);
    int client = socket(AF_UNIX, SOCK_STREAM, 0);
    CHECK(listener >= 0 && client >= 0);
    CHECK(bind(listener, (struct sockaddr *)&un, sizeof(un)) == 0 && listen(listener, 1) == 0);
    CHECK(connect(client, (struct sockaddr *)&un, sizeof(un)) == 0 && available(client, 0));
    int server = accept(listener, NULL, NULL);
    CHECK(server >= 0 && write(server, "test", 4) == 4 && available(client, 4));
    CHECK(read(client, buffer, sizeof(buffer)) == 4 && available(client, 0));
    CHECK(unlink(path) == 0 && close(server) == 0 && close(client) == 0 && close(listener) == 0);

    struct sockaddr_in addr = {.sin_family = AF_INET, .sin_port = htons(47775)};
    CHECK(inet_pton(AF_INET, "10.0.2.15", &addr.sin_addr) == 1);
    int udp = socket(AF_INET, SOCK_DGRAM, 0);
    int sender = socket(AF_INET, SOCK_DGRAM, 0);
    CHECK(udp >= 0 && sender >= 0 && available(udp, 0));
    CHECK(bind(udp, (struct sockaddr *)&addr, sizeof(addr)) == 0);
    CHECK(sendto(sender, "abc", 3, 0, (struct sockaddr *)&addr, sizeof(addr)) == 3);
    CHECK(sendto(sender, "defgh", 5, 0, (struct sockaddr *)&addr, sizeof(addr)) == 5);
    CHECK(available(udp, 3) && available(udp, 3));
    CHECK(recv(udp, buffer, sizeof(buffer), 0) == 3 && available(udp, 5));
    CHECK(recv(udp, buffer, sizeof(buffer), 0) == 5 && available(udp, 0));
    CHECK(close(sender) == 0 && close(udp) == 0);

    int count = 42;
    CHECK(ioctl(-1, FIONREAD, &count) == -1 && errno == EBADF && count == 42);
    CHECK(ioctl(STDOUT_FILENO, FIONREAD, &count) == -1 && errno == ENOTTY && count == 42);
    puts("glib-fionread: ok");
    return 0;
}
