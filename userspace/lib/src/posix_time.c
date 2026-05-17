#include <errno.h>
#include <srvros/sys.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <utime.h>

#define SRVROS_TICKS_PER_SECOND 100

time_t time(time_t *out) {
    time_t seconds = (time_t)(srv_ticks() / SRVROS_TICKS_PER_SECOND);
    if (out != 0) {
        *out = seconds;
    }
    return seconds;
}

static int is_leap_year(int year) {
    return ((year % 4) == 0 && (year % 100) != 0) || (year % 400) == 0;
}

static int month_days(int year, int month) {
    static const int days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (month == 1 && is_leap_year(year)) {
        return 29;
    }
    return days[month];
}

struct tm *localtime(const time_t *timer) {
    static struct tm result;
    long long seconds = timer != 0 ? *timer : 0;
    if (seconds < 0) {
        seconds = 0;
    }

    long long days = seconds / 86400;
    long long rem = seconds % 86400;
    result.tm_hour = (int)(rem / 3600);
    rem %= 3600;
    result.tm_min = (int)(rem / 60);
    result.tm_sec = (int)(rem % 60);
    result.tm_wday = (int)((days + 4) % 7);

    int year = 1970;
    while (1) {
        int year_days = is_leap_year(year) ? 366 : 365;
        if (days < year_days) {
            break;
        }
        days -= year_days;
        year++;
    }

    result.tm_year = year - 1900;
    result.tm_yday = (int)days;
    int month = 0;
    while (month < 11) {
        int mdays = month_days(year, month);
        if (days < mdays) {
            break;
        }
        days -= mdays;
        month++;
    }
    result.tm_mon = month;
    result.tm_mday = (int)days + 1;
    result.tm_isdst = 0;
    return &result;
}

time_t mktime(struct tm *timeptr) {
    if (timeptr == 0) {
        errno = EINVAL;
        return (time_t)-1;
    }
    int year = timeptr->tm_year + 1900;
    if (year < 1970 || timeptr->tm_mon < 0 || timeptr->tm_mon > 11 || timeptr->tm_mday < 1) {
        errno = EINVAL;
        return (time_t)-1;
    }

    long long days = 0;
    for (int y = 1970; y < year; y++) {
        days += is_leap_year(y) ? 366 : 365;
    }
    for (int m = 0; m < timeptr->tm_mon; m++) {
        days += month_days(year, m);
    }
    days += timeptr->tm_mday - 1;
    return (time_t)(days * 86400 +
        timeptr->tm_hour * 3600 +
        timeptr->tm_min * 60 +
        timeptr->tm_sec);
}

clock_t clock(void) {
    return (clock_t)srv_ticks();
}

int utime(const char *path, const struct utimbuf *times) {
    (void)times;
    struct stat st;
    return stat(path, &st);
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
    return srv_sleep_ticks(ticks) < 0 ? -1 : 0;
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
    uint64_t ticks = (uint64_t)srv_ticks();
    tv->tv_sec = (time_t)(ticks / SRVROS_TICKS_PER_SECOND);
    tv->tv_usec = (suseconds_t)((ticks % SRVROS_TICKS_PER_SECOND) * (1000000ull / SRVROS_TICKS_PER_SECOND));
    return 0;
}

unsigned int sleep(unsigned int seconds) {
    return srv_sleep_ticks((uint64_t)seconds * SRVROS_TICKS_PER_SECOND) < 0 ? seconds : 0;
}

int usleep(unsigned int usec) {
    struct timespec request = {
        .tv_sec = (time_t)(usec / 1000000u),
        .tv_nsec = (long)(usec % 1000000u) * 1000L,
    };
    return nanosleep(&request, 0);
}
