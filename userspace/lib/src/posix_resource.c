#include <errno.h>
#include <srvros/sys.h>
#include <string.h>
#include <sys/resource.h>
#include <unistd.h>

#define SRVROS_DEFAULT_STACK_LIMIT (2ull * 1024ull * 1024ull)
#define SRVROS_DEFAULT_DATA_LIMIT (512ull * 1024ull * 1024ull)
#define SRVROS_DEFAULT_CORE_LIMIT 0ull

static struct rlimit resource_limits[RLIM_NLIMITS];
static int resource_limits_initialized;

static void init_limits(void) {
    if (resource_limits_initialized) {
        return;
    }
    for (int i = 0; i < RLIM_NLIMITS; i++) {
        resource_limits[i].rlim_cur = RLIM_INFINITY;
        resource_limits[i].rlim_max = RLIM_INFINITY;
    }
    resource_limits[RLIMIT_STACK].rlim_cur = SRVROS_DEFAULT_STACK_LIMIT;
    resource_limits[RLIMIT_STACK].rlim_max = SRVROS_DEFAULT_STACK_LIMIT;
    resource_limits[RLIMIT_DATA].rlim_cur = SRVROS_DEFAULT_DATA_LIMIT;
    resource_limits[RLIMIT_DATA].rlim_max = SRVROS_DEFAULT_DATA_LIMIT;
    resource_limits[RLIMIT_CORE].rlim_cur = SRVROS_DEFAULT_CORE_LIMIT;
    resource_limits[RLIMIT_CORE].rlim_max = SRVROS_DEFAULT_CORE_LIMIT;
    resource_limits[RLIMIT_NOFILE].rlim_cur = (rlim_t)sysconf(_SC_OPEN_MAX);
    resource_limits[RLIMIT_NOFILE].rlim_max = (rlim_t)sysconf(_SC_OPEN_MAX);
    resource_limits_initialized = 1;
}

static int valid_resource(int resource) {
    switch (resource) {
    case RLIMIT_CPU:
    case RLIMIT_FSIZE:
    case RLIMIT_DATA:
    case RLIMIT_STACK:
    case RLIMIT_CORE:
    case RLIMIT_RSS:
    case RLIMIT_NOFILE:
    case RLIMIT_AS:
        return 1;
    default:
        return 0;
    }
}

int getrlimit(int resource, struct rlimit *limit) {
    if (limit == 0 || !valid_resource(resource)) {
        errno = EINVAL;
        return -1;
    }
    init_limits();
    *limit = resource_limits[resource];
    return 0;
}

int getrlimit64(int resource, struct rlimit *limit) {
    return getrlimit(resource, limit);
}

int setrlimit(int resource, const struct rlimit *limit) {
    if (limit == 0 || !valid_resource(resource) || limit->rlim_cur > limit->rlim_max) {
        errno = EINVAL;
        return -1;
    }
    init_limits();
    if (limit->rlim_max > resource_limits[resource].rlim_max) {
        errno = EPERM;
        return -1;
    }
    resource_limits[resource] = *limit;
    return 0;
}

int setrlimit64(int resource, const struct rlimit *limit) {
    return setrlimit(resource, limit);
}

int getpriority(int which, int who) {
    (void)which;
    (void)who;
    return 0;
}

int setpriority(int which, int who, int prio) {
    (void)which;
    (void)who;
    (void)prio;
    return 0;
}

int getrusage(int who, struct rusage *usage) {
    if (usage == 0 || (who != RUSAGE_SELF && who != RUSAGE_CHILDREN && who != RUSAGE_THREAD)) {
        errno = EINVAL;
        return -1;
    }
    memset(usage, 0, sizeof(*usage));
    if (who == RUSAGE_CHILDREN) {
        return 0;
    }

    uint64_t ticks = srv_ticks();
    long ticks_per_second = sysconf(_SC_CLK_TCK);
    if (ticks_per_second <= 0) {
        ticks_per_second = 100;
    }
    usage->ru_utime.tv_sec = (time_t)(ticks / (uint64_t)ticks_per_second);
    usage->ru_utime.tv_usec = (suseconds_t)(((ticks % (uint64_t)ticks_per_second) * 1000000ull) /
        (uint64_t)ticks_per_second);

    struct srv_meminfo info;
    if (srv_meminfo(&info) == 0) {
        usage->ru_maxrss = (long)(info.used_bytes / 1024u);
    }
    return 0;
}
