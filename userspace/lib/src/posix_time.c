#include <errno.h>
#include <srvros/sys.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>

#define SRVROS_TICKS_PER_SECOND 100

time_t time(time_t *out) {
    time_t seconds = (time_t)(srv_ticks() / SRVROS_TICKS_PER_SECOND);
    if (out != 0) {
        *out = seconds;
    }
    return seconds;
}

int clock_gettime(int clock_id, struct timespec *tp) {
    if (tp == 0 || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        errno = EINVAL;
        return -1;
    }
    uint64_t ticks = (uint64_t)srv_ticks();
    tp->tv_sec = (time_t)(ticks / SRVROS_TICKS_PER_SECOND);
    tp->tv_nsec = (long)((ticks % SRVROS_TICKS_PER_SECOND) * (1000000000ull / SRVROS_TICKS_PER_SECOND));
    return 0;
}

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv == 0) {
        errno = EINVAL;
        return -1;
    }
    uint64_t ticks = (uint64_t)srv_ticks();
    tv->tv_sec = (time_t)(ticks / SRVROS_TICKS_PER_SECOND);
    tv->tv_usec = (suseconds_t)((ticks % SRVROS_TICKS_PER_SECOND) * (1000000ull / SRVROS_TICKS_PER_SECOND));
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    return srv_sleep_ticks((uint64_t)seconds * SRVROS_TICKS_PER_SECOND) < 0 ? seconds : 0;
}

int usleep(unsigned int usec) {
    uint64_t ticks = ((uint64_t)usec * SRVROS_TICKS_PER_SECOND + 999999) / 1000000;
    if (ticks == 0 && usec != 0) {
        ticks = 1;
    }
    return srv_sleep_ticks(ticks) < 0 ? -1 : 0;
}
