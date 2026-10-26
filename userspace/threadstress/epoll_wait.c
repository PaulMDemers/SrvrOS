#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <time.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("epoll-wait line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int started;
static volatile sig_atomic_t caught;
struct action { int kind, ep, fd; pthread_t waiter; };
static void handler(int sig) { (void)sig; caught++; }
static int add(int ep,int fd,uint64_t tag) {
    struct epoll_event event={.events=EPOLLIN,.data.u64=tag};
    return epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event);
}
static int64_t now_ns(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC,&now)) return -1;
    return now.tv_sec*1000000000+now.tv_nsec;
}
static void *change(void *arg) {
    struct action *a=arg;
    while (!__atomic_load_n(&started,__ATOMIC_ACQUIRE)) sched_yield();
    if (usleep(30000)) return (void *)1;
    uint64_t one=1;
    int result;
    switch (a->kind) {
    case 0: result=write(a->fd,&one,8)==8?0:-1; break;
    case 1: result=write(a->fd,"x",1)==1?0:-1; break;
    case 2: result=add(a->ep,a->fd,321); break;
    case 3: result=close(a->ep); break;
    default: result=pthread_kill(a->waiter,SIGUSR2); break;
    }
    return result ? (void *)1 : NULL;
}
static int changed_wait(int kind,int timeout,int mixed) {
    int ep=epoll_create1(0), fd=eventfd(kind==2?1:0,EFD_NONBLOCK), p[2];
    CHECK(ep>=0 && fd>=0 && pipe(p)==0);
    if (kind!=2 && kind!=3) CHECK(add(ep,fd,321)==0);
    if (mixed || kind==1) CHECK(add(ep,p[0],123)==0);
    struct action action={kind,ep,kind==1?p[1]:fd,pthread_self()};
    pthread_t worker; __atomic_store_n(&started,0,__ATOMIC_RELAXED);
    CHECK(pthread_create(&worker,NULL,change,&action)==0);
    struct epoll_event events[2];
    __atomic_store_n(&started,1,__ATOMIC_RELEASE);
    int n=epoll_wait(ep,events,2,timeout), error=errno;
    void *result; CHECK(pthread_join(worker,&result)==0 && !result);
    if (kind==3) CHECK(n==-1 && error==EBADF);
    else if (kind==4) CHECK(n==-1 && error==EINTR && caught);
    else CHECK(n==1 && events[0].data.u64==(kind==1?123:321));
    if (kind!=3) CHECK(close(ep)==0);
    CHECK(close(fd)==0 && close(p[0])==0 && close(p[1])==0);
    return 0;
}
int posix_epoll_wait_test(void) {
    int ep=epoll_create1(0), p[2]; struct epoll_event events[2];
    CHECK(ep>=0 && pipe(p)==0);
    int64_t start=now_ns(); CHECK(start>=0);
    CHECK(epoll_wait(ep,events,2,23)==0 && now_ns()-start>=23000000);
    CHECK(add(ep,p[0],123)==0);
    for (int i=0;i<20;i++) CHECK(epoll_wait(ep,events,2,0)==0);
    int fd=eventfd(1,EFD_NONBLOCK); CHECK(fd>=0 && add(ep,fd,321)==0);
    /* Ready synthetic state must not block on the idle native pipe. */
    CHECK(epoll_wait(ep,events,2,-1)==1 && events[0].data.u64==321);
    CHECK(close(ep)==0 && close(fd)==0 && close(p[0])==0 && close(p[1])==0);
    struct sigaction action={.sa_handler=handler,.sa_flags=SA_RESTART},old;
    sigset_t empty,previous; sigemptyset(&empty);
    CHECK(sigaction(SIGUSR2,&action,&old)==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&empty,&previous)==0);
    for (int i=0;i<4;i++) {
        CHECK(changed_wait(0,-1,0)==0);       /* synthetic only */
        CHECK(changed_wait(0,1000,1)==0);     /* mixed descriptors */
        CHECK(changed_wait(1,-1,0)==0);       /* native readiness */
        CHECK(changed_wait(2,-1,0)==0);       /* ADD into an empty wait */
        CHECK(changed_wait(3,-1,0)==0);       /* close an empty instance */
        caught=0; CHECK(changed_wait(4,INT_MAX,i%2)==0);
    }
    CHECK(sigaction(SIGUSR2,&old,NULL)==0);
    CHECK(pthread_sigmask(SIG_SETMASK,&previous,NULL)==0);
    puts("threadstress: epoll-wait ok");
    return 0;
}
