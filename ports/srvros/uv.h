#ifndef SRVROS_UV_H
#define SRVROS_UV_H

#include <stddef.h>
#include <stdint.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>

#define UV_RUN_DEFAULT 0
#define UV_RUN_ONCE 1
#define UV_RUN_NOWAIT 2

#define UV_EOF (-4095)

typedef struct uv_loop_s uv_loop_t;
typedef struct uv_handle_s uv_handle_t;
typedef struct uv_timer_s uv_timer_t;
typedef struct uv_tcp_s uv_tcp_t;
typedef struct uv_udp_s uv_udp_t;
typedef struct uv_connect_s uv_connect_t;
typedef struct uv_write_s uv_write_t;
typedef struct uv_udp_send_s uv_udp_send_t;
typedef struct uv_fs_s uv_fs_t;

typedef struct {
    char *base;
    size_t len;
} uv_buf_t;

typedef void (*uv_close_cb)(uv_handle_t *handle);
typedef void (*uv_timer_cb)(uv_timer_t *handle);
typedef void (*uv_connection_cb)(uv_tcp_t *server, int status);
typedef void (*uv_connect_cb)(uv_connect_t *request, int status);
typedef void (*uv_write_cb)(uv_write_t *request, int status);
typedef void (*uv_alloc_cb)(uv_handle_t *handle, size_t suggested_size, uv_buf_t *buffer);
typedef void (*uv_read_cb)(uv_tcp_t *stream, ssize_t nread, const uv_buf_t *buffer);
typedef void (*uv_udp_send_cb)(uv_udp_send_t *request, int status);
typedef void (*uv_udp_recv_cb)(uv_udp_t *handle,
    ssize_t nread,
    const uv_buf_t *buffer,
    const struct sockaddr *addr,
    unsigned flags);
typedef void (*uv_fs_cb)(uv_fs_t *request);

struct uv_handle_s {
    uv_loop_t *loop;
    int fd;
    int type;
    int active;
    int closing;
    uv_close_cb close_cb;
    uv_handle_t *next;
};

struct uv_loop_s {
    uv_handle_t *handles;
    uint64_t now_ms;
    int stop_flag;
};

struct uv_timer_s {
    uv_handle_t handle;
    uv_timer_cb timer_cb;
    uint64_t timeout_ms;
    uint64_t repeat_ms;
};

struct uv_tcp_s {
    uv_handle_t handle;
    uv_connection_cb connection_cb;
    uv_alloc_cb alloc_cb;
    uv_read_cb read_cb;
};

struct uv_udp_s {
    uv_handle_t handle;
    uv_alloc_cb alloc_cb;
    uv_udp_recv_cb recv_cb;
};

struct uv_connect_s {
    uv_tcp_t *handle;
    void *data;
};

struct uv_write_s {
    uv_tcp_t *handle;
    void *data;
};

struct uv_udp_send_s {
    uv_udp_t *handle;
    void *data;
};

struct uv_fs_s {
    uv_loop_t *loop;
    int fs_type;
    ssize_t result;
    struct stat statbuf;
    const char *path;
    void *data;
};

uv_loop_t *uv_default_loop(void);
int uv_loop_init(uv_loop_t *loop);
int uv_run(uv_loop_t *loop, int mode);
void uv_stop(uv_loop_t *loop);
int uv_loop_alive(const uv_loop_t *loop);
void uv_update_time(uv_loop_t *loop);
uint64_t uv_now(const uv_loop_t *loop);

uv_buf_t uv_buf_init(char *base, unsigned int length);

int uv_timer_init(uv_loop_t *loop, uv_timer_t *handle);
int uv_timer_start(uv_timer_t *handle, uv_timer_cb cb, uint64_t timeout, uint64_t repeat);
int uv_timer_stop(uv_timer_t *handle);
int uv_timer_again(uv_timer_t *handle);
void uv_timer_set_repeat(uv_timer_t *handle, uint64_t repeat);
uint64_t uv_timer_get_repeat(const uv_timer_t *handle);

int uv_tcp_init(uv_loop_t *loop, uv_tcp_t *handle);
int uv_tcp_bind(uv_tcp_t *handle, const struct sockaddr *addr, unsigned int flags);
int uv_listen(uv_tcp_t *stream, int backlog, uv_connection_cb cb);
int uv_accept(uv_tcp_t *server, uv_tcp_t *client);
int uv_tcp_connect(uv_connect_t *request,
    uv_tcp_t *handle,
    const struct sockaddr *addr,
    uv_connect_cb cb);
int uv_read_start(uv_tcp_t *stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb);
int uv_read_stop(uv_tcp_t *stream);
int uv_write(uv_write_t *request,
    uv_tcp_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    uv_write_cb cb);

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

void uv_close(uv_handle_t *handle, uv_close_cb close_cb);
int uv_ip4_addr(const char *ip, int port, struct sockaddr_in *addr);
const char *uv_strerror(int error);

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
int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb);
void uv_fs_req_cleanup(uv_fs_t *request);

#endif
