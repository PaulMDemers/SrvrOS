#include "uv.h"

#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/random.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/wait.h>
#include <termios.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#include <srvros/sys.h>

#define UV_MAX_POLL_HANDLES 16
#define UV_HANDLE_TIMER UV_TIMER
#define UV_HANDLE_TCP UV_TCP
#define UV_HANDLE_UDP UV_UDP
#define UV_HANDLE_POLL UV_POLL
#define UV_HANDLE_ASYNC UV_ASYNC
#define UV_HANDLE_PREPARE UV_PREPARE
#define UV_HANDLE_CHECK UV_CHECK
#define UV_HANDLE_IDLE UV_IDLE
#define UV_HANDLE_PIPE UV_NAMED_PIPE
#define UV_HANDLE_PROCESS UV_PROCESS
#define UV_HANDLE_TTY UV_TTY
#define UV_HANDLE_SIGNAL UV_SIGNAL
#define UV_MAX_STDIO_HANDLES 32
#define UV_SCANDIR_INITIAL_CAPACITY 8
#define UV_REALPATH_MAX 160
#define UV_WORKER_POOL_SIZE 4

static uv_loop_t default_loop;
static int default_loop_ready;
int __posix_make_path(const char *path, char *out, size_t capacity);

static int next_timer_timeout(const uv_loop_t *loop);
static int uv_scandir_append(uv_fs_t *request, const struct dirent *entry);
static int uv_error_from_errno(void);
static void set_nonblocking(int fd);
static void fs_queue_done(uv_loop_t *loop, uv_fs_t *request);
static int uv_signal_supported(int signum);
static int random_fill(void *buffer, size_t buffer_length);

struct uv_worker_job {
    void (*run)(void *arg);
    void *arg;
    struct uv_worker_job *next;
};

static pthread_once_t worker_pool_once = PTHREAD_ONCE_INIT;
static pthread_mutex_t worker_pool_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t worker_pool_cond = PTHREAD_COND_INITIALIZER;
static pthread_t worker_pool_threads[UV_WORKER_POOL_SIZE];
static struct uv_worker_job *worker_pool_head;
static struct uv_worker_job *worker_pool_tail;
static uv_tty_vtermstate_t tty_vterm_state = UV_TTY_UNSUPPORTED;
static int tty_reset_fd = -1;
static struct termios tty_reset_termios;
static int signal_watch_counts[64];

static uint64_t monotonic_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000ull + (uint64_t)ts.tv_nsec / 1000000ull;
}

static void sleep_ms(uint64_t ms) {
    struct timespec ts;
    ts.tv_sec = (time_t)(ms / 1000u);
    ts.tv_nsec = (long)((ms % 1000u) * 1000000u);
    (void)nanosleep(&ts, 0);
}

static void loop_wakeup(uv_loop_t *loop) {
    if (loop == 0 || loop->wake_write_fd < 0) {
        return;
    }
    char byte = 1;
    ssize_t written = write(loop->wake_write_fd, &byte, 1);
    (void)written;
}

static void loop_drain_wakeup(uv_loop_t *loop) {
    if (loop == 0 || loop->wake_read_fd < 0) {
        return;
    }
    char buffer[32];
    while (read(loop->wake_read_fd, buffer, sizeof(buffer)) > 0) {
    }
}

static int loop_ensure_wakeup(uv_loop_t *loop) {
    if (loop == 0) {
        return UV_EINVAL;
    }
    if (loop->wake_read_fd >= 0 && loop->wake_write_fd >= 0) {
        return 0;
    }
    int wake_fds[2];
    if (pipe(wake_fds) < 0) {
        return uv_error_from_errno();
    }
    loop->wake_read_fd = wake_fds[0];
    loop->wake_write_fd = wake_fds[1];
    set_nonblocking(loop->wake_read_fd);
    set_nonblocking(loop->wake_write_fd);
    return 0;
}

static void *worker_pool_thread(void *arg) {
    (void)arg;
    for (;;) {
        pthread_mutex_lock(&worker_pool_mutex);
        while (worker_pool_head == 0) {
            pthread_cond_wait(&worker_pool_cond, &worker_pool_mutex);
        }
        struct uv_worker_job *job = worker_pool_head;
        worker_pool_head = job->next;
        if (worker_pool_head == 0) {
            worker_pool_tail = 0;
        }
        pthread_mutex_unlock(&worker_pool_mutex);

        job->run(job->arg);
        free(job);
    }
    return 0;
}

static void worker_pool_init(void) {
    for (size_t i = 0; i < UV_WORKER_POOL_SIZE; i++) {
        if (pthread_create(&worker_pool_threads[i], 0, worker_pool_thread, 0) == 0) {
            (void)pthread_detach(worker_pool_threads[i]);
        }
    }
}

static int worker_pool_submit(void (*run)(void *arg), void *arg) {
    if (run == 0) {
        return UV_EINVAL;
    }
    pthread_once(&worker_pool_once, worker_pool_init);
    struct uv_worker_job *job = malloc(sizeof(*job));
    if (job == 0) {
        return UV_ENOMEM;
    }
    job->run = run;
    job->arg = arg;
    job->next = 0;

    pthread_mutex_lock(&worker_pool_mutex);
    if (worker_pool_tail != 0) {
        worker_pool_tail->next = job;
    } else {
        worker_pool_head = job;
    }
    worker_pool_tail = job;
    pthread_cond_signal(&worker_pool_cond);
    pthread_mutex_unlock(&worker_pool_mutex);
    return 0;
}

static int worker_pool_cancel(void *arg) {
    int canceled = 0;
    pthread_mutex_lock(&worker_pool_mutex);
    struct uv_worker_job **link = &worker_pool_head;
    while (*link != 0) {
        struct uv_worker_job *job = *link;
        if (job->arg == arg) {
            *link = job->next;
            if (worker_pool_tail == job) {
                worker_pool_tail = 0;
                for (struct uv_worker_job *tail = worker_pool_head; tail != 0; tail = tail->next) {
                    if (tail->next == 0) {
                        worker_pool_tail = tail;
                        break;
                    }
                }
            }
            free(job);
            canceled = 1;
            break;
        }
        link = &job->next;
    }
    pthread_mutex_unlock(&worker_pool_mutex);
    return canceled;
}

static int uv_error_from_errno(void) {
    return uv_translate_sys_error(errno);
}

static void set_nonblocking(int fd) {
    int flags;
    if (fd < 0) {
        return;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags | O_NONBLOCK);
    }
}

static void set_blocking(int fd) {
    int flags;
    if (fd < 0) {
        return;
    }
    flags = fcntl(fd, F_GETFL, 0);
    if (flags >= 0) {
        (void)fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
}

static void register_handle(uv_loop_t *loop, uv_handle_t *handle) {
    if (handle->loop == loop) {
        return;
    }
    handle->loop = loop;
    handle->next = loop->handles;
    loop->handles = handle;
}

static void init_handle(uv_loop_t *loop, uv_handle_t *handle, int fd, int type) {
    memset(handle, 0, sizeof(*handle));
    handle->fd = fd;
    handle->type = type;
    handle->referenced = 1;
    handle->loop = 0;
    register_handle(loop, handle);
}

uv_loop_t *uv_default_loop(void) {
    if (!default_loop_ready) {
        (void)uv_loop_init(&default_loop);
        default_loop_ready = 1;
    }
    return &default_loop;
}

int uv_loop_init(uv_loop_t *loop) {
    if (loop == 0) {
        return -EINVAL;
    }
    memset(loop, 0, sizeof(*loop));
    loop->wake_read_fd = -1;
    loop->wake_write_fd = -1;
    if (pthread_mutex_init(&loop->queue_mutex, 0) != 0) {
        return UV_ENOMEM;
    }
    loop->now_ms = monotonic_ms();
    return 0;
}

int uv_loop_close(uv_loop_t *loop) {
    if (loop == 0) {
        return UV_EINVAL;
    }
    if (uv_loop_alive(loop)) {
        return UV_EBUSY;
    }
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        if (!handle->closing) {
            return UV_EBUSY;
        }
    }
    if (loop->wake_read_fd >= 0) {
        close(loop->wake_read_fd);
    }
    if (loop->wake_write_fd >= 0) {
        close(loop->wake_write_fd);
    }
    pthread_mutex_destroy(&loop->queue_mutex);
    memset(loop, 0, sizeof(*loop));
    loop->wake_read_fd = -1;
    loop->wake_write_fd = -1;
    return 0;
}

void *uv_loop_get_data(const uv_loop_t *loop) {
    return loop != 0 ? loop->data : 0;
}

void uv_loop_set_data(uv_loop_t *loop, void *data) {
    if (loop != 0) {
        loop->data = data;
    }
}

int uv_backend_fd(const uv_loop_t *loop) {
    if (loop == 0) {
        return UV_EINVAL;
    }
    int status = loop_ensure_wakeup((uv_loop_t *)loop);
    return status < 0 ? status : loop->wake_read_fd;
}

int uv_backend_timeout(const uv_loop_t *loop) {
    if (loop == 0) {
        return 0;
    }
    return next_timer_timeout(loop);
}

void uv_update_time(uv_loop_t *loop) {
    if (loop != 0) {
        loop->now_ms = monotonic_ms();
    }
}

uint64_t uv_now(const uv_loop_t *loop) {
    return loop != 0 ? loop->now_ms : monotonic_ms();
}

void uv_stop(uv_loop_t *loop) {
    if (loop != 0) {
        loop->stop_flag = 1;
        loop_wakeup(loop);
    }
}

int uv_loop_alive(const uv_loop_t *loop) {
    if (loop == 0) {
        return 0;
    }
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        if (handle->active && handle->referenced && !handle->closing) {
            return 1;
        }
    }
    for (uv_work_t *work = loop->work_queue; work != 0; work = work->next) {
        return 1;
    }
    pthread_mutex_lock((pthread_mutex_t *)&loop->queue_mutex);
    int has_fs = loop->fs_queue != 0 || loop->pending_fs_count != 0;
    pthread_mutex_unlock((pthread_mutex_t *)&loop->queue_mutex);
    if (has_fs) {
        return 1;
    }
    if (loop->getaddrinfo_queue != 0) {
        return 1;
    }
    if (loop->random_queue != 0) {
        return 1;
    }
    return 0;
}

uv_buf_t uv_buf_init(char *base, unsigned int length) {
    uv_buf_t buffer;
    buffer.base = base;
    buffer.len = length;
    return buffer;
}

size_t uv_handle_size(uv_handle_type type) {
    switch (type) {
        case UV_ASYNC:
            return sizeof(uv_async_t);
        case UV_POLL:
            return sizeof(uv_poll_t);
        case UV_NAMED_PIPE:
            return sizeof(uv_pipe_t);
        case UV_PROCESS:
            return sizeof(uv_process_t);
        case UV_PREPARE:
            return sizeof(uv_prepare_t);
        case UV_CHECK:
            return sizeof(uv_check_t);
        case UV_IDLE:
            return sizeof(uv_idle_t);
        case UV_TCP:
            return sizeof(uv_tcp_t);
        case UV_TIMER:
            return sizeof(uv_timer_t);
        case UV_TTY:
            return sizeof(uv_tty_t);
        case UV_UDP:
            return sizeof(uv_udp_t);
        case UV_SIGNAL:
            return sizeof(uv_signal_t);
        default:
            return (size_t)-1;
    }
}

const char *uv_handle_type_name(uv_handle_type type) {
    switch (type) {
        case UV_ASYNC:
            return "async";
        case UV_POLL:
            return "poll";
        case UV_NAMED_PIPE:
            return "pipe";
        case UV_PROCESS:
            return "process";
        case UV_PREPARE:
            return "prepare";
        case UV_CHECK:
            return "check";
        case UV_IDLE:
            return "idle";
        case UV_TCP:
            return "tcp";
        case UV_TIMER:
            return "timer";
        case UV_TTY:
            return "tty";
        case UV_UDP:
            return "udp";
        case UV_SIGNAL:
            return "signal";
        case UV_FILE:
            return "file";
        default:
            return 0;
    }
}

uv_handle_type uv_handle_get_type(const uv_handle_t *handle) {
    return handle != 0 ? (uv_handle_type)handle->type : UV_UNKNOWN_HANDLE;
}

void *uv_handle_get_data(const uv_handle_t *handle) {
    return handle != 0 ? handle->data : 0;
}

void uv_handle_set_data(uv_handle_t *handle, void *data) {
    if (handle != 0) {
        handle->data = data;
    }
}

uv_loop_t *uv_handle_get_loop(const uv_handle_t *handle) {
    return handle != 0 ? handle->loop : 0;
}

int uv_is_active(const uv_handle_t *handle) {
    return handle != 0 && handle->active && !handle->closing;
}

int uv_is_closing(const uv_handle_t *handle) {
    return handle != 0 && handle->closing;
}

static uv_tcp_t *tcp_from_stream(uv_stream_t *stream);
static const uv_tcp_t *tcp_from_const_stream(const uv_stream_t *stream);
static uv_pipe_t *pipe_from_stream(uv_stream_t *stream);
static const uv_pipe_t *pipe_from_const_stream(const uv_stream_t *stream);
static uv_tty_t *tty_from_stream(uv_stream_t *stream);
static const uv_tty_t *tty_from_const_stream(const uv_stream_t *stream);

void uv_ref(uv_handle_t *handle) {
    if (handle != 0) {
        handle->referenced = 1;
    }
}

void uv_unref(uv_handle_t *handle) {
    if (handle != 0) {
        handle->referenced = 0;
    }
}

int uv_has_ref(const uv_handle_t *handle) {
    return handle != 0 && handle->referenced;
}

void uv_walk(uv_loop_t *loop, uv_walk_cb cb, void *arg) {
    if (loop == 0 || cb == 0) {
        return;
    }
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        cb(handle, arg);
    }
}

int uv_fileno(const uv_handle_t *handle, uv_os_fd_t *fd) {
    if (handle == 0 || fd == 0 || handle->fd < 0 || handle->closing) {
        return UV_EBADF;
    }
    *fd = handle->fd;
    return 0;
}

int uv_is_readable(const uv_stream_t *stream) {
    const uv_tcp_t *tcp = tcp_from_const_stream(stream);
    const uv_pipe_t *pipe_handle = pipe_from_const_stream(stream);
    const uv_tty_t *tty = tty_from_const_stream(stream);
    return (tcp != 0 && !tcp->handle.closing) ||
        (pipe_handle != 0 && !pipe_handle->handle.closing) ||
        (tty != 0 && !tty->handle.closing);
}

int uv_is_writable(const uv_stream_t *stream) {
    const uv_tcp_t *tcp = tcp_from_const_stream(stream);
    const uv_pipe_t *pipe_handle = pipe_from_const_stream(stream);
    const uv_tty_t *tty = tty_from_const_stream(stream);
    return (tcp != 0 && !tcp->handle.closing) ||
        (pipe_handle != 0 && !pipe_handle->handle.closing) ||
        (tty != 0 && !tty->handle.closing);
}

size_t uv_stream_get_write_queue_size(const uv_stream_t *stream) {
    const uv_tcp_t *tcp = tcp_from_const_stream(stream);
    const uv_pipe_t *pipe_handle = pipe_from_const_stream(stream);
    const uv_tty_t *tty = tty_from_const_stream(stream);
    if (tcp != 0) {
        return tcp->write_queue_size;
    }
    if (pipe_handle != 0) {
        return pipe_handle->write_queue_size;
    }
    return tty != 0 ? tty->write_queue_size : 0;
}

static uv_tcp_t *tcp_from_stream(uv_stream_t *stream) {
    uv_tcp_t *tcp = (uv_tcp_t *)stream;
    if (tcp == 0 || tcp->handle.type != UV_HANDLE_TCP) {
        return 0;
    }
    return tcp;
}

static const uv_tcp_t *tcp_from_const_stream(const uv_stream_t *stream) {
    const uv_tcp_t *tcp = (const uv_tcp_t *)stream;
    if (tcp == 0 || tcp->handle.type != UV_HANDLE_TCP) {
        return 0;
    }
    return tcp;
}

static uv_pipe_t *pipe_from_stream(uv_stream_t *stream) {
    uv_pipe_t *pipe_handle = (uv_pipe_t *)stream;
    if (pipe_handle == 0 || pipe_handle->handle.type != UV_HANDLE_PIPE) {
        return 0;
    }
    return pipe_handle;
}

static const uv_pipe_t *pipe_from_const_stream(const uv_stream_t *stream) {
    const uv_pipe_t *pipe_handle = (const uv_pipe_t *)stream;
    if (pipe_handle == 0 || pipe_handle->handle.type != UV_HANDLE_PIPE) {
        return 0;
    }
    return pipe_handle;
}

static uv_tty_t *tty_from_stream(uv_stream_t *stream) {
    uv_tty_t *tty = (uv_tty_t *)stream;
    if (tty == 0 || tty->handle.type != UV_HANDLE_TTY) {
        return 0;
    }
    return tty;
}

static const uv_tty_t *tty_from_const_stream(const uv_stream_t *stream) {
    const uv_tty_t *tty = (const uv_tty_t *)stream;
    if (tty == 0 || tty->handle.type != UV_HANDLE_TTY) {
        return 0;
    }
    return tty;
}

size_t uv_req_size(uv_req_type type) {
    switch (type) {
        case UV_CONNECT:
            return sizeof(uv_connect_t);
        case UV_WRITE:
            return sizeof(uv_write_t);
        case UV_SHUTDOWN:
            return sizeof(uv_shutdown_t);
        case UV_UDP_SEND:
            return sizeof(uv_udp_send_t);
        case UV_FS:
            return sizeof(uv_fs_t);
        case UV_WORK:
            return sizeof(uv_work_t);
        case UV_GETADDRINFO:
            return sizeof(uv_getaddrinfo_t);
        case UV_RANDOM:
            return sizeof(uv_random_t);
        default:
            return (size_t)-1;
    }
}

const char *uv_req_type_name(uv_req_type type) {
    switch (type) {
        case UV_CONNECT:
            return "connect";
        case UV_WRITE:
            return "write";
        case UV_SHUTDOWN:
            return "shutdown";
        case UV_UDP_SEND:
            return "udp_send";
        case UV_FS:
            return "fs";
        case UV_WORK:
            return "work";
        case UV_GETADDRINFO:
            return "getaddrinfo";
        case UV_RANDOM:
            return "random";
        default:
            return 0;
    }
}

uv_req_type uv_req_get_type(const uv_req_t *request) {
    return request != 0 ? (uv_req_type)request->type : UV_UNKNOWN_REQ;
}

void *uv_req_get_data(const uv_req_t *request) {
    return request != 0 ? request->data : 0;
}

void uv_req_set_data(uv_req_t *request, void *data) {
    if (request != 0) {
        request->data = data;
    }
}

int uv_cancel(uv_req_t *request) {
    if (request == 0) {
        return UV_EINVAL;
    }
    if (request->type == UV_WORK) {
        uv_work_t *work = (uv_work_t *)request;
        if (work->done || work->loop == 0 || !worker_pool_cancel(work)) {
            return UV_EBUSY;
        }
        work->status = UV_ECANCELED;
        work->done = 1;
        loop_wakeup(work->loop);
        return 0;
    }
    if (request->type == UV_FS) {
        uv_fs_t *fs = (uv_fs_t *)request;
        if (fs->cb == 0 || fs->loop == 0 || !worker_pool_cancel(fs)) {
            return UV_EBUSY;
        }
        fs->result = UV_ECANCELED;
        fs_queue_done(fs->loop, fs);
        return 0;
    }
    return UV_ENOSYS;
}

uint64_t uv_hrtime(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
        return 0;
    }
    return (uint64_t)ts.tv_sec * 1000000000ull + (uint64_t)ts.tv_nsec;
}

uv_pid_t uv_os_getpid(void) {
    return getpid();
}

uv_pid_t uv_os_getppid(void) {
    return getppid();
}

int uv_os_getenv(const char *name, char *buffer, size_t *size) {
    if (name == 0 || buffer == 0 || size == 0 || name[0] == '\0' || strchr(name, '=') != 0) {
        return UV_EINVAL;
    }
    const char *value = getenv(name);
    if (value == 0) {
        return UV_ENOENT;
    }
    size_t needed = strlen(value) + 1;
    if (*size < needed) {
        *size = needed;
        return UV_ENOBUFS;
    }
    memcpy(buffer, value, needed);
    *size = needed - 1;
    return 0;
}

int uv_os_setenv(const char *name, const char *value) {
    if (name == 0 || value == 0 || name[0] == '\0' || strchr(name, '=') != 0) {
        return UV_EINVAL;
    }
    return setenv(name, value, 1) < 0 ? uv_error_from_errno() : 0;
}

int uv_os_unsetenv(const char *name) {
    if (name == 0 || name[0] == '\0' || strchr(name, '=') != 0) {
        return UV_EINVAL;
    }
    return unsetenv(name) < 0 ? uv_error_from_errno() : 0;
}

static int copy_sized_string(char *buffer, size_t *size, const char *value) {
    if (buffer == 0 || size == 0 || value == 0) {
        return UV_EINVAL;
    }
    size_t length = strlen(value);
    if (*size <= length) {
        *size = length + 1;
        return UV_ENOBUFS;
    }
    memcpy(buffer, value, length + 1);
    *size = length;
    return 0;
}

int uv_exepath(char *buffer, size_t *size) {
    if (buffer == 0 || size == 0) {
        return UV_EINVAL;
    }
    char path[UV_REALPATH_MAX];
    long length = srv_exepath(path, sizeof(path));
    if (length < 0) {
        const char *fallback = getenv("_");
        if (fallback == 0 || fallback[0] == '\0') {
            fallback = "/fat/bin/srvros-app";
        }
        return copy_sized_string(buffer, size, fallback);
    }
    return copy_sized_string(buffer, size, path);
}

int uv_cwd(char *buffer, size_t *size) {
    if (buffer == 0 || size == 0) {
        return UV_EINVAL;
    }
    char cwd[UV_REALPATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) == 0) {
        return uv_error_from_errno();
    }
    return copy_sized_string(buffer, size, cwd);
}

int uv_chdir(const char *dir) {
    if (dir == 0) {
        return UV_EINVAL;
    }
    return chdir(dir) < 0 ? uv_error_from_errno() : 0;
}

uint64_t uv_get_free_memory(void) {
    struct srv_meminfo info;
    return srv_meminfo(&info) < 0 ? 0 : info.free_bytes;
}

uint64_t uv_get_total_memory(void) {
    struct srv_meminfo info;
    return srv_meminfo(&info) < 0 ? 0 : info.total_bytes;
}

static int random_fill(void *buffer, size_t buffer_length) {
    ssize_t count = getrandom(buffer, buffer_length, 0);
    return count == (ssize_t)buffer_length ? 0 : uv_error_from_errno();
}

int uv_random(uv_loop_t *loop,
    uv_random_t *request,
    void *buffer,
    size_t buffer_length,
    unsigned int flags,
    uv_random_cb cb) {
    if (buffer_length > 0x7fffffffu) {
        return UV_E2BIG;
    }
    if (buffer == 0 || flags != 0 || (cb != 0 && (loop == 0 || request == 0))) {
        return UV_EINVAL;
    }
    if (cb == 0) {
        int status = random_fill(buffer, buffer_length);
        if (request != 0) {
            memset(request, 0, sizeof(*request));
            request->type = UV_RANDOM;
            request->buffer = buffer;
            request->buffer_length = buffer_length;
            request->status = status;
        }
        return status;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_RANDOM;
    request->loop = loop;
    request->cb = cb;
    request->buffer = buffer;
    request->buffer_length = buffer_length;
    request->status = random_fill(buffer, buffer_length);
    request->next = loop->random_queue;
    loop->random_queue = request;
    return 0;
}

int uv_timer_init(uv_loop_t *loop, uv_timer_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_TIMER);
    return 0;
}

int uv_timer_start(uv_timer_t *handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat) {
    if (handle == 0 || cb == 0 || handle->handle.loop == 0) {
        return -EINVAL;
    }
    uv_update_time(handle->handle.loop);
    handle->timer_cb = cb;
    handle->timeout_ms = handle->handle.loop->now_ms + timeout;
    handle->repeat_ms = repeat;
    handle->handle.active = 1;
    return 0;
}

int uv_timer_stop(uv_timer_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->handle.active = 0;
    return 0;
}

int uv_timer_again(uv_timer_t *handle) {
    if (handle == 0 || handle->timer_cb == 0 || handle->repeat_ms == 0 || handle->handle.loop == 0) {
        return -EINVAL;
    }
    return uv_timer_start(handle, handle->timer_cb, handle->repeat_ms, handle->repeat_ms);
}

void uv_timer_set_repeat(uv_timer_t *handle, uint64_t repeat) {
    if (handle != 0) {
        handle->repeat_ms = repeat;
    }
}

uint64_t uv_timer_get_repeat(const uv_timer_t *handle) {
    return handle != 0 ? handle->repeat_ms : 0;
}

uint64_t uv_timer_get_due_in(const uv_timer_t *handle) {
    uv_loop_t *loop;
    if (handle == 0 || handle->handle.loop == 0 || !handle->handle.active) {
        return 0;
    }
    loop = handle->handle.loop;
    uv_update_time(loop);
    if (handle->timeout_ms <= loop->now_ms) {
        return 0;
    }
    return handle->timeout_ms - loop->now_ms;
}

int uv_prepare_init(uv_loop_t *loop, uv_prepare_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_PREPARE);
    return 0;
}

int uv_prepare_start(uv_prepare_t *handle, uv_prepare_cb cb) {
    if (handle == 0 || cb == 0) {
        return -EINVAL;
    }
    handle->prepare_cb = cb;
    handle->handle.active = 1;
    return 0;
}

int uv_prepare_stop(uv_prepare_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->handle.active = 0;
    return 0;
}

int uv_check_init(uv_loop_t *loop, uv_check_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_CHECK);
    return 0;
}

int uv_check_start(uv_check_t *handle, uv_check_cb cb) {
    if (handle == 0 || cb == 0) {
        return -EINVAL;
    }
    handle->check_cb = cb;
    handle->handle.active = 1;
    return 0;
}

int uv_check_stop(uv_check_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->handle.active = 0;
    return 0;
}

int uv_idle_init(uv_loop_t *loop, uv_idle_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_IDLE);
    return 0;
}

int uv_idle_start(uv_idle_t *handle, uv_idle_cb cb) {
    if (handle == 0 || cb == 0) {
        return -EINVAL;
    }
    handle->idle_cb = cb;
    handle->handle.active = 1;
    return 0;
}

int uv_idle_stop(uv_idle_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->handle.active = 0;
    return 0;
}

static int next_timer_timeout(const uv_loop_t *loop) {
    uint64_t best = UINT64_MAX;
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_timer_t *timer = (uv_timer_t *)base;
        if (base->type == UV_HANDLE_TIMER && base->active && !base->closing && timer->timer_cb != 0) {
            if (timer->timeout_ms <= loop->now_ms) {
                return 0;
            }
            if (timer->timeout_ms < best) {
                best = timer->timeout_ms;
            }
        }
    }
    if (best == UINT64_MAX) {
        return -1;
    }
    uint64_t delay = best - loop->now_ms;
    return delay > 1000u ? 1000 : (int)delay;
}

static void run_due_timers(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_timer_t *timer = (uv_timer_t *)base;
        if (base->type != UV_HANDLE_TIMER || !base->active || base->closing || timer->timer_cb == 0) {
            continue;
        }
        if (timer->timeout_ms > loop->now_ms) {
            continue;
        }
        uv_timer_cb cb = timer->timer_cb;
        if (timer->repeat_ms != 0) {
            timer->timeout_ms = loop->now_ms + timer->repeat_ms;
        } else {
            base->active = 0;
        }
        cb(timer);
    }
}

static void run_idle_handles(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_idle_t *idle = (uv_idle_t *)base;
        if (base->type == UV_HANDLE_IDLE && base->active && !base->closing && idle->idle_cb != 0) {
            idle->idle_cb(idle);
        }
    }
}

static void run_prepare_handles(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_prepare_t *prepare = (uv_prepare_t *)base;
        if (base->type == UV_HANDLE_PREPARE && base->active && !base->closing && prepare->prepare_cb != 0) {
            prepare->prepare_cb(prepare);
        }
    }
}

static void run_check_handles(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_check_t *check = (uv_check_t *)base;
        if (base->type == UV_HANDLE_CHECK && base->active && !base->closing && check->check_cb != 0) {
            check->check_cb(check);
        }
    }
}

static int has_active_phase(const uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        if (base->active && !base->closing &&
            (base->type == UV_HANDLE_IDLE || base->type == UV_HANDLE_PREPARE || base->type == UV_HANDLE_CHECK)) {
            return 1;
        }
    }
    return 0;
}

static void run_pending_async(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_async_t *async = (uv_async_t *)base;
        if (base->type != UV_HANDLE_ASYNC || !base->active || base->closing || async->async_cb == 0 ||
            !async->pending) {
            continue;
        }
        async->pending = 0;
        async->async_cb(async);
    }
}

static void run_done_work(uv_loop_t *loop) {
    uv_work_t **link = &loop->work_queue;
    while (*link != 0) {
        uv_work_t *work = *link;
        if (!work->done) {
            link = &work->next;
            continue;
        }
        *link = work->next;
        work->next = 0;
        if (work->after_work_cb != 0) {
            work->after_work_cb(work, work->status);
        }
    }
}

static void run_done_fs(uv_loop_t *loop) {
    pthread_mutex_lock(&loop->queue_mutex);
    uv_fs_t *request = loop->fs_queue;
    loop->fs_queue = 0;
    pthread_mutex_unlock(&loop->queue_mutex);
    while (request != 0) {
        uv_fs_t *next = request->next;
        request->next = 0;
        if (request->cb != 0) {
            request->cb(request);
        }
        pthread_mutex_lock(&loop->queue_mutex);
        if (loop->pending_fs_count > 0) {
            loop->pending_fs_count--;
        }
        pthread_mutex_unlock(&loop->queue_mutex);
        request = next;
    }
}

static void run_process_exits(uv_loop_t *loop) {
    for (uv_handle_t *base = loop->handles; base != 0; base = base->next) {
        uv_process_t *process = (uv_process_t *)base;
        int status = 0;
        pid_t result;
        if (base->type != UV_HANDLE_PROCESS || !base->active || base->closing || process->pid <= 0) {
            continue;
        }
        result = waitpid(process->pid, &status, WNOHANG);
        if (result <= 0) {
            continue;
        }
        base->active = 0;
        process->exit_status = WIFEXITED(status) ? WEXITSTATUS(status) : status;
        process->term_signal = WIFSIGNALED(status) ? WTERMSIG(status) : 0;
        if (process->exit_cb != 0) {
            process->exit_cb(process, process->exit_status, process->term_signal);
        }
    }
}

static void run_done_getaddrinfo(uv_loop_t *loop) {
    uv_getaddrinfo_t *request = loop->getaddrinfo_queue;
    loop->getaddrinfo_queue = 0;
    while (request != 0) {
        uv_getaddrinfo_t *next = request->next;
        request->next = 0;
        if (request->cb != 0) {
            request->cb(request, request->status, request->status == 0 ? request->result : 0);
        }
        request = next;
    }
}

static void run_done_random(uv_loop_t *loop) {
    uv_random_t *request = loop->random_queue;
    loop->random_queue = 0;
    while (request != 0) {
        uv_random_t *next = request->next;
        request->next = 0;
        if (request->cb != 0) {
            request->cb(request, request->status, request->buffer, request->buffer_length);
        }
        request = next;
    }
}

static short poll_events_for_handle(uv_handle_t *handle) {
    if (handle->fd < 0 || !handle->active || handle->closing) {
        return 0;
    }
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        short events = 0;
        if (tcp->connection_cb != 0 || tcp->read_cb != 0) {
            events |= POLLIN;
        }
        if (tcp->connect_req != 0 || tcp->write_queue_head != 0 || tcp->shutdown_req != 0) {
            events |= POLLOUT;
        }
        return events;
    }
    if (handle->type == UV_HANDLE_PIPE) {
        uv_pipe_t *pipe_handle = (uv_pipe_t *)handle;
        short events = 0;
        if (pipe_handle->read_cb != 0) {
            events |= POLLIN;
        }
        if (pipe_handle->write_queue_head != 0) {
            events |= POLLOUT;
        }
        return events;
    }
    if (handle->type == UV_HANDLE_TTY) {
        uv_tty_t *tty = (uv_tty_t *)handle;
        short events = 0;
        if (tty->read_cb != 0) {
            events |= POLLIN;
        }
        if (tty->write_queue_head != 0) {
            events |= POLLOUT;
        }
        return events;
    }
    if (handle->type == UV_HANDLE_UDP) {
        uv_udp_t *udp = (uv_udp_t *)handle;
        return udp->recv_cb != 0 ? POLLIN : 0;
    }
    if (handle->type == UV_HANDLE_POLL) {
        uv_poll_t *poll_handle = (uv_poll_t *)handle;
        short events = 0;
        if ((poll_handle->events & UV_READABLE) != 0) {
            events |= POLLIN;
        }
        if ((poll_handle->events & UV_WRITABLE) != 0) {
            events |= POLLOUT;
        }
        return poll_handle->poll_cb != 0 ? events : 0;
    }
    return 0;
}

static int socket_pending_error(int error) {
    return error == EINPROGRESS || error == EALREADY || error == EAGAIN || error == EWOULDBLOCK;
}

static int tcp_socket_error(uv_tcp_t *tcp) {
    int error = 0;
    socklen_t length = sizeof(error);
    if (tcp == 0 || tcp->handle.fd < 0) {
        return EBADF;
    }
    if (getsockopt(tcp->handle.fd, SOL_SOCKET, SO_ERROR, &error, &length) < 0) {
        return errno;
    }
    return error;
}

static void update_tcp_active(uv_tcp_t *tcp) {
    if (tcp == 0 || tcp->handle.closing) {
        return;
    }
    tcp->handle.active = tcp->connection_cb != 0 ||
        tcp->read_cb != 0 ||
        tcp->connect_req != 0 ||
        tcp->shutdown_req != 0 ||
        tcp->write_queue_head != 0;
}

static void dispatch_tcp_shutdown(uv_tcp_t *tcp) {
    uv_shutdown_t *request;
    uv_shutdown_cb cb;
    int status = 0;
    if (tcp == 0 || tcp->shutdown_req == 0 || tcp->write_queue_head != 0) {
        return;
    }
    request = tcp->shutdown_req;
    cb = tcp->shutdown_cb;
    tcp->shutdown_req = 0;
    tcp->shutdown_cb = 0;
    if (shutdown(tcp->handle.fd, SHUT_WR) < 0) {
        status = uv_error_from_errno();
    }
    update_tcp_active(tcp);
    if (cb != 0) {
        cb(request, status);
    }
}

static void dispatch_tcp_connect(uv_tcp_t *tcp) {
    uv_connect_t *request;
    uv_connect_cb cb;
    int error;
    if (tcp == 0 || tcp->connect_req == 0) {
        return;
    }
    error = tcp_socket_error(tcp);
    if (socket_pending_error(error)) {
        return;
    }
    request = tcp->connect_req;
    cb = tcp->connect_cb;
    tcp->connect_req = 0;
    tcp->connect_cb = 0;
    update_tcp_active(tcp);
    if (cb != 0) {
        cb(request, error == 0 ? 0 : uv_translate_sys_error(error));
    }
}

static void dispatch_tcp_writes(uv_tcp_t *tcp) {
    while (tcp != 0 && tcp->write_queue_head != 0) {
        uv_write_t *request = tcp->write_queue_head;
        int status = 0;
        while (request->offset < request->length) {
            ssize_t written = send(tcp->handle.fd,
                request->buffer + request->offset,
                request->length - request->offset,
                MSG_DONTWAIT);
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                update_tcp_active(tcp);
                return;
            }
            if (written < 0) {
                status = uv_error_from_errno();
                break;
            }
            if (written == 0) {
                update_tcp_active(tcp);
                return;
            }
            request->offset += (size_t)written;
            tcp->write_queue_size -= (size_t)written;
        }
        tcp->write_queue_head = request->next;
        if (tcp->write_queue_head == 0) {
            tcp->write_queue_tail = 0;
        }
        request->next = 0;
        if (status < 0 && tcp->write_queue_size >= request->length - request->offset) {
            tcp->write_queue_size -= request->length - request->offset;
        }
        free(request->buffer);
        request->buffer = 0;
        request->length = 0;
        request->offset = 0;
        if (request->write_cb != 0) {
            request->write_cb(request, status);
        }
        if (status < 0) {
            break;
        }
    }
    update_tcp_active(tcp);
    dispatch_tcp_shutdown(tcp);
}

static void update_pipe_active(uv_pipe_t *pipe_handle) {
    if (pipe_handle == 0 || pipe_handle->handle.closing) {
        return;
    }
    pipe_handle->handle.active = pipe_handle->read_cb != 0 || pipe_handle->write_queue_head != 0;
}

static int drain_write_queue(int fd,
    uv_write_t **head,
    uv_write_t **tail,
    size_t *write_queue_size,
    int use_send) {
    while (*head != 0) {
        uv_write_t *request = *head;
        int status = 0;
        while (request->offset < request->length) {
            ssize_t written = use_send ?
                send(fd, request->buffer + request->offset, request->length - request->offset, MSG_DONTWAIT) :
                write(fd, request->buffer + request->offset, request->length - request->offset);
            if (written < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return 1;
            }
            if (written < 0) {
                status = uv_error_from_errno();
                break;
            }
            if (written == 0) {
                return 1;
            }
            request->offset += (size_t)written;
            *write_queue_size -= (size_t)written;
        }
        *head = request->next;
        if (*head == 0) {
            *tail = 0;
        }
        request->next = 0;
        if (status < 0 && *write_queue_size >= request->length - request->offset) {
            *write_queue_size -= request->length - request->offset;
        }
        free(request->buffer);
        request->buffer = 0;
        request->length = 0;
        request->offset = 0;
        if (request->write_cb != 0) {
            request->write_cb(request, status);
        }
        if (status < 0) {
            break;
        }
    }
    return 0;
}

static void dispatch_pipe_writes(uv_pipe_t *pipe_handle) {
    if (pipe_handle == 0) {
        return;
    }
    (void)drain_write_queue(pipe_handle->handle.fd,
        &pipe_handle->write_queue_head,
        &pipe_handle->write_queue_tail,
        &pipe_handle->write_queue_size,
        0);
    update_pipe_active(pipe_handle);
}

static void update_tty_active(uv_tty_t *tty) {
    if (tty == 0 || tty->handle.closing) {
        return;
    }
    tty->handle.active = tty->read_cb != 0 || tty->write_queue_head != 0;
}

static void dispatch_tty_writes(uv_tty_t *tty) {
    if (tty == 0) {
        return;
    }
    (void)drain_write_queue(tty->handle.fd,
        &tty->write_queue_head,
        &tty->write_queue_tail,
        &tty->write_queue_size,
        0);
    update_tty_active(tty);
}

static void dispatch_writable(uv_handle_t *handle) {
    if (handle == 0 || handle->fd < 0 || handle->closing) {
        return;
    }
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        dispatch_tcp_connect(tcp);
        dispatch_tcp_writes(tcp);
        dispatch_tcp_shutdown(tcp);
    } else if (handle->type == UV_HANDLE_PIPE) {
        dispatch_pipe_writes((uv_pipe_t *)handle);
    } else if (handle->type == UV_HANDLE_TTY) {
        dispatch_tty_writes((uv_tty_t *)handle);
    }
}

static int uv_events_from_poll(short revents) {
    int events = 0;
    if ((revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
        events |= UV_READABLE;
    }
    if ((revents & (POLLOUT | POLLERR)) != 0) {
        events |= UV_WRITABLE;
    }
    if ((revents & (POLLHUP | POLLNVAL)) != 0) {
        events |= UV_DISCONNECT;
    }
    return events;
}

static void dispatch_readable(uv_handle_t *handle) {
    if (handle == 0 || handle->fd < 0 || handle->closing) {
        return;
    }
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        if (tcp->connection_cb != 0) {
            tcp->connection_cb((uv_stream_t *)tcp, 0);
            return;
        }
        if (tcp->read_cb != 0 && tcp->alloc_cb != 0) {
            uv_buf_t buffer;
            tcp->alloc_cb(handle, 4096, &buffer);
            if (buffer.base == 0 || buffer.len == 0) {
                tcp->read_cb((uv_stream_t *)tcp, -ENOMEM, &buffer);
                return;
            }
            ssize_t count = recv(handle->fd, buffer.base, buffer.len, 0);
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }
            tcp->read_cb((uv_stream_t *)tcp, count == 0 ? UV_EOF : count < 0 ? uv_error_from_errno() : count, &buffer);
        }
        return;
    }
    if (handle->type == UV_HANDLE_PIPE) {
        uv_pipe_t *pipe_handle = (uv_pipe_t *)handle;
        if (pipe_handle->read_cb != 0 && pipe_handle->alloc_cb != 0) {
            uv_buf_t buffer;
            pipe_handle->alloc_cb(handle, 4096, &buffer);
            if (buffer.base == 0 || buffer.len == 0) {
                pipe_handle->read_cb((uv_stream_t *)pipe_handle, -ENOMEM, &buffer);
                return;
            }
            ssize_t count = read(handle->fd, buffer.base, buffer.len);
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }
            pipe_handle->read_cb((uv_stream_t *)pipe_handle,
                count == 0 ? UV_EOF : count < 0 ? uv_error_from_errno() : count,
                &buffer);
        }
        return;
    }
    if (handle->type == UV_HANDLE_TTY) {
        uv_tty_t *tty = (uv_tty_t *)handle;
        if (tty->read_cb != 0 && tty->alloc_cb != 0) {
            uv_buf_t buffer;
            tty->alloc_cb(handle, 4096, &buffer);
            if (buffer.base == 0 || buffer.len == 0) {
                tty->read_cb((uv_stream_t *)tty, -ENOMEM, &buffer);
                return;
            }
            ssize_t count = read(handle->fd, buffer.base, buffer.len);
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }
            tty->read_cb((uv_stream_t *)tty,
                count == 0 ? UV_EOF : count < 0 ? uv_error_from_errno() : count,
                &buffer);
        }
        return;
    }
    if (handle->type == UV_HANDLE_UDP) {
        uv_udp_t *udp = (uv_udp_t *)handle;
        if (udp->recv_cb != 0 && udp->alloc_cb != 0) {
            uv_buf_t buffer;
            struct sockaddr_in peer;
            socklen_t peer_len = sizeof(peer);
            udp->alloc_cb(handle, 4096, &buffer);
            if (buffer.base == 0 || buffer.len == 0) {
                udp->recv_cb(udp, -ENOMEM, &buffer, 0, 0);
                return;
            }
            ssize_t count = recvfrom(handle->fd, buffer.base, buffer.len, 0, (struct sockaddr *)&peer, &peer_len);
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }
            udp->recv_cb(udp,
                count < 0 ? uv_error_from_errno() : count,
                &buffer,
                count < 0 ? 0 : (const struct sockaddr *)&peer,
                0);
        }
    }
}

static void dispatch_poll(uv_handle_t *handle, short revents) {
    int status = 0;
    if (handle == 0 || handle->type != UV_HANDLE_POLL || handle->closing || handle->fd < 0) {
        return;
    }
    uv_poll_t *poll_handle = (uv_poll_t *)handle;
    if (poll_handle->poll_cb != 0 && !poll_handle->dispatching) {
        if ((revents & POLLNVAL) != 0) {
            status = UV_EBADF;
        } else if ((revents & POLLERR) != 0 && (revents & (POLLIN | POLLOUT | POLLHUP)) == 0) {
            status = UV_EIO;
        }
        poll_handle->dispatching = 1;
        poll_handle->poll_cb(poll_handle, status, uv_events_from_poll(revents));
        poll_handle->dispatching = 0;
    }
}

static int has_active_tcp_reader(const uv_loop_t *loop) {
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        if (handle->type == UV_HANDLE_TCP && handle->active && !handle->closing && tcp->read_cb != 0) {
            return 1;
        }
    }
    return 0;
}

static void dispatch_active_tcp_reads(uv_loop_t *loop) {
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        if (handle->type == UV_HANDLE_TCP && handle->active && !handle->closing && tcp->read_cb != 0) {
            dispatch_readable(handle);
        }
    }
}

static void run_pending_signals(uv_loop_t *loop) {
    if (loop == 0) {
        return;
    }
    for (;;) {
        uint64_t signal = 0;
        if (srv_signal_poll(&signal) < 0 || signal == 0) {
            return;
        }
        for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
            if (handle->type != UV_HANDLE_SIGNAL || !handle->active || handle->closing) {
                continue;
            }
            uv_signal_t *signal_handle = (uv_signal_t *)handle;
            if (signal_handle->signum != (int)signal || signal_handle->signal_cb == 0) {
                continue;
            }
            uv_signal_cb cb = signal_handle->signal_cb;
            int oneshot = signal_handle->oneshot;
            cb(signal_handle, (int)signal);
            if (oneshot && !signal_handle->handle.closing) {
                (void)uv_signal_stop(signal_handle);
            }
        }
    }
}

static int run_once(uv_loop_t *loop, int mode) {
    struct pollfd fds[UV_MAX_POLL_HANDLES];
    uv_handle_t *handles[UV_MAX_POLL_HANDLES];
    nfds_t count = 0;
    int timeout;
    int ready = 0;

    uv_update_time(loop);
    run_due_timers(loop);
    run_pending_async(loop);
    run_done_work(loop);
    run_done_fs(loop);
    run_process_exits(loop);
    run_done_getaddrinfo(loop);
    run_done_random(loop);
    run_pending_signals(loop);
    if (loop->stop_flag || !uv_loop_alive(loop)) {
        return 0;
    }
    run_idle_handles(loop);
    run_prepare_handles(loop);
    if (loop->stop_flag || !uv_loop_alive(loop)) {
        return 0;
    }

    timeout = mode == UV_RUN_NOWAIT ? 0 : next_timer_timeout(loop);
    if (has_active_phase(loop)) {
        timeout = 0;
    }
    if (has_active_tcp_reader(loop) && (timeout < 0 || timeout > 20)) {
        timeout = 20;
    }
    if (timeout != 0 && count < UV_MAX_POLL_HANDLES && loop_ensure_wakeup(loop) == 0) {
        fds[count].fd = loop->wake_read_fd;
        fds[count].events = POLLIN;
        fds[count].revents = 0;
        handles[count] = 0;
        count++;
    }
    for (uv_handle_t *handle = loop->handles; handle != 0 && count < UV_MAX_POLL_HANDLES; handle = handle->next) {
        short events = poll_events_for_handle(handle);
        if (events == 0) {
            continue;
        }
        fds[count].fd = handle->fd;
        fds[count].events = events;
        fds[count].revents = 0;
        handles[count] = handle;
        count++;
    }

    if (count == 0) {
        if (timeout > 0) {
            sleep_ms((uint64_t)timeout);
        } else if (timeout < 0 && loop->work_queue != 0) {
            sleep_ms(1);
        }
    } else {
        ready = poll(fds, count, timeout);
        if (ready < 0) {
            return uv_error_from_errno();
        }
        for (nfds_t i = 0; i < count; i++) {
            if (handles[i] == 0) {
                if (fds[i].revents != 0) {
                    loop_drain_wakeup(loop);
                }
            } else if (handles[i]->type == UV_HANDLE_POLL && fds[i].revents != 0) {
                dispatch_poll(handles[i], fds[i].revents);
            } else {
                if ((fds[i].revents & (POLLOUT | POLLERR)) != 0) {
                    dispatch_writable(handles[i]);
                }
                if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                    dispatch_readable(handles[i]);
                }
            }
        }
    }
    dispatch_active_tcp_reads(loop);
    run_check_handles(loop);

    uv_update_time(loop);
    run_due_timers(loop);
    run_pending_async(loop);
    run_done_work(loop);
    run_done_fs(loop);
    run_process_exits(loop);
    run_done_getaddrinfo(loop);
    run_done_random(loop);
    run_pending_signals(loop);
    return uv_loop_alive(loop) ? 1 : 0;
}

int uv_run(uv_loop_t *loop, int mode) {
    int result = 0;
    if (loop == 0) {
        return -EINVAL;
    }
    loop->stop_flag = 0;
    do {
        result = run_once(loop, mode);
        if (result < 0 || mode == UV_RUN_ONCE || mode == UV_RUN_NOWAIT) {
            break;
        }
    } while (result > 0 && !loop->stop_flag);
    return result < 0 ? result : uv_loop_alive(loop);
}

int uv_tcp_init(uv_loop_t *loop, uv_tcp_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    int fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (fd < 0) {
        return uv_error_from_errno();
    }
    init_handle(loop, &handle->handle, fd, UV_HANDLE_TCP);
    return 0;
}

int uv_tcp_bind(uv_tcp_t *handle, const struct sockaddr *addr, unsigned int flags) {
    (void)flags;
    if (handle == 0 || addr == 0) {
        return -EINVAL;
    }
    return bind(handle->handle.fd, addr, sizeof(struct sockaddr_in)) < 0 ? uv_error_from_errno() : 0;
}

int uv_listen(uv_stream_t *stream, int backlog, uv_connection_cb cb) {
    uv_tcp_t *tcp = tcp_from_stream(stream);
    if (tcp == 0 || cb == 0) {
        return -EINVAL;
    }
    if (listen(tcp->handle.fd, backlog) < 0) {
        return uv_error_from_errno();
    }
    tcp->connection_cb = cb;
    tcp->handle.active = 1;
    return 0;
}

int uv_accept(uv_stream_t *server, uv_stream_t *client) {
    uv_tcp_t *tcp_server = tcp_from_stream(server);
    uv_tcp_t *tcp_client = tcp_from_stream(client);
    if (tcp_server == 0 || tcp_client == 0) {
        return -EINVAL;
    }
    int fd = accept(tcp_server->handle.fd, 0, 0);
    if (fd < 0) {
        return uv_error_from_errno();
    }
    set_nonblocking(fd);
    if (tcp_client->handle.fd >= 0) {
        close(tcp_client->handle.fd);
    }
    tcp_client->handle.fd = fd;
    tcp_client->handle.active = 0;
    return 0;
}

int uv_tcp_connect(uv_connect_t *request,
    uv_tcp_t *handle,
    const struct sockaddr *addr,
    uv_connect_cb cb) {
    int status;
    if (request == 0 || handle == 0 || addr == 0) {
        return -EINVAL;
    }
    if (handle->connect_req != 0) {
        return UV_EALREADY;
    }
    request->type = UV_CONNECT;
    request->handle = handle;
    set_nonblocking(handle->handle.fd);
    status = connect(handle->handle.fd, addr, sizeof(struct sockaddr_in)) < 0 ? uv_error_from_errno() : 0;
    if (status < 0 && status != uv_translate_sys_error(EINPROGRESS) && status != UV_EALREADY && status != UV_EAGAIN) {
        return status;
    }
    handle->connect_req = request;
    handle->connect_cb = cb;
    handle->handle.active = 1;
    return 0;
}

int uv_read_start(uv_stream_t *stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
    uv_tcp_t *tcp = tcp_from_stream(stream);
    uv_pipe_t *pipe_handle = pipe_from_stream(stream);
    uv_tty_t *tty = tty_from_stream(stream);
    if ((tcp == 0 && pipe_handle == 0 && tty == 0) || alloc_cb == 0 || read_cb == 0) {
        return -EINVAL;
    }
    if (tcp != 0) {
        tcp->alloc_cb = alloc_cb;
        tcp->read_cb = read_cb;
        set_nonblocking(tcp->handle.fd);
        tcp->handle.active = 1;
    } else if (pipe_handle != 0) {
        pipe_handle->alloc_cb = alloc_cb;
        pipe_handle->read_cb = read_cb;
        set_nonblocking(pipe_handle->handle.fd);
        pipe_handle->handle.active = 1;
    } else {
        tty->alloc_cb = alloc_cb;
        tty->read_cb = read_cb;
        set_nonblocking(tty->handle.fd);
        tty->handle.active = 1;
    }
    return 0;
}

int uv_read_stop(uv_stream_t *stream) {
    uv_tcp_t *tcp = tcp_from_stream(stream);
    uv_pipe_t *pipe_handle = pipe_from_stream(stream);
    uv_tty_t *tty = tty_from_stream(stream);
    if (tcp == 0 && pipe_handle == 0 && tty == 0) {
        return -EINVAL;
    }
    if (tcp != 0) {
        tcp->read_cb = 0;
        tcp->alloc_cb = 0;
        update_tcp_active(tcp);
    } else if (pipe_handle != 0) {
        pipe_handle->read_cb = 0;
        pipe_handle->alloc_cb = 0;
        update_pipe_active(pipe_handle);
    } else {
        tty->read_cb = 0;
        tty->alloc_cb = 0;
        update_tty_active(tty);
    }
    return 0;
}

int uv_write(uv_write_t *request,
    uv_stream_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    uv_write_cb cb) {
    uv_tcp_t *tcp = tcp_from_stream(handle);
    uv_pipe_t *pipe_handle = pipe_from_stream(handle);
    uv_tty_t *tty = tty_from_stream(handle);
    size_t total = 0;
    size_t cursor = 0;
    if (request == 0 || (tcp == 0 && pipe_handle == 0 && tty == 0) || buffers == 0) {
        return -EINVAL;
    }
    uv_handle_t *base = tcp != 0 ? &tcp->handle : pipe_handle != 0 ? &pipe_handle->handle : &tty->handle;
    if (base->fd < 0 || base->closing) {
        return UV_EBADF;
    }
    for (unsigned int i = 0; i < buffer_count; i++) {
        total += buffers[i].len;
    }
    request->type = UV_WRITE;
    request->handle = handle;
    request->write_cb = cb;
    request->length = total;
    request->offset = 0;
    request->next = 0;
    request->buffer = total == 0 ? 0 : malloc(total);
    if (total != 0 && request->buffer == 0) {
        return UV_ENOMEM;
    }
    for (unsigned int i = 0; i < buffer_count; i++) {
        if (buffers[i].len != 0) {
            memcpy(request->buffer + cursor, buffers[i].base, buffers[i].len);
            cursor += buffers[i].len;
        }
    }
    if (tcp != 0 && tcp->write_queue_tail != 0) {
        tcp->write_queue_tail->next = request;
    } else if (tcp != 0) {
        tcp->write_queue_head = request;
    } else if (pipe_handle != 0 && pipe_handle->write_queue_tail != 0) {
        pipe_handle->write_queue_tail->next = request;
    } else if (pipe_handle != 0) {
        pipe_handle->write_queue_head = request;
    } else if (tty->write_queue_tail != 0) {
        tty->write_queue_tail->next = request;
    } else {
        tty->write_queue_head = request;
    }
    if (tcp != 0) {
        tcp->write_queue_tail = request;
        tcp->write_queue_size += total;
        tcp->handle.active = 1;
    } else if (pipe_handle != 0) {
        pipe_handle->write_queue_tail = request;
        pipe_handle->write_queue_size += total;
        pipe_handle->handle.active = 1;
    } else {
        tty->write_queue_tail = request;
        tty->write_queue_size += total;
        tty->handle.active = 1;
    }
    return 0;
}

int uv_shutdown(uv_shutdown_t *request, uv_stream_t *handle, uv_shutdown_cb cb) {
    uv_tcp_t *tcp = tcp_from_stream(handle);
    if (request == 0 || tcp == 0) {
        return -EINVAL;
    }
    if (tcp->handle.fd < 0 || tcp->handle.closing) {
        return UV_EBADF;
    }
    if (tcp->shutdown_req != 0) {
        return UV_EALREADY;
    }
    request->type = UV_SHUTDOWN;
    request->handle = handle;
    request->cb = cb;
    tcp->shutdown_req = request;
    tcp->shutdown_cb = cb;
    tcp->handle.active = 1;
    return 0;
}

int uv_pipe(uv_file fds[2], int read_flags, int write_flags) {
    if (fds == 0) {
        return UV_EINVAL;
    }
    if (pipe(fds) < 0) {
        return uv_error_from_errno();
    }
    if ((read_flags & UV_NONBLOCK_PIPE) != 0) {
        set_nonblocking(fds[0]);
    }
    if ((write_flags & UV_NONBLOCK_PIPE) != 0) {
        set_nonblocking(fds[1]);
    }
    return 0;
}

int uv_pipe_init(uv_loop_t *loop, uv_pipe_t *handle, int ipc) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_PIPE);
    handle->ipc = ipc != 0;
    return 0;
}

int uv_pipe_open(uv_pipe_t *handle, uv_file fd) {
    if (handle == 0 || fd < 0 || handle->handle.type != UV_HANDLE_PIPE) {
        return UV_EINVAL;
    }
    if (handle->handle.fd >= 0) {
        close(handle->handle.fd);
    }
    handle->handle.fd = fd;
    set_nonblocking(fd);
    return 0;
}

uv_handle_type uv_guess_handle(uv_file file) {
    if (file < 0) {
        return UV_UNKNOWN_HANDLE;
    }
    if (isatty(file)) {
        return UV_TTY;
    }
    struct stat st;
    if (fstat(file, &st) == 0 && S_ISREG(st.st_mode)) {
        return UV_FILE;
    }
    return UV_UNKNOWN_HANDLE;
}

int uv_tty_init(uv_loop_t *loop, uv_tty_t *handle, uv_file fd, int readable) {
    (void)readable;
    if (loop == 0 || handle == 0 || fd < 0 || !isatty(fd)) {
        return UV_EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, fd, UV_HANDLE_TTY);
    handle->mode = UV_TTY_MODE_NORMAL;
    if (tcgetattr(fd, &handle->original_termios) == 0) {
        handle->termios_saved = 1;
    }
    return 0;
}

static void tty_make_raw(struct termios *termios) {
    termios->c_iflag &= ~ICRNL;
    termios->c_lflag &= ~(ECHO | ICANON);
    termios->c_cc[VMIN] = 1;
    termios->c_cc[VTIME] = 0;
}

int uv_tty_set_mode(uv_tty_t *handle, uv_tty_mode_t mode) {
    if (handle == 0 || handle->handle.fd < 0 || handle->handle.closing) {
        return UV_EBADF;
    }
    if (mode != UV_TTY_MODE_NORMAL &&
        mode != UV_TTY_MODE_RAW &&
        mode != UV_TTY_MODE_IO &&
        mode != UV_TTY_MODE_RAW_VT) {
        return UV_EINVAL;
    }
    if (!handle->termios_saved && tcgetattr(handle->handle.fd, &handle->original_termios) < 0) {
        return uv_error_from_errno();
    }
    handle->termios_saved = 1;
    struct termios next = handle->original_termios;
    if (mode == UV_TTY_MODE_RAW || mode == UV_TTY_MODE_IO || mode == UV_TTY_MODE_RAW_VT) {
        tty_make_raw(&next);
    }
    if (tcsetattr(handle->handle.fd, TCSANOW, &next) < 0) {
        return uv_error_from_errno();
    }
    if (mode != UV_TTY_MODE_NORMAL && tty_reset_fd < 0) {
        tty_reset_fd = handle->handle.fd;
        tty_reset_termios = handle->original_termios;
    }
    handle->mode = mode;
    return 0;
}

int uv_tty_reset_mode(void) {
    if (tty_reset_fd < 0) {
        return 0;
    }
    if (tcsetattr(tty_reset_fd, TCSANOW, &tty_reset_termios) < 0) {
        return uv_error_from_errno();
    }
    tty_reset_fd = -1;
    return 0;
}

int uv_tty_get_winsize(uv_tty_t *handle, int *width, int *height) {
    struct winsize ws;
    if (handle == 0 || width == 0 || height == 0 || handle->handle.fd < 0 || handle->handle.closing) {
        return UV_EINVAL;
    }
    if (ioctl(handle->handle.fd, TIOCGWINSZ, &ws) < 0) {
        return uv_error_from_errno();
    }
    *width = ws.ws_col;
    *height = ws.ws_row;
    return 0;
}

int uv_tty_set_vterm_state(uv_tty_vtermstate_t state) {
    if (state != UV_TTY_UNSUPPORTED && state != UV_TTY_SUPPORTED) {
        return UV_EINVAL;
    }
    tty_vterm_state = state;
    return 0;
}

int uv_tty_get_vterm_state(uv_tty_vtermstate_t *state) {
    if (state == 0) {
        return UV_EINVAL;
    }
    *state = tty_vterm_state;
    return 0;
}

int uv_poll_init(uv_loop_t *loop, uv_poll_t *handle, int fd) {
    if (loop == 0 || handle == 0 || fd < 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, fd, UV_HANDLE_POLL);
    return 0;
}

int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *handle, int fd) {
    return uv_poll_init(loop, handle, fd);
}

int uv_poll_start(uv_poll_t *handle, int events, uv_poll_cb cb) {
    if (handle == 0 || cb == 0 || handle->handle.closing ||
        (events & ~(UV_READABLE | UV_WRITABLE | UV_DISCONNECT)) != 0) {
        return -EINVAL;
    }
    handle->events = events;
    handle->poll_cb = cb;
    handle->handle.active = events != 0;
    return 0;
}

int uv_poll_stop(uv_poll_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->events = 0;
    handle->poll_cb = 0;
    handle->handle.active = 0;
    return 0;
}

int uv_async_init(uv_loop_t *loop, uv_async_t *handle, uv_async_cb async_cb) {
    if (loop == 0 || handle == 0 || async_cb == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_ASYNC);
    handle->async_cb = async_cb;
    return 0;
}

int uv_async_send(uv_async_t *handle) {
    if (handle == 0 || handle->async_cb == 0) {
        return -EINVAL;
    }
    handle->pending = 1;
    handle->handle.active = 1;
    loop_wakeup(handle->handle.loop);
    return 0;
}

struct uv_thread_start {
    uv_thread_cb entry;
    void *arg;
};

static void *uv_thread_start_trampoline(void *arg) {
    struct uv_thread_start *start = arg;
    uv_thread_cb entry = start->entry;
    void *entry_arg = start->arg;
    free(start);
    entry(entry_arg);
    return 0;
}

int uv_thread_create_ex(uv_thread_t *thread, const uv_thread_options_t *params, uv_thread_cb entry, void *arg) {
    if (thread == 0 || entry == 0) {
        return UV_EINVAL;
    }
    pthread_attr_t attr;
    pthread_attr_t *attr_ptr = 0;
    if (params != 0) {
        if ((params->flags & ~UV_THREAD_HAS_STACK_SIZE) != 0) {
            return UV_EINVAL;
        }
        int status = pthread_attr_init(&attr);
        if (status != 0) {
            return uv_translate_sys_error(status);
        }
        attr_ptr = &attr;
        if ((params->flags & UV_THREAD_HAS_STACK_SIZE) != 0) {
            status = pthread_attr_setstacksize(&attr, params->stack_size);
            if (status != 0) {
                (void)pthread_attr_destroy(&attr);
                return uv_translate_sys_error(status);
            }
        }
    }
    struct uv_thread_start *start = malloc(sizeof(*start));
    if (start == 0) {
        if (attr_ptr != 0) {
            (void)pthread_attr_destroy(attr_ptr);
        }
        return UV_ENOMEM;
    }
    start->entry = entry;
    start->arg = arg;
    int status = pthread_create(thread, attr_ptr, uv_thread_start_trampoline, start);
    if (attr_ptr != 0) {
        (void)pthread_attr_destroy(attr_ptr);
    }
    if (status != 0) {
        free(start);
        return uv_translate_sys_error(status);
    }
    return 0;
}

int uv_thread_create(uv_thread_t *thread, uv_thread_cb entry, void *arg) {
    return uv_thread_create_ex(thread, 0, entry, arg);
}

int uv_thread_detach(uv_thread_t *thread) {
    if (thread == 0) {
        return UV_EINVAL;
    }
    int status = pthread_detach(*thread);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

int uv_thread_join(uv_thread_t *thread) {
    if (thread == 0) {
        return UV_EINVAL;
    }
    int status = pthread_join(*thread, 0);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

uv_thread_t uv_thread_self(void) {
    return pthread_self();
}

int uv_thread_equal(const uv_thread_t *left, const uv_thread_t *right) {
    return left != 0 && right != 0 && pthread_equal(*left, *right);
}

int uv_mutex_init(uv_mutex_t *mutex) {
    int status = pthread_mutex_init(mutex, 0);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

int uv_mutex_init_recursive(uv_mutex_t *mutex) {
    pthread_mutexattr_t attr;
    int status = pthread_mutexattr_init(&attr);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    status = pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    if (status == 0) {
        status = pthread_mutex_init(mutex, &attr);
    }
    (void)pthread_mutexattr_destroy(&attr);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

void uv_mutex_destroy(uv_mutex_t *mutex) {
    (void)pthread_mutex_destroy(mutex);
}

void uv_mutex_lock(uv_mutex_t *mutex) {
    (void)pthread_mutex_lock(mutex);
}

int uv_mutex_trylock(uv_mutex_t *mutex) {
    int status = pthread_mutex_trylock(mutex);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

void uv_mutex_unlock(uv_mutex_t *mutex) {
    (void)pthread_mutex_unlock(mutex);
}

int uv_rwlock_init(uv_rwlock_t *lock) {
    if (lock == 0) {
        return UV_EINVAL;
    }
    memset(lock, 0, sizeof(*lock));
    int status = pthread_mutex_init(&lock->mutex, 0);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    status = pthread_cond_init(&lock->cond, 0);
    if (status != 0) {
        (void)pthread_mutex_destroy(&lock->mutex);
        return uv_translate_sys_error(status);
    }
    return 0;
}

void uv_rwlock_destroy(uv_rwlock_t *lock) {
    if (lock == 0) {
        return;
    }
    (void)pthread_cond_destroy(&lock->cond);
    (void)pthread_mutex_destroy(&lock->mutex);
}

void uv_rwlock_rdlock(uv_rwlock_t *lock) {
    (void)pthread_mutex_lock(&lock->mutex);
    while (lock->writer || lock->waiting_writers != 0) {
        (void)pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    lock->readers++;
    (void)pthread_mutex_unlock(&lock->mutex);
}

int uv_rwlock_tryrdlock(uv_rwlock_t *lock) {
    int status = pthread_mutex_trylock(&lock->mutex);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    if (lock->writer || lock->waiting_writers != 0) {
        (void)pthread_mutex_unlock(&lock->mutex);
        return UV_EBUSY;
    }
    lock->readers++;
    (void)pthread_mutex_unlock(&lock->mutex);
    return 0;
}

void uv_rwlock_rdunlock(uv_rwlock_t *lock) {
    (void)pthread_mutex_lock(&lock->mutex);
    if (lock->readers != 0) {
        lock->readers--;
        if (lock->readers == 0) {
            (void)pthread_cond_broadcast(&lock->cond);
        }
    }
    (void)pthread_mutex_unlock(&lock->mutex);
}

void uv_rwlock_wrlock(uv_rwlock_t *lock) {
    (void)pthread_mutex_lock(&lock->mutex);
    lock->waiting_writers++;
    while (lock->writer || lock->readers != 0) {
        (void)pthread_cond_wait(&lock->cond, &lock->mutex);
    }
    lock->waiting_writers--;
    lock->writer = 1;
    (void)pthread_mutex_unlock(&lock->mutex);
}

int uv_rwlock_trywrlock(uv_rwlock_t *lock) {
    int status = pthread_mutex_trylock(&lock->mutex);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    if (lock->writer || lock->readers != 0) {
        (void)pthread_mutex_unlock(&lock->mutex);
        return UV_EBUSY;
    }
    lock->writer = 1;
    (void)pthread_mutex_unlock(&lock->mutex);
    return 0;
}

void uv_rwlock_wrunlock(uv_rwlock_t *lock) {
    (void)pthread_mutex_lock(&lock->mutex);
    lock->writer = 0;
    (void)pthread_cond_broadcast(&lock->cond);
    (void)pthread_mutex_unlock(&lock->mutex);
}

int uv_sem_init(uv_sem_t *sem, unsigned int value) {
    if (sem == 0) {
        return UV_EINVAL;
    }
    memset(sem, 0, sizeof(*sem));
    int status = pthread_mutex_init(&sem->mutex, 0);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    status = pthread_cond_init(&sem->cond, 0);
    if (status != 0) {
        (void)pthread_mutex_destroy(&sem->mutex);
        return uv_translate_sys_error(status);
    }
    sem->value = value;
    return 0;
}

void uv_sem_destroy(uv_sem_t *sem) {
    if (sem == 0) {
        return;
    }
    (void)pthread_cond_destroy(&sem->cond);
    (void)pthread_mutex_destroy(&sem->mutex);
}

void uv_sem_post(uv_sem_t *sem) {
    (void)pthread_mutex_lock(&sem->mutex);
    sem->value++;
    (void)pthread_cond_signal(&sem->cond);
    (void)pthread_mutex_unlock(&sem->mutex);
}

void uv_sem_wait(uv_sem_t *sem) {
    (void)pthread_mutex_lock(&sem->mutex);
    while (sem->value == 0) {
        (void)pthread_cond_wait(&sem->cond, &sem->mutex);
    }
    sem->value--;
    (void)pthread_mutex_unlock(&sem->mutex);
}

int uv_sem_trywait(uv_sem_t *sem) {
    int status = pthread_mutex_lock(&sem->mutex);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    if (sem->value == 0) {
        (void)pthread_mutex_unlock(&sem->mutex);
        return UV_EAGAIN;
    }
    sem->value--;
    (void)pthread_mutex_unlock(&sem->mutex);
    return 0;
}

int uv_cond_init(uv_cond_t *cond) {
    int status = pthread_cond_init(cond, 0);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

void uv_cond_destroy(uv_cond_t *cond) {
    (void)pthread_cond_destroy(cond);
}

void uv_cond_signal(uv_cond_t *cond) {
    (void)pthread_cond_signal(cond);
}

void uv_cond_broadcast(uv_cond_t *cond) {
    (void)pthread_cond_broadcast(cond);
}

void uv_cond_wait(uv_cond_t *cond, uv_mutex_t *mutex) {
    (void)pthread_cond_wait(cond, mutex);
}

int uv_cond_timedwait(uv_cond_t *cond, uv_mutex_t *mutex, uint64_t timeout) {
    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return uv_error_from_errno();
    }
    uint64_t sec = timeout / 1000000000ull;
    uint64_t nsec = timeout % 1000000000ull;
    struct timespec deadline = {
        .tv_sec = now.tv_sec + (time_t)sec,
        .tv_nsec = now.tv_nsec + (long)nsec,
    };
    if (deadline.tv_nsec >= 1000000000L) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000L;
    }
    int status = pthread_cond_timedwait(cond, mutex, &deadline);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

void uv_once(uv_once_t *guard, void (*callback)(void)) {
    (void)pthread_once(guard, callback);
}

int uv_key_create(uv_key_t *key) {
    int status = pthread_key_create(key, 0);
    return status == 0 ? 0 : uv_translate_sys_error(status);
}

void uv_key_delete(uv_key_t *key) {
    if (key != 0) {
        (void)pthread_key_delete(*key);
    }
}

void *uv_key_get(uv_key_t *key) {
    return key != 0 ? pthread_getspecific(*key) : 0;
}

void uv_key_set(uv_key_t *key, void *value) {
    if (key != 0) {
        (void)pthread_setspecific(*key, value);
    }
}

int uv_barrier_init(uv_barrier_t *barrier, unsigned int count) {
    if (barrier == 0 || count == 0) {
        return UV_EINVAL;
    }
    memset(barrier, 0, sizeof(*barrier));
    int status = pthread_mutex_init(&barrier->mutex, 0);
    if (status != 0) {
        return uv_translate_sys_error(status);
    }
    status = pthread_cond_init(&barrier->cond, 0);
    if (status != 0) {
        (void)pthread_mutex_destroy(&barrier->mutex);
        return uv_translate_sys_error(status);
    }
    barrier->threshold = count;
    return 0;
}

void uv_barrier_destroy(uv_barrier_t *barrier) {
    if (barrier == 0) {
        return;
    }
    (void)pthread_cond_destroy(&barrier->cond);
    (void)pthread_mutex_destroy(&barrier->mutex);
}

int uv_barrier_wait(uv_barrier_t *barrier) {
    (void)pthread_mutex_lock(&barrier->mutex);
    unsigned int generation = barrier->generation;
    barrier->count++;
    if (barrier->count == barrier->threshold) {
        barrier->count = 0;
        barrier->generation++;
        (void)pthread_cond_broadcast(&barrier->cond);
        (void)pthread_mutex_unlock(&barrier->mutex);
        return 1;
    }
    while (generation == barrier->generation) {
        (void)pthread_cond_wait(&barrier->cond, &barrier->mutex);
    }
    (void)pthread_mutex_unlock(&barrier->mutex);
    return 0;
}

static void work_thread_main(void *arg) {
    uv_work_t *request = arg;
    if (request->work_cb != 0) {
        request->work_cb(request);
    }
    request->status = 0;
    request->done = 1;
    loop_wakeup(request->loop);
}

int uv_queue_work(uv_loop_t *loop, uv_work_t *request, uv_work_cb work_cb, uv_after_work_cb after_work_cb) {
    if (loop == 0 || request == 0 || work_cb == 0) {
        return -EINVAL;
    }
    void *data = request->data;
    memset(request, 0, sizeof(*request));
    request->data = data;
    request->type = UV_WORK;
    request->loop = loop;
    request->work_cb = work_cb;
    request->after_work_cb = after_work_cb;
    request->next = loop->work_queue;
    loop->work_queue = request;

    int status = worker_pool_submit(work_thread_main, request);
    if (status != 0) {
        loop->work_queue = request->next;
        request->next = 0;
        return status;
    }
    return 0;
}

int uv_udp_init(uv_loop_t *loop, uv_udp_t *handle) {
    if (loop == 0 || handle == 0) {
        return -EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (fd < 0) {
        return uv_error_from_errno();
    }
    init_handle(loop, &handle->handle, fd, UV_HANDLE_UDP);
    return 0;
}

int uv_udp_bind(uv_udp_t *handle, const struct sockaddr *addr, unsigned int flags) {
    (void)flags;
    if (handle == 0 || addr == 0) {
        return -EINVAL;
    }
    return bind(handle->handle.fd, addr, sizeof(struct sockaddr_in)) < 0 ? uv_error_from_errno() : 0;
}

int uv_udp_recv_start(uv_udp_t *handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb) {
    if (handle == 0 || alloc_cb == 0 || recv_cb == 0) {
        return -EINVAL;
    }
    handle->alloc_cb = alloc_cb;
    handle->recv_cb = recv_cb;
    set_nonblocking(handle->handle.fd);
    handle->handle.active = 1;
    return 0;
}

int uv_udp_recv_stop(uv_udp_t *handle) {
    if (handle == 0) {
        return -EINVAL;
    }
    handle->alloc_cb = 0;
    handle->recv_cb = 0;
    handle->handle.active = 0;
    return 0;
}

int uv_udp_send(uv_udp_send_t *request,
    uv_udp_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    const struct sockaddr *addr,
    uv_udp_send_cb cb) {
    if (request == 0 || handle == 0 || buffers == 0 || addr == 0) {
        return -EINVAL;
    }
    request->type = UV_UDP_SEND;
    request->handle = handle;
    int status = 0;
    for (unsigned int i = 0; i < buffer_count; i++) {
        ssize_t sent = sendto(handle->handle.fd, buffers[i].base, buffers[i].len, 0, addr, sizeof(struct sockaddr_in));
        if (sent < 0 || (size_t)sent != buffers[i].len) {
            status = sent < 0 ? uv_error_from_errno() : -EIO;
            break;
        }
    }
    if (cb != 0) {
        cb(request, status);
    }
    return status;
}

static int uv_gai_status(int status) {
    if (status == 0) {
        return 0;
    }
    switch (status) {
        case EAI_FAIL:
            return UV_EAI_FAIL;
        case EAI_MEMORY:
            return UV_EAI_MEMORY;
        case EAI_NONAME:
            return UV_EAI_NONAME;
        case EAI_SERVICE:
            return UV_EAI_SERVICE;
        default:
            return UV_EAI_FAIL;
    }
}

int uv_getaddrinfo(uv_loop_t *loop,
    uv_getaddrinfo_t *request,
    uv_getaddrinfo_cb cb,
    const char *node,
    const char *service,
    const struct addrinfo *hints) {
    struct addrinfo *result = 0;
    int status;
    if (loop == 0 || request == 0) {
        return UV_EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_GETADDRINFO;
    request->loop = loop;
    request->cb = cb;
    status = uv_gai_status(getaddrinfo(node, service, hints, &result));
    request->status = status;
    request->result = status == 0 ? result : 0;
    if (status != 0 && result != 0) {
        freeaddrinfo(result);
    }
    if (cb == 0) {
        return status;
    }
    request->next = loop->getaddrinfo_queue;
    loop->getaddrinfo_queue = request;
    return 0;
}

void uv_freeaddrinfo(struct addrinfo *ai) {
    freeaddrinfo(ai);
}

static int configure_spawn_stdio(posix_spawn_file_actions_t *actions,
    const uv_stdio_container_t *stdio,
    int index,
    int parent_fds[],
    int child_fds[],
    uv_pipe_t *parent_pipes[]) {
    int flags = stdio[index].flags;
    int target_fd = index;
    parent_fds[index] = -1;
    child_fds[index] = -1;
    parent_pipes[index] = 0;
    if ((flags & UV_IGNORE) != 0 || flags == 0) {
        return posix_spawn_file_actions_addclose(actions, target_fd);
    }
    if ((flags & UV_INHERIT_FD) != 0) {
        return posix_spawn_file_actions_adddup2(actions, stdio[index].data.fd, target_fd);
    }
    if ((flags & UV_INHERIT_STREAM) != 0) {
        uv_os_fd_t fd = -1;
        int error = uv_fileno((const uv_handle_t *)stdio[index].data.stream, &fd);
        if (error < 0) {
            return -error;
        }
        return posix_spawn_file_actions_adddup2(actions, fd, target_fd);
    }
    if ((flags & UV_CREATE_PIPE) != 0) {
        int fds[2];
        uv_pipe_t *pipe_handle = pipe_from_stream(stdio[index].data.stream);
        int child_uses_read = (flags & UV_READABLE_PIPE) != 0 || (index == 0 && (flags & UV_WRITABLE_PIPE) == 0);
        if ((flags & UV_READABLE_PIPE) != 0 && (flags & UV_WRITABLE_PIPE) != 0) {
            if (pipe_handle == 0 || srv_pipe_pair(fds) < 0) {
                return errno != 0 ? errno : EINVAL;
            }
            parent_fds[index] = fds[0];
            child_fds[index] = fds[1];
            if ((flags & UV_NONBLOCK_PIPE) != 0) {
                set_nonblocking(child_fds[index]);
            } else {
                set_blocking(child_fds[index]);
            }
            if (uv_pipe_open(pipe_handle, parent_fds[index]) < 0) {
                close(fds[0]);
                close(fds[1]);
                parent_fds[index] = -1;
                child_fds[index] = -1;
                return EINVAL;
            }
            parent_pipes[index] = pipe_handle;
            return posix_spawn_file_actions_adddup2(actions, child_fds[index], target_fd);
        }
        if (pipe_handle == 0 || pipe(fds) < 0) {
            return errno != 0 ? errno : EINVAL;
        }
        if (child_uses_read) {
            child_fds[index] = fds[0];
            parent_fds[index] = fds[1];
        } else {
            child_fds[index] = fds[1];
            parent_fds[index] = fds[0];
        }
        if ((flags & UV_NONBLOCK_PIPE) != 0) {
            set_nonblocking(child_fds[index]);
        } else {
            set_blocking(child_fds[index]);
        }
        if (uv_pipe_open(pipe_handle, parent_fds[index]) < 0) {
            close(fds[0]);
            close(fds[1]);
            parent_fds[index] = -1;
            child_fds[index] = -1;
            return EINVAL;
        }
        parent_pipes[index] = pipe_handle;
        return posix_spawn_file_actions_adddup2(actions, child_fds[index], target_fd);
    }
    return EINVAL;
}

static void clear_spawn_parent_pipe_fd(uv_pipe_t *pipe_handle, int fd) {
    if (pipe_handle != 0 && pipe_handle->handle.fd == fd) {
        pipe_handle->handle.fd = -1;
        pipe_handle->handle.active = 0;
    }
}

int uv_spawn(uv_loop_t *loop, uv_process_t *process, const uv_process_options_t *options) {
    posix_spawn_file_actions_t actions;
    int parent_fds[UV_MAX_STDIO_HANDLES];
    int child_fds[UV_MAX_STDIO_HANDLES];
    uv_pipe_t *parent_pipes[UV_MAX_STDIO_HANDLES];
    int actions_ready = 0;
    int cwd_changed = 0;
    pid_t pid = -1;
    int error = 0;
    char saved_cwd[UV_REALPATH_MAX];
    if (loop == 0 || process == 0 || options == 0 || options->file == 0 || options->args == 0) {
        return UV_EINVAL;
    }
    if (options->stdio_count < 0 || options->stdio_count > UV_MAX_STDIO_HANDLES) {
        return UV_EINVAL;
    }
    if (options->stdio_count > 0 && options->stdio == 0) {
        return UV_EINVAL;
    }
    if ((options->flags & (UV_PROCESS_SETUID | UV_PROCESS_SETGID | UV_PROCESS_DETACHED)) != 0) {
        return UV_ENOSYS;
    }
    for (int i = 0; i < UV_MAX_STDIO_HANDLES; i++) {
        parent_fds[i] = -1;
        child_fds[i] = -1;
        parent_pipes[i] = 0;
    }
    memset(process, 0, sizeof(*process));
    init_handle(loop, &process->handle, -1, UV_HANDLE_PROCESS);
    process->exit_cb = options->exit_cb;
    if (posix_spawn_file_actions_init(&actions) != 0) {
        return UV_ENOMEM;
    }
    actions_ready = 1;
    for (int i = 0; i < options->stdio_count; i++) {
        error = configure_spawn_stdio(&actions, options->stdio, i, parent_fds, child_fds, parent_pipes);
        if (error != 0) {
            goto out;
        }
    }
    if (options->cwd != 0) {
        if (getcwd(saved_cwd, sizeof(saved_cwd)) == 0 || chdir(options->cwd) < 0) {
            error = errno != 0 ? errno : ENOENT;
            goto out;
        }
        cwd_changed = 1;
    }
    error = posix_spawnp(&pid, options->file, &actions, 0, options->args, options->env);
    if (cwd_changed) {
        int spawn_error = error;
        (void)chdir(saved_cwd);
        error = spawn_error;
    }
    if (error == 0) {
        process->pid = pid;
        process->handle.active = 1;
    }
out:
    if (actions_ready) {
        posix_spawn_file_actions_destroy(&actions);
    }
    for (int i = 0; i < UV_MAX_STDIO_HANDLES; i++) {
        if (child_fds[i] >= 0) {
            close(child_fds[i]);
        }
        if (error != 0 && parent_fds[i] >= 0) {
            close(parent_fds[i]);
            clear_spawn_parent_pipe_fd(parent_pipes[i], parent_fds[i]);
        }
    }
    return error == 0 ? 0 : uv_translate_sys_error(error);
}

int uv_process_kill(uv_process_t *process, int signum) {
    if (process == 0) {
        return UV_EINVAL;
    }
    return uv_kill(process->pid, signum);
}

int uv_kill(int pid, int signum) {
    if (pid <= 0 || !uv_signal_supported(signum)) {
        return UV_ESRCH;
    }
    return srv_kill_signal((uint64_t)pid, (uint64_t)signum) < 0 ? UV_ESRCH : 0;
}

uv_pid_t uv_process_get_pid(const uv_process_t *process) {
    return process != 0 ? process->pid : 0;
}

static int uv_signal_supported(int signum) {
    return signum == SIGINT || signum == SIGTERM;
}

int uv_signal_init(uv_loop_t *loop, uv_signal_t *handle) {
    if (loop == 0 || handle == 0) {
        return UV_EINVAL;
    }
    memset(handle, 0, sizeof(*handle));
    init_handle(loop, &handle->handle, -1, UV_HANDLE_SIGNAL);
    return 0;
}

int uv_signal_start(uv_signal_t *handle, uv_signal_cb signal_cb, int signum) {
    if (handle == 0 || handle->handle.closing || signal_cb == 0 || !uv_signal_supported(signum)) {
        return UV_EINVAL;
    }
    if (handle->handle.active) {
        (void)uv_signal_stop(handle);
    }
    if (signal_watch_counts[signum] == 0 &&
        srv_signal_config((uint64_t)signum, SRV_SIGNAL_CATCH) < 0) {
        return UV_ENOSYS;
    }
    signal_watch_counts[signum]++;
    handle->signal_cb = signal_cb;
    handle->signum = signum;
    handle->oneshot = 0;
    handle->handle.active = 1;
    return 0;
}

int uv_signal_start_oneshot(uv_signal_t *handle, uv_signal_cb signal_cb, int signum) {
    int result = uv_signal_start(handle, signal_cb, signum);
    if (result == 0) {
        handle->oneshot = 1;
    }
    return result;
}

int uv_signal_stop(uv_signal_t *handle) {
    if (handle == 0 || handle->handle.closing) {
        return UV_EINVAL;
    }
    int signum = handle->signum;
    if (handle->handle.active && uv_signal_supported(signum) && signal_watch_counts[signum] > 0) {
        signal_watch_counts[signum]--;
        if (signal_watch_counts[signum] == 0) {
            (void)srv_signal_config((uint64_t)signum, SRV_SIGNAL_DEFAULT);
        }
    }
    handle->signal_cb = 0;
    handle->signum = 0;
    handle->oneshot = 0;
    handle->handle.active = 0;
    return 0;
}

void uv_close(uv_handle_t *handle, uv_close_cb close_cb) {
    if (handle == 0 || handle->closing) {
        return;
    }
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        while (tcp->write_queue_head != 0) {
            uv_write_t *request = tcp->write_queue_head;
            tcp->write_queue_head = request->next;
            free(request->buffer);
            request->buffer = 0;
            request->next = 0;
        }
        tcp->write_queue_tail = 0;
        tcp->write_queue_size = 0;
        tcp->connect_req = 0;
        tcp->connect_cb = 0;
        tcp->shutdown_req = 0;
        tcp->shutdown_cb = 0;
    } else if (handle->type == UV_HANDLE_PIPE) {
        uv_pipe_t *pipe_handle = (uv_pipe_t *)handle;
        while (pipe_handle->write_queue_head != 0) {
            uv_write_t *request = pipe_handle->write_queue_head;
            pipe_handle->write_queue_head = request->next;
            free(request->buffer);
            request->buffer = 0;
            request->next = 0;
        }
        pipe_handle->write_queue_tail = 0;
        pipe_handle->write_queue_size = 0;
    } else if (handle->type == UV_HANDLE_TTY) {
        uv_tty_t *tty = (uv_tty_t *)handle;
        while (tty->write_queue_head != 0) {
            uv_write_t *request = tty->write_queue_head;
            tty->write_queue_head = request->next;
            free(request->buffer);
            request->buffer = 0;
            request->next = 0;
        }
        if (tty->termios_saved && tty->mode != UV_TTY_MODE_NORMAL) {
            (void)tcsetattr(tty->handle.fd, TCSANOW, &tty->original_termios);
            if (tty_reset_fd == tty->handle.fd) {
                tty_reset_fd = -1;
            }
        }
        tty->write_queue_tail = 0;
        tty->write_queue_size = 0;
        tty->read_cb = 0;
        tty->alloc_cb = 0;
    } else if (handle->type == UV_HANDLE_SIGNAL) {
        uv_signal_t *signal_handle = (uv_signal_t *)handle;
        int signum = signal_handle->signum;
        if (handle->active && uv_signal_supported(signum) && signal_watch_counts[signum] > 0) {
            signal_watch_counts[signum]--;
            if (signal_watch_counts[signum] == 0) {
                (void)srv_signal_config((uint64_t)signum, SRV_SIGNAL_DEFAULT);
            }
        }
        signal_handle->signal_cb = 0;
        signal_handle->signum = 0;
        signal_handle->oneshot = 0;
    }
    handle->active = 0;
    handle->closing = 1;
    handle->close_cb = close_cb;
    if (handle->fd >= 0 && handle->type != UV_HANDLE_POLL && handle->type != UV_HANDLE_TTY) {
        close(handle->fd);
        handle->fd = -1;
    }
    if (close_cb != 0) {
        close_cb(handle);
    }
}

int uv_ip4_addr(const char *ip, int port, struct sockaddr_in *addr) {
    if (ip == 0 || addr == 0 || port < 0 || port > 65535) {
        return -EINVAL;
    }
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = htons((uint16_t)port);
    if (inet_pton(AF_INET, ip, &addr->sin_addr) != 1) {
        return -EINVAL;
    }
    return 0;
}

struct uv_error_info {
    int code;
    const char *name;
    const char *message;
};

static const struct uv_error_info uv_errors[] = {
#define XX(code, message) {UV_##code, #code, message},
    UV_ERRNO_MAP(XX)
#undef XX
};

static const struct uv_error_info *uv_find_error(int error) {
    if (error > 0) {
        error = -error;
    }
    for (size_t i = 0; i < sizeof(uv_errors) / sizeof(uv_errors[0]); i++) {
        if (uv_errors[i].code == error) {
            return &uv_errors[i];
        }
    }
    return 0;
}

static char *copy_error_string(char *buffer, size_t buffer_length, const char *text) {
    if (buffer == 0 || buffer_length == 0) {
        return buffer;
    }
    if (text == 0) {
        text = "";
    }
    snprintf(buffer, buffer_length, "%s", text);
    return buffer;
}

int uv_translate_sys_error(int sys_errno) {
    if (sys_errno <= 0) {
        return sys_errno;
    }
    return UV__ERR(sys_errno);
}

const char *uv_err_name(int error) {
    const struct uv_error_info *info = uv_find_error(error);
    return info != 0 ? info->name : "UNKNOWN";
}

char *uv_err_name_r(int error, char *buffer, size_t buffer_length) {
    const struct uv_error_info *info = uv_find_error(error);
    if (info != 0) {
        return copy_error_string(buffer, buffer_length, info->name);
    }
    if (buffer != 0 && buffer_length != 0) {
        snprintf(buffer, buffer_length, "Unknown system error %d", error);
    }
    return buffer;
}

const char *uv_strerror(int error) {
    const struct uv_error_info *info = uv_find_error(error);
    if (info != 0) {
        return info->message;
    }
    if (error < 0) {
        error = -error;
    }
    return strerror(error);
}

char *uv_strerror_r(int error, char *buffer, size_t buffer_length) {
    return copy_error_string(buffer, buffer_length, uv_strerror(error));
}

static void fs_queue_done(uv_loop_t *loop, uv_fs_t *request) {
    request->next = 0;
    pthread_mutex_lock(&loop->queue_mutex);
    if (loop->fs_queue == 0) {
        loop->fs_queue = request;
        pthread_mutex_unlock(&loop->queue_mutex);
        loop_wakeup(loop);
        return;
    }
    uv_fs_t *tail = loop->fs_queue;
    while (tail->next != 0) {
        tail = tail->next;
    }
    tail->next = request;
    pthread_mutex_unlock(&loop->queue_mutex);
    loop_wakeup(loop);
}

static char *uv_strdup_text(const char *text) {
    if (text == 0) {
        return 0;
    }
    size_t length = strlen(text) + 1;
    char *copy = malloc(length);
    if (copy != 0) {
        memcpy(copy, text, length);
    }
    return copy;
}

static int fs_prepare(uv_loop_t *loop, uv_fs_t *request, uv_fs_type type, const char *path, uv_fs_cb cb) {
    if (request == 0 || (cb != 0 && loop == 0)) {
        return UV_EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->loop = loop;
    request->fs_type = type;
    request->cb = cb;
    if (path != 0) {
        request->path = uv_strdup_text(path);
        if (request->path == 0) {
            return UV_ENOMEM;
        }
    }
    return 0;
}

static int fs_copy_new_path(uv_fs_t *request, const char *path) {
    request->new_path = uv_strdup_text(path);
    return request->new_path != 0 ? 0 : UV_ENOMEM;
}

static int fs_copy_buffers(uv_fs_t *request, const uv_buf_t buffers[], unsigned int buffer_count) {
    request->buffers = calloc(buffer_count, sizeof(request->buffers[0]));
    if (request->buffers == 0) {
        return UV_ENOMEM;
    }
    memcpy(request->buffers, buffers, buffer_count * sizeof(request->buffers[0]));
    request->buffer_count = buffer_count;
    return 0;
}

static void fs_execute(uv_fs_t *request) {
    request->result = UV_EINVAL;
    switch (request->fs_type) {
        case UV_FS_OPEN: {
            int fd = open(request->path, request->flags, request->mode);
            request->result = fd < 0 ? uv_error_from_errno() : fd;
            break;
        }
        case UV_FS_CLOSE:
            request->result = close(request->fd) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_READ: {
            if (request->offset >= 0 && lseek(request->fd, request->offset, SEEK_SET) < 0) {
                request->result = uv_error_from_errno();
                break;
            }
            ssize_t total = 0;
            for (unsigned int i = 0; i < request->buffer_count; i++) {
                ssize_t count = read(request->fd, request->buffers[i].base, request->buffers[i].len);
                if (count < 0) {
                    request->result = uv_error_from_errno();
                    return;
                }
                total += count;
                if ((size_t)count < request->buffers[i].len) {
                    break;
                }
            }
            request->result = total;
            break;
        }
        case UV_FS_WRITE: {
            if (request->offset >= 0 && lseek(request->fd, request->offset, SEEK_SET) < 0) {
                request->result = uv_error_from_errno();
                break;
            }
            ssize_t total = 0;
            for (unsigned int i = 0; i < request->buffer_count; i++) {
                size_t offset_in_buffer = 0;
                while (offset_in_buffer < request->buffers[i].len) {
                    ssize_t count = write(request->fd,
                        request->buffers[i].base + offset_in_buffer,
                        request->buffers[i].len - offset_in_buffer);
                    if (count < 0) {
                        request->result = uv_error_from_errno();
                        return;
                    }
                    total += count;
                    offset_in_buffer += (size_t)count;
                }
            }
            request->result = total;
            break;
        }
        case UV_FS_FSYNC:
            request->result = fsync(request->fd) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_FDATASYNC:
            request->result = fsync(request->fd) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_FTRUNCATE:
            request->result = ftruncate(request->fd, (off_t)request->offset) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_SENDFILE: {
            char buffer[512];
            size_t remaining = request->length < 0 ? 0 : (size_t)request->length;
            ssize_t total = 0;
            if (request->offset >= 0 && lseek(request->flags, request->offset, SEEK_SET) < 0) {
                request->result = uv_error_from_errno();
                break;
            }
            while (remaining > 0) {
                size_t chunk = remaining < sizeof(buffer) ? remaining : sizeof(buffer);
                ssize_t count = read(request->flags, buffer, chunk);
                if (count < 0) {
                    request->result = uv_error_from_errno();
                    return;
                }
                if (count == 0) {
                    break;
                }
                size_t written_total = 0;
                while (written_total < (size_t)count) {
                    ssize_t written = write(request->fd, buffer + written_total, (size_t)count - written_total);
                    if (written < 0) {
                        request->result = uv_error_from_errno();
                        return;
                    }
                    written_total += (size_t)written;
                    total += written;
                }
                remaining -= (size_t)count;
                if ((size_t)count < chunk) {
                    break;
                }
            }
            request->result = total;
            break;
        }
        case UV_FS_UTIME: {
            struct utimbuf times = {
                .actime = (time_t)request->atime,
                .modtime = (time_t)request->mtime,
            };
            request->result = utime(request->path, &times) < 0 ? uv_error_from_errno() : 0;
            break;
        }
        case UV_FS_FUTIME: {
            if (srv_futime(request->fd, (uint64_t)request->atime, (uint64_t)request->mtime) < 0) {
                request->result = UV_EBADF;
                break;
            }
            request->result = 0;
            break;
        }
        case UV_FS_MKDIR:
            request->result = mkdir(request->path, (mode_t)request->mode) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_RMDIR:
            request->result = rmdir(request->path) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_RENAME:
            request->result = rename(request->path, request->new_path) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_UNLINK:
            request->result = unlink(request->path) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_STAT: {
            int result = stat(request->path, &request->statbuf);
            request->result = result < 0 ? uv_error_from_errno() : 0;
            break;
        }
        case UV_FS_LSTAT: {
            int result = stat(request->path, &request->statbuf);
            request->result = result < 0 ? uv_error_from_errno() : 0;
            break;
        }
        case UV_FS_FSTAT: {
            int result = fstat(request->fd, &request->statbuf);
            request->result = result < 0 ? uv_error_from_errno() : 0;
            break;
        }
        case UV_FS_ACCESS:
            request->result = access(request->path, request->mode) < 0 ? uv_error_from_errno() : 0;
            break;
        case UV_FS_SCANDIR: {
            DIR *dir = opendir(request->path);
            if (dir == 0) {
                request->result = uv_error_from_errno();
                break;
            }
            struct dirent *entry;
            while ((entry = readdir(dir)) != 0) {
                int error = uv_scandir_append(request, entry);
                if (error < 0) {
                    closedir(dir);
                    request->result = error;
                    return;
                }
            }
            closedir(dir);
            request->result = (ssize_t)request->dirent_count;
            break;
        }
        case UV_FS_REALPATH: {
            char *resolved = malloc(UV_REALPATH_MAX);
            if (resolved == 0) {
                request->result = UV_ENOMEM;
                break;
            }
            if (__posix_make_path(request->path, resolved, UV_REALPATH_MAX) < 0) {
                free(resolved);
                request->result = uv_error_from_errno();
                break;
            }
            request->ptr = resolved;
            request->result = 0;
            break;
        }
        default:
            request->result = UV_ENOSYS;
            break;
    }
}

static void fs_worker_main(void *arg) {
    uv_fs_t *request = arg;
    fs_execute(request);
    fs_queue_done(request->loop, request);
}

static int fs_finish(uv_fs_t *request, uv_fs_cb cb) {
    if (cb != 0) {
        pthread_mutex_lock(&request->loop->queue_mutex);
        request->loop->pending_fs_count++;
        pthread_mutex_unlock(&request->loop->queue_mutex);
        int status = worker_pool_submit(fs_worker_main, request);
        if (status != 0) {
            pthread_mutex_lock(&request->loop->queue_mutex);
            request->loop->pending_fs_count--;
            pthread_mutex_unlock(&request->loop->queue_mutex);
            return status;
        }
        return 0;
    }
    fs_execute(request);
    return request->result < 0 ? (int)request->result : 0;
}

int uv_fs_open(uv_loop_t *loop, uv_fs_t *request, const char *path, int flags, int mode, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_OPEN, path, cb);
    if (error < 0) {
        return error;
    }
    request->flags = flags;
    request->mode = mode;
    return fs_finish(request, cb);
}

int uv_fs_close(uv_loop_t *loop, uv_fs_t *request, int fd, uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_CLOSE, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    return fs_finish(request, cb);
}

int uv_fs_read(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb) {
    if (request == 0 || buffers == 0 || buffer_count == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_READ, 0, cb);
    if (error < 0) {
        return error;
    }
    error = fs_copy_buffers(request, buffers, buffer_count);
    if (error < 0) {
        uv_fs_req_cleanup(request);
        return error;
    }
    request->fd = fd;
    request->offset = offset;
    return fs_finish(request, cb);
}

int uv_fs_write(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb) {
    if (request == 0 || buffers == 0 || buffer_count == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_WRITE, 0, cb);
    if (error < 0) {
        return error;
    }
    error = fs_copy_buffers(request, buffers, buffer_count);
    if (error < 0) {
        uv_fs_req_cleanup(request);
        return error;
    }
    request->fd = fd;
    request->offset = offset;
    return fs_finish(request, cb);
}

int uv_fs_fsync(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_FSYNC, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    return fs_finish(request, cb);
}

int uv_fs_fdatasync(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_FDATASYNC, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    return fs_finish(request, cb);
}

int uv_fs_ftruncate(uv_loop_t *loop, uv_fs_t *request, uv_file fd, int64_t offset, uv_fs_cb cb) {
    if (offset < 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_FTRUNCATE, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    request->offset = offset;
    return fs_finish(request, cb);
}

int uv_fs_sendfile(uv_loop_t *loop,
    uv_fs_t *request,
    uv_file out_fd,
    uv_file in_fd,
    int64_t in_offset,
    size_t length,
    uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_SENDFILE, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = out_fd;
    request->flags = in_fd;
    request->offset = in_offset;
    request->length = (int64_t)length;
    return fs_finish(request, cb);
}

int uv_fs_utime(uv_loop_t *loop, uv_fs_t *request, const char *path, double atime, double mtime, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_UTIME, path, cb);
    if (error < 0) {
        return error;
    }
    request->atime = atime;
    request->mtime = mtime;
    return fs_finish(request, cb);
}

int uv_fs_futime(uv_loop_t *loop, uv_fs_t *request, uv_file fd, double atime, double mtime, uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_FUTIME, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    request->atime = atime;
    request->mtime = mtime;
    return fs_finish(request, cb);
}

int uv_fs_mkdir(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_MKDIR, path, cb);
    if (error < 0) {
        return error;
    }
    request->mode = mode;
    return fs_finish(request, cb);
}

int uv_fs_rmdir(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_RMDIR, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_rename(uv_loop_t *loop, uv_fs_t *request, const char *path, const char *new_path, uv_fs_cb cb) {
    if (request == 0 || path == 0 || new_path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_RENAME, path, cb);
    if (error < 0) {
        return error;
    }
    error = fs_copy_new_path(request, new_path);
    if (error < 0) {
        uv_fs_req_cleanup(request);
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_UNLINK, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_STAT, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_lstat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_LSTAT, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_fstat(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb) {
    int error = fs_prepare(loop, request, UV_FS_FSTAT, 0, cb);
    if (error < 0) {
        return error;
    }
    request->fd = fd;
    return fs_finish(request, cb);
}

int uv_fs_access(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_ACCESS, path, cb);
    if (error < 0) {
        return error;
    }
    request->mode = mode;
    return fs_finish(request, cb);
}

static uv_dirent_type_t uv_dirent_type_from_posix(unsigned char type) {
    if (type == DT_DIR) {
        return UV_DIRENT_DIR;
    }
    if (type == DT_REG) {
        return UV_DIRENT_FILE;
    }
    return UV_DIRENT_UNKNOWN;
}

static int uv_scandir_append(uv_fs_t *request, const struct dirent *entry) {
    if (request->dirent_count != 0 &&
        (request->dirent_count & (request->dirent_count - 1)) == 0 &&
        request->dirent_count >= UV_SCANDIR_INITIAL_CAPACITY) {
        size_t next_capacity = request->dirent_count * 2;
        uv_dirent_t *grown = realloc(request->dirents, next_capacity * sizeof(grown[0]));
        if (grown == 0) {
            return UV_ENOMEM;
        }
        request->dirents = grown;
    } else if (request->dirents == 0) {
        request->dirents = calloc(UV_SCANDIR_INITIAL_CAPACITY, sizeof(request->dirents[0]));
        if (request->dirents == 0) {
            return UV_ENOMEM;
        }
    }
    char *name = uv_strdup_text(entry->d_name);
    if (name == 0) {
        return UV_ENOMEM;
    }
    request->dirents[request->dirent_count].name = name;
    request->dirents[request->dirent_count].type = uv_dirent_type_from_posix(entry->d_type);
    request->dirent_count++;
    return 0;
}

int uv_fs_scandir(uv_loop_t *loop, uv_fs_t *request, const char *path, int flags, uv_fs_cb cb) {
    (void)flags;
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_SCANDIR, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

int uv_fs_scandir_next(uv_fs_t *request, uv_dirent_t *entry) {
    if (request == 0 || entry == 0 || request->fs_type != UV_FS_SCANDIR || request->result < 0) {
        return UV_EINVAL;
    }
    if (request->dirent_index >= request->dirent_count) {
        return UV_EOF;
    }
    *entry = request->dirents[request->dirent_index++];
    return 0;
}

int uv_fs_realpath(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    if (request == 0 || path == 0) {
        return UV_EINVAL;
    }
    int error = fs_prepare(loop, request, UV_FS_REALPATH, path, cb);
    if (error < 0) {
        return error;
    }
    return fs_finish(request, cb);
}

uv_fs_type uv_fs_get_type(const uv_fs_t *request) {
    return request != 0 ? request->fs_type : UV_FS_UNKNOWN;
}

ssize_t uv_fs_get_result(const uv_fs_t *request) {
    return request != 0 ? request->result : UV_EINVAL;
}

int uv_fs_get_system_error(const uv_fs_t *request) {
    if (request == 0 || request->result >= 0) {
        return 0;
    }
    return (int)request->result;
}

void *uv_fs_get_ptr(const uv_fs_t *request) {
    return request != 0 ? request->ptr : 0;
}

const char *uv_fs_get_path(const uv_fs_t *request) {
    return request != 0 ? request->path : 0;
}

uv_stat_t *uv_fs_get_statbuf(uv_fs_t *request) {
    return request != 0 ? &request->statbuf : 0;
}

void uv_fs_req_cleanup(uv_fs_t *request) {
    if (request == 0) {
        return;
    }
    free((void *)request->path);
    request->path = 0;
    free((void *)request->new_path);
    request->new_path = 0;
    free(request->buffers);
    request->buffers = 0;
    request->buffer_count = 0;
    free(request->ptr);
    request->ptr = 0;
    for (size_t i = 0; i < request->dirent_count; i++) {
        free((void *)request->dirents[i].name);
    }
    free(request->dirents);
    request->dirents = 0;
    request->dirent_count = 0;
    request->dirent_index = 0;
    request->next = 0;
}
