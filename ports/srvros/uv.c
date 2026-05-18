#include "uv.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <poll.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#define UV_MAX_POLL_HANDLES 16
#define UV_HANDLE_TIMER UV_TIMER
#define UV_HANDLE_TCP UV_TCP
#define UV_HANDLE_UDP UV_UDP
#define UV_HANDLE_POLL UV_POLL
#define UV_HANDLE_ASYNC UV_ASYNC

static uv_loop_t default_loop;
static int default_loop_ready;

static int next_timer_timeout(const uv_loop_t *loop);

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

int uv_loop_close(uv_loop_t *loop) {
    if (loop == 0) {
        return UV_EINVAL;
    }
    if (uv_loop_alive(loop)) {
        return UV_EBUSY;
    }
    memset(loop, 0, sizeof(*loop));
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
    (void)loop;
    return UV_ENOSYS;
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
    for (uv_work_t *work = loop->work_queue; work != 0; work = work->next) {
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
        case UV_TCP:
            return sizeof(uv_tcp_t);
        case UV_TIMER:
            return sizeof(uv_timer_t);
        case UV_UDP:
            return sizeof(uv_udp_t);
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
        case UV_TCP:
            return "tcp";
        case UV_TIMER:
            return "timer";
        case UV_UDP:
            return "udp";
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

size_t uv_req_size(uv_req_type type) {
    switch (type) {
        case UV_CONNECT:
            return sizeof(uv_connect_t);
        case UV_WRITE:
            return sizeof(uv_write_t);
        case UV_UDP_SEND:
            return sizeof(uv_udp_send_t);
        case UV_FS:
            return sizeof(uv_fs_t);
        case UV_WORK:
            return sizeof(uv_work_t);
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
        case UV_UDP_SEND:
            return "udp_send";
        case UV_FS:
            return "fs";
        case UV_WORK:
            return "work";
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

static int uv_events_from_poll(short revents) {
    int events = 0;
    if ((revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
        events |= UV_READABLE;
    }
    if ((revents & (POLLOUT | POLLERR)) != 0) {
        events |= UV_WRITABLE;
    }
    return events;
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
            if (count < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
                return;
            }
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
    if (handle->type != UV_HANDLE_POLL) {
        return;
    }
    uv_poll_t *poll_handle = (uv_poll_t *)handle;
    if (poll_handle->poll_cb != 0) {
        poll_handle->poll_cb(poll_handle, 0, uv_events_from_poll(revents));
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
    if (loop->stop_flag || !uv_loop_alive(loop)) {
        return 0;
    }

    timeout = mode == UV_RUN_NOWAIT ? 0 : next_timer_timeout(loop);
    if (has_active_tcp_reader(loop) && (timeout < 0 || timeout > 20)) {
        timeout = 20;
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
            if (handles[i]->type == UV_HANDLE_POLL && fds[i].revents != 0) {
                dispatch_poll(handles[i], fds[i].revents);
            } else if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
                dispatch_readable(handles[i]);
            }
        }
    }
    dispatch_active_tcp_reads(loop);

    uv_update_time(loop);
    run_due_timers(loop);
    run_pending_async(loop);
    run_done_work(loop);
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
    set_nonblocking(fd);
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
    request->type = UV_CONNECT;
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
    set_nonblocking(stream->handle.fd);
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
    request->type = UV_WRITE;
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
    if (handle == 0 || cb == 0 || (events & ~(UV_READABLE | UV_WRITABLE)) != 0) {
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
    return 0;
}

static void *work_thread_main(void *arg) {
    uv_work_t *request = arg;
    if (request->work_cb != 0) {
        request->work_cb(request);
    }
    request->status = 0;
    request->done = 1;
    return 0;
}

int uv_queue_work(uv_loop_t *loop, uv_work_t *request, uv_work_cb work_cb, uv_after_work_cb after_work_cb) {
    if (loop == 0 || request == 0 || work_cb == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_WORK;
    request->loop = loop;
    request->work_cb = work_cb;
    request->after_work_cb = after_work_cb;
    request->next = loop->work_queue;
    loop->work_queue = request;

    pthread_t thread;
    int status = pthread_create(&thread, 0, work_thread_main, request);
    if (status != 0) {
        loop->work_queue = request->next;
        request->next = 0;
        return -status;
    }
    (void)pthread_detach(thread);
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

void uv_close(uv_handle_t *handle, uv_close_cb close_cb) {
    if (handle == 0 || handle->closing) {
        return;
    }
    handle->active = 0;
    handle->closing = 1;
    handle->close_cb = close_cb;
    if (handle->fd >= 0 && handle->type != UV_HANDLE_POLL) {
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
    request->type = UV_FS;
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
    request->type = UV_FS;
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
    request->type = UV_FS;
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
    request->type = UV_FS;
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

int uv_fs_mkdir(uv_loop_t *loop, uv_fs_t *request, const char *path, int mode, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->path = path;
    return fs_finish(request, mkdir(path, (mode_t)mode) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_rmdir(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->path = path;
    return fs_finish(request, rmdir(path) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_rename(uv_loop_t *loop, uv_fs_t *request, const char *path, const char *new_path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0 || new_path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->path = path;
    return fs_finish(request, rename(path, new_path) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_unlink(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->path = path;
    return fs_finish(request, unlink(path) < 0 ? uv_error_from_errno() : 0, cb);
}

int uv_fs_stat(uv_loop_t *loop, uv_fs_t *request, const char *path, uv_fs_cb cb) {
    (void)loop;
    if (request == 0 || path == 0) {
        return -EINVAL;
    }
    memset(request, 0, sizeof(*request));
    request->type = UV_FS;
    request->path = path;
    int result = stat(path, &request->statbuf);
    return fs_finish(request, result < 0 ? uv_error_from_errno() : 0, cb);
}

void uv_fs_req_cleanup(uv_fs_t *request) {
    (void)request;
}
