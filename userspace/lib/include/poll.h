#ifndef SRVROS_POSIX_POLL_H
#define SRVROS_POSIX_POLL_H

#include <stddef.h>
#include <stdint.h>

#include <srvros/syscall_numbers.h>

typedef size_t nfds_t;

struct pollfd {
    int fd;
    short events;
    short revents;
};

#define POLLIN SRV_POLLIN
#define POLLOUT SRV_POLLOUT
#define POLLERR SRV_POLLERR
#define POLLHUP SRV_POLLHUP
#define POLLNVAL SRV_POLLNVAL

int poll(struct pollfd *fds, nfds_t nfds, int timeout);

#endif
