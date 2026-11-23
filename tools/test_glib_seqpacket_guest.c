/* AF_UNIX record and SCM_RIGHTS behavior on real target sockets. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <srvros/sys.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/uio.h>
#include <sys/un.h>
#include <unistd.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-seqpacket: failure %s line=%d errno=%d\n", phase, __LINE__, errno); \
        return 1; \
    } \
} while (0)

int main(void) {
    int pair[2], pipefd[2];
    errno = 0;
    CHECK("unbound-socket", socket(AF_UNIX, SOCK_SEQPACKET, 0) == -1 && errno == EPROTONOSUPPORT);
    CHECK("pair", socketpair(AF_UNIX, SOCK_SEQPACKET | SOCK_NONBLOCK | SOCK_CLOEXEC, 0, pair) == 0);
    for (unsigned i = 0; i < 2; ++i) {
        int flags = fcntl(pair[i], F_GETFL);
        int fdflags = fcntl(pair[i], F_GETFD);
        CHECK("flags", flags == (O_RDWR | O_NONBLOCK) && fdflags >= 0 && (fdflags & FD_CLOEXEC));
        int type = 0;
        socklen_t len = sizeof(type);
        CHECK("type", getsockopt(pair[i], SOL_SOCKET, SO_TYPE, &type, &len) == 0 &&
              len == sizeof(type) && type == SOCK_SEQPACKET);
    }
    int duplicate = dup(pair[0]);
    CHECK("dup", duplicate >= 0);
    int dtype = 0;
    socklen_t dlen = sizeof(dtype);
    CHECK("dup-type", getsockopt(duplicate, SOL_SOCKET, SO_TYPE, &dtype, &dlen) == 0 && dtype == SOCK_SEQPACKET);
    struct sockaddr_un address = {0};
    socklen_t addrlen = sizeof(address);
    CHECK("family", getsockname(duplicate, (struct sockaddr *)&address, &addrlen) == 0 &&
          address.sun_family == AF_UNIX && address.sun_path[0] == '\0');
    addrlen = sizeof(address);
    CHECK("peer-family", getpeername(duplicate, (struct sockaddr *)&address, &addrlen) == 0 &&
          address.sun_family == AF_UNIX && address.sun_path[0] == '\0');
    CHECK("dup-close", close(duplicate) == 0);
    CHECK("pipe", pipe(pipefd) == 0);
    int rights[4] = {pipefd[1], -1, -1, -1};
    for (unsigned i = 1; i < 4; ++i) {
        rights[i] = dup(pipefd[1]);
        CHECK("rights-dup", rights[i] >= 0);
    }

    char left[] = "first-", right[] = "record";
    struct iovec vec[2] = {{left, sizeof(left)-1}, {right, sizeof(right)-1}};
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(sizeof(rights))]; } control = {0};
    struct msghdr message = {0};
    message.msg_iov = vec; message.msg_iovlen = 2;
    message.msg_control = control.bytes; message.msg_controllen = sizeof(control.bytes);
    struct cmsghdr *c = CMSG_FIRSTHDR(&message);
    CHECK("send-cmsg", c != NULL);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(rights));
    memcpy(CMSG_DATA(c), rights, sizeof(rights));
    CHECK("legacy-rights-send-refused", srv_pipe_send_rights(pair[0], rights, 4) < 0);
    CHECK("send-first", sendmsg(pair[0], &message, 0) == 12);
    int legacy = -1;
    uint64_t legacy_flags = 0;
    CHECK("legacy-rights-receive-refused", srv_pipe_recv_rights(pair[1], &legacy, 1, &legacy_flags) < 0 &&
          legacy == -1);
    for (unsigned i = 0; i < 4; ++i) CHECK("source-close", close(rights[i]) == 0);
    char second[] = "second";
    struct iovec v2[2] = {{second, 2}, {second + 2, 4}};
    struct msghdr m2 = {0}; m2.msg_iov = v2; m2.msg_iovlen = 2;
    CHECK("send-second", sendmsg(pair[0], &m2, 0) == 6);
    struct pollfd p = {.fd = pair[1], .events = POLLIN};
    CHECK("ready", poll(&p, 1, 0) == 1 && (p.revents & POLLIN));
    struct stat socket_stat = {0};
    CHECK("packet-stat", fstat(pair[1], &socket_stat) == 0 &&
          S_ISSOCK(socket_stat.st_mode) && socket_stat.st_size == 0);

    char data[32] = {0};
    struct iovec receive = {data, 5};
    union { struct cmsghdr align; unsigned char bytes[CMSG_SPACE(sizeof(rights))]; } received = {0};
    struct msghdr m = {0}; m.msg_iov = &receive; m.msg_iovlen = 1;
    m.msg_control = received.bytes; m.msg_controllen = sizeof(received.bytes);
    receive.iov_base = (void *)(uintptr_t)1;
    errno = 0;
    CHECK("copyout-failure", recvmsg(pair[1], &m, MSG_CMSG_CLOEXEC) == -1 && errno == EFAULT);
    receive.iov_base = data;
    CHECK("truncated-first", recvmsg(pair[1], &m, MSG_CMSG_CLOEXEC) == 5 &&
          (m.msg_flags & MSG_TRUNC) && !(m.msg_flags & MSG_CTRUNC) && memcmp(data, "first", 5) == 0);
    c = CMSG_FIRSTHDR(&m);
    CHECK("receive-rights", c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS &&
          c->cmsg_len == CMSG_LEN(sizeof(rights)));
    int returned[4]; memcpy(returned, CMSG_DATA(c), sizeof(returned));
    for (unsigned i = 0; i < 4; ++i) {
        int fdflags = fcntl(returned[i], F_GETFD);
        CHECK("rights-cloexec", fdflags >= 0 && (fdflags & FD_CLOEXEC));
        CHECK("rights-write", write(returned[i], "r", 1) == 1);
        CHECK("rights-close", close(returned[i]) == 0);
    }
    CHECK("rights-pipe", read(pipefd[0], data, 4) == 4 && memcmp(data, "rrrr", 4) == 0);
    CHECK("rights-pipe-close", close(pipefd[0]) == 0);
    memset(data, 0, sizeof(data)); receive.iov_len = sizeof(data);
    m = (struct msghdr){0}; m.msg_iov = &receive; m.msg_iovlen = 1;
    CHECK("second-record", recvmsg(pair[1], &m, 0) == 6 && m.msg_flags == 0 && memcmp(data, "second", 6) == 0);
    p.revents = 0; CHECK("empty", poll(&p, 1, 0) == 0);
    errno = 0; CHECK("would-block", recv(pair[1], data, sizeof(data), 0) == -1 && errno == EAGAIN);

    static unsigned char big[4097], copy[4096];
    memset(big, 0x5a, sizeof(big));
    errno = 0;
    CHECK("oversize", send(pair[0], big, sizeof(big), 0) == -1 && errno == EMSGSIZE);
    p.revents = 0; CHECK("no-prefix", poll(&p, 1, 0) == 0);
    CHECK("max-send", send(pair[0], big, 4096, 0) == 4096);
    CHECK("max-recv", recv(pair[1], copy, sizeof(copy), 0) == 4096 && memcmp(big, copy, sizeof(copy)) == 0);
    /* Scalar reads consume one record even when their buffer is short. */
    CHECK("scalar-send", write(pair[0], "abcdef", 6) == 6 && write(pair[0], "XY", 2) == 2);
    CHECK("scalar-short", read(pair[1], data, 3) == 3 && memcmp(data, "abc", 3) == 0);
    CHECK("scalar-next", read(pair[1], data, sizeof(data)) == 2 && memcmp(data, "XY", 2) == 0);

    CHECK("vector-send", writev(pair[0], vec, 2) == 12);
    CHECK("vector-followup", write(pair[0], "Y", 1) == 1);
    struct iovec receive_vec[2] = {{data, 3}, {data + 3, 9}};
    CHECK("vector-receive", readv(pair[1], receive_vec, 2) == 12 && memcmp(data, "first-record", 12) == 0);
    CHECK("vector-next", read(pair[1], data, sizeof(data)) == 1 && data[0] == 'Y');

    /* Queue fills only with whole records; a failed send cannot add a prefix. */
    unsigned queued = 0;
    for (; queued < 128; ++queued) {
        ssize_t sent = send(pair[0], big, 4096, 0);
        if (sent < 0 && errno == EAGAIN) break;
        CHECK("fill-record", sent == 4096);
    }
    CHECK("fill-bounded", queued > 0 && queued < 128);
    errno = 0;
    CHECK("full-reject", send(pair[0], big, 4096, 0) == -1 && errno == EAGAIN);
    for (unsigned i = 0; i < queued; ++i)
        CHECK("drain-record", recv(pair[1], copy, sizeof(copy), 0) == 4096 && memcmp(copy, big, sizeof(copy)) == 0);
    errno = 0;
    CHECK("drain-empty", recv(pair[1], data, sizeof(data), 0) == -1 && errno == EAGAIN);

    int transferred_pair[2];
    CHECK("peek-pair", socketpair(AF_UNIX, SOCK_SEQPACKET, 0, transferred_pair) == 0);
    memset(&control, 0, sizeof(control));
    message = (struct msghdr){0}; message.msg_iov = vec; message.msg_iovlen = 2;
    message.msg_control = control.bytes; message.msg_controllen = CMSG_SPACE(sizeof(int));
    c = CMSG_FIRSTHDR(&message);
    c->cmsg_level = SOL_SOCKET; c->cmsg_type = SCM_RIGHTS; c->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(c), &transferred_pair[0], sizeof(int));
    CHECK("peek-send", sendmsg(pair[0], &message, 0) == 12 && close(transferred_pair[0]) == 0);
    for (unsigned i = 0; i < 2; ++i) {
        memset(&received, 0, sizeof(received));
        receive.iov_len = sizeof(data);
        m = (struct msghdr){0}; m.msg_iov = &receive; m.msg_iovlen = 1;
        m.msg_control = received.bytes; m.msg_controllen = sizeof(received.bytes);
        CHECK("peek-receive", recvmsg(pair[1], &m, i == 0 ? MSG_PEEK | MSG_CMSG_CLOEXEC : MSG_CMSG_CLOEXEC) == 12 &&
              m.msg_flags == 0 && memcmp(data, "first-record", 12) == 0);
        c = CMSG_FIRSTHDR(&m);
        CHECK("peek-rights", c && c->cmsg_level == SOL_SOCKET && c->cmsg_type == SCM_RIGHTS &&
              c->cmsg_len == CMSG_LEN(sizeof(int)));
        int copied; memcpy(&copied, CMSG_DATA(c), sizeof(copied));
        int flags = fcntl(copied, F_GETFD);
        CHECK("peek-cloexec", flags >= 0 && (flags & FD_CLOEXEC));
        dtype = 0;
        dlen = sizeof(dtype);
        CHECK("transferred-type", getsockopt(copied, SOL_SOCKET, SO_TYPE, &dtype, &dlen) == 0 &&
              dtype == SOCK_SEQPACKET);
        CHECK("peek-right-live", write(copied, "p", 1) == 1 && close(copied) == 0);
    }
    for (unsigned i = 0; i < 2; ++i)
        CHECK("peek-right-data", read(transferred_pair[1], data, sizeof(data)) == 1 && data[0] == 'p');
    CHECK("peek-right-close", close(transferred_pair[1]) == 0);

    CHECK("zero-send", send(pair[0], "", 0, 0) == 0);
    CHECK("zero-followup", send(pair[0], "z", 1, 0) == 1);
    p.revents = 0; CHECK("zero-ready", poll(&p, 1, 0) == 1 && (p.revents & POLLIN));
    CHECK("zero-receive", recv(pair[1], data, sizeof(data), 0) == 0);
    CHECK("zero-next", recv(pair[1], data, sizeof(data), 0) == 1 && data[0] == 'z');
    CHECK("sender-close", close(pair[0]) == 0);
    p.revents = 0; CHECK("close-poll", poll(&p, 1, 1000) == 1 && (p.revents & (POLLIN | POLLHUP)));
    CHECK("eof", recv(pair[1], data, sizeof(data), 0) == 0);
    CHECK("receiver-close", close(pair[1]) == 0);

    int stream[2];
    CHECK("stream-pair", socketpair(AF_UNIX, SOCK_STREAM, 0, stream) == 0);
    CHECK("stream-write", write(stream[0], "ab", 2) == 2 && write(stream[0], "cd", 2) == 2);
    CHECK("stream-read", read(stream[1], data, 4) == 4 && memcmp(data, "abcd", 4) == 0);
    CHECK("stream-close", close(stream[0]) == 0 && close(stream[1]) == 0);
    puts("glib-seqpacket: ok");
    return 0;
}
