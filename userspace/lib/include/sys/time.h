#ifndef SRVROS_POSIX_SYS_TIME_H
#define SRVROS_POSIX_SYS_TIME_H

#include <sys/types.h>

struct timeval {
    time_t tv_sec;
    suseconds_t tv_usec;
};

#ifdef __cplusplus
extern "C" {
#endif

int gettimeofday(struct timeval *tv, void *tz);
int utimes(const char *path, const struct timeval times[2]);

#ifdef __cplusplus
}
#endif

#endif
