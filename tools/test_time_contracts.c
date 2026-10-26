/* Real libc timing entry points, with modeled kernel services. */
#include <errno.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/times.h>
#include <limits.h>
#include <time.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("time-contracts: line %d failed: %s\n", __LINE__, #x); exit(1); } } while (0)
static int error_value, realtime_calls, monotonic_calls, usage_calls;
static long realtime_value, monotonic_value, usage_error;
static uint64_t resolution_us;
static long tick_value;
static int allow_ticks, tick_calls;
static int64_t fail_usage_who = SRV_USAGE_SELF;
static struct srv_usage self_usage, child_usage;
static int allow_sleep, sleep_calls, dispatch_calls, before_signal, after_signal;
static long sleep_result, sleep_advance_us, handler_advance_us;
static long post_clock;
static int change_clock;
static uint64_t submitted_ticks;
static long sleep_tick_advance;
static void advance(long us) {
    if (monotonic_value >= 0) monotonic_value += us;
    tick_value += us/10000;
}
int *__errno_location(void) { return &error_value; }
long srv_realtime_us(void) { realtime_calls++; return realtime_value; }
long srv_monotonic_us(void) { monotonic_calls++; return monotonic_value; }
long srv_usage(int64_t who, struct srv_usage *out) {
    CHECK(who == SRV_USAGE_SELF || who == SRV_USAGE_THREAD || who == SRV_USAGE_CHILDREN);
    usage_calls++;
    if (usage_error && who == fail_usage_who) return usage_error;
    *out = who == SRV_USAGE_CHILDREN ? child_usage : self_usage;
    out->resolution_us = resolution_us;
    return 0;
}
/* Other entry points share this translation unit; unexpected services fail. */
long srv_ticks(void) { CHECK(allow_ticks); tick_calls++; return tick_value; }
long srv_sleep_ticks(uint64_t ticks) {
    CHECK(allow_sleep); sleep_calls++; submitted_ticks = ticks;
    advance(sleep_advance_us);
    tick_value += sleep_tick_advance;
    if (change_clock) monotonic_value = post_clock;
    return sleep_result;
}
int __posix_signal_dispatch_pending(void) {
    CHECK(allow_sleep);
    int signal = dispatch_calls++ == 0 ? before_signal : after_signal;
    if (signal) advance(handler_advance_us);
    return signal;
}
int utimensat(int fd, const char *path, const struct timespec times[2], int flags) {
    (void)fd; (void)path; (void)times; (void)flags; CHECK(0); return -1;
}
static void reset(void) {
    realtime_value = 123000000; monotonic_value = 456000000;
    usage_error = 0; resolution_us = 1;
    realtime_calls = monotonic_calls = usage_calls = 0;
    tick_calls = allow_ticks = 0; tick_value = 0;
    fail_usage_who = SRV_USAGE_SELF;
    self_usage = (struct srv_usage){.user_us = 123456, .system_us = 234567};
    child_usage = (struct srv_usage){.user_us = 345678, .system_us = 456789};
    allow_sleep = sleep_calls = dispatch_calls = before_signal = after_signal = 0;
    sleep_result = sleep_advance_us = handler_advance_us = 0;
    change_clock = 0; post_clock = 0; submitted_ticks = 0; sleep_tick_advance = 0;
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
static void check_accounting(const struct tms *usage) {
    CHECK(usage->tms_utime == 12 && usage->tms_stime == 23);
    CHECK(usage->tms_cutime == 34 && usage->tms_cstime == 45);
}
static void times_tests(void) {
    const long samples[] = {0, 1, 9999, 10000, 1234567, LONG_MAX};
    for (int fallback = 0; fallback < 2; fallback++) {
        for (unsigned i = 0; i < sizeof(samples) / sizeof(*samples); i++) {
            for (int output = 0; output < 2; output++) {
                reset();
                allow_ticks = fallback;
                monotonic_value = fallback ? -1 : samples[i];
                tick_value = samples[i];
                struct tms usage = {-7, -8, -9, -10};
                CHECK(times(output ? &usage : NULL) == (fallback ? samples[i] : samples[i]/10000));
                CHECK(errno == EDOM && usage_calls == 2 && monotonic_calls == 1 && tick_calls == fallback);
                if (output) check_accounting(&usage);
                else CHECK(usage.tms_utime == -7 && usage.tms_stime == -8 && usage.tms_cutime == -9 && usage.tms_cstime == -10);
            }
        }
        reset(); allow_ticks = fallback;
        monotonic_value = fallback ? -1 : 1230000; tick_value = 123;
        struct tms usage;
        clock_t start = times(&usage);
        tick_value += 50;
        if (!fallback) monotonic_value += 500000;
        CHECK(times(&usage) - start == 50 && errno == EDOM);
        check_accounting(&usage);
    }
    const int errors[] = {EIO, ESRCH};
    for (unsigned e = 0; e < sizeof(errors)/sizeof(*errors); e++) {
        for (int child = 0; child < 2; child++) {
            for (int output = 0; output < 2; output++) {
                reset(); usage_error = -errors[e];
                fail_usage_who = child ? SRV_USAGE_CHILDREN : SRV_USAGE_SELF;
                struct tms usage = {-7, -8, -9, -10};
                CHECK(times(output ? &usage : NULL) == (clock_t)-1 && errno == errors[e]);
                CHECK(usage_calls == child+1 && monotonic_calls == 0 && tick_calls == 0);
                CHECK(usage.tms_utime == -7 && usage.tms_stime == -8 && usage.tms_cutime == -9 && usage.tms_cstime == -10);
            }
        }
    }
    puts("time-contracts: times reference/fallback, boundaries, accounting and errors PASS");
}
static void sleep_reset(int fallback) {
    reset(); allow_sleep = allow_ticks = 1;
    monotonic_value = fallback ? -1 : 1000000; tick_value = 100;
}
static int relative_sleep(int api, const struct timespec *request, struct timespec *remainder) {
    return api == 0 ? nanosleep(request, remainder) :
        clock_nanosleep(api == 1 ? CLOCK_REALTIME : CLOCK_MONOTONIC, 0, request, remainder);
}
static void interrupted(int api, int result) {
    CHECK(result == (api ? EINTR : -1));
    CHECK(errno == (api ? EDOM : EINTR));
}
static void sleep_tests(void) {
    for (int api = 0; api < 3; api++) {
        for (int fallback = 0; fallback < 2; fallback++) {
            for (int alias = 0; alias < 2; alias++) {
                sleep_reset(fallback); sleep_result = SRV_ERR_INTR; after_signal = 1;
                sleep_advance_us = 250000;
                struct timespec request = {.tv_sec=1,.tv_nsec=123}, remainder = {7,9};
                struct timespec *out = alias ? &request : &remainder;
                interrupted(api, relative_sleep(api, &request, out));
                CHECK(out->tv_sec == 0 && out->tv_nsec == 750000123);
                CHECK(sleep_calls == 1 && submitted_ticks == 102 && dispatch_calls == 2);
            }
            sleep_reset(fallback); before_signal = 1;
            struct timespec request = {1,123}, remainder = {7,9};
            interrupted(api, relative_sleep(api,&request,&remainder));
            CHECK(remainder.tv_sec == 1 && remainder.tv_nsec == 123 && sleep_calls == 0);
            sleep_reset(fallback); sleep_result = SRV_ERR_INTR; sleep_advance_us = 2000000;
            interrupted(api, relative_sleep(api,&request,&remainder));
            CHECK(remainder.tv_sec == 0 && remainder.tv_nsec == 0);
            sleep_reset(fallback); sleep_result = -1;
            remainder = (struct timespec){7,9};
            CHECK(relative_sleep(api,&request,&remainder) == (api ? EIO : -1));
            CHECK(errno == (api ? EDOM : EIO) && remainder.tv_sec == 7 && remainder.tv_nsec == 9);
            sleep_reset(fallback); sleep_result = SRV_ERR_INTR;
            interrupted(api, relative_sleep(api,&request,NULL));
            sleep_reset(fallback); request = (struct timespec){0,1};
            CHECK(relative_sleep(api,&request,&remainder) == 0 && submitted_ticks == 2);
            CHECK(remainder.tv_sec == 7 && remainder.tv_nsec == 9);
            sleep_reset(fallback); request = (struct timespec){0};
            CHECK(relative_sleep(api,&request,&remainder) == 0 && sleep_calls == 0);
            const struct timespec invalid[] = {{-1,0},{0,-1},{0,1000000000},{LONG_MAX,0}};
            for (unsigned i=0; i<sizeof(invalid)/sizeof(*invalid); i++) {
                sleep_reset(fallback);
                CHECK(relative_sleep(api,&invalid[i],&remainder) == (api ? EINVAL : -1));
                CHECK(errno == (api ? EDOM : EINVAL) && sleep_calls == 0 && dispatch_calls == 0);
                CHECK(remainder.tv_sec == 7 && remainder.tv_nsec == 9);
            }
        }
    }
    /* Reference failure or regression at wake uses the tick fallback. */
    for (int failed=0; failed<2; failed++) {
        sleep_reset(0); sleep_result=SRV_ERR_INTR; sleep_advance_us=250000;
        change_clock=1; post_clock=failed ? -1 : 500000;
        struct timespec request={1,123},remainder;
        CHECK(nanosleep(&request,&remainder)==-1 && errno==EINTR);
        CHECK(remainder.tv_sec==0 && remainder.tv_nsec==750000123);
    }
    for (int clock_id=CLOCK_REALTIME; clock_id<=CLOCK_MONOTONIC; clock_id++) {
        for (int alias=0; alias<2; alias++) {
            sleep_reset(0); realtime_value=monotonic_value;
            sleep_result=SRV_ERR_INTR; sleep_advance_us=250000; after_signal=1;
            struct timespec request={2,123},remainder={7,9};
            struct timespec *out=alias ? &request : &remainder;
            CHECK(clock_nanosleep(clock_id,TIMER_ABSTIME,&request,out)==EINTR && errno==EDOM);
            CHECK(out->tv_sec==(alias ? 2 : 7) && out->tv_nsec==(alias ? 123 : 9));
        }
    }
    puts("time-contracts: sleep interruption, aliasing, fallback, boundaries and absolute output PASS");
}
static int sleep_handler_audit(void) {
    unsigned failures=0;
    for (int api=0; api<3; api++) for (int fallback=0; fallback<2; fallback++)
        for (int before=0; before<2; before++) for (int alias=0; alias<2; alias++) {
            sleep_reset(fallback); sleep_result=SRV_ERR_INTR;
            before_signal=before; after_signal=!before;
            sleep_advance_us=250000; handler_advance_us=1500000;
            struct timespec request={1,123},remainder={7,9};
            struct timespec *out=alias ? &request : &remainder;
            interrupted(api,relative_sleep(api,&request,out));
            long seconds=before ? 1 : 0, nanos=before ? 123 : 750000123;
            printf("time-contracts: handler audit api=%d fallback=%d before=%d alias=%d remaining=%lld.%09ld expected=%ld.%09ld\n",
                api,fallback,before,alias,(long long)out->tv_sec,out->tv_nsec,seconds,nanos);
            if(out->tv_sec!=seconds || out->tv_nsec!=nanos) failures++;
        }
    printf("time-contracts: handler audit failures=%u/24\n",failures);
    return failures ? 1 : 0;
}
static int sleep_overflow_audit(void) {
    unsigned failures=0;
    /* The first elapsed tick count whose conversion to microseconds wraps. */
    uint64_t delta=UINT64_MAX/10000+1;
    for(int api=0; api<3; api++) for(int alias=0; alias<2; alias++) {
        sleep_reset(1); sleep_result=SRV_ERR_INTR; sleep_tick_advance=(long)delta;
        struct timespec request={(time_t)(delta/100+1),0},remainder={7,9};
        struct timespec *out=alias ? &request : &remainder;
        interrupted(api,relative_sleep(api,&request,out));
        long expected=(long)(100-delta%100)*10000000;
        printf("time-contracts: overflow audit api=%d alias=%d remaining=%lld.%09ld expected=0.%09ld\n",
            api,alias,(long long)out->tv_sec,out->tv_nsec,expected);
        if(out->tv_sec!=0 || out->tv_nsec!=expected) failures++;
    }
    printf("time-contracts: overflow audit failures=%u/6\n",failures);
    return failures ? 1 : 0;
}
int main(int argc, char **argv) {
    if(argc==2 && !strcmp(argv[1],"sleep-handler-audit")) return sleep_handler_audit();
    if(argc==2 && !strcmp(argv[1],"sleep-overflow-audit")) return sleep_overflow_audit();
    resolution_tests(); times_tests(); sleep_tests(); return 0;
}
