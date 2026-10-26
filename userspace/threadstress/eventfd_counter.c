#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("eventfd-counter line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int start_writers;
struct writer { int fd, accepted, refused; };
static void *fill_worker(void *arg) {
    struct writer *w=arg;
    while (!__atomic_load_n(&start_writers,__ATOMIC_ACQUIRE)) sched_yield();
    uint64_t one=1;
    for (int i=0;i<32;i++) {
        ssize_t n=write(w->fd,&one,8);
        if (n==8) w->accepted++;
        else if (n==-1 && errno==EAGAIN) w->refused++;
        else return (void *)1;
        sched_yield();
    }
    return NULL;
}
static void *drain_worker(void *arg) {
    if (usleep(30000)) return (void *)1;
    uint64_t value;
    if (read((int)(intptr_t)arg,&value,8)!=8 || value!=UINT64_MAX-1) return (void *)1;
    return NULL;
}
int posix_eventfd_counter_test(void) {
    int fd=eventfd(UINT_MAX,EFD_NONBLOCK), ep=epoll_create1(0);
    CHECK(fd>=0 && ep>=0);
    struct epoll_event watch={.events=EPOLLIN|EPOLLOUT,.data.u64=123},got;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&watch)==0);
    uint64_t value=UINT64_MAX-2-UINT_MAX, readback;
    CHECK(write(fd,&value,8)==8);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==(EPOLLIN|EPOLLOUT));
    value=2; CHECK(write(fd,&value,8)==-1 && errno==EAGAIN);
    value=1; CHECK(write(fd,&value,8)==8);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLIN);
    value=0; CHECK(write(fd,&value,8)==8);
    value=UINT64_MAX; CHECK(write(fd,&value,8)==-1 && errno==EINVAL);
    value=1; CHECK(write(fd,&value,8)==-1 && errno==EAGAIN);
    CHECK(read(fd,&readback,8)==8 && readback==UINT64_MAX-1);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLOUT);
    value=UINT64_MAX; CHECK(write(fd,&value,8)==-1 && errno==EINVAL);
    CHECK(read(fd,&readback,8)==-1 && errno==EAGAIN);

    /* Four real writers compete for only 31 remaining increments. Failed
     * writes must neither change the counter nor consume another writer's room. */
    for (int round=0;round<8;round++) {
        value=UINT64_MAX-1-31; CHECK(write(fd,&value,8)==8);
        pthread_t threads[4]; struct writer writers[4];
        __atomic_store_n(&start_writers,0,__ATOMIC_RELAXED);
        for (int i=0;i<4;i++) {
            writers[i]=(struct writer){.fd=fd};
            CHECK(pthread_create(&threads[i],NULL,fill_worker,&writers[i])==0);
        }
        __atomic_store_n(&start_writers,1,__ATOMIC_RELEASE);
        int accepted=0,refused=0;
        for (int i=0;i<4;i++) {
            void *result; CHECK(pthread_join(threads[i],&result)==0 && !result);
            accepted+=writers[i].accepted; refused+=writers[i].refused;
        }
        CHECK(accepted==31 && refused==97);
        CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLIN && got.data.u64==123);
        CHECK(read(fd,&readback,8)==8 && readback==UINT64_MAX-1);
        CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLOUT);
    }
    value=UINT64_MAX-1; CHECK(write(fd,&value,8)==8);
    watch.events=EPOLLOUT; CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,fd,&watch)==0);
    CHECK(epoll_wait(ep,&got,1,0)==0);
    pthread_t drainer; CHECK(pthread_create(&drainer,NULL,drain_worker,(void *)(intptr_t)fd)==0);
    CHECK(epoll_wait(ep,&got,1,2000)==1 && got.events==EPOLLOUT);
    void *result; CHECK(pthread_join(drainer,&result)==0 && !result);
    CHECK(read(fd,&readback,8)==-1 && errno==EAGAIN);
    CHECK(close(fd)==0 && close(ep)==0);
    puts("threadstress: eventfd-counter ok");
    return 0;
}
