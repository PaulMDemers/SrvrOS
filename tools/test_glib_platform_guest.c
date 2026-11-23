/* Native prerequisites for the GLib main context and desktop wait bridge. */
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/eventfd.h>
#include <sys/socket.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

#include <srvros/gui.h>

int gui_wait_capture(void);

#define CHECK(x) do { \
    if (!(x)) { \
        printf("glib-platform: failure line=%d errno=%d\n", __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int channel[2];
static int release_pipe[2];
static atomic_int worker_error;

static int64_t monotonic_ms(void) {
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) return -1;
    return (int64_t)t.tv_sec * 1000 + t.tv_nsec / 1000000;
}

static void *worker(void *unused) {
    (void)unused;
    const char first = 'a', second = 'b';
    if (write(channel[1], &first, 1) != 1) { atomic_store(&worker_error, 1); return 0; }
    struct pollfd release = { .fd = release_pipe[0], .events = POLLIN };
    char signal;
    if (poll(&release, 1, 5000) != 1 || !(release.revents & POLLIN) ||
        read(release_pipe[0], &signal, 1) != 1 || signal != 'r') {
        atomic_store(&worker_error, 1);
        return 0;
    }
    if (write(channel[1], &second, 1) != 1) atomic_store(&worker_error, 1);
    return 0;
}

static int wake_and_drain(char expected) {
    struct pollfd p = { .fd = channel[0], .events = POLLIN };
    if (poll(&p, 1, 3000) != 1 || !(p.revents & POLLIN)) return 0;
    char c;
    if (read(channel[0], &c, 1) != 1 || c != expected) return 0;
    errno = 0;
    return read(channel[0], &c, 1) == -1 && errno == EAGAIN;
}

static int refuse_gui_interest(int fd) {
    /* The kernel validates descriptors before consulting the GUI peer. */
    struct srv_gui_wait request = {
        .abi_version = SRV_GUI_WAIT_VERSION,
        .struct_size = sizeof(request),
        .server = 1,
        .count = 1,
        .fds = {{.fd = fd, .events = POLLIN}},
    };
    return gui_wait_io(&request) < 0;
}

int main(void) {
    printf("glib-platform: start\n");
    enum { host_name_max = 63 };
    CHECK(sysconf(_SC_HOST_NAME_MAX) == host_name_max);
    struct utsname system_name;
    char hostname[host_name_max + 1];
    memset(hostname, 0x5a, sizeof(hostname));
    CHECK(uname(&system_name) == 0);
    CHECK(gethostname(hostname, sizeof(hostname)) == 0);
    CHECK(strcmp(hostname, "srvros") == 0 && strcmp(hostname, system_name.nodename) == 0);
    char small[] = {'A', 'B', 'C'};
    CHECK(gethostname(&small[1], 1) == 0 && small[0] == 'A' && small[1] == '\0' && small[2] == 'C');
    errno = 0;
    CHECK(gethostname(&small[1], 0) == -1 && errno == EINVAL && small[0] == 'A' && small[1] == '\0' && small[2] == 'C');
    CHECK(pipe2(channel, O_NONBLOCK | O_CLOEXEC) == 0);
    CHECK(pipe2(release_pipe, O_NONBLOCK | O_CLOEXEC) == 0);
    for (int i = 0; i != 2; ++i) {
        int status = fcntl(channel[i], F_GETFL);
        int desc = fcntl(channel[i], F_GETFD);
        CHECK(status >= 0 && (status & O_NONBLOCK));
        CHECK(desc >= 0 && (desc & FD_CLOEXEC));
    }
    pthread_t thread;
    CHECK(pthread_create(&thread, 0, worker, 0) == 0);
    CHECK(wake_and_drain('a'));
    struct pollfd p = { .fd = channel[0], .events = POLLIN };
    int64_t start = monotonic_ms();
    CHECK(start >= 0);
    CHECK(poll(&p, 1, 30) == 0 && p.revents == 0);
    int64_t end = monotonic_ms();
    CHECK(end >= start && end - start >= 20 && end - start < 3000);
    const char release = 'r';
    CHECK(write(release_pipe[1], &release, 1) == 1);
    CHECK(wake_and_drain('b'));
    CHECK(pthread_join(thread, 0) == 0 && !atomic_load(&worker_error));
    CHECK(close(channel[0]) == 0 && close(channel[1]) == 0);
    CHECK(close(release_pipe[0]) == 0 && close(release_pipe[1]) == 0);
    p.fd = channel[0]; p.revents = 0;
    CHECK(poll(&p, 1, 0) == 1 && (p.revents & POLLNVAL));

    int event = eventfd(0, EFD_NONBLOCK | EFD_CLOEXEC);
    CHECK(event >= 0);
    CHECK(refuse_gui_interest(event));
    p.fd = event; p.events = POLLIN; p.revents = 0;
    CHECK(poll(&p, 1, 0) == 0);
    uint64_t value = 0;
    errno = 0;
    CHECK(read(event, &value, sizeof(value)) == -1 && errno == EAGAIN);
    value = 3;
    CHECK(write(event, &value, sizeof(value)) == (ssize_t)sizeof(value));
    CHECK(poll(&p, 1, 0) == 1 && (p.revents & POLLIN));
    value = 0;
    CHECK(read(event, &value, sizeof(value)) == (ssize_t)sizeof(value) && value == 3);
    p.revents = 0;
    CHECK(poll(&p, 1, 0) == 0);
    value = UINT64_MAX;
    errno = 0;
    CHECK(write(event, &value, sizeof(value)) == -1 && errno == EINVAL);
    CHECK(close(event) == 0);

    int socket_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
    CHECK(socket_fd >= 0);
    CHECK(refuse_gui_interest(socket_fd));
    CHECK(close(socket_fd) == 0);

    CHECK(dlopen(NULL, RTLD_NOW) == NULL && errno == ENOSYS && dlerror());
    CHECK(gui_wait_capture() == 0);
    printf("glib-platform: ok\n");
    return 0;
}
