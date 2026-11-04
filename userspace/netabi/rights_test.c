#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <unistd.h>

struct rights_fixture {
    int pair[2];
    int pipe[2];
    int alias;
};

union one_right {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(sizeof(int))];
};

union two_rights {
    struct cmsghdr alignment;
    unsigned char bytes[CMSG_SPACE(2 * sizeof(int))];
};

static int send_fds(int fd, const int *fds, size_t count) {
    union two_rights control = {0};
    char byte = 'C';
    struct iovec iov = {&byte, 1};
    struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = control.bytes, .msg_controllen = CMSG_SPACE(count * sizeof(int))};
    struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(count * sizeof(int));
    memcpy(CMSG_DATA(header), fds, count * sizeof(int));
    return sendmsg(fd, &msg, MSG_DONTWAIT) == 1;
}

static int pipe_eof(int fd) {
    char byte;
    return fcntl(fd, F_SETFL, O_NONBLOCK) == 0 && read(fd, &byte, 1) == 0;
}

static int cycle_once(int indirect) {
    int a[2] = {-1, -1}, b[2] = {-1, -1}, witness[2] = {-1, -1};
    int ok = socketpair(AF_UNIX, SOCK_STREAM, 0, a) == 0 &&
        (!indirect || socketpair(AF_UNIX, SOCK_STREAM, 0, b) == 0) && pipe(witness) == 0;
    if (ok) {
        int rights[] = {indirect ? b[1] : a[1], witness[1]};
        ok = send_fds(a[0], rights, 2);
        if (ok && indirect) ok = send_fds(b[0], &a[1], 1);
    }
    for (int i = 0; i < 2; i++) {
        if (a[i] >= 0) close(a[i]);
        if (b[i] >= 0) close(b[i]);
    }
    if (witness[1] >= 0) close(witness[1]);
    if (ok) ok = pipe_eof(witness[0]);
    if (witness[0] >= 0) close(witness[0]);
    return ok;
}

static int cycle_live_alias(void) {
    int pair[2] = {-1, -1}, witness[2] = {-1, -1}, alias = -1;
    int received[2] = {-1, -1};
    int ok = socketpair(AF_UNIX, SOCK_STREAM, 0, pair) == 0 && pipe(witness) == 0 &&
        (alias = dup(pair[1])) >= 0;
    if (ok) {
        int rights[] = {pair[1], witness[1]};
        ok = send_fds(pair[0], rights, 2);
    }
    if (pair[0] >= 0) close(pair[0]);
    if (pair[1] >= 0) close(pair[1]);
    if (witness[1] >= 0) close(witness[1]);
    if (ok) {
        char byte = 0;
        ok = fcntl(witness[0], F_SETFL, O_NONBLOCK) == 0 &&
            read(witness[0], &byte, 1) == -1 && errno == EAGAIN;
    }
    if (ok) {
        char byte = 0;
        union two_rights control = {0};
        struct iovec iov = {&byte, 1};
        struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1,
            .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes)};
        ssize_t count = recvmsg(alias, &msg, MSG_DONTWAIT);
        struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
        ok = count == 1 && byte == 'C' && header && header->cmsg_level == SOL_SOCKET &&
            header->cmsg_type == SCM_RIGHTS && header->cmsg_len == CMSG_LEN(sizeof(received));
        if (ok) {
            memcpy(received, CMSG_DATA(header), sizeof(received));
            ok = write(received[1], "L", 1) == 1 && read(witness[0], &byte, 1) == 1 && byte == 'L';
        }
    }
    for (int i = 0; i < 2; i++) if (received[i] >= 0) close(received[i]);
    if (alias >= 0) close(alias);
    if (ok) ok = pipe_eof(witness[0]);
    if (witness[0] >= 0) close(witness[0]);
    return ok;
}

static int check_cycles(const char *name, int indirect) {
    int ok = 1;
    /* More iterations than the kernel pipe pool expose retained cycle entries. */
    for (int i = 0; i < 80 && ok; i++) ok = cycle_once(indirect);
    printf("netabi: rights %s %s\n", name, ok ? "ok" : "failed");
    return ok;
}

static ssize_t send_right(struct rights_fixture *f, char byte) {
    union one_right control = {0};
    struct iovec iov = {&byte, 1};
    struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes)};
    struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
    header->cmsg_level = SOL_SOCKET;
    header->cmsg_type = SCM_RIGHTS;
    header->cmsg_len = CMSG_LEN(sizeof(int));
    memcpy(CMSG_DATA(header), &f->pipe[1], sizeof(int));
    return sendmsg(f->pair[0], &msg, MSG_DONTWAIT);
}

static ssize_t receive_flags(int fd, char *bytes, size_t capacity, int flags, int *received_fd) {
    union one_right control;
    struct iovec iov = {bytes, capacity};
    struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1,
        .msg_control = control.bytes, .msg_controllen = sizeof(control.bytes)};
    ssize_t n = recvmsg(fd, &msg, flags | MSG_CMSG_CLOEXEC | MSG_DONTWAIT);
    *received_fd = -1;
    struct cmsghdr *header = CMSG_FIRSTHDR(&msg);
    if (n >= 0 && header && header->cmsg_level == SOL_SOCKET &&
        header->cmsg_type == SCM_RIGHTS && header->cmsg_len == CMSG_LEN(sizeof(int))) {
        memcpy(received_fd, CMSG_DATA(header), sizeof(int));
    }
    return n;
}

static ssize_t receive(int fd, char *bytes, size_t capacity, int *received_fd) {
    return receive_flags(fd, bytes, capacity, 0, received_fd);
}

static int rejected_send(struct rights_fixture *f) {
    char bytes[512] = {0};
    ssize_t n;
    do {
        n = send(f->pair[0], bytes, sizeof(bytes), MSG_DONTWAIT);
    } while (n == sizeof(bytes));
    if (n != -1 || errno != EAGAIN || send_right(f, 'R') != -1 || errno != EAGAIN)
        return 0;
    if (close(f->pair[0]) < 0) return 0;
    f->pair[0] = -1;
    do {
        n = recv(f->pair[1], bytes, sizeof(bytes), MSG_DONTWAIT);
    } while (n > 0);
    if (n != -1 || errno != EAGAIN || send(f->alias, "P", 1, 0) != 1) return 0;
    int received_fd;
    n = receive(f->pair[1], bytes, 1, &received_fd);
    if (received_fd >= 0) close(received_fd);
    return n == 1 && bytes[0] == 'P' && received_fd == -1;
}

static int empty_receive(struct rights_fixture *f) {
    char byte = 0;
    int received_fd;
    if (send_right(f, 'R') != 1) return 0;
    ssize_t n = receive(f->pair[1], &byte, 0, &received_fd);
    if (received_fd >= 0) close(received_fd);
    if (n != 0 || received_fd != -1) return 0;
    n = receive(f->pair[1], &byte, 1, &received_fd);
    int ok = n == 1 && byte == 'R' && received_fd >= 0 &&
        fcntl(received_fd, F_GETFD) == FD_CLOEXEC &&
        write(received_fd, "X", 1) == 1 && read(f->pipe[0], &byte, 1) == 1 && byte == 'X';
    if (received_fd >= 0) close(received_fd);
    return ok;
}

static int prefix_receive(struct rights_fixture *f) {
    if (send(f->alias, "P", 1, 0) != 1 || send_right(f, 'R') != 1) return 0;
    char bytes[2];
    int received_fd;
    ssize_t n = receive(f->pair[1], bytes, sizeof(bytes), &received_fd);
    if (received_fd >= 0) close(received_fd);
    if (n != 1 || bytes[0] != 'P' || received_fd != -1) return 0;
    n = receive(f->pair[1], bytes, sizeof(bytes), &received_fd);
    if (received_fd >= 0) close(received_fd);
    return n == 1 && bytes[0] == 'R' && received_fd >= 0;
}

static int peek_and_discard(struct rights_fixture *f) {
    if (send_right(f, 'R') != 1 || close(f->pipe[1]) < 0) return 0;
    f->pipe[1] = -1;
    char byte = 0;
    int peek_fd;
    ssize_t n = receive_flags(f->pair[1], &byte, 1, MSG_PEEK, &peek_fd);
    if (n != 1 || byte != 'R' || peek_fd < 0) return 0;
    int ok = fcntl(peek_fd, F_GETFD) == FD_CLOEXEC;
    close(peek_fd);
    struct iovec iov = {&byte, 1};
    struct msghdr msg = {.msg_iov = &iov, .msg_iovlen = 1};
    byte = 0;
    return ok && recvmsg(f->pair[1], &msg, MSG_DONTWAIT) == 1 &&
        byte == 'R' && (msg.msg_flags & MSG_CTRUNC) && msg.msg_controllen == 0 &&
        fcntl(f->pipe[0], F_SETFL, O_NONBLOCK) == 0 && read(f->pipe[0], &byte, 1) == 0;
}

static int receive_fault(struct rights_fixture *f) {
    void *guard = mmap(NULL, 4096, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (guard == MAP_FAILED) return 0;
    char byte = '?';
    union one_right control = {0};
    struct iovec iov[] = {{&byte, 1}, {guard, 1}};
    struct msghdr msg = {.msg_iov = iov, .msg_iovlen = 2, .msg_control = control.bytes,
        .msg_controllen = sizeof(control.bytes)};
    int ok = send_right(f, 'R') == 1 && send(f->alias, "S", 1, 0) == 1 &&
        recvmsg(f->pair[1], &msg, MSG_DONTWAIT) == -1 && errno == EFAULT && byte == '?';
    msg.msg_iovlen = 1;
    msg.msg_control = guard;
    msg.msg_controllen = sizeof(control.bytes);
    ok = ok && recvmsg(f->pair[1], &msg, MSG_DONTWAIT) == -1 && errno == EFAULT && byte == '?';
    if (ok) {
        int received_fd;
        ok = receive(f->pair[1], &byte, 1, &received_fd) == 1 && byte == 'R' && received_fd >= 0;
        if (received_fd >= 0) close(received_fd);
    }
    munmap(guard, 4096);
    return ok;
}

static int check_rights(const char *name, int (*run)(struct rights_fixture *)) {
    struct rights_fixture f = {{-1, -1}, {-1, -1}, -1};
    int ok = socketpair(AF_UNIX, SOCK_STREAM, 0, f.pair) == 0 &&
        pipe(f.pipe) == 0 && (f.alias = dup(f.pair[0])) >= 0 && run(&f);
    if (f.alias >= 0) close(f.alias);
    for (int i = 0; i < 2; i++) {
        if (f.pair[i] >= 0) close(f.pair[i]);
        if (f.pipe[i] >= 0) close(f.pipe[i]);
    }
    printf("netabi: rights %s %s\n", name, ok ? "ok" : "failed");
    return ok;
}

int netabi_rights_test(void) {
    int ok = check_rights("rejected send across alias close", rejected_send);
    ok &= check_rights("zero-capacity receive", empty_receive);
    ok &= check_rights("unrelated prefix", prefix_receive);
    ok &= check_rights("peek and discard", peek_and_discard);
    if (ok) ok = check_rights("invalid output preserves queue", receive_fault);
    if (ok) {
        int live = cycle_live_alias();
        printf("netabi: rights live cycle alias %s\n", live ? "ok" : "failed");
        ok &= live;
    }
    ok &= check_cycles("self cycle reclaimed", 0);
    ok &= check_cycles("indirect cycle reclaimed", 1);
    return ok ? 0 : -1;
}
