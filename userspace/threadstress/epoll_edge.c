#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <sys/epoll.h>
#include <sys/eventfd.h>
#include <unistd.h>

#define CHECK(x) do { if (!(x)) { printf("epoll-edge line=%d errno=%d\n",__LINE__,errno); return -1; } } while (0)
int posix_epoll_edge_test(void) {
    struct epoll_event got[1];
        int ep=epoll_create1(0), alias=dup(ep), p[2];
        CHECK(ep>=0 && alias>=0 && pipe(p)==0);
        int counter=eventfd(1,EFD_NONBLOCK); CHECK(counter>=0);
        CHECK(write(p[1],"x",1)==1);
        for (int kind=0;kind<2;kind++) {
            int fd=kind?p[0]:counter;
            struct epoll_event event={.events=EPOLLIN|EPOLLET,.data.u64=999};
            for (int i=0;i<70;i++) {
                CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==-1 && errno==EOPNOTSUPP);
            }
            CHECK(epoll_wait(ep,got,1,0)==0);
            event.events=EPOLLIN; event.data.u64=123;
            CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
            event.events=EPOLLOUT|EPOLLET|EPOLLONESHOT; event.data.u64=999;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==-1 && errno==EOPNOTSUPP);
            for (int i=0;i<2;i++) {
                CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==EPOLLIN && got[0].data.u64==123);
            }
            /* DEL ignores its event argument, including unsupported bits. */
            CHECK(epoll_ctl(alias,EPOLL_CTL_DEL,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==0);
            event.events=EPOLLIN|EPOLLONESHOT; event.data.u64=456;
            CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==1 && got[0].data.u64==456);
            event.events=EPOLLIN|EPOLLONESHOT|EPOLLET; event.data.u64=999;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==-1 && errno==EOPNOTSUPP);
            CHECK(epoll_wait(ep,got,1,0)==0); /* failed MOD must not rearm */
            event.events=EPOLLIN|EPOLLONESHOT; event.data.u64=789;
            CHECK(epoll_ctl(alias,EPOLL_CTL_MOD,fd,&event)==0);
            CHECK(epoll_wait(ep,got,1,0)==1 && got[0].events==EPOLLIN && got[0].data.u64==789);
            CHECK(epoll_wait(ep,got,1,0)==0);
            CHECK(epoll_ctl(ep,EPOLL_CTL_DEL,fd,NULL)==0);
        }
        CHECK(close(counter)==0 && close(p[0])==0 && close(p[1])==0);
        CHECK(close(alias)==0 && close(ep)==0);
    puts("threadstress: epoll-edge ok");
    return 0;
}
