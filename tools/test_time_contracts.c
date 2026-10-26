/* Real libc timing entry points, with modeled kernel services. */
#include <errno.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <string.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { printf("time-contracts: line %d failed: %s\n", __LINE__, #x); exit(1); } } while (0)
static int error_value, realtime_calls, monotonic_calls, usage_calls;
static long realtime_value, monotonic_value, usage_error;
static uint64_t resolution_us;
static long tick_value;
static int allow_ticks;
int *__errno_location(void) { return &error_value; }
long srv_realtime_us(void) { realtime_calls++; return realtime_value; }
long srv_monotonic_us(void) { monotonic_calls++; return monotonic_value; }
long srv_usage(int64_t who, struct srv_usage *out) {
    CHECK(who == SRV_USAGE_SELF || who == SRV_USAGE_THREAD || who == SRV_USAGE_CHILDREN);
    usage_calls++;
    if (usage_error) return usage_error;
    *out = (struct srv_usage){.resolution_us = resolution_us};
    return 0;
}
/* Other entry points share this translation unit; unexpected services fail. */
long srv_ticks(void) { CHECK(allow_ticks); return tick_value; }
long srv_sleep_ticks(uint64_t ticks) { (void)ticks; CHECK(0); return 0; }
int __posix_signal_dispatch_pending(void) { CHECK(0); return 0; }
int utimensat(int fd, const char *path, const struct timespec times[2], int flags) {
    (void)fd; (void)path; (void)times; (void)flags; CHECK(0); return -1;
}
static void reset(void) {
    realtime_value = 123000000; monotonic_value = 456000000;
    usage_error = 0; resolution_us = 1;
    realtime_calls = monotonic_calls = usage_calls = 0;
    errno = EDOM;
}
static void resolution_tests(void) {
    const int clocks[] = {CLOCK_REALTIME, CLOCK_MONOTONIC,
        CLOCK_PROCESS_CPUTIME_ID, CLOCK_THREAD_CPUTIME_ID};
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(*clocks); i++) {
        for (int fallback = 0; fallback < 2; fallback++) {
            for (int output = 0; output < 2; output++) {
                reset();
                if (fallback) { monotonic_value = -1; resolution_us = 10000; }
                struct timespec res = {.tv_sec = 7, .tv_nsec = 9};
                CHECK(clock_getres(clocks[i], output ? &res : NULL) == 0 && errno == EDOM);
                CHECK(res.tv_sec == (output ? 0 : 7));
                CHECK(res.tv_nsec == (output ? (fallback ? 10000000 : 1000) : 9));
                /* Source validation must not disappear when no output is requested. */
                if (clocks[i] == CLOCK_REALTIME) CHECK(realtime_calls == 1 && usage_calls == 0);
                if (clocks[i] == CLOCK_MONOTONIC) CHECK(realtime_calls == 0 && usage_calls == 0);
                if (clocks[i] >= CLOCK_PROCESS_CPUTIME_ID) CHECK(usage_calls == 1 && realtime_calls == 0);
            }
        }
    }
    const int invalid[] = {-1, 4, 99};
    for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
        for (int output = 0; output < 2; output++) {
            reset();
            struct timespec res = {.tv_sec = 7, .tv_nsec = 9};
            CHECK(clock_getres(invalid[i], output ? &res : NULL) == -1 && errno == EINVAL);
            CHECK(res.tv_sec == 7 && res.tv_nsec == 9);
            CHECK(realtime_calls == 0 && monotonic_calls == 0 && usage_calls == 0);
        }
    }
    for (int output = 0; output < 2; output++) {
        reset(); realtime_value = -1;
        struct timespec res = {.tv_sec = 7, .tv_nsec = 9};
        CHECK(clock_getres(CLOCK_REALTIME, output ? &res : NULL) == -1 && errno == EIO);
        CHECK(res.tv_sec == 7 && res.tv_nsec == 9 && realtime_calls == 1);
        CHECK(monotonic_calls == 0 && usage_calls == 0);
        const int errors[] = {EIO, ESRCH};
        for (unsigned e = 0; e < sizeof(errors) / sizeof(*errors); e++) {
            for (int clock_id = CLOCK_PROCESS_CPUTIME_ID; clock_id <= CLOCK_THREAD_CPUTIME_ID; clock_id++) {
                reset(); usage_error = -errors[e];
                CHECK(clock_getres(clock_id, output ? &res : NULL) == -1 && errno == errors[e]);
                CHECK(res.tv_sec == 7 && res.tv_nsec == 9 && usage_calls == 1);
            }
        }
        /* The monotonic fallback must not depend on realtime availability. */
        reset(); realtime_value = monotonic_value = -1;
        CHECK(clock_getres(CLOCK_MONOTONIC, output ? &res : NULL) == 0 && errno == EDOM);
        CHECK(realtime_calls == 0);
        if (output) CHECK(res.tv_sec == 0 && res.tv_nsec == 10000000);
    }
    puts("time-contracts: optional resolution, invalid clocks, source errors and fallback PASS");
}
/* Explicit known-failure audit; excluded from default positive checks. */
static void times_fallback_audit(void) {
    reset();
    struct tms usage;
    struct timespec before, after;
    monotonic_value = 1230000;
    clock_t high_start = times(&usage);
    monotonic_value += 500000;
    CHECK(times(&usage) - high_start == 50);
    monotonic_value = -1; allow_ticks = 1; tick_value = 123;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &before) == 0);
    clock_t start = times(&usage);
    tick_value += 50;
    CHECK(clock_gettime(CLOCK_MONOTONIC, &after) == 0);
    clock_t end = times(&usage);
    CHECK((after.tv_sec-before.tv_sec)*1000000000L + after.tv_nsec-before.tv_nsec == 500000000L);
    printf("time-contracts: times fallback start=%ld end=%ld delta=%ld expected=50\n", start, end, end-start);
    CHECK(end-start == 50);
}
int main(int argc, char **argv) {
    if (argc == 2 && !strcmp(argv[1], "times-fallback-audit")) { times_fallback_audit(); return 0; }
    resolution_tests(); return 0;
}
