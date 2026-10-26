#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("epoll-oneshot line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int start_waiters;
struct waiter { int ep,ready; uint64_t tag; };
static void *wait_once(void *arg) {
    struct waiter *w=arg;
    while (!__atomic_load_n(&start_waiters,__ATOMIC_ACQUIRE)) sched_yield();
    struct epoll_event event;
    int n=epoll_wait(w->ep,&event,1,80);
    if (n<0 || n>1 || (n==1 && (event.data.u64!=w->tag || event.events!=EPOLLIN))) return (void *)1;
    w->ready=n;
    return NULL;
}
int posix_epoll_oneshot_test(void) {
    int ep=epoll_create1(0),alias=dup(ep),fd=eventfd(1,EFD_NONBLOCK);
    CHECK(ep>=0 && alias>=0 && fd>=0);
    struct epoll_event event={.events=EPOLLIN|EPOLLONESHOT,.data.u64=123},got;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLIN && got.data.u64==123);
    CHECK(epoll_wait(alias,&got,1,0)==0);
    CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,NULL)==-1 && errno==EINVAL);
    CHECK(epoll_wait(ep,&got,1,0)==0);
    CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==0);
    CHECK(epoll_wait(ep,&got,1,0)==1 && epoll_wait(alias,&got,1,0)==0);
    event.events=EPOLLOUT|EPOLLONESHOT; event.data.u64=456;
    CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==0);
    CHECK(epoll_wait(alias,&got,1,0)==1 && got.events==EPOLLOUT && got.data.u64==456);
    CHECK(epoll_wait(ep,&got,1,0)==0);
    event.events=EPOLLIN; CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&event)==0);
    CHECK(epoll_wait(ep,&got,1,0)==1 && epoll_wait(alias,&got,1,0)==1);
    CHECK(close(fd)==0);

    int p[2]; CHECK(pipe(p)==0 && write(p[1],"x",1)==1);
    event.events=EPOLLIN|EPOLLONESHOT; event.data.u64=100;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,p[0],&event)==0);
    for (int round=0;round<8;round++) {
        event.data.u64=100+(unsigned)round;
        CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,p[0],&event)==0);
        struct waiter waiters[4]; pthread_t workers[4];
        __atomic_store_n(&start_waiters,0,__ATOMIC_RELAXED);
        for (int i=0;i<4;i++) {
            waiters[i]=(struct waiter){.ep=i&1?alias:ep,.tag=event.data.u64};
            CHECK(pthread_create(&workers[i],NULL,wait_once,&waiters[i])==0);
        }
        __atomic_store_n(&start_waiters,1,__ATOMIC_RELEASE);
        int delivered=0;
        for (int i=0;i<4;i++) {
            void *result; CHECK(pthread_join(workers[i],&result)==0 && !result);
            delivered+=waiters[i].ready;
        }
        CHECK(delivered==1 && epoll_wait(ep,&got,1,0)==0);
    }
    CHECK(close(p[1])==0);
    CHECK(epoll_wait(ep,&got,1,0)==0); /* even HUP stays disabled until MOD */
    event.events=EPOLLONESHOT; event.data.u64=789;
    CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,p[0],&event)==0);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLHUP && got.data.u64==789);
    CHECK(epoll_wait(alias,&got,1,0)==0);
    CHECK(close(p[0])==0 && close(alias)==0 && close(ep)==0);
    puts("threadstress: epoll-oneshot ok");
    return 0;
}
