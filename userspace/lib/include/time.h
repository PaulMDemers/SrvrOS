#ifndef SRVROS_POSIX_TIME_H
#define SRVROS_POSIX_TIME_H

#include <sys/types.h>

#define CLOCK_REALTIME 0
#define CLOCK_MONOTONIC 1
#define CLOCKS_PER_SEC 100

typedef long clock_t;

struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};

struct timespec {
    time_t tv_sec;
    long tv_nsec;
};

time_t time(time_t *out);
struct tm *localtime(const time_t *timer);
time_t mktime(struct tm *timeptr);
clock_t clock(void);
int clock_gettime(int clock_id, struct timespec *tp);
int nanosleep(const struct timespec *request, struct timespec *remaining);
int clock_nanosleep(int clock_id, int flags, const struct timespec *request, struct timespec *remaining);

#endif
