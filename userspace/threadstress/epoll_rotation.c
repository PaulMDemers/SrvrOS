#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("epoll-rotation line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
static int add(int ep,int fd,uint32_t events,uint64_t tag) {
    struct epoll_event event={.events=events,.data.u64=tag};
    return epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event);
}
int posix_epoll_rotation_test(void) {
    int ep=epoll_create1(0),alias=dup(ep),p[2],fds[64];
    CHECK(ep>=0 && alias>=0 && pipe(p)==0 && write(p[1],"x",1)==1);
    /* Fill all 64 watch slots with three synthetic and 61 native registrations.
     * Repeated aliases deliberately watch the same persistent pipe readiness. */
    for (int i=0;i<64;i++) {
        fds[i]=i<3?eventfd(1,EFD_NONBLOCK):(i==3?p[0]:dup(p[0]));
        CHECK(fds[i]>=0 && add(ep,fds[i],EPOLLIN,(unsigned)i+1)==0);
    }
    struct epoll_event batch[7];
    for (int call=0;call<20;call++) {
        CHECK(epoll_wait(call&1?alias:ep,batch,7,0)==7);
        for (int i=0;i<7;i++) CHECK(batch[i].data.u64==(unsigned)(call*7+i)%64+1);
    }
    for (int i=0;i<64;i++) CHECK(close(fds[i])==0);
    CHECK(close(p[1])==0 && close(ep)==0 && close(alias)==0);

    ep=epoll_create1(0); CHECK(ep>=0 && pipe(p)==0);
    CHECK(add(ep,p[0],0,111)==0 && write(p[1],"x",1)==1);
    CHECK(epoll_wait(ep,batch,7,0)==0); /* IN is not requested. */
    CHECK(close(p[1])==0);
    CHECK(epoll_wait(ep,batch,7,0)==1 && batch[0].events==EPOLLHUP && batch[0].data.u64==111);
    char byte; CHECK(read(p[0],&byte,1)==1 && byte=='x');
    CHECK(epoll_wait(ep,batch,7,0)==1 && batch[0].events==EPOLLHUP);
    CHECK(close(p[0])==0 && epoll_wait(ep,batch,7,0)==0);

    CHECK(pipe(p)==0 && add(ep,p[1],0,222)==0);
    CHECK(epoll_wait(ep,batch,7,0)==0 && close(p[0])==0);
    CHECK(epoll_wait(ep,batch,7,0)==1 && batch[0].events==EPOLLERR && batch[0].data.u64==222);
    struct epoll_event event={.events=EPOLLERR,.data.u64=333};
    CHECK(epoll_ctl(ep,EPOLL_CTL_MOD,p[1],&event)==0);
    CHECK(epoll_wait(ep,batch,7,0)==1 && batch[0].events==EPOLLERR && batch[0].data.u64==333);
    CHECK(close(p[1])==0 && close(ep)==0);
    puts("threadstress: epoll-rotation ok");
    return 0;
}
