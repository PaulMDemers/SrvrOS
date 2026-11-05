/* Real eventfd implementation with a modeled logical I/O backend and lock. */
#include <errno.h>
#include <pthread.h>
#include <sys/eventfd.h>
#include <srvros/sys.h>
#include "../userspace/lib/src/signal_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("signal-eventfd: %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int error_value, locked, guarded, ends, calls, mode;
int *__errno_location(void) { return &error_value; }
int pthread_mutex_lock(pthread_mutex_t *p) { (void)p; CHECK(!locked);locked=1;return 0; }
int pthread_mutex_unlock(pthread_mutex_t *p) { (void)p;CHECK(locked);locked=0;return 0; }
unsigned __posix_signal_info_depth(void) { return 0; }
int __posix_signal_public_available(void) { return 1; }
long srv_fcntl(int fd,int cmd,uint64_t flags) { (void)fd;(void)cmd;(void)flags;CHECK(0);return -1; }
int __posix_socket_is_pseudo(int fd) { (void)fd;return 0; }
int __posix_signal_dispatch_pending_io(int *restart) { *restart=0;return 0; }
long srv_poll(struct srv_pollfd *fds,size_t n,int64_t timeout) { (void)fds;(void)n;(void)timeout;CHECK(0);return -1; }
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) {
    CHECK(locked && anchor && !selector && !deadline && !fds && !count && !flags && !mask);
    CHECK((kind==SRV_SIGNAL_ROOT_READ && primary==SYS_READ)||(kind==SRV_SIGNAL_ROOT_WRITE && primary==SYS_WRITE));r->ticket=1;return 0;
}
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t c,int f) { CHECK(locked && r->ticket==1 && !guarded && !c && !f);if(mode==1)return SRV_ERR_INTR;guarded=1;return 5; }
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t c) { CHECK(!locked && guarded && r->ticket==1 && c==5);guarded=0; }
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t nr,const uint64_t a[6],const struct srv_signal_descriptor_v1 *d,size_t n) { CHECK(!locked && !guarded && r->ticket==1 && nr==SYS_POLL && !a[0] && !a[1] && a[2]==10 && !a[3] && !a[4] && !a[5] && !d && !n);calls++;return SRV_ERR_INTR; }
void __posix_signal_io_end(struct posix_signal_io *r) { CHECK(!locked && !guarded && r->ticket==1);ends++;r->ticket=0; }
ssize_t __posix_eventfd_read(int,void *,size_t);
ssize_t __posix_eventfd_write(int,const void *,size_t);
int main(void) {
    int fd=eventfd(2,0);CHECK(fd>=0);uint64_t value=0;
    CHECK(__posix_eventfd_read(fd,&value,8)==8 && value==2 && ends==1);
    CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EINTR && calls==1 && ends==2);
    mode=1;value=7;CHECK(__posix_eventfd_write(fd,&value,8)==-1 && errno==EINTR && ends==3);
    mode=0;CHECK(__posix_eventfd_read(fd,&value,8)==-1 && errno==EINTR && calls==2);
    puts("signal-eventfd: ok");return 0;
}
