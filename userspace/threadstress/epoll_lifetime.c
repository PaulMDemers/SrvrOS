#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <sys/wait.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("epoll-lifetime line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int add(int ep,int fd,uint64_t tag) {
    struct epoll_event e={.events=EPOLLIN,.data.u64=tag};
    return epoll_ctl(ep,EPOLL_CTL_ADD,fd,&e);
}
static int instance_cycle(uint64_t tag) {
    struct epoll_event event;
    int fd=eventfd(1,EFD_NONBLOCK), ep=epoll_create1(EPOLL_CLOEXEC);
    CHECK(fd>=0 && ep>=0 && add(ep,fd,tag)==0);
    int alias=dup(ep); CHECK(alias>=0);
    CHECK(close(ep)==0);
    CHECK(epoll_wait(alias,&event,1,0)==1 && event.data.u64==tag);
    CHECK(close(fd)==0 && epoll_wait(alias,&event,1,0)==0);
    CHECK(close(alias)==0);
    return 0;
}
static void *lifetime_worker(void *arg) {
    for (int i=0;i<16;i++) if (instance_cycle((uintptr_t)arg*100+(unsigned)i)) return (void *)1;
    return NULL;
}
int posix_epoll_lifetime_test(void) {
    struct epoll_event events[4];
    for (int i=0;i<24;i++) CHECK(instance_cycle((unsigned)i)==0);
    int ep=epoll_create1(0), fd=eventfd(1,EFD_NONBLOCK); CHECK(ep>=0 && fd>=0);
    CHECK(add(ep,fd,123)==0 && close(ep)==0);
    int fresh=epoll_create1(0); CHECK(fresh==ep && epoll_wait(fresh,events,4,0)==0);
    CHECK(add(fresh,fd,234)==0 && close(fd)==0);
    int next=eventfd(2,EFD_NONBLOCK); CHECK(next==fd && epoll_wait(fresh,events,4,0)==0);
    CHECK(close(next)==0);

    int p[2]; CHECK(pipe(p)==0);
    int alias=dup(p[0]); CHECK(alias>=0);
    CHECK(add(fresh,p[0],10)==0 && add(fresh,alias,20)==0);
    CHECK(write(p[1],"x",1)==1 && close(p[0])==0);
    CHECK(epoll_wait(fresh,events,4,0)==2 && events[0].data.u64==10 && events[1].data.u64==20);
    CHECK(close(alias)==0 && epoll_wait(fresh,events,4,0)==0 && close(p[1])==0);

    int a=fcntl(fresh,F_DUPFD,20), b=fcntl(fresh,F_DUPFD_CLOEXEC,30);
    CHECK(a>=20 && b>=30 && fcntl(a,F_GETFD)==0 && fcntl(b,F_GETFD)==FD_CLOEXEC);
    int other=epoll_create1(0), other_alias=dup(other); CHECK(other>=0 && other_alias>=0);
    fd=eventfd(1,EFD_NONBLOCK); CHECK(fd>=0);
    CHECK(add(other,fd,555)==0 && add(fresh,fd,666)==0);
    CHECK(dup2(fresh,other)==other && dup3(fresh,40,O_CLOEXEC)==40);
    CHECK(epoll_wait(other,events,4,0)==1 && events[0].data.u64==666);
    CHECK(epoll_wait(other_alias,events,4,0)==1 && events[0].data.u64==555);
    CHECK(dup2(-1,other)==-1 && errno==EBADF);
    CHECK(epoll_wait(other,events,4,0)==1 && events[0].data.u64==666);
    CHECK(dup2(other,other)==other && dup3(other,other,0)==-1 && errno==EINVAL);
    CHECK(fcntl(40,F_GETFD)==FD_CLOEXEC);
    CHECK(close(fresh)==0 && close(a)==0 && close(b)==0 && close(other)==0);
    CHECK(epoll_wait(40,events,4,0)==1 && events[0].data.u64==666);
    CHECK(close(40)==0 && close(other_alias)==0 && close(fd)==0);

    pthread_t threads[4];
    for (int i=0;i<4;i++) CHECK(pthread_create(&threads[i],NULL,lifetime_worker,(void *)(uintptr_t)(i+1))==0);
    for (int i=0;i<4;i++) { void *result; CHECK(pthread_join(threads[i],&result)==0 && !result); }
    /* No epoll slot may keep libc's fork guard set after final retirement. */
    pid_t child=fork(); CHECK(child>=0);
    if (!child) _exit(0);
    int status=-1; CHECK(waitpid(child,&status,0)==child && WIFEXITED(status) && WEXITSTATUS(status)==0);
    puts("threadstress: epoll-lifetime ok");
    return 0;
}
