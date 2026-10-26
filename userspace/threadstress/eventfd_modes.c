#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("eventfd-modes line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int start_readers;
struct reader { int fd, consumed, empty; };
static void *consume(void *arg) {
    struct reader *r=arg;
    while (!__atomic_load_n(&start_readers,__ATOMIC_ACQUIRE)) sched_yield();
    for (int i=0;i<32;i++) {
        uint64_t value=0;
        ssize_t n=read(r->fd,&value,8);
        if (n==8 && value==1) r->consumed++;
        else if (n==-1 && errno==EAGAIN) r->empty++;
        else return (void *)1;
        sched_yield();
    }
    return NULL;
}
int posix_eventfd_modes_test(void) {
    CHECK(eventfd(0,0x40000000)==-1 && errno==EINVAL);
    CHECK(eventfd(0,-1)==-1 && errno==EINVAL);
    uint64_t value;
    for (int bits=0;bits<8;bits++) {
        int flags=(bits&1?EFD_SEMAPHORE:0)|(bits&2?EFD_NONBLOCK:0)|(bits&4?EFD_CLOEXEC:0);
        int fd=eventfd(3,flags); CHECK(fd>=0);
        CHECK(fcntl(fd,F_GETFD)==(bits&4?FD_CLOEXEC:0));
        CHECK((fcntl(fd,F_GETFL)&O_NONBLOCK)==(bits&2?O_NONBLOCK:0));
        CHECK(fcntl(fd,F_SETFD,0)==0 && fcntl(fd,F_GETFD)==0);
        CHECK(fcntl(fd,F_SETFL,O_NONBLOCK)==0);
        CHECK(read(fd,&value,8)==8 && value==(bits&1?1:3));
        if (bits&1) {
            CHECK(read(fd,&value,8)==8 && value==1);
            CHECK(read(fd,&value,8)==8 && value==1);
        }
        CHECK(read(fd,&value,8)==-1 && errno==EAGAIN);
        CHECK(close(fd)==0);
    }
    int fd=eventfd(0,EFD_NONBLOCK|EFD_SEMAPHORE),ep=epoll_create1(0);
    CHECK(fd>=0 && ep>=0);
    struct epoll_event watch={.events=EPOLLIN|EPOLLOUT,.data.u64=123},got;
    CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&watch)==0);
    for (int round=0;round<8;round++) {
        value=31; CHECK(write(fd,&value,8)==8);
        CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==(EPOLLIN|EPOLLOUT));
        struct reader readers[4]; pthread_t workers[4];
        __atomic_store_n(&start_readers,0,__ATOMIC_RELAXED);
        for (int i=0;i<4;i++) {
            readers[i]=(struct reader){.fd=fd};
            CHECK(pthread_create(&workers[i],NULL,consume,&readers[i])==0);
        }
        __atomic_store_n(&start_readers,1,__ATOMIC_RELEASE);
        int consumed=0,empty=0;
        for (int i=0;i<4;i++) {
            void *result; CHECK(pthread_join(workers[i],&result)==0 && !result);
            consumed+=readers[i].consumed; empty+=readers[i].empty;
        }
        CHECK(consumed==31 && empty==97);
        CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLOUT);
    }
    value=UINT64_MAX-1; CHECK(write(fd,&value,8)==8);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLIN);
    CHECK(read(fd,&value,8)==8 && value==1);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==(EPOLLIN|EPOLLOUT));
    value=2; CHECK(write(fd,&value,8)==-1 && errno==EAGAIN);
    value=1; CHECK(write(fd,&value,8)==8);
    CHECK(epoll_wait(ep,&got,1,0)==1 && got.events==EPOLLIN);
    CHECK(close(fd)==0 && close(ep)==0);
    puts("threadstress: eventfd-modes ok");
    return 0;
}
