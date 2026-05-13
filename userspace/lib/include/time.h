#ifndef SRVROS_POSIX_TIME_H
#define SRVROS_POSIX_TIME_H

#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

time_t time(time_t *out);
int clock_gettime(int clock_id, struct timespec *tp);

#endif
