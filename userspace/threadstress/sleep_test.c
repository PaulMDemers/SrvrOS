#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>

static volatile sig_atomic_t received;
static unsigned sleep_started;
static void handle_signal(int signal) { if(signal==SIGUSR2) received++; }
static void *wake_sleeper(void *unused) {
    (void)unused;
    while(!__atomic_load_n(&sleep_started,__ATOMIC_ACQUIRE)) sched_yield();
    if(usleep(20000) || kill(getpid(),SIGUSR2)) return (void *)1;
    return NULL;
}
#define CHECK(x) do { if(!(x)) { printf("threadstress: sleep line=%d errno=%d\n",__LINE__,errno);return -1;} } while(0)
int posix_sleep_test(void) {
    struct sigaction handler={.sa_handler=handle_signal,.sa_flags=SA_RESTART|SA_RESETHAND},old;
    CHECK(sigaction(SIGUSR2,&handler,&old)==0);
    struct sigaction unsupported=handler;unsupported.sa_flags=SA_SIGINFO;errno=0;
    CHECK(sigaction(SIGUSR2,&unsupported,NULL)==-1 && errno==EINVAL);
    unsupported.sa_flags=SA_ONSTACK;CHECK(sigaction(SIGUSR2,&unsupported,NULL)==-1 && errno==EINVAL);
    unsupported.sa_flags=0;CHECK(sigaction(SIGKILL,&unsupported,NULL)==-1 && errno==EINVAL);
    CHECK(sigaction(SIGSTOP,&unsupported,NULL)==-1 && errno==EINVAL);
    sigset_t block,previous,current;sigemptyset(&block);
    sigaddset(&block,SIGUSR2);sigaddset(&block,SIGKILL);sigaddset(&block,SIGSTOP);
    CHECK(pthread_sigmask(SIG_BLOCK,&block,&previous)==0);
    CHECK(pthread_sigmask(SIG_BLOCK,NULL,&current)==0);
    CHECK(!sigismember(&current,SIGKILL) && !sigismember(&current,SIGSTOP));
    pthread_t worker;received=0;__atomic_store_n(&sleep_started,0,__ATOMIC_RELAXED);
    CHECK(pthread_create(&worker,NULL,wake_sleeper,NULL)==0);
    CHECK(pthread_sigmask(SIG_UNBLOCK,&block,NULL)==0);
    struct timespec interval={.tv_sec=1,.tv_nsec=123},start,end;
    CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0);
    __atomic_store_n(&sleep_started,1,__ATOMIC_RELEASE);
    errno=0;CHECK(nanosleep(&interval,&interval)==-1 && errno==EINTR);
    CHECK(received==1 && interval.tv_sec==0 && interval.tv_nsec>0 && interval.tv_nsec<1000000000);
    CHECK(clock_gettime(CLOCK_MONOTONIC,&end)==0);
    CHECK(end.tv_sec-start.tv_sec<=1);
    void *result=NULL;CHECK(pthread_join(worker,&result)==0 && !result);
    struct sigaction reset;CHECK(sigaction(SIGUSR2,NULL,&reset)==0 && reset.sa_handler==SIG_DFL);
    CHECK(sigaction(SIGUSR2,&old,NULL)==0 && pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    interval=(struct timespec){.tv_nsec=30000000};
    CHECK(clock_gettime(CLOCK_MONOTONIC,&start)==0 && nanosleep(&interval,NULL)==0 && clock_gettime(CLOCK_MONOTONIC,&end)==0);
    CHECK((end.tv_sec-start.tv_sec)*1000000000LL+end.tv_nsec-start.tv_nsec>=30000000);
    struct timespec absolute=end;
    absolute.tv_nsec+=30000000;if(absolute.tv_nsec>=1000000000) { absolute.tv_sec++;absolute.tv_nsec-=1000000000; }
    interval=(struct timespec){.tv_sec=7,.tv_nsec=123};errno=EDOM;
    CHECK(clock_nanosleep(CLOCK_MONOTONIC,TIMER_ABSTIME,&absolute,&interval)==0 && errno==EDOM);
    CHECK(interval.tv_sec==7 && interval.tv_nsec==123);
    CHECK(clock_gettime(CLOCK_MONOTONIC,&end)==0 &&
        (end.tv_sec>absolute.tv_sec || (end.tv_sec==absolute.tv_sec && end.tv_nsec>=absolute.tv_nsec)));
    absolute=(struct timespec){0};CHECK(clock_nanosleep(CLOCK_REALTIME,TIMER_ABSTIME,&absolute,NULL)==0);
    CHECK(clock_nanosleep(99,0,&absolute,NULL)==EINVAL && errno==EDOM);
    absolute.tv_nsec=1000000000;CHECK(nanosleep(&absolute,NULL)==-1 && errno==EINVAL);
    puts("threadstress: sleep ok");return 0;
}
