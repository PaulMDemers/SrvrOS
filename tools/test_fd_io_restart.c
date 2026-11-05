/* Host fault injection for libc's interrupted descriptor I/O continuations. */
#include <assert.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <srvros/sys.h>

/* Use the guest ABI for the call under test; host fcntl constants and flock
 * layout differ on Darwin. */
enum { TEST_F_SETLKW = 7, TEST_F_WRLCK = 2 };
struct test_flock {
    short type;
    short whence;
    int64_t start;
    int64_t length;
    int64_t pid;
};

enum scenario {
    RESTART_SAME_FD,
    RESTART_REPLACED_FD,
    SIGNAL_WITHOUT_RESTART,
    ERROR_WITH_SIGNAL,
    RESTART_REPLACED_PSEUDO,
    PARTIAL_WRITE,
    SOCKET_ERROR_WITH_SIGNAL,
    SOCKET_ERROR_WITHOUT_SIGNAL,
};

ssize_t audit_read(int fd, void *buffer, size_t length);
ssize_t audit_write(int fd, const void *buffer, size_t length);
ssize_t audit_readv(int fd, const struct iovec *iov, int count);
ssize_t audit_writev(int fd, const struct iovec *iov, int count);
int audit_fcntl(int fd, int command, ...);
ssize_t __posix_write_with_socket_error(int fd, const void *buffer,
    size_t length, int *socket_error);

#ifdef __APPLE__
/* The guest object uses the glibc-style symbol; Darwin's errno is __error(). */
int *__errno_location(void) { return &errno; }
#endif

static enum scenario scenario;
static unsigned calls;
static unsigned dispatches;
static unsigned captures;
static uint64_t identities[1002];
static int socket_error;

int __posix_socket_pending_error(int fd) {
    assert(fd == 7);
    return socket_error;
}

int __posix_eventfd_is_pseudo(int fd) {
    (void)fd;
    return 0;
}
int __posix_eventfd_is_pseudo_safe(int fd){(void)fd;assert(!"unexpected public-I/O event query");return 0;}
uint64_t __posix_event_capture_identity_locked(int fd){(void)fd;assert(!"unexpected public-I/O capture");return 0;}
ssize_t __posix_eventfd_read(int fd, void *buffer, size_t length) {
    (void)fd; (void)buffer; (void)length;
    assert(!"unexpected eventfd read");
    return -1;
}
ssize_t __posix_eventfd_write(int fd, const void *buffer, size_t length) {
    (void)fd; (void)buffer; (void)length;
    assert(!"unexpected eventfd write");
    return -1;
}
int __posix_socket_is_pseudo(int fd) {
    return fd == 1000;
}
int __posix_socket_real_fd(int fd) {
    return fd == 1000 ? 7 : fd;
}

uint64_t __posix_event_capture_identity(int fd) {
    captures++;
    return identities[fd];
}
int __posix_event_identity_current(int fd, uint64_t identity) {
    return identity != 0 && identities[fd] == identity;
}

int __posix_signal_dispatch_pending_restartable(void) {
    dispatches++;
    if (scenario == RESTART_REPLACED_FD) identities[7]++;
    if (scenario == RESTART_REPLACED_PSEUDO) identities[1000]++;
    errno = EDOM;
    return scenario != SIGNAL_WITHOUT_RESTART;
}
int __posix_signal_dispatch_pending(void) {
    dispatches++;
    if (scenario == SOCKET_ERROR_WITHOUT_SIGNAL) return 0;
    /* Model a handler replacing a socket with a different pending error. */
    socket_error = EIO;
    errno = EDOM;
    return 1;
}

static long operation(void) {
    calls++;
    if (scenario == ERROR_WITH_SIGNAL) return SRV_ERR_FAULT;
    if (scenario == SOCKET_ERROR_WITH_SIGNAL || scenario == SOCKET_ERROR_WITHOUT_SIGNAL)
        return SRV_ERR_BADF;
    if (scenario == PARTIAL_WRITE) return 2;
    return calls == 1 ? SRV_ERR_INTR : 3;
}
long srv_read(int fd, void *buffer, size_t length) {
    assert(fd == 7);
    (void)buffer; (void)length;
    return operation();
}
long srv_write(int fd, const void *buffer, size_t length) {
    assert(fd == 7);
    (void)buffer; (void)length;
    return operation();
}
long srv_syscall5(long operation_number, long fd, long iov, long count,
    long kind, long offset) {
    (void)operation_number; (void)iov; (void)count; (void)kind; (void)offset;
    assert(fd == 7);
    return operation();
}
long srv_fcntl(int fd, int command, uint64_t argument) {
    assert(fd == 7 && command == SRV_F_SETLKW);
    const struct srv_flock *lock = (const struct srv_flock *)argument;
    assert(lock->type == SRV_F_WRLCK);
    calls++;
    return calls == 1 ? SRV_ERR_INTR : 0;
}

/* Remaining fcntl branches are linked but are not exercised in this fixture. */
void __posix_event_lock(void) {
    assert(!"unexpected lifecycle lock");
}
void __posix_event_unlock(void) {
    assert(!"unexpected lifecycle unlock");
}
int __posix_event_prepare_dup(int fd) {
    (void)fd;
    assert(!"unexpected duplication");
    return 0;
}
void __posix_event_note_dup(int old_fd, int new_fd) {
    (void)old_fd; (void)new_fd;
    assert(!"unexpected duplication");
}
void __posix_socket_note_dup(int old_fd, int new_fd) {
    (void)old_fd; (void)new_fd;
    assert(!"unexpected duplication");
}
int __posix_socket_fcntl(int fd, int command, uint64_t flags) {
    (void)fd; (void)command; (void)flags;
    assert(!"unexpected pseudo fcntl");
    return -1;
}
int __posix_eventfd_fcntl(int fd, int command, uint64_t flags) {
    (void)fd; (void)command; (void)flags;
    assert(!"unexpected pseudo fcntl");
    return -1;
}

static void reset(enum scenario next) {
    scenario = next;
    calls = dispatches = captures = 0;
    identities[7] = 1;
    identities[1000] = 2;
    socket_error = ECONNRESET;
    errno = 0;
}

int main(void) {
    char buffer[4];
    struct iovec iov = {buffer, sizeof(buffer)};
    struct test_flock lock = {.type = TEST_F_WRLCK};
    int saved_error = 0;

    reset(RESTART_SAME_FD);
    assert(audit_read(7, buffer, sizeof(buffer)) == 3);
    assert(calls == 2 && dispatches == 2 && captures == 1);

    reset(RESTART_REPLACED_FD);
    assert(audit_read(7, buffer, sizeof(buffer)) == -1);
    assert(errno == EBADF && calls == 1);

    reset(RESTART_REPLACED_FD);
    assert(audit_write(7, buffer, sizeof(buffer)) == -1);
    assert(errno == EBADF && calls == 1);

    reset(RESTART_REPLACED_FD);
    assert(audit_writev(7, &iov, 1) == -1);
    assert(errno == EBADF && calls == 1);

    reset(SIGNAL_WITHOUT_RESTART);
    assert(audit_write(7, buffer, sizeof(buffer)) == -1);
    assert(errno == EINTR && calls == 1);

    reset(ERROR_WITH_SIGNAL);
    assert(audit_read(7, buffer, sizeof(buffer)) == -1);
    assert(errno == EINTR && calls == 1 && captures == 0);

    reset(RESTART_REPLACED_PSEUDO);
    assert(audit_read(1000, buffer, sizeof(buffer)) == -1);
    assert(errno == EBADF && calls == 1 && captures == 2);

    reset(RESTART_REPLACED_FD);
    assert(audit_readv(7, &iov, 1) == -1);
    assert(errno == EBADF && calls == 1);

    reset(RESTART_REPLACED_FD);
    assert(audit_fcntl(7, TEST_F_SETLKW, &lock) == -1);
    assert(errno == EBADF && calls == 1 && captures == 1);

    reset(RESTART_SAME_FD);
    assert(audit_fcntl(7, TEST_F_SETLKW, &lock) == 0);
    assert(calls == 2 && captures == 1);

    reset(PARTIAL_WRITE);
    assert(audit_write(7, buffer, sizeof(buffer)) == 2);
    assert(calls == 1 && captures == 0);

    reset(SOCKET_ERROR_WITHOUT_SIGNAL);
    assert(__posix_write_with_socket_error(7, buffer, sizeof(buffer), &saved_error) == -1);
    assert(errno == EBADF && saved_error == ECONNRESET && socket_error == ECONNRESET);

    saved_error = 0;
    reset(SOCKET_ERROR_WITH_SIGNAL);
    assert(__posix_write_with_socket_error(7, buffer, sizeof(buffer), &saved_error) == -1);
    assert(errno == EBADF && saved_error == ECONNRESET && socket_error == EIO);

    puts("fd-io-restart: PASS restart, replacement, lock, signal and partial I/O");
    return 0;
}
