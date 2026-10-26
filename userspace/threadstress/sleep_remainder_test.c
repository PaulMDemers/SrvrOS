#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: sleep-remainder line=%d errno=%d failed\n", __LINE__, errno); return -1; } } while (0)
static unsigned ready;
static pthread_t sleeper;
static volatile sig_atomic_t caught, handler_error;
static uint64_t handler_enter, handler_leave, handler_duration;
static uint64_t now_ns(void) {
    struct timespec value;
    if (clock_gettime(CLOCK_MONOTONIC,&value)) { handler_error=1; return 0; }
    return (uint64_t)value.tv_sec*1000000000+(uint64_t)value.tv_nsec;
}
static void handler(int signal) {
    (void)signal;
    handler_enter=now_ns();
    while (now_ns()-handler_enter<handler_duration) { }
    handler_leave=now_ns(); caught++;
}
static void *interrupt_sleeper(void *unused) {
    (void)unused;
    while (!__atomic_load_n(&ready,__ATOMIC_ACQUIRE)) sched_yield();
    if (usleep(20000) || pthread_kill(sleeper,SIGUSR2)) return (void *)1;
    return NULL;
}
static int run_cases(int audit) {
    struct sigaction action={.sa_handler=handler,.sa_flags=SA_RESTART},old;
    CHECK(sigaction(SIGUSR2,&action,&old)==0);
    sigset_t mask,previous; sigemptyset(&mask); sigaddset(&mask,SIGUSR2);
    CHECK(pthread_sigmask(SIG_UNBLOCK,&mask,&previous)==0);
    unsigned failures=0;
    for (int api=0; api<3; api++) for (int alias=0; alias<2; alias++) {
        caught=handler_error=0; handler_duration=audit ? 600000000 : 0;
        sleeper=pthread_self(); __atomic_store_n(&ready,0,__ATOMIC_RELAXED);
        pthread_t worker; CHECK(pthread_create(&worker,NULL,interrupt_sleeper,NULL)==0);
        struct timespec request={0,500000000},remainder={7,9};
        struct timespec *out=alias ? &request : &remainder;
        uint64_t start=now_ns(); errno=EDOM;
        __atomic_store_n(&ready,1,__ATOMIC_RELEASE);
        int result=api==0 ? nanosleep(&request,out) :
            clock_nanosleep(api==1 ? CLOCK_REALTIME : CLOCK_MONOTONIC,0,&request,out);
        CHECK(result==(api ? EINTR : -1) && errno==(api ? EDOM : EINTR));
        void *value=NULL; CHECK(pthread_join(worker,&value)==0 && !value);
        CHECK(caught==1 && !handler_error && handler_enter>=start && handler_enter-start<250000000);
        uint64_t remaining=(uint64_t)out->tv_sec*1000000000+(uint64_t)out->tv_nsec;
        CHECK(out->tv_sec>=0 && out->tv_nsec>=0 && out->tv_nsec<1000000000);
        if (remaining<200000000 || remaining>500000000) failures++;
        if (audit) printf("threadstress: sleep-handler-audit api=%d alias=%d entry_ns=%llu handler_ns=%llu remaining_ns=%llu\n",
            api,alias,(unsigned long long)(handler_enter-start),(unsigned long long)(handler_leave-handler_enter),
            (unsigned long long)remaining);
    }
    CHECK(sigaction(SIGUSR2,&old,NULL)==0 && pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    if (failures) { printf("threadstress: %s failed cases=%u/6\n",audit ? "sleep-handler-audit" : "sleep-remainder",failures); return 1; }
    puts(audit ? "threadstress: sleep-handler-audit ok\nthreadstress: ok" : "threadstress: sleep-remainder ok");
    return 0;
}
int posix_sleep_remainder_test(void) { return run_cases(0); }
int posix_sleep_handler_audit(void) { return run_cases(1); }
