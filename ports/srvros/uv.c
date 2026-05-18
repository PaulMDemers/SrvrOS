#include "uv.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define UV_MAX_POLL_HANDLES 16
#define UV_HANDLE_TIMER 1
#define UV_HANDLE_TCP 2
#define UV_HANDLE_UDP 3

static uv_loop_t default_loop;
static int default_loop_ready;

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

static int uv_error_from_errno(void) {
    return errno > 0 ? -errno : -1;
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
    loop->now_ms = monotonic_ms();
    return 0;
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
    }
}

int uv_loop_alive(const uv_loop_t *loop) {
    if (loop == 0) {
        return 0;
    }
    for (uv_handle_t *handle = loop->handles; handle != 0; handle = handle->next) {
        if (handle->active && !handle->closing) {
            return 1;
        }
    }
    return 0;
}

uv_buf_t uv_buf_init(char *base, unsigned int length) {
    uv_buf_t buffer;
    buffer.base = base;
    buffer.len = length;
    return buffer;
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

static short poll_events_for_handle(uv_handle_t *handle) {
    if (handle->fd < 0 || !handle->active || handle->closing) {
        return 0;
    }
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        return tcp->connection_cb != 0 || tcp->read_cb != 0 ? POLLIN : 0;
    }
    if (handle->type == UV_HANDLE_UDP) {
        uv_udp_t *udp = (uv_udp_t *)handle;
        return udp->recv_cb != 0 ? POLLIN : 0;
    }
    return 0;
}

static void dispatch_readable(uv_handle_t *handle) {
    if (handle->type == UV_HANDLE_TCP) {
        uv_tcp_t *tcp = (uv_tcp_t *)handle;
        if (tcp->connection_cb != 0) {
            tcp->connection_cb(tcp, 0);
            return;
        }
        if (tcp->read_cb != 0 && tcp->alloc_cb != 0) {
            uv_buf_t buffer;
            tcp->alloc_cb(handle, 4096, &buffer);
            if (buffer.base == 0 || buffer.len == 0) {
                tcp->read_cb(tcp, -ENOMEM, &buffer);
                return;
            }
            ssize_t count = recv(handle->fd, buffer.base, buffer.len, 0);
            tcp->read_cb(tcp, count == 0 ? UV_EOF : count < 0 ? uv_error_from_errno() : count, &buffer);
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
            udp->recv_cb(udp,
                count < 0 ? uv_error_from_errno() : count,
                &buffer,
                count < 0 ? 0 : (const struct sockaddr *)&peer,
                0);
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
    if (loop->stop_flag || !uv_loop_alive(loop)) {
        return 0;
    }

    timeout = mode == UV_RUN_NOWAIT ? 0 : next_timer_timeout(loop);
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
        }
    } else {
        ready = poll(fds, count, timeout);
        if (ready < 0) {
            return uv_error_from_errno();
        }
        for (nfds_t i = 0; i < count; i++) {
            if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                dispatch_readable(handles[i]);
            }
        }
    }

    uv_update_time(loop);
    run_due_timers(loop);
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

int uv_listen(uv_tcp_t *stream, int backlog, uv_connection_cb cb) {
    if (stream == 0 || cb == 0) {
        return -EINVAL;
    }
    if (listen(stream->handle.fd, backlog) < 0) {
        return uv_error_from_errno();
    }
    stream->connection_cb = cb;
    stream->handle.active = 1;
    return 0;
}

int uv_accept(uv_tcp_t *server, uv_tcp_t *client) {
    if (server == 0 || client == 0) {
        return -EINVAL;
    }
    int fd = accept(server->handle.fd, 0, 0);
    if (fd < 0) {
        return uv_error_from_errno();
    }
    if (client->handle.fd >= 0) {
        close(client->handle.fd);
    }
    client->handle.fd = fd;
    client->handle.active = 0;
    return 0;
}

int uv_tcp_connect(uv_connect_t *request,
    uv_tcp_t *handle,
    const struct sockaddr *addr,
    uv_connect_cb cb) {
    if (request == 0 || handle == 0 || addr == 0) {
        return -EINVAL;
    }
    request->handle = handle;
    int status = connect(handle->handle.fd, addr, sizeof(struct sockaddr_in)) < 0 ? uv_error_from_errno() : 0;
    if (cb != 0) {
        cb(request, status);
    }
    return status;
}

int uv_read_start(uv_tcp_t *stream, uv_alloc_cb alloc_cb, uv_read_cb read_cb) {
    if (stream == 0 || alloc_cb == 0 || read_cb == 0) {
        return -EINVAL;
    }
    stream->alloc_cb = alloc_cb;
    stream->read_cb = read_cb;
    stream->handle.active = 1;
    return 0;
}

int uv_read_stop(uv_tcp_t *stream) {
    if (stream == 0) {
        return -EINVAL;
    }
    stream->read_cb = 0;
    stream->alloc_cb = 0;
    stream->handle.active = stream->connection_cb != 0;
    return 0;
}

int uv_write(uv_write_t *request,
    uv_tcp_t *handle,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    uv_write_cb cb) {
    if (request == 0 || handle == 0 || buffers == 0) {
        return -EINVAL;
    }
    request->handle = handle;
    int status = 0;
    for (unsigned int i = 0; i < buffer_count; i++) {
        size_t offset = 0;
        while (offset < buffers[i].len) {
            ssize_t written = send(handle->handle.fd, buffers[i].base + offset, buffers[i].len - offset, 0);
            if (written < 0) {
                status = uv_error_from_errno();
                break;
            }
            offset += (size_t)written;
        }
    }
    if (cb != 0) {
        cb(request, status);
    }
    return status;
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

void uv_close(uv_handle_t *handle, uv_close_cb close_cb) {
    if (handle == 0 || handle->closing) {
        return;
    }
    handle->active = 0;
    handle->closing = 1;
    handle->close_cb = close_cb;
    if (handle->fd >= 0) {
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

const char *uv_strerror(int error) {
    if (error == UV_EOF) {
        return "end of file";
    }
    if (error < 0) {
        error = -error;
    }
    return strerror(error);
}

static int fs_finish(uv_fs_t *request, ssize_t result, uv_fs_cb cb) {
    request->result = result;
    if (cb != 0) {
        cb(request);
    }
    return result < 0 ? (int)result : 0;
}

int uv_fs_open(uv_loop_t *loop, uv_fs_t *request, const char *path, int flags, int mode, uv_fs_cb cb) {
    (void)mode;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->loop = loop;
    request->path = path;
    int fd = open(path, flags, mode);
    return fs_finish(request, fd < 0 ? uv_error_from_errno() : fd, cb);
}

int uv_fs_close(uv_loop_t *loop, uv_fs_t *request, int fd, uv_fs_cb cb) {
    (void)loop;
    if (request == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    return fs_finish(request, close(fd) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_read(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || buffers == 0 || buffer_count == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    if (offset >= 0 && lseek(fd, offset, SEEK_SET) < 0) {
        return fs_finish(request, uv_error_from_errno(), cb);
    }
    ssize_t total = 0;
    for (unsigned int i = 0; i < buffer_count; i++) {
        ssize_t count = read(fd, buffers[i].base, buffers[i].len);
        if (count < 0) {
            return fs_finish(request, uv_error_from_errno(), cb);
        }
        total += count;
        if ((size_t)count < buffers[i].len) {
            break;
        }
    }
    return fs_finish(request, total, cb);
}

int uv_fs_write(uv_loop_t *loop,
    uv_fs_t *request,
    int fd,
    const uv_buf_t buffers[],
    unsigned int buffer_count,
    int64_t offset,
    uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || buffers == 0 || buffer_count == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    if (offset >= 0 && lseek(fd, offset, SEEK_SET) < 0) {
        return fs_finish(request, uv_error_from_errno(), cb);
    }
    ssize_t total = 0;
    for (unsigned int i = 0; i < buffer_count; i++) {
        size_t offset_in_buffer = 0;
        while (offset_in_buffer < buffers[i].len) {
            ssize_t count = write(fd, buffers[i].base + offset_in_buffer, buffers[i].len - offset_in_buffer);
            if (count < 0) {
                return fs_finish(request, uv_error_from_errno(), cb);
            }
            total += count;
            offset_in_buffer += (size_t)count;
        }
    }
    return fs_finish(request, total, cb);
}

int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->path = path;
    return fs_finish(request, unlink(path) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->path = path;
    int result = stat(path, &request->statbuf);
    return fs_finish(request, result < 0 ? uv_error_from_errno() : 0, cb);
}

void uv_fs_req_cleanup(uv_fs_t *request) {
    (void)request;
}
