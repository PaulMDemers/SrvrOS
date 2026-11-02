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
    return ok ? 0 : -1;
}
