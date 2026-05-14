#ifndef SRVROS_POSIX_SYS_SELECT_H
#define SRVROS_POSIX_SYS_SELECT_H

#include <stddef.h>
#include <stdint.h>

#include <sys/time.h>

#define FD_SETSIZE 64

typedef struct fd_set {
    uint64_t bits[(FD_SETSIZE + 63) / 64];
} fd_set;

#define FD_ZERO(set) do { \
    for (size_t fd_zero_i = 0; fd_zero_i < sizeof((set)->bits) / sizeof((set)->bits[0]); fd_zero_i++) { \
        (set)->bits[fd_zero_i] = 0; \
    } \
} while (0)

#define FD_SET(fd, set) do { \
    if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
        (set)->bits[(fd) / 64] |= (1ull << ((fd) % 64)); \
    } \
} while (0)

#define FD_CLR(fd, set) do { \
    if ((fd) >= 0 && (fd) < FD_SETSIZE) { \
        (set)->bits[(fd) / 64] &= ~(1ull << ((fd) % 64)); \
    } \
} while (0)

#define FD_ISSET(fd, set) \
    ((fd) >= 0 && (fd) < FD_SETSIZE && (((set)->bits[(fd) / 64] & (1ull << ((fd) % 64))) != 0))

int select(int nfds,
    fd_set *readfds,
    fd_set *writefds,
    fd_set *exceptfds,
    struct timeval *timeout);

#endif
