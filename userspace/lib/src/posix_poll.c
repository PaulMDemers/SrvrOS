#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include <srvros/sys.h>

int __posix_socket_poll_fd(int fd);
short __posix_socket_poll_update(int fd, short requested, short current_revents);
int __posix_signal_dispatch_pending(void);
int __posix_eventfd_poll(int fd, short requested, uint64_t *identity);

#define POSIX_POLL_MAX 64

static int poll_trace_enabled(void) {
    const char *trace = getenv("SRVROS_POLL_TRACE");
    return trace != 0 && trace[0] != '\0' && trace[0] != '0';
}

static int poll_native_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout) {
    struct srv_pollfd srv_fds[POSIX_POLL_MAX];
    if (nfds > POSIX_POLL_MAX || (nfds != 0 && fds == 0)) {
        errno = EINVAL;
        return -1;
    }

    int trace = poll_trace_enabled();
    int pre_ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        int real_fd = fds[i].fd < 0 ? -1 : __posix_socket_poll_fd(fds[i].fd);
        srv_fds[i].fd = real_fd;
        srv_fds[i].events = fds[i].events;
        srv_fds[i].revents = 0;
        fds[i].revents = 0;
        if (trace) {
            fprintf(stderr,
                "srvros-poll: in fd=%d real=%d events=0x%x timeout=%lld\n",
                fds[i].fd,
                real_fd,
                (unsigned int)fds[i].events,
                (long long)timeout);
        }
        if (fds[i].fd >= 0 && real_fd < 0) {
            fds[i].revents = POLLNVAL;
            pre_ready++;
        }
    }

    int dispatched_before = __posix_signal_dispatch_pending();
    long result = srv_poll(srv_fds, nfds, pre_ready != 0 || dispatched_before != 0 ? 0 : timeout);
    int dispatched_after = __posix_signal_dispatch_pending();
    if (result < 0) {
        errno = result < -1 ? (int)-result : EINVAL;
        return -1;
    }
    if (trace) {
        fprintf(stderr, "srvros-poll: result=%ld\n", result);
    }
    if (result == 0 && (dispatched_before != 0 || dispatched_after != 0)) {
        errno = EINTR;
        return -1;
    }

    int ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].revents == POLLNVAL) {
            ready++;
            continue;
        }
        fds[i].revents = __posix_socket_poll_update(fds[i].fd,
            fds[i].events,
            srv_fds[i].revents);
        if (trace) {
            fprintf(stderr,
                "srvros-poll: out fd=%d real=%d revents=0x%x\n",
                fds[i].fd,
                srv_fds[i].fd,
                (unsigned int)fds[i].revents);
        }
        if (fds[i].revents != 0) {
            ready++;
        }
    }
    (void)result;
    return ready;
}

int __posix_poll_ms(struct pollfd *fds,nfds_t nfds,int64_t timeout) {
    if (nfds > POSIX_POLL_MAX || (nfds && !fds)) { errno = EINVAL; return -1; }
    uint64_t identities[POSIX_POLL_MAX] = {0};
    int synthetic[POSIX_POLL_MAX], has_synthetic = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        synthetic[i] = __posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]);
        has_synthetic |= synthetic[i] >= 0;
    }
    if (!has_synthetic) return poll_native_ms(fds, nfds, timeout);
    struct timespec start = {0};
    if (timeout > 0 && clock_gettime(CLOCK_MONOTONIC, &start) < 0) return -1;
    int64_t remaining = timeout;
    for (;;) {
        struct pollfd native[POSIX_POLL_MAX];
        int pre_ready = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            native[i] = fds[i];
            if (synthetic[i] >= 0) {
                synthetic[i] = __posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]);
                native[i].fd = -1;
                pre_ready |= synthetic[i] != 0;
            }
        }
        int64_t slice = pre_ready || timeout == 0 ? 0 :
            remaining < 0 || remaining > 10 ? 10 : remaining;
        if (poll_native_ms(native, nfds, slice) < 0) return -1;
        int ready = 0;
        for (nfds_t i = 0; i < nfds; i++) {
            fds[i].revents = synthetic[i] >= 0 ?
                (short)__posix_eventfd_poll(fds[i].fd, fds[i].events, &identities[i]) : native[i].revents;
            ready += fds[i].revents != 0;
        }
        if (ready || timeout == 0) return ready;
        if (timeout > 0) {
            struct timespec now;
            if (clock_gettime(CLOCK_MONOTONIC, &now) < 0) return -1;
            int64_t seconds = now.tv_sec - start.tv_sec;
            long ns = now.tv_nsec - start.tv_nsec;
            if (ns < 0) { seconds--; ns += 1000000000; }
            /* Compare before multiplication, including saturated ppoll/select
             * timeouts. Floor elapsed time so sub-ms waits never finish early. */
            if (seconds > timeout / 1000) return 0;
            int64_t elapsed = seconds < 0 ? 0 : seconds * 1000;
            if (elapsed >= timeout || ns / 1000000 >= timeout - elapsed) return 0;
            remaining = timeout - elapsed - ns / 1000000;
        }
    }
}

int poll(struct pollfd *fds,nfds_t nfds,int timeout) { return __posix_poll_ms(fds,nfds,timeout); }

static int64_t timeval_to_ms(const struct timeval *timeout) {
    if (timeout == 0) {
        return -1;
    }
    int64_t part=(timeout->tv_usec+999)/1000;
    if(timeout->tv_sec>(INT64_MAX-part)/1000) return INT64_MAX;
    return timeout->tv_sec*1000+part;
}

int __posix_select_ms(int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    int64_t timeout) {
    struct pollfd fds[POSIX_POLL_MAX];
    fd_set read_result;
    fd_set write_result;
    fd_set except_result;
    int count = 0;

    if (nfds < 0 || nfds > FD_SETSIZE) {
        errno = EINVAL;
        return -1;
    }

    for (int fd = 0; fd < nfds; fd++) {
        short events = 0;
        if (readfds != 0 && FD_ISSET(fd, readfds)) {
            events |= POLLIN;
        }
        if (writefds != 0 && FD_ISSET(fd, writefds)) {
            events |= POLLOUT;
        }
        if (exceptfds != 0 && FD_ISSET(fd, exceptfds)) events |= POLLPRI;
        if (events != 0) {
            if (count >= POSIX_POLL_MAX) {
                errno = EINVAL;
                return -1;
            }
            fds[count].fd = fd;
            fds[count].events = events;
            fds[count].revents = 0;
            count++;
        }
    }

    int result = __posix_poll_ms(fds,(nfds_t)count,timeout);
    if (result < 0) {
        return -1;
    }

    FD_ZERO(&read_result);
    FD_ZERO(&write_result);
    FD_ZERO(&except_result);
    int bits=0;
    for (int i = 0; i < count; i++) {
        if (fds[i].revents & POLLNVAL) { errno=EBADF; return -1; }
        if ((fds[i].events & POLLIN) && (fds[i].revents & (POLLIN | POLLHUP | POLLERR))) {
            FD_SET(fds[i].fd, &read_result);
            bits++;
        }
        if ((fds[i].events & POLLOUT) && (fds[i].revents & (POLLOUT | POLLERR))) {
            FD_SET(fds[i].fd, &write_result);
            bits++;
        }
        if ((fds[i].events & POLLPRI) && (fds[i].revents & POLLPRI)) {
            FD_SET(fds[i].fd,&except_result); bits++;
        }
    }

    if (readfds != 0) {
        memcpy(readfds, &read_result, sizeof(*readfds));
    }
    if (writefds != 0) {
        memcpy(writefds, &write_result, sizeof(*writefds));
    }
    if (exceptfds != 0) memcpy(exceptfds,&except_result,sizeof(*exceptfds));
    return bits;
}

int select(int nfds,fd_set *readfds,fd_set *writefds,fd_set *exceptfds,struct timeval *timeout) {
    if(timeout && (timeout->tv_sec<0 || timeout->tv_usec<0 || timeout->tv_usec>=1000000)) {
        errno=EINVAL;return -1;
    }
    return __posix_select_ms(nfds,readfds,writefds,exceptfds,timeval_to_ms(timeout));
}
