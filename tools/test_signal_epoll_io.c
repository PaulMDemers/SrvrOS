/* Actual epoll/eventfd adapter; mocked native descriptors and signal gateway. */
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <poll.h>

#include <sys/eventfd.h>
#include <sys/epoll.h>
#include <srvros/sys.h>
#include "../userspace/lib/src/signal_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){printf("signal-epoll: %d: %s\n",__LINE__,#x);exit(1);}}while(0)
FILE *stderr;
static int error_value,locked,guarded,ends,captures,calls,mode,masked;
int *__errno_location(void){return &error_value;}
int pthread_mutex_lock(pthread_mutex_t *p){(void)p;CHECK(!locked);locked=1;return 0;}
int pthread_mutex_unlock(pthread_mutex_t *p){(void)p;CHECK(locked);locked=0;return 0;}
int pipe(int fds[2]){fds[0]=4;fds[1]=5;return 0;}
long srv_close(int fd){(void)fd;return 0;}
long srv_fcntl(int fd,int cmd,uint64_t arg){(void)fd;(void)cmd;(void)arg;return 0;}
long srv_monotonic_us(void){return 1000000;}
long srv_ticks(void){return 100;}
long srv_signal_mask(uint64_t how,uint64_t mask,uint64_t *old){(void)how;(void)mask;(void)old;CHECK(0);return -1;}
long srv_poll(struct srv_pollfd *f,size_t n,int64_t t){(void)f;(void)n;(void)t;CHECK(0);return -1;}
int __posix_socket_is_pseudo(int fd){return fd==1000;}
unsigned __posix_signal_info_depth(void){return 0;}
int __posix_signal_public_available(void){return 1;}
int __posix_signal_dispatch_pending(void){return 0;}
int __pospos_unused(void){return 0;}
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask){CHECK(locked && kind==SRV_SIGNAL_ROOT_EPOLL && primary==SYS_POLL && !selector && anchor && !deadline && !fds && !count && flags==(SRV_SIGNAL_IO_POLL_LIST|(masked?SRV_SIGNAL_IO_LEGACY_MASKED:0)) && mask==(masked?8u:0u));r->ticket=3;return 0;}
int __posix_signal_io_capture_descriptors(struct posix_signal_io *r,const int64_t *fds,size_t count,uint64_t flags,struct srv_signal_descriptor_v1 *out){CHECK(locked && r->ticket==3 && flags==SRV_SIGNAL_IO_POLL_LIST);
    if(mode==3){CHECK(count==1 && fds[0]==6);out[0]=(struct srv_signal_descriptor_v1){6,55};}else CHECK(!count);
    captures++;return 0;}
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t nr,const uint64_t a[6],const struct srv_signal_descriptor_v1 *v,size_t n){CHECK(!locked && !guarded && r->ticket==3 && nr==SYS_POLL && a[2]==0);
    if(mode==3){CHECK(n==1 && v->fd==6 && v->value==55 && a[1]==1);((struct srv_pollfd *)(uintptr_t)a[0])->revents=POLLIN;}
    else CHECK(a[1]==0 && !n);calls++;return mode==1?SRV_ERR_INTR:mode==3?1:0;}
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t c,int f){CHECK(locked && !guarded && r->ticket==3 && !c && !f);if(mode==2)return SRV_ERR_INTR;guarded=1;return 7;}
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t c){CHECK(!locked && guarded && r->ticket==3 && c==7);guarded=0;}
void __posix_signal_io_end(struct posix_signal_io *r){CHECK(!locked && !guarded && r->ticket==3);ends++;r->ticket=0;}
int main(void){int ep=epoll_create1(0),fd=eventfd(1,0);CHECK(ep==4 && fd>=0);struct epoll_event pseudo={.events=EPOLLIN};CHECK(epoll_ctl(ep,EPOLL_CTL_ADD,1000,&pseudo)==-1 && errno==EOPNOTSUPP);struct epoll_event watch={.events=EPOLLIN|EPOLLONESHOT,.data.u64=42},out={0};CHECK(!epoll_ctl(ep,EPOLL_CTL_ADD,fd,&watch));mode=2;CHECK(epoll_wait(ep,&out,1,0)==-1 && errno==EINTR && !out.events);mode=0;CHECK(epoll_wait(ep,&out,1,0)==1 && out.events==EPOLLIN && out.data.u64==42);out.events=0;CHECK(epoll_wait(ep,&out,1,0)==0 && !out.events);CHECK(captures==3 && calls==3 && ends==3);sigset_t mask=8;masked=1;CHECK(epoll_pwait(ep,&out,1,0,&mask)==0);
    masked=0;struct epoll_event native={.events=EPOLLIN,.data.u64=99};CHECK(!epoll_ctl(ep,EPOLL_CTL_ADD,6,&native));
    mode=3;CHECK(epoll_wait(ep,&out,1,0)==1 && out.events==EPOLLIN && out.data.u64==99);puts("signal-epoll: ok");return 0;}
