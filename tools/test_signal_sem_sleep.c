/* Real semaphore and nanosleep entry points; kernel/logical I/O are modeled. */
#include <errno.h>
#include <semaphore.h>
#include <time.h>
#include <srvros/sys.h>
#include "../userspace/lib/src/signal_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("signal-sem-sleep: line %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static int err, depth, guarded, ended, local_calls, yield_calls, begin_calls, mode, available=1, fallback_sleep;
static uint64_t kind_seen, call_ticks;
int *__errno_location(void) { return &err; }
unsigned __posix_signal_info_depth(void) { return depth; }
int __posix_signal_public_available(void) { return available; }
long srv_ticks(void) { return 20; }
long srv_monotonic_us(void) { CHECK(!available); return 100; }
long srv_sleep_ticks(uint64_t ticks) { CHECK(!available && ticks==102); fallback_sleep++; return 0; }
int __posix_signal_dispatch_pending(void) { return 0; }
int __posix_signal_dispatch_pending_io(int *restart) { *restart=0; return 0; }
void __posix_signal_runtime_failure(void) { CHECK(0); __builtin_unreachable(); }
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) {
    CHECK(!depth && anchor && !selector && !deadline && !fds && !count && !flags && !mask);
    CHECK((kind==SRV_SIGNAL_ROOT_SLEEP && primary==SYS_SLEEP_TICKS) || (kind==SRV_SIGNAL_ROOT_SEM_WAIT && primary==SYS_YIELD));
    begin_calls++;kind_seen=kind;r->ticket=9;return 0;
}
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t cookie,int finalize) {
    CHECK(r->ticket==9 && !guarded && !cookie && !finalize);local_calls++;
    if(mode==1)return SRV_ERR_INTR;guarded=1;return 5;
}
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t c) { CHECK(r->ticket==9 && guarded && c==5);guarded=0; }
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t number,const uint64_t a[6],const struct srv_signal_descriptor_v1 *v,size_t n) {
    CHECK(r->ticket==9 && !guarded && !v && !n && !a[1] && !a[2] && !a[3] && !a[4] && !a[5]);
    if(number==SYS_YIELD) { CHECK(!a[0]);yield_calls++;return SRV_ERR_INTR; }
    CHECK(number==SYS_SLEEP_TICKS);call_ticks=a[0];return mode==2?0:SRV_ERR_INTR;
}
void __posix_signal_io_end(struct posix_signal_io *r) { CHECK(r->ticket==9 && !guarded);r->ticket=0;ended++; }
int __posix_signal_io_result(struct posix_signal_io *r,struct srv_signal_io_result_v1 *o) {
    CHECK(r->ticket==9);*o=(struct srv_signal_io_result_v1){.size=sizeof(*o),.version=1,.cookie=mode==4?0:8,.syscall_number=SYS_SLEEP_TICKS,.result=mode==2?0:SRV_ERR_INTR,.elapsed_us=250000,.elapsed_ticks=25,.validity=mode==3?SRV_SIGNAL_IO_RESULT_TICKS_VALID:SRV_SIGNAL_IO_RESULT_MONO_VALID};return 0;
}
int main(void) {
    sem_t sem={1}; depth=1;CHECK(sem_wait(&sem)==-1 && errno==EOPNOTSUPP && sem.__value==1 && !begin_calls);
    depth=0;CHECK(!sem_wait(&sem) && sem.__value==0 && local_calls==1 && ended==1 && !yield_calls);
    mode=1;sem.__value=1;CHECK(sem_wait(&sem)==-1 && errno==EINTR && sem.__value==1 && ended==2);
    mode=0;sem.__value=0;CHECK(sem_wait(&sem)==-1 && errno==EINTR && yield_calls==1 && ended==3);
    struct timespec ts={1,123};mode=0;CHECK(nanosleep(&ts,&ts)==-1 && errno==EINTR && ts.tv_sec==0 && ts.tv_nsec==750000123 && call_ticks==102 && kind_seen==SRV_SIGNAL_ROOT_SLEEP);
    ts=(struct timespec){1,123};mode=3;CHECK(nanosleep(&ts,&ts)==-1 && ts.tv_sec==0 && ts.tv_nsec==750000123);
    ts=(struct timespec){1,123};mode=4;CHECK(nanosleep(&ts,&ts)==-1 && ts.tv_sec==1 && ts.tv_nsec==123);
    ts=(struct timespec){1,123};mode=2;CHECK(!nanosleep(&ts,&ts) && ts.tv_sec==1 && ts.tv_nsec==123);
    depth=1;CHECK(nanosleep(&ts,&ts)==-1 && errno==EOPNOTSUPP && ts.tv_sec==1 && ts.tv_nsec==123);
    depth=0;available=0;sem.__value=1;CHECK(!sem_wait(&sem) && !sem.__value);
    ts=(struct timespec){1,123};CHECK(!nanosleep(&ts,0) && fallback_sleep==1);
    puts("signal-sem-sleep: ok");return 0;
}
