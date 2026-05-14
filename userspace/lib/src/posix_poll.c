#include <errno.h>
#include <poll.h>
#include <stdint.h>
#include <string.h>
#include <sys/select.h>
#include <sys/time.h>

#include <srvros/sys.h>

int __posix_socket_poll_fd(int fd);

#define POSIX_POLL_MAX 32

int poll(struct pollfd *fds, nfds_t nfds, int timeout) {
    struct srv_pollfd srv_fds[POSIX_POLL_MAX];
    if (nfds > POSIX_POLL_MAX || (nfds != 0 && fds == 0)) {
        errno = EINVAL;
        return -1;
    }

    int pre_ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        int real_fd = fds[i].fd < 0 ? -1 : __posix_socket_poll_fd(fds[i].fd);
        srv_fds[i].fd = real_fd;
        srv_fds[i].events = fds[i].events;
        srv_fds[i].revents = 0;
        fds[i].revents = 0;
        if (fds[i].fd >= 0 && real_fd < 0) {
            fds[i].revents = POLLNVAL;
            pre_ready++;
        }
    }

    long result = srv_poll(srv_fds, nfds, pre_ready != 0 ? 0 : timeout);
    if (result < 0) {
        errno = EINVAL;
        return -1;
    }

    int ready = 0;
    for (nfds_t i = 0; i < nfds; i++) {
        if (fds[i].revents == POLLNVAL) {
            ready++;
            continue;
        }
        fds[i].revents = srv_fds[i].revents;
        if (fds[i].revents != 0) {
            ready++;
        }
    }
    (void)result;
    return ready;
}

static int timeval_to_ms(struct timeval *timeout) {
    if (timeout == 0) {
        return -1;
    }
    if (timeout->tv_sec < 0 || timeout->tv_usec < 0) {
        return -1;
    }
    int64_t ms = timeout->tv_sec * 1000 + (timeout->tv_usec + 999) / 1000;
    return ms > 2147483647 ? 2147483647 : (int)ms;
}

int select(int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timeval *timeout) {
    struct pollfd fds[POSIX_POLL_MAX];
    fd_set read_result;
    fd_set write_result;
    int count = 0;
    (void)exceptfds;

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

    int result = poll(fds, (nfds_t)count, timeval_to_ms(timeout));
    if (result < 0) {
        return -1;
    }

    FD_ZERO(&read_result);
    FD_ZERO(&write_result);
    for (int i = 0; i < count; i++) {
        if ((fds[i].revents & (POLLIN | POLLHUP | POLLERR)) != 0) {
            FD_SET(fds[i].fd, &read_result);
        }
        if ((fds[i].revents & (POLLOUT | POLLERR)) != 0) {
            FD_SET(fds[i].fd, &write_result);
        }
    }

    if (readfds != 0) {
        memcpy(readfds, &read_result, sizeof(*readfds));
    }
    if (writefds != 0) {
        memcpy(writefds, &write_result, sizeof(*writefds));
    }
    return result;
}
