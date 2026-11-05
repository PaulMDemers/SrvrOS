/* Actual pread/pwrite wrappers, modeled descriptor lifecycle and kernel I/O. */
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/uio.h>
#include <srvros/sys.h>
#include "../userspace/lib/src/signal_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("signal-positional: %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int error_value, depth, available=1, locked, begins, ends, calls, pseudo, interrupt;
static uint64_t operation;
int *__errno_location(void) { return &error_value; }
unsigned __posix_signal_info_depth(void) { return depth; }
int __posix_signal_public_available(void) { return available; }
void __posix_event_lock(void) { CHECK(!locked);locked=1; }
void __posix_event_unlock(void) { CHECK(locked);locked=0; }
void __posix_event_note_dup(int a,int b) { (void)a;(void)b;CHECK(0); }
void __posix_socket_note_dup(int a,int b) { (void)a;(void)b;CHECK(0); }
int __posix_event_prepare_dup(int fd) { (void)fd;CHECK(0);return 0; }
int __posix_eventfd_fcntl(int fd,int c,uint64_t f) { (void)fd;(void)c;(void)f;CHECK(0);return -1; }
int __posix_socket_fcntl(int fd,int c,uint64_t f) { (void)fd;(void)c;(void)f;CHECK(0);return -1; }
long srv_fcntl(int fd,int c,uint64_t f) { (void)fd;(void)c;(void)f;CHECK(0);return -1; }
int __posix_socket_real_fd(int fd) { return fd; }
uint64_t __posix_event_capture_identity_locked(int fd) { (void)fd;return 1; }
uint64_t __posix_event_capture_identity(int fd) { (void)fd;return 1; }
int __posix_event_identity_current(int fd,uint64_t id) { (void)fd;(void)id;return 1; }
int __posix_signal_dispatch_pending_restartable(void) { return 0; }
int __posix_socket_pending_error(int fd) { CHECK(fd==4 && !locked);return 61; }
ssize_t __posix_eventfd_read(int fd,void *b,size_t n) { (void)fd;(void)b;(void)n;CHECK(0);return -1; }
ssize_t __posix_eventfd_write(int fd,const void *b,size_t n) { (void)fd;(void)b;(void)n;CHECK(0);return -1; }
long srv_read(int fd,void *b,size_t n) { (void)fd;(void)b;(void)n;CHECK(0);return -1; }
long srv_write(int fd,const void *b,size_t n) { (void)fd;(void)b;(void)n;CHECK(0);return -1; }
long srv_syscall5(long nr,long a,long b,long c,long d,long e) { (void)nr;(void)a;(void)b;(void)c;(void)d;(void)e;CHECK(0);return -1; }
int __posix_socket_is_pseudo(int fd) { (void)fd;return pseudo; }
int __posix_eventfd_is_pseudo(int fd) { (void)fd;return 0; }
int __posix_eventfd_is_pseudo_safe(int fd) { (void)fd;return 0; }
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) {
    CHECK(locked && anchor && (primary==SYS_VECTOR_IO || !selector) && !deadline && count==1 && *fds==4 && !flags && !mask);
    CHECK((kind==SRV_SIGNAL_ROOT_RAW && (primary==SYS_PREAD || primary==SYS_PWRITE || primary==SYS_VECTOR_IO || primary==SYS_FCNTL)) || (kind==SRV_SIGNAL_ROOT_READ && (primary==SYS_READ || primary==SYS_VECTOR_IO)) || (kind==SRV_SIGNAL_ROOT_WRITE && (primary==SYS_WRITE || primary==SYS_VECTOR_IO)));
    operation=primary;begins++;r->ticket=1;r->count=1;r->descriptors[0]=(struct srv_signal_descriptor_v1){4,19};return 0;
}
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t nr,const uint64_t a[6],const struct srv_signal_descriptor_v1 *v,size_t count) {
    CHECK(!locked && r->ticket==1 && nr==operation && count==1 && v==r->descriptors && v->value==19 && a[0]==4 && !a[5]);
    if(nr==SYS_PREAD || nr==SYS_PWRITE) CHECK(a[2]==5 && a[3]==3 && !a[4]);
    if(nr==SYS_READ || nr==SYS_WRITE) CHECK(a[2]==5 && !a[3] && !a[4]);
    if(nr==SYS_FCNTL) { struct srv_flock *l=(void *)(uintptr_t)a[2];CHECK(a[1]==SRV_F_SETLKW && l->start==3 && l->len==5 && !a[3] && !a[4]); }
    calls++;return interrupt==2?SRV_ERR_BADF:interrupt?SRV_ERR_INTR:5;
}
void __posix_signal_io_end(struct posix_signal_io *r) { CHECK(!locked && r->ticket==1);r->ticket=0;ends++; }
int __posix_signal_dispatch_pending(void) { return 0; }
long srv_pread(int fd,void *b,size_t n,int64_t off) { (void)fd;(void)b;(void)n;(void)off;CHECK(!available);return 5; }
long srv_pwrite(int fd,const void *b,size_t n,int64_t off) { (void)fd;(void)b;(void)n;(void)off;CHECK(!available);return 5; }
ssize_t __posix_read_with_socket_error(int,void *,size_t,int *);
ssize_t __posix_write_with_socket_error(int,const void *,size_t,int *);
int main(void) {
    char b[5]; depth=1;CHECK(pread(4,b,5,3)==-1 && errno==EOPNOTSUPP && !begins);
    depth=0;CHECK(pread(4,b,5,3)==5 && begins==1 && calls==1 && ends==1);
    interrupt=1;CHECK(pwrite(4,b,5,3)==-1 && errno==EINTR && begins==2 && calls==2 && ends==2);
    pseudo=1;CHECK(pread(4,b,5,3)==-1 && errno==ESPIPE && begins==2);pseudo=0;
    interrupt=0;CHECK(read(4,b,5)==5 && write(4,b,5)==5);
    int socket_error=0;interrupt=2;
    CHECK(__posix_read_with_socket_error(4,b,5,&socket_error)==-1 && socket_error==61 && errno==EBADF);
    socket_error=0;CHECK(__posix_write_with_socket_error(4,b,5,&socket_error)==-1 && socket_error==61 && errno==EBADF);
    depth=1;CHECK(__posix_read_with_socket_error(4,b,5,&socket_error)==-1 && errno==EOPNOTSUPP);depth=0;interrupt=0;
    struct iovec v={b,5};CHECK(readv(4,&v,1)==5 && writev(4,&v,1)==5);
    CHECK(preadv(4,&v,1,3)==5 && pwritev(4,&v,1,3)==5);
    struct flock fl={.l_type=F_WRLCK,.l_whence=SEEK_SET,.l_start=3,.l_len=5};
    CHECK(fcntl(4,F_SETLKW,&fl)==0);
    interrupt=1;CHECK(fcntl(4,F_SETLKW,&fl)==-1 && errno==EINTR && fl.l_start==3 && fl.l_len==5);interrupt=0;
    depth=1;CHECK(fcntl(4,F_SETLKW,&fl)==-1 && errno==EOPNOTSUPP);depth=0;
    available=0;CHECK(pread(4,b,5,3)==5 && pwrite(4,b,5,3)==5 && begins==12);
    puts("signal-positional: ok");return 0;
}
