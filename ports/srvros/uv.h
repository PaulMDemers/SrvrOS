#ifndef SRVROS_UV_H
#define SRVROS_UV_H

#include <stddef.h>
#include <stdint.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "../upstream/libuv/include/uv/errno.h"
#include "../upstream/libuv/include/uv/version.h"

#define UV_ERRNO_MAP(XX) \
    XX(E2BIG, "argument list too long") \
    XX(EACCES, "permission denied") \
    XX(EADDRINUSE, "address already in use") \
    XX(EADDRNOTAVAIL, "address not available") \
    XX(EAFNOSUPPORT, "address family not supported") \
    XX(EAGAIN, "resource temporarily unavailable") \
    XX(EAI_AGAIN, "temporary failure") \
    XX(EAI_FAIL, "permanent failure") \
    XX(EAI_FAMILY, "ai_family not supported") \
    XX(EAI_MEMORY, "out of memory") \
    XX(EAI_NONAME, "unknown node or service") \
    XX(EAI_SERVICE, "service not available for socket type") \
    XX(EALREADY, "connection already in progress") \
    XX(EBADF, "bad file descriptor") \
    XX(EBUSY, "resource busy or locked") \
    XX(ECANCELED, "operation canceled") \
    XX(ECONNABORTED, "software caused connection abort") \
    XX(ECONNREFUSED, "connection refused") \
    XX(ECONNRESET, "connection reset by peer") \
    XX(EDESTADDRREQ, "destination address required") \
    XX(EEXIST, "file already exists") \
    XX(EFAULT, "bad address in system call argument") \
    XX(EFBIG, "file too large") \
    XX(EHOSTUNREACH, "host is unreachable") \
    XX(EINTR, "interrupted system call") \
    XX(EINVAL, "invalid argument") \
    XX(EIO, "i/o error") \
    XX(EISCONN, "socket is already connected") \
    XX(EISDIR, "illegal operation on a directory") \
    XX(ELOOP, "too many symbolic links encountered") \
    XX(EMFILE, "too many open files") \
    XX(EMSGSIZE, "message too long") \
    XX(ENAMETOOLONG, "name too long") \
    XX(ENETDOWN, "network is down") \
    XX(ENETUNREACH, "network is unreachable") \
    XX(ENFILE, "file table overflow") \
    XX(ENOBUFS, "no buffer space available") \
    XX(ENODEV, "no such device") \
    XX(ENOENT, "no such file or directory") \
    XX(ENOMEM, "not enough memory") \
    XX(ENOPROTOOPT, "protocol not available") \
    XX(ENOSPC, "no space left on device") \
    XX(ENOSYS, "function not implemented") \
    XX(ENOTCONN, "socket is not connected") \
    XX(ENOTDIR, "not a directory") \
    XX(ENOTEMPTY, "directory not empty") \
    XX(ENOTSOCK, "socket operation on non-socket") \
    XX(ENOTSUP, "operation not supported on socket") \
    XX(ENOTTY, "inappropriate ioctl for device") \
    XX(ENXIO, "no such device or address") \
    XX(EOF, "end of file") \
    XX(EOVERFLOW, "value too large for defined data type") \
    XX(EPERM, "operation not permitted") \
    XX(EPIPE, "broken pipe") \
    XX(EPROTO, "protocol error") \
    XX(EPROTONOSUPPORT, "protocol not supported") \
    XX(EPROTOTYPE, "protocol wrong type for socket") \
    XX(ERANGE, "result too large") \
    XX(EROFS, "read-only file system") \
    XX(ESHUTDOWN, "cannot send after transport endpoint shutdown") \
    XX(ESPIPE, "invalid seek") \
    XX(ESRCH, "no such process") \
    XX(ETIMEDOUT, "connection timed out") \
    XX(EXDEV, "cross-device link not permitted") \
    XX(UNKNOWN, "unknown error")

typedef enum {
#define XX(code, _) UV_##code = UV__##code,
    UV_ERRNO_MAP(XX)
#undef XX
    UV_ERRNO_MAX = UV__EOF - 1
} uv_errno_t;

#define UV_RUN_DEFAULT 0
#define UV_RUN_ONCE 1
#define UV_RUN_NOWAIT 2

#define UV_READABLE 1
#define UV_WRITABLE 2
#define UV_DISCONNECT 4

typedef struct uv_loop_s uv_loop_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_req_s uv_req_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_prepare_s uv_prepare_t;
typedef struct uv_check_s uv_check_t;
typedef struct uv_idle_s uv_idle_t;
typedef struct uv_stream_s uv_stream_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_pipe_s uv_pipe_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_poll_s uv_poll_t;
typedef struct uv_async_s uv_async_t;
typedef struct uv_process_s uv_process_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
typedef struct uv_work_s uv_work_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef int uv_os_fd_t;
typedef int uv_pid_t;

typedef enum {
    UV_IGNORE = 0x00,
    UV_CREATE_PIPE = 0x01,
    UV_INHERIT_FD = 0x02,
    UV_INHERIT_STREAM = 0x04,
    UV_READABLE_PIPE = 0x10,
    UV_WRITABLE_PIPE = 0x20
} uv_stdio_flags;

typedef enum {
    UV_UNKNOWN_HANDLE = 0,
    UV_ASYNC,
    UV_CHECK,
    UV_FS_EVENT,
    UV_FS_POLL,
    UV_HANDLE,
    UV_IDLE,
    UV_NAMED_PIPE,
    UV_POLL,
    UV_PREPARE,
    UV_PROCESS,
    UV_STREAM,
    UV_TCP,
    UV_TIMER,
    UV_TTY,
    UV_UDP,
    UV_SIGNAL,
    UV_FILE,
    UV_HANDLE_TYPE_MAX
} uv_handle_type;

typedef enum {
    UV_UNKNOWN_REQ = 0,
    UV_REQ,
    UV_CONNECT,
    UV_WRITE,
    UV_SHUTDOWN,
    UV_UDP_SEND,
    UV_FS,
    UV_WORK,
    UV_GETADDRINFO,
    UV_GETNAMEINFO,
    UV_RANDOM,
    UV_REQ_TYPE_PRIVATE,
    UV_REQ_TYPE_MAX
} uv_req_type;

typedef struct {
    char *base;
    size_t len;
} uv_buf_t;

typedef void (*uv_close_cb)(uv_handle_t *handle);
typedef void (*uv_timer_cb)(uv_timer_t *handle);
typedef void (*uv_connection_cb)(uv_stream_t *server, int status);
typedef void (*uv_connect_cb)(uv_connect_t *request, int status);
typedef void (*uv_write_cb)(uv_write_t *request, int status);
typedef void (*uv_shutdown_cb)(uv_shutdown_t *request, int status);
typedef void (*uv_alloc_cb)(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer);
typedef void (*uv_read_cb)(uv_stream_t *stream, ssize_t nread, const uv_buf_t *buffer);
typedef void (*uv_poll_cb)(uv_poll_t *handle, int status, int events);
typedef void (*uv_async_cb)(uv_async_t *handle);
typedef void (*uv_prepare_cb)(uv_prepare_t *handle);
typedef void (*uv_check_cb)(uv_check_t *handle);
typedef void (*uv_idle_cb)(uv_idle_t *handle);
typedef void (*uv_walk_cb)(uv_handle_t *handle, void *arg);
typedef void (*uv_exit_cb)(uv_process_t *process, int64_t exit_status, int term_signal);
typedef void (*uv_udp_send_cb)(uv_udp_send_t *request, int status);
typedef void (*uv_udp_recv_cb)(uv_udp_t *handle,
    ssize_t nread,
    const uv_buf_t *buffer,
    const struct sockaddr *addr,
    unsigned flags);
typedef void (*uv_fs_cb)(uv_fs_t *request);
typedef void (*uv_work_cb)(uv_work_t *request);
typedef void (*uv_after_work_cb)(uv_work_t *request, int status);
typedef void (*uv_getaddrinfo_cb)(uv_getaddrinfo_t *request, int status, struct addrinfo *result);

struct uv_handle_s {
    uv_loop_t *loop;
    void *data;
    int fd;
    int type;
    int active;
    int closing;
    int referenced;
    uv_close_cb close_cb;
    uv_handle_t *next;
};

struct uv_loop_s {
    uv_handle_t *handles;
    uv_work_t *work_queue;
    uv_getaddrinfo_t *getaddrinfo_queue;
    void *data;
    uint64_t now_ms;
    int stop_flag;
};

struct uv_req_s {
    void *data;
    int type;
};

struct uv_timer_s {
    uv_handle_t handle;
    uv_timer_cb timer_cb;
    uint64_t timeout_ms;
    uint64_t repeat_ms;
};

struct uv_prepare_s {
    uv_handle_t handle;
    uv_prepare_cb prepare_cb;
};

struct uv_check_s {
    uv_handle_t handle;
    uv_check_cb check_cb;
};

struct uv_idle_s {
    uv_handle_t handle;
    uv_idle_cb idle_cb;
};

struct uv_tcp_s {
    uv_handle_t handle;
    uv_connection_cb connection_cb;
    uv_connect_t *connect_req;
    uv_connect_cb connect_cb;
    uv_shutdown_t *shutdown_req;
    uv_shutdown_cb shutdown_cb;
    uv_alloc_cb alloc_cb;
    uv_read_cb read_cb;
    uv_write_t *write_queue_head;
    uv_write_t *write_queue_tail;
    size_t write_queue_size;
};

struct uv_stream_s {
    uv_handle_t handle;
};

struct uv_pipe_s {
    uv_handle_t handle;
    uv_alloc_cb alloc_cb;
    uv_read_cb read_cb;
    uv_write_t *write_queue_head;
    uv_write_t *write_queue_tail;
    size_t write_queue_size;
    int ipc;
};

struct uv_udp_s {
    uv_handle_t handle;
    uv_alloc_cb alloc_cb;
    uv_udp_recv_cb recv_cb;
};

struct uv_poll_s {
    uv_handle_t handle;
    uv_poll_cb poll_cb;
    int events;
    int dispatching;
};

struct uv_async_s {
    uv_handle_t handle;
    uv_async_cb async_cb;
    int pending;
};

struct uv_connect_s {
    void *data;
    int type;
    uv_tcp_t *handle;
};

struct uv_write_s {
    void *data;
    int type;
    uv_stream_t *handle;
    uv_write_cb write_cb;
    char *buffer;
    size_t length;
    size_t offset;
    uv_write_t *next;
};

struct uv_shutdown_s {
    void *data;
    int type;
    uv_stream_t *handle;
    uv_shutdown_cb cb;
};

struct uv_udp_send_s {
    void *data;
    int type;
    uv_udp_t *handle;
};

struct uv_fs_s {
    void *data;
    int type;
    uv_loop_t *loop;
    int fs_type;
    ssize_t result;
    struct stat statbuf;
    const char *path;
};

struct uv_work_s {
    void *data;
    int type;
    uv_loop_t *loop;
    uv_work_cb work_cb;
    uv_after_work_cb after_work_cb;
    int status;
    volatile int done;
    uv_work_t *next;
};

struct uv_getaddrinfo_s {
    void *data;
    int type;
    uv_loop_t *loop;
    uv_getaddrinfo_cb cb;
    struct addrinfo *result;
    int status;
    uv_getaddrinfo_t *next;
};

typedef struct {
    int flags;
    union {
        uv_stream_t *stream;
        int fd;
    } data;
} uv_stdio_container_t;

typedef struct {
    uv_exit_cb exit_cb;
    const char *file;
    char **args;
    char **env;
    const char *cwd;
    unsigned int flags;
    int stdio_count;
    uv_stdio_container_t *stdio;
} uv_process_options_t;

struct uv_process_s {
    uv_handle_t handle;
    uv_exit_cb exit_cb;
    uv_pid_t pid;
    int exit_status;
    int term_signal;
};

uv_loop_t *uv_default_loop(void);
unsigned int uv_version(void);
const char *uv_version_string(void);
int uv_loop_init(uv_loop_t *loop);
int uv_loop_close(uv_loop_t *loop);
int uv_run(uv_loop_t *loop, int mode);
void uv_stop(uv_loop_t *loop);
int uv_loop_alive(const uv_loop_t *loop);
void *uv_loop_get_data(const uv_loop_t *loop);
void uv_loop_set_data(uv_loop_t *loop, void *data);
int uv_backend_fd(const uv_loop_t *loop);
int uv_backend_timeout(const uv_loop_t *loop);
void uv_update_time(uv_loop_t *loop);
uint64_t uv_now(const uv_loop_t *loop);

size_t uv_handle_size(uv_handle_type type);
const char *uv_handle_type_name(uv_handle_type type);
uv_handle_type uv_handle_get_type(const uv_handle_t *handle);
void *uv_handle_get_data(const uv_handle_t *handle);
void uv_handle_set_data(uv_handle_t *handle, void *data);
uv_loop_t *uv_handle_get_loop(const uv_handle_t *handle);
int uv_is_active(const uv_handle_t *handle);
int uv_is_closing(const uv_handle_t *handle);
void uv_ref(uv_handle_t *handle);
void uv_unref(uv_handle_t *handle);
int uv_has_ref(const uv_handle_t *handle);
void uv_walk(uv_loop_t *loop, uv_walk_cb cb, void *arg);
int uv_fileno(const uv_handle_t *handle, uv_os_fd_t *fd);
int uv_is_readable(const uv_stream_t *stream);
int uv_is_writable(const uv_stream_t *stream);
size_t uv_stream_get_write_queue_size(const uv_stream_t *stream);

size_t uv_req_size(uv_req_type type);
const char *uv_req_type_name(uv_req_type type);
uv_req_type uv_req_get_type(const uv_req_t *request);
void *uv_req_get_data(const uv_req_t *request);
void uv_req_set_data(uv_req_t *request, void *data);

uv_buf_t uv_buf_init(char *base, unsigned int length);

int uv_timer_init(uv_loop_t *loop, uv_timer_t *handle);
int uv_timer_start(uv_timer_t *handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat);
int uv_timer_stop(uv_timer_t *handle);
int uv_timer_again(uv_timer_t *handle);
void uv_timer_set_repeat(uv_timer_t *handle, uint64_t repeat);
uint64_t uv_timer_get_repeat(const uv_timer_t *handle);
uint64_t uv_timer_get_due_in(const uv_timer_t *handle);

int uv_prepare_init(uv_loop_t *loop, uv_prepare_t *handle);
int uv_prepare_start(uv_prepare_t *handle, uv_prepare_cb cb);
int uv_prepare_stop(uv_prepare_t *handle);
int uv_check_init(uv_loop_t *loop, uv_check_t *handle);
int uv_check_start(uv_check_t *handle, uv_check_cb cb);
int uv_check_stop(uv_check_t *handle);
int uv_idle_init(uv_loop_t *loop, uv_idle_t *handle);
int uv_idle_start(uv_idle_t *handle, uv_idle_cb cb);
int uv_idle_stop(uv_idle_t *handle);

int uv_tcp_init(uv_loop_t *loop, uv_tcp_t *handle);
int uv_tcp_bind(uv_tcp_t *handle, const struct sockaddr *addr, unsigned int flags);
int uv_listen(uv_stream_t *stream, int backlog, uv_connection_cb cb);
int uv_accept(uv_stream_t *server, uv_stream_t *client);
int uv_tcp_connect(uv_connect_t *request,
    uv_tcp_t *handle,
    const struct sockaddr *addr,
    uv_connect_cb cb);
int uv_read_start(uv_stream_t *stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb);
int uv_read_stop(uv_stream_t *stream);
int uv_write(uv_write_t *request,
    uv_stream_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    uv_write_cb cb);
int uv_shutdown(uv_shutdown_t *request, uv_stream_t *handle, uv_shutdown_cb cb);

int uv_pipe_init(uv_loop_t *loop, uv_pipe_t *handle, int ipc);
int uv_pipe_open(uv_pipe_t *handle, uv_os_fd_t fd);

int uv_poll_init(uv_loop_t *loop, uv_poll_t *handle, int fd);
int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *handle, int fd);
int uv_poll_start(uv_poll_t *handle, int events, uv_poll_cb cb);
int uv_poll_stop(uv_poll_t *handle);

int uv_async_init(uv_loop_t *loop, uv_async_t *handle, uv_async_cb async_cb);
int uv_async_send(uv_async_t *handle);

int uv_queue_work(uv_loop_t *loop, uv_work_t *request, uv_work_cb work_cb, uv_after_work_cb after_work_cb);

int uv_udp_init(uv_loop_t *loop, uv_udp_t *handle);
int uv_udp_bind(uv_udp_t *handle, const struct sockaddr *addr, unsigned int flags);
int uv_udp_recv_start(uv_udp_t *handle, uv_alloc_cb alloc_cb, uv_udp_recv_cb recv_cb);
int uv_udp_recv_stop(uv_udp_t *handle);
int uv_udp_send(uv_udp_send_t *request,
    uv_udp_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    const struct sockaddr *addr,
    uv_udp_send_cb cb);

int uv_getaddrinfo(uv_loop_t *loop,
    uv_getaddrinfo_t *request,
    uv_getaddrinfo_cb cb,
    const char *node,
    const char *service,
    const struct addrinfo *hints);
void uv_freeaddrinfo(struct addrinfo *ai);

int uv_spawn(uv_loop_t *loop, uv_process_t *process, const uv_process_options_t *options);
int uv_process_kill(uv_process_t *process, int signum);
int uv_kill(int pid, int signum);

void uv_close(uv_handle_t *handle, uv_close_cb close_cb);
int uv_ip4_addr(const char *ip, int port, struct sockaddr_in *addr);
int uv_translate_sys_error(int sys_errno);
const char *uv_err_name(int error);
char *uv_err_name_r(int error, char *buffer, size_t buffer_length);
const char *uv_strerror(int error);
char *uv_strerror_r(int error, char *buffer, size_t buffer_length);

int uv_fs_open(uv_loop_t *loop, uv_fs_t *request, const char *path, int flags, int mode, uv_fs_cb cb);
int uv_fs_close(uv_loop_t *loop, uv_fs_t *request, int fd, uv_fs_cb cb);
int uv_fs_read(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb);
int uv_fs_write(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb);
int uv_fs_mkdir(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb);
int uv_fs_rmdir(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_rename(uv_loop_t *loop, uv_fs_t *request, const char *path, const char *new_path, uv_fs_cb cb);
int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
void uv_fs_req_cleanup(uv_fs_t *request);

#endif
