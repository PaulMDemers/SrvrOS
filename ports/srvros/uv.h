#ifndef SRVROS_UV_H
#define SRVROS_UV_H

#include <stddef.h>
#include <stdint.h>
#include <netdb.h>
#include <netinet/in.h>
#include <pthread.h>
#include <termios.h>
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

#define UV_PRIORITY_LOW 19
#define UV_PRIORITY_BELOW_NORMAL 10
#define UV_PRIORITY_NORMAL 0
#define UV_PRIORITY_ABOVE_NORMAL -7
#define UV_PRIORITY_HIGH -14
#define UV_PRIORITY_HIGHEST -20

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
typedef struct uv_tty_s uv_tty_t;
typedef struct uv_signal_s uv_signal_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_shutdown_s uv_shutdown_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;
typedef struct uv_work_s uv_work_t;
typedef struct uv_getaddrinfo_s uv_getaddrinfo_t;
typedef struct uv_random_s uv_random_t;
typedef struct uv_dirent_s uv_dirent_t;
typedef struct uv_cpu_info_s uv_cpu_info_t;
typedef struct uv_interface_address_s uv_interface_address_t;
typedef struct uv_passwd_s uv_passwd_t;
typedef struct uv_group_s uv_group_t;
typedef struct uv_utsname_s uv_utsname_t;
typedef struct uv_rusage_s uv_rusage_t;
typedef struct uv_env_item_s uv_env_item_t;
typedef int uv_os_fd_t;
typedef int uv_file;
typedef int uv_pid_t;
typedef unsigned long uv_uid_t;
typedef unsigned long uv_gid_t;
typedef struct stat uv_stat_t;
typedef pthread_t uv_thread_t;
typedef pthread_once_t uv_once_t;
typedef pthread_mutex_t uv_mutex_t;
typedef pthread_cond_t uv_cond_t;
typedef pthread_key_t uv_key_t;

#define UV_ONCE_INIT PTHREAD_ONCE_INIT

typedef struct {
    uv_mutex_t mutex;
    uv_cond_t cond;
    unsigned int readers;
    unsigned int writer;
    unsigned int waiting_writers;
} uv_rwlock_t;

typedef struct {
    uv_mutex_t mutex;
    uv_cond_t cond;
    unsigned int value;
} uv_sem_t;

typedef struct {
    uv_mutex_t mutex;
    uv_cond_t cond;
    unsigned int threshold;
    unsigned int count;
    unsigned int generation;
} uv_barrier_t;

typedef enum {
    UV_THREAD_NO_FLAGS = 0x00,
    UV_THREAD_HAS_STACK_SIZE = 0x01,
} uv_thread_create_flags;

typedef enum {
    UV_TTY_MODE_NORMAL,
    UV_TTY_MODE_RAW,
    UV_TTY_MODE_IO,
    UV_TTY_MODE_RAW_VT
} uv_tty_mode_t;

typedef enum {
    UV_TTY_UNSUPPORTED,
    UV_TTY_SUPPORTED
} uv_tty_vtermstate_t;

typedef struct uv_thread_options_s {
    unsigned int flags;
    size_t stack_size;
} uv_thread_options_t;

typedef enum {
    UV_IGNORE = 0x00,
    UV_CREATE_PIPE = 0x01,
    UV_INHERIT_FD = 0x02,
    UV_INHERIT_STREAM = 0x04,
    UV_READABLE_PIPE = 0x10,
    UV_WRITABLE_PIPE = 0x20,
    UV_NONBLOCK_PIPE = 0x40,
    UV_OVERLAPPED_PIPE = 0x40
} uv_stdio_flags;

enum uv_process_flags {
    UV_PROCESS_SETUID = (1 << 0),
    UV_PROCESS_SETGID = (1 << 1),
    UV_PROCESS_WINDOWS_VERBATIM_ARGUMENTS = (1 << 2),
    UV_PROCESS_DETACHED = (1 << 3),
    UV_PROCESS_WINDOWS_HIDE = (1 << 4),
    UV_PROCESS_WINDOWS_HIDE_CONSOLE = (1 << 5),
    UV_PROCESS_WINDOWS_HIDE_GUI = (1 << 6),
    UV_PROCESS_WINDOWS_FILE_PATH_EXACT_NAME = (1 << 7)
};

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

typedef enum {
    UV_DIRENT_UNKNOWN,
    UV_DIRENT_FILE,
    UV_DIRENT_DIR,
    UV_DIRENT_LINK,
    UV_DIRENT_FIFO,
    UV_DIRENT_SOCKET,
    UV_DIRENT_CHAR,
    UV_DIRENT_BLOCK
} uv_dirent_type_t;

typedef struct {
    long tv_sec;
    long tv_usec;
} uv_timeval_t;

struct uv_cpu_times_s {
    uint64_t user;
    uint64_t nice;
    uint64_t sys;
    uint64_t idle;
    uint64_t irq;
};

struct uv_cpu_info_s {
    char *model;
    int speed;
    struct uv_cpu_times_s cpu_times;
};

#ifndef AF_INET6
#define AF_INET6 10
struct in6_addr {
    unsigned char s6_addr[16];
};

struct sockaddr_in6 {
    sa_family_t sin6_family;
    in_port_t sin6_port;
    uint32_t sin6_flowinfo;
    struct in6_addr sin6_addr;
    uint32_t sin6_scope_id;
};
#endif

struct uv_interface_address_s {
    char *name;
    char phys_addr[6];
    int is_internal;
    union {
        struct sockaddr_in address4;
        struct sockaddr_in6 address6;
    } address;
    union {
        struct sockaddr_in netmask4;
        struct sockaddr_in6 netmask6;
    } netmask;
};

struct uv_passwd_s {
    char *username;
    unsigned long uid;
    unsigned long gid;
    char *shell;
    char *homedir;
};

struct uv_group_s {
    char *groupname;
    unsigned long gid;
    char **members;
};

struct uv_utsname_s {
    char sysname[256];
    char release[256];
    char version[256];
    char machine[256];
};

struct uv_rusage_s {
    uv_timeval_t ru_utime;
    uv_timeval_t ru_stime;
    uint64_t ru_maxrss;
    uint64_t ru_ixrss;
    uint64_t ru_idrss;
    uint64_t ru_isrss;
    uint64_t ru_minflt;
    uint64_t ru_majflt;
    uint64_t ru_nswap;
    uint64_t ru_inblock;
    uint64_t ru_oublock;
    uint64_t ru_msgsnd;
    uint64_t ru_msgrcv;
    uint64_t ru_nsignals;
    uint64_t ru_nvcsw;
    uint64_t ru_nivcsw;
};

struct uv_env_item_s {
    char *name;
    char *value;
};

typedef enum {
    UV_FS_UNKNOWN = -1,
    UV_FS_CUSTOM,
    UV_FS_OPEN,
    UV_FS_CLOSE,
    UV_FS_READ,
    UV_FS_WRITE,
    UV_FS_SENDFILE,
    UV_FS_STAT,
    UV_FS_LSTAT,
    UV_FS_FSTAT,
    UV_FS_FTRUNCATE,
    UV_FS_UTIME,
    UV_FS_FUTIME,
    UV_FS_ACCESS,
    UV_FS_CHMOD,
    UV_FS_FCHMOD,
    UV_FS_FSYNC,
    UV_FS_FDATASYNC,
    UV_FS_UNLINK,
    UV_FS_RMDIR,
    UV_FS_MKDIR,
    UV_FS_MKDTEMP,
    UV_FS_RENAME,
    UV_FS_SCANDIR,
    UV_FS_LINK,
    UV_FS_SYMLINK,
    UV_FS_READLINK,
    UV_FS_CHOWN,
    UV_FS_FCHOWN,
    UV_FS_REALPATH,
    UV_FS_COPYFILE,
    UV_FS_LCHOWN,
    UV_FS_OPENDIR,
    UV_FS_READDIR,
    UV_FS_CLOSEDIR,
    UV_FS_STATFS,
    UV_FS_MKSTEMP,
    UV_FS_LUTIME
} uv_fs_type;

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
typedef void (*uv_signal_cb)(uv_signal_t *handle, int signum);
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
typedef void (*uv_random_cb)(uv_random_t *request, int status, void *buffer, size_t buffer_length);
typedef void (*uv_thread_cb)(void *arg);

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
    uv_fs_t *fs_queue;
    uv_getaddrinfo_t *getaddrinfo_queue;
    uv_random_t *random_queue;
    pthread_mutex_t queue_mutex;
    void *data;
    uint64_t now_ms;
    int wake_read_fd;
    int wake_write_fd;
    int pending_fs_count;
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

struct uv_tty_s {
    uv_handle_t handle;
    uv_alloc_cb alloc_cb;
    uv_read_cb read_cb;
    uv_write_t *write_queue_head;
    uv_write_t *write_queue_tail;
    size_t write_queue_size;
    struct termios original_termios;
    int termios_saved;
    int mode;
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

struct uv_signal_s {
    uv_handle_t handle;
    uv_signal_cb signal_cb;
    int signum;
    int oneshot;
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
    uv_fs_type fs_type;
    uv_fs_cb cb;
    ssize_t result;
    void *ptr;
    struct stat statbuf;
    const char *path;
    const char *new_path;
    uv_buf_t *buffers;
    unsigned int buffer_count;
    int fd;
    int flags;
    int mode;
    int64_t offset;
    int64_t length;
    double atime;
    double mtime;
    uv_dirent_t *dirents;
    size_t dirent_count;
    size_t dirent_index;
    uv_fs_t *next;
};

struct uv_dirent_s {
    const char *name;
    uv_dirent_type_t type;
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

struct uv_random_s {
    void *data;
    int type;
    uv_loop_t *loop;
    uv_random_cb cb;
    void *buffer;
    size_t buffer_length;
    int status;
    uv_random_t *next;
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
int uv_cancel(uv_req_t *request);
uint64_t uv_hrtime(void);
char **uv_setup_args(int argc, char **argv);
int uv_get_process_title(char *buffer, size_t size);
int uv_set_process_title(const char *title);
int uv_resident_set_memory(size_t *rss);
int uv_uptime(double *uptime);
int uv_getrusage(uv_rusage_t *rusage);
int uv_getrusage_thread(uv_rusage_t *rusage);
uv_pid_t uv_os_getpid(void);
uv_pid_t uv_os_getppid(void);
int uv_os_getenv(const char *name, char *buffer, size_t *size);
int uv_os_setenv(const char *name, const char *value);
int uv_os_unsetenv(const char *name);
int uv_os_environ(uv_env_item_t **envitems, int *count);
void uv_os_free_environ(uv_env_item_t *envitems, int count);
int uv_os_homedir(char *buffer, size_t *size);
int uv_os_tmpdir(char *buffer, size_t *size);
int uv_os_get_passwd(uv_passwd_t *pwd);
int uv_os_get_passwd2(uv_passwd_t *pwd, uv_uid_t uid);
void uv_os_free_passwd(uv_passwd_t *pwd);
int uv_os_get_group(uv_group_t *grp, uv_gid_t gid);
void uv_os_free_group(uv_group_t *grp);
int uv_os_getpriority(uv_pid_t pid, int *priority);
int uv_os_setpriority(uv_pid_t pid, int priority);
unsigned int uv_available_parallelism(void);
int uv_cpu_info(uv_cpu_info_t **cpu_infos, int *count);
void uv_free_cpu_info(uv_cpu_info_t *cpu_infos, int count);
int uv_interface_addresses(uv_interface_address_t **addresses, int *count);
void uv_free_interface_addresses(uv_interface_address_t *addresses, int count);
int uv_os_uname(uv_utsname_t *buffer);
void uv_loadavg(double avg[3]);
int uv_exepath(char *buffer, size_t *size);
int uv_cwd(char *buffer, size_t *size);
int uv_chdir(const char *dir);
uint64_t uv_get_free_memory(void);
uint64_t uv_get_total_memory(void);
int uv_random(uv_loop_t *loop, uv_random_t *request, void *buffer, size_t buffer_length, unsigned int flags, uv_random_cb cb);

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

int uv_pipe(uv_file fds[2], int read_flags, int write_flags);
int uv_pipe_init(uv_loop_t *loop, uv_pipe_t *handle, int ipc);
int uv_pipe_open(uv_pipe_t *handle, uv_file fd);

uv_handle_type uv_guess_handle(uv_file file);
int uv_tty_init(uv_loop_t *loop, uv_tty_t *handle, uv_file fd, int readable);
int uv_tty_set_mode(uv_tty_t *handle, uv_tty_mode_t mode);
int uv_tty_reset_mode(void);
int uv_tty_get_winsize(uv_tty_t *handle, int *width, int *height);
int uv_tty_set_vterm_state(uv_tty_vtermstate_t state);
int uv_tty_get_vterm_state(uv_tty_vtermstate_t *state);

int uv_poll_init(uv_loop_t *loop, uv_poll_t *handle, int fd);
int uv_poll_init_socket(uv_loop_t *loop, uv_poll_t *handle, int fd);
int uv_poll_start(uv_poll_t *handle, int events, uv_poll_cb cb);
int uv_poll_stop(uv_poll_t *handle);

int uv_async_init(uv_loop_t *loop, uv_async_t *handle, uv_async_cb async_cb);
int uv_async_send(uv_async_t *handle);

int uv_queue_work(uv_loop_t *loop, uv_work_t *request, uv_work_cb work_cb, uv_after_work_cb after_work_cb);

int uv_thread_create(uv_thread_t *thread, uv_thread_cb entry, void *arg);
int uv_thread_create_ex(uv_thread_t *thread, const uv_thread_options_t *params, uv_thread_cb entry, void *arg);
int uv_thread_detach(uv_thread_t *thread);
int uv_thread_join(uv_thread_t *thread);
uv_thread_t uv_thread_self(void);
int uv_thread_equal(const uv_thread_t *left, const uv_thread_t *right);

int uv_mutex_init(uv_mutex_t *mutex);
int uv_mutex_init_recursive(uv_mutex_t *mutex);
void uv_mutex_destroy(uv_mutex_t *mutex);
void uv_mutex_lock(uv_mutex_t *mutex);
int uv_mutex_trylock(uv_mutex_t *mutex);
void uv_mutex_unlock(uv_mutex_t *mutex);

int uv_rwlock_init(uv_rwlock_t *lock);
void uv_rwlock_destroy(uv_rwlock_t *lock);
void uv_rwlock_rdlock(uv_rwlock_t *lock);
int uv_rwlock_tryrdlock(uv_rwlock_t *lock);
void uv_rwlock_rdunlock(uv_rwlock_t *lock);
void uv_rwlock_wrlock(uv_rwlock_t *lock);
int uv_rwlock_trywrlock(uv_rwlock_t *lock);
void uv_rwlock_wrunlock(uv_rwlock_t *lock);

int uv_sem_init(uv_sem_t *sem, unsigned int value);
void uv_sem_destroy(uv_sem_t *sem);
void uv_sem_post(uv_sem_t *sem);
void uv_sem_wait(uv_sem_t *sem);
int uv_sem_trywait(uv_sem_t *sem);

int uv_cond_init(uv_cond_t *cond);
void uv_cond_destroy(uv_cond_t *cond);
void uv_cond_signal(uv_cond_t *cond);
void uv_cond_broadcast(uv_cond_t *cond);
void uv_cond_wait(uv_cond_t *cond, uv_mutex_t *mutex);
int uv_cond_timedwait(uv_cond_t *cond, uv_mutex_t *mutex, uint64_t timeout);

void uv_once(uv_once_t *guard, void (*callback)(void));
int uv_key_create(uv_key_t *key);
void uv_key_delete(uv_key_t *key);
void *uv_key_get(uv_key_t *key);
void uv_key_set(uv_key_t *key, void *value);

int uv_barrier_init(uv_barrier_t *barrier, unsigned int count);
void uv_barrier_destroy(uv_barrier_t *barrier);
int uv_barrier_wait(uv_barrier_t *barrier);

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
uv_pid_t uv_process_get_pid(const uv_process_t *process);

int uv_signal_init(uv_loop_t *loop, uv_signal_t *handle);
int uv_signal_start(uv_signal_t *handle, uv_signal_cb signal_cb, int signum);
int uv_signal_start_oneshot(uv_signal_t *handle, uv_signal_cb signal_cb, int signum);
int uv_signal_stop(uv_signal_t *handle);

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
int uv_fs_fsync(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb);
int uv_fs_fdatasync(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb);
int uv_fs_ftruncate(uv_loop_t *loop, uv_fs_t *request, uv_file fd, int64_t offset, uv_fs_cb cb);
int uv_fs_sendfile(uv_loop_t *loop,
    uv_fs_t *request,
    uv_file out_fd,
    uv_file in_fd,
    int64_t in_offset,
    size_t length,
    uv_fs_cb cb);
int uv_fs_utime(uv_loop_t *loop, uv_fs_t *request, const char *path, double atime, double mtime, uv_fs_cb cb);
int uv_fs_futime(uv_loop_t *loop, uv_fs_t *request, uv_file fd, double atime, double mtime, uv_fs_cb cb);
int uv_fs_mkdir(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb);
int uv_fs_rmdir(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_rename(uv_loop_t *loop, uv_fs_t *request, const char *path, const char *new_path, uv_fs_cb cb);
int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_lstat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_fstat(uv_loop_t *loop, uv_fs_t *request, uv_file fd, uv_fs_cb cb);
int uv_fs_access(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb);
int uv_fs_scandir(uv_loop_t *loop, uv_fs_t *request, const char *path, int flags, uv_fs_cb cb);
int uv_fs_scandir_next(uv_fs_t *request, uv_dirent_t *entry);
int uv_fs_realpath(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
uv_fs_type uv_fs_get_type(const uv_fs_t *request);
ssize_t uv_fs_get_result(const uv_fs_t *request);
int uv_fs_get_system_error(const uv_fs_t *request);
void *uv_fs_get_ptr(const uv_fs_t *request);
const char *uv_fs_get_path(const uv_fs_t *request);
uv_stat_t *uv_fs_get_statbuf(uv_fs_t *request);
void uv_fs_req_cleanup(uv_fs_t *request);

#endif
