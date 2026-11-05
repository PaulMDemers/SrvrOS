/* Actual poll wrapper with modeled native poll, descriptor and signal services. */
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <sys/select.h>
#include <srvros/sys.h>
#include "../userspace/lib/src/signal_runtime.h"
#include "../userspace/lib/src/socket_signal.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if(!(x)) { printf("signal-poll: %d: %s\n",__LINE__,#x);exit(1); } } while(0)
FILE *stderr;
static int error_value,locked,guarded,begins,ends,calls,mode,masked;
int *__errno_location(void) {return &error_value;}
unsigned __posix_signal_info_depth(void) {return 0;}
int __posix_signal_public_available(void) {return 1;}
void __posix_event_lock(void) { CHECK(!locked);locked=1; }
void __posix_event_unlock(void) { CHECK(locked);locked=0; }
uint64_t __posix_event_capture_identity_locked(int fd) { CHECK(locked && fd==4);return 1; }
int __posix_event_identity_matches_locked(int fd,uint64_t id) { CHECK(locked);return fd==4 && id==1; }
int __posix_eventfd_poll_locked(int fd,short events,uint64_t *id) { (void)fd;(void)events;(void)id;CHECK(locked);return -1; }
int __posix_eventfd_poll(int fd,short events,uint64_t *id) { (void)fd;(void)events;(void)id;CHECK(0);return -1; }
void __posix_socket_poll_prepare(int fd,struct posix_socket_poll_snapshot *out) { CHECK(locked);*out=(struct posix_socket_poll_snapshot){.fd=fd,.native_fd=-1}; }
short __posix_socket_poll_commit(int fd,short events,short revents,const struct posix_socket_poll_snapshot *s) { (void)events;CHECK(locked && guarded && fd==s->fd);return revents; }
int __posix_socket_poll_fd(int fd) { return fd; }
short __posix_socket_poll_update(int fd,short events,short revents) { (void)fd;(void)events;(void)revents;CHECK(0);return 0; }
long srv_monotonic_us(void) {return 1000000;}
long srv_ticks(void) {return 100;}
long srv_signal_mask(uint64_t how,uint64_t mask,uint64_t *old) {(void)how;(void)mask;(void)old;CHECK(0);return -1;}
long srv_poll(struct srv_pollfd *fds,size_t count,int64_t timeout) { (void)fds;(void)count;(void)timeout;CHECK(0);return -1; }
int __posix_signal_dispatch_pending(void) {return mode==3 || mode==4;}
void __posix_signal_runtime_failure(void) { CHECK(0);__builtin_unreachable(); }
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) { CHECK(locked && kind==SRV_SIGNAL_ROOT_POLL && primary==SYS_POLL && !selector && anchor && deadline==(mode==4?1001000u:0u) && count==1 && *fds==4 && flags==(SRV_SIGNAL_IO_POLL_LIST|(masked?SRV_SIGNAL_IO_LEGACY_MASKED:0)) && mask==(masked?8u:0u)); r->ticket=3;r->count=1;r->descriptors[0]=(struct srv_signal_descriptor_v1){4,22};begins++;return 0; }
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t nr,const uint64_t a[6],const struct srv_signal_descriptor_v1 *v,size_t n) {CHECK(!locked && r->ticket==3 && nr==SYS_POLL && n==1 && v->fd==4 && v->value==22 && a[1]==1 && a[2]==(mode==4?1u:0u));calls++; if(mode==1)return SRV_ERR_INTR;if(mode==3 || mode==4)return 0;((struct srv_pollfd *)(uintptr_t)a[0])->revents=POLLIN;return 1;}
int __posix_signal_io_result(struct posix_signal_io *r,struct srv_signal_io_result_v1 *out) { CHECK(!locked && r->ticket==3);*out=(struct srv_signal_io_result_v1){.size=64,.version=1,.cookie=9,.syscall_number=SYS_POLL,.result=(mode==3 || mode==4)?0:1,.validity=mode==4?0:SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE};return 0; }
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t cookie,int finalize) {CHECK(locked && r->ticket==3 && ((mode==4 && !cookie && !finalize) || (mode!=4 && cookie==9 && finalize)));if(mode==2)return SRV_ERR_INTR;guarded=1;return 6;}
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t cookie) {CHECK(!locked && guarded && r->ticket==3 && cookie==6);guarded=0;}
void __posix_signal_io_end(struct posix_signal_io *r) {CHECK(!locked && !guarded && r->ticket==3);ends++;r->ticket=0;}
int main(void) { struct pollfd p={.fd=4,.events=POLLIN,.revents=0};CHECK(poll(&p,1,0)==1 && p.revents==POLLIN && begins==1 && calls==1 && ends==1);mode=1;p.revents=77;CHECK(poll(&p,1,0)==-1 && errno==EINTR && p.revents==77);mode=2;CHECK(poll(&p,1,0)==-1 && errno==EINTR && p.revents==77);mode=3;p.revents=77;CHECK(poll(&p,1,0)==0 && p.revents==0);
    mode=4;p.revents=77;CHECK(poll(&p,1,1)==-1 && errno==EINTR && p.revents==77);
    mode=0;fd_set set;FD_ZERO(&set);FD_SET(4,&set);struct timespec zero={0,0};sigset_t mask=8;masked=1;CHECK(pselect(5,&set,0,0,&zero,&mask)==1 && FD_ISSET(4,&set));
    p.revents=0;CHECK(ppoll(&p,1,&zero,&mask)==1 && p.revents==POLLIN);
    puts("signal-poll: ok");return 0; }
