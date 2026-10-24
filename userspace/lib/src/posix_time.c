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
    /* Preserve an aliased request/remainder and check arithmetic before the
     * kernel converts the relative interval into an absolute tick deadline. */
    struct timespec interval=*request;
    uint64_t fraction=((uint64_t)interval.tv_nsec*SRVROS_TICKS_PER_SECOND+999999999ull)/1000000000ull;
    uint64_t now=(uint64_t)srv_ticks();
    if (now>UINT64_MAX-fraction-1 ||
        (uint64_t)interval.tv_sec>(UINT64_MAX-now-fraction-1)/SRVROS_TICKS_PER_SECOND) {
        errno=EINVAL;return -1;
    }
    uint64_t ticks=(uint64_t)interval.tv_sec*SRVROS_TICKS_PER_SECOND+fraction;
    if (!ticks) return 0;
    /* We may enter part way through the current tick. Include that partial
     * tick so the sleep cannot finish earlier than the requested interval. */
    ticks++;
    long start=srv_monotonic_us();
    int dispatched_before = __posix_signal_dispatch_pending();
    long slept = dispatched_before ? SRV_ERR_INTR : srv_sleep_ticks(ticks);
    int dispatched_after = __posix_signal_dispatch_pending();
    if (slept<0) {
        int error=(slept==SRV_ERR_INTR || dispatched_before || dispatched_after)?EINTR:EIO;
        if (error==EINTR && remaining) {
            long end=srv_monotonic_us();
            uint64_t elapsed=start>=0 && end>=start?(uint64_t)(end-start):
                ((uint64_t)srv_ticks()-now)*10000;
            time_t seconds=(time_t)(elapsed/1000000);
            long nanoseconds=(long)(elapsed%1000000)*1000;
            if (seconds>interval.tv_sec || (seconds==interval.tv_sec && nanoseconds>=interval.tv_nsec)) {
                *remaining=(struct timespec){0};
            } else {
                remaining->tv_sec=interval.tv_sec-seconds;
                remaining->tv_nsec=interval.tv_nsec-nanoseconds;
                if (remaining->tv_nsec<0) { remaining->tv_sec--;remaining->tv_nsec+=1000000000L; }
            }
        }
        errno=error;return -1;
    }
    return 0;
}

int clock_nanosleep(int clock_id, int flags, const struct timespec *request, struct timespec *remaining) {
    if ((clock_id!=CLOCK_REALTIME && clock_id!=CLOCK_MONOTONIC) ||
        (flags!=0 && flags!=TIMER_ABSTIME) || !request || request->tv_sec<0 ||
        request->tv_nsec<0 || request->tv_nsec>=1000000000L) return EINVAL;
    int saved_errno=errno,error=0;
    struct timespec interval=*request;
    if (flags==TIMER_ABSTIME) {
        struct timespec now;
        if (clock_gettime(clock_id,&now)<0) { error=errno;goto done; }
        if (interval.tv_sec<now.tv_sec || (interval.tv_sec==now.tv_sec && interval.tv_nsec<=now.tv_nsec)) goto done;
        interval.tv_sec-=now.tv_sec;interval.tv_nsec-=now.tv_nsec;
        if (interval.tv_nsec<0) { interval.tv_sec--;interval.tv_nsec+=1000000000L; }
    }
    if (nanosleep(&interval,flags==TIMER_ABSTIME?NULL:remaining)<0) error=errno;
done:
    errno=saved_errno;return error;
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
    struct timespec request={.tv_sec=seconds},remaining=request;
    if (nanosleep(&request,&remaining)==0) return 0;
    return (unsigned int)remaining.tv_sec+(remaining.tv_nsec!=0);
}

int usleep(unsigned int usec) {
    struct timespec request = {
        .tv_sec = (time_t)(usec / 1000000u),
        .tv_nsec = (long)(usec % 1000000u) * 1000L,
    };
    return nanosleep(&request, 0);
}
