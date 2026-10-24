#define poll audit_poll
#define select audit_select
#include "../userspace/lib/src/posix_poll.c"
#include <assert.h>

static short readiness[FD_SETSIZE],requested[FD_SETSIZE];
static int64_t last_timeout;
static int calls,dispatch;
long srv_poll(struct srv_pollfd *fds,size_t count,int64_t timeout) {
    calls++;last_timeout=timeout;long ready=0;
    for(size_t i=0;i<count;i++) {
        if(fds[i].fd<0) { fds[i].revents=0;continue; }
        assert(fds[i].fd<FD_SETSIZE);
        requested[fds[i].fd]=fds[i].events;
        fds[i].revents=readiness[fds[i].fd];
        if(fds[i].revents) ready++;
    }
    return ready;
}
int __posix_socket_poll_fd(int fd) { return fd==1000?7:fd; }
short __posix_socket_poll_update(int fd,short wanted,short actual) { (void)fd;(void)wanted;return actual; }
int __posix_signal_dispatch_pending(void) { int result=dispatch;dispatch=0;return result; }
int main(void) {
    fd_set r,w,e;struct timeval timeout={0};
    FD_ZERO(&r);FD_ZERO(&w);FD_ZERO(&e);
    FD_SET(7,&r);FD_SET(7,&w);FD_SET(7,&e);
    readiness[7]=POLLIN|POLLOUT|POLLPRI;
    assert(select(8,&r,&w,&e,&timeout)==3);
    assert(FD_ISSET(7,&r) && FD_ISSET(7,&w) && FD_ISSET(7,&e));
    assert(requested[7]==(POLLIN|POLLOUT|POLLPRI));
    readiness[7]=0;
    assert(select(8,&r,&w,&e,&timeout)==0);
    assert(!FD_ISSET(7,&r) && !FD_ISSET(7,&w) && !FD_ISSET(7,&e));
    FD_SET(7,&r);readiness[7]=POLLERR|POLLHUP;
    assert(select(8,&r,&w,&e,&timeout)==1 && FD_ISSET(7,&r) && !FD_ISSET(7,&w));
    readiness[7]=POLLNVAL;errno=0;
    assert(select(8,&r,NULL,NULL,&timeout)==-1 && errno==EBADF && FD_ISSET(7,&r));
    int before=calls;
    timeout.tv_sec=-1;errno=0;assert(select(0,NULL,NULL,NULL,&timeout)==-1 && errno==EINVAL);
    timeout.tv_sec=0;timeout.tv_usec=-1;assert(select(0,NULL,NULL,NULL,&timeout)==-1);
    timeout.tv_usec=1000000;assert(select(0,NULL,NULL,NULL,&timeout)==-1 && calls==before);
    timeout.tv_sec=INT64_MAX;timeout.tv_usec=999999;
    assert(select(0,NULL,NULL,NULL,&timeout)==0 && last_timeout==INT64_MAX);
    timeout=(struct timeval){.tv_usec=1};
    assert(select(0,NULL,NULL,NULL,&timeout)==0 && last_timeout==1);
    FD_ZERO(&w);FD_SET(1000,&w);readiness[7]=POLLOUT;timeout=(struct timeval){0};
    assert(select(1001,NULL,&w,NULL,&timeout)==1 && FD_ISSET(1000,&w));
    FD_ZERO(&r);for(int i=0;i<65;i++) FD_SET(i,&r);
    assert(select(65,&r,NULL,NULL,&timeout)==-1 && errno==EINVAL);
    dispatch=1;errno=0;
    assert(poll(NULL,0,-1)==-1 && errno==EINTR && last_timeout==0);
    struct pollfd ignored={.fd=-1,.events=POLLIN,.revents=POLLNVAL};
    assert(poll(&ignored,1,0)==0 && !ignored.revents);
    puts("posix-poll-host: PASS (bit counts, priority masks, invalid FDs, requested sets, timeout bounds, high socket FD and pre-dispatched signal)");
}
