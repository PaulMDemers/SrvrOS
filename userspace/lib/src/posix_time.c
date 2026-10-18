#include <errno.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#define SRVROS_TICKS_PER_SECOND 100

int __posix_signal_dispatch_pending(void);

time_t time(time_t *out) {
    long us = srv_realtime_us();
    if (us < 0) { errno = EIO; return (time_t)-1; }
    time_t seconds = (time_t)(us / 1000000);
    if (out != 0) {
        *out = seconds;
    }
    return seconds;
}

clock_t clock(void) {
    return (clock_t)srv_ticks();
}

int utime(const char *path, const struct utimbuf *times) {
    if (path == 0) {
        errno = EINVAL;
        return -1;
    }
    time_t atime;
    time_t mtime;
    if (times != 0) {
        atime = times->actime;
        mtime = times->modtime;
    } else {
        atime = time(0);
        if (atime == (time_t)-1) return -1;
        mtime = atime;
    }
    if (srv_utime(path, (uint64_t)atime, (uint64_t)mtime) < 0) {
        errno = ENOENT;
        return -1;
    }
    return 0;
}

int clock_gettime(int clock_id, struct timespec *tp) {
    if (tp == 0 || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        errno = EINVAL;
        return -1;
    }
    long us = clock_id == CLOCK_REALTIME ? srv_realtime_us() : srv_monotonic_us();
    if (us < 0 && clock_id == CLOCK_MONOTONIC) us = srv_ticks() * 10000;
    if (us < 0) { errno = EIO; return -1; }
    tp->tv_sec = (time_t)(us / 1000000);
    tp->tv_nsec = (long)((us % 1000000) * 1000);
    return 0;
}

int clock_getres(int clock_id, struct timespec *tp) {
    if (tp == 0 || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        errno = EINVAL;
        return -1;
    }
    if (clock_id == CLOCK_REALTIME && srv_realtime_us() < 0) { errno = EIO; return -1; }
    tp->tv_sec = 0;
    tp->tv_nsec = srv_monotonic_us() >= 0 ? 1000 : 10000000;
    return 0;
}

int nanosleep(const struct timespec *request, struct timespec *remaining) {
    if (request == 0 ||
        request->tv_sec < 0 ||
        request->tv_nsec < 0 ||
        request->tv_nsec >= 1000000000L) {
        errno = EINVAL;
        return -1;
    }
    (void)remaining;
    uint64_t ticks = (uint64_t)request->tv_sec * SRVROS_TICKS_PER_SECOND;
    ticks += ((uint64_t)request->tv_nsec * SRVROS_TICKS_PER_SECOND + 999999999ull) / 1000000000ull;
    if (ticks == 0 && (request->tv_sec != 0 || request->tv_nsec != 0)) {
        ticks = 1;
    }
    int dispatched_before = __posix_signal_dispatch_pending();
    long slept = srv_sleep_ticks(ticks);
    int dispatched_after = __posix_signal_dispatch_pending();
    int result = slept < 0 ? -1 : 0;
    if (slept < 0 && (slept == SRV_ERR_INTR || dispatched_before != 0 || dispatched_after != 0)) {
        errno = EINTR;
    }
    return result;
}

int clock_nanosleep(int clock_id, int flags, const struct timespec *request, struct timespec *remaining) {
    if (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC) {
        errno = EINVAL;
        return EINVAL;
    }
    if (flags != 0) {
        errno = ENOSYS;
        return ENOSYS;
    }
    return nanosleep(request, remaining) == 0 ? 0 : errno;
}

int gettimeofday(struct timeval *tv, void *tz) {
    (void)tz;
    if (tv == 0) {
        errno = EINVAL;
        return -1;
    }
    long us = srv_realtime_us();
    if (us < 0) { errno = EIO; return -1; }
    tv->tv_sec = (time_t)(us / 1000000);
    tv->tv_usec = (suseconds_t)(us % 1000000);
    return 0;
}

int utimes(const char *path, const struct timeval times[2]) {
    if (path == 0) {
        errno = EINVAL;
        return -1;
    }
    if (times == 0) {
        return utime(path, 0);
    }
    struct utimbuf buffer = {
        .actime = times[0].tv_sec,
        .modtime = times[1].tv_sec,
    };
    return utime(path, &buffer);
}

unsigned int sleep(unsigned int seconds) {
    int dispatched_before = __posix_signal_dispatch_pending();
    long slept = srv_sleep_ticks((uint64_t)seconds * SRVROS_TICKS_PER_SECOND);
    int dispatched_after = __posix_signal_dispatch_pending();
    unsigned int result = slept < 0 ? seconds : 0;
    if (slept < 0 && (slept == SRV_ERR_INTR || dispatched_before != 0 || dispatched_after != 0)) {
        errno = EINTR;
    }
    return result;
}

int usleep(unsigned int usec) {
    struct timespec request = {
        .tv_sec = (time_t)(usec / 1000000u),
        .tv_nsec = (long)(usec % 1000000u) * 1000L,
    };
    return nanosleep(&request, 0);
}
