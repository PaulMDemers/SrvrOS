/* Real waitpid wrapper; signal gateway and wait backend are modeled. */
#include <errno.h>
#include <srvros/sys.h>
#include <sys/wait.h>
#include "../userspace/lib/src/signal_runtime.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do { if (!(x)) { printf("signal-waitpid: line %d: %s\n",__LINE__,#x);exit(1); } } while (0)
enum { POSITIVE, NOHANG_ZERO, CHILD_ERROR, FAULT_ERROR, LEGACY_RESTART, STICKY_RESTART };
static int error_value, depth, available=1, mode, calls, begins, ends, guards, guarded, legacy_calls, fallback_calls, late_info;
static int *caller_status;
static struct posix_signal_io *active;
int *__errno_location(void) { return &error_value; }
unsigned __posix_signal_info_depth(void) { return (unsigned)depth; }
int __posix_signal_public_available(void) { return available; }
int __posix_signal_dispatch_pending(void) { return 0; }
int __posix_signal_dispatch_pending_restartable(void) {
    legacy_calls++;
    return (mode==LEGACY_RESTART || mode==STICKY_RESTART) && legacy_calls==1;
}
void __posix_signal_runtime_failure(void) { CHECK(0);__builtin_unreachable(); }
int __posix_signal_io_begin(struct posix_signal_io *r,uint64_t kind,uint64_t primary,uint64_t selector,uint64_t anchor,uint64_t deadline,const int64_t *fds,size_t count,uint64_t flags,uint64_t mask) {
    CHECK(!active && !depth && anchor && kind==SRV_SIGNAL_ROOT_WAITPID && primary==SYS_WAIT && !selector && !deadline && !fds && !count && !flags && !mask);
    active=r;r->ticket=23;begins++;return 0;
}
long __posix_signal_io_call(struct posix_signal_io *r,uint64_t number,const uint64_t args[6],const struct srv_signal_descriptor_v1 *expected,size_t count) {
    CHECK(active==r && r->ticket==23 && !guarded && number==SYS_WAIT && !expected && !count && (int64_t)args[0]==-1 && args[1] && (args[2]&SRV_WAIT_POSIX) && !args[3] && !args[4] && !args[5]);
    calls++;
    if(mode==NOHANG_ZERO) { CHECK(args[2]&SRV_WAIT_NOHANG);return 0; }
    if(mode==CHILD_ERROR)return -ECHILD;
    if(mode==FAULT_ERROR)return SRV_ERR_FAULT;
    if((mode==LEGACY_RESTART && calls==1) || mode==STICKY_RESTART)return SRV_ERR_INTR;
    *(uint64_t *)(uintptr_t)args[1]=UINT64_C(0x2a00);
    late_info=1; /* Native completion precedes a modeled INFO delivery. */
    return 42;
}
int __posix_signal_io_result(struct posix_signal_io *r,struct srv_signal_io_result_v1 *out) {
    CHECK(active==r && r->ticket==23 && (mode==POSITIVE || mode==LEGACY_RESTART));
    *out=(struct srv_signal_io_result_v1){.size=64,.version=1,.cookie=71,.syscall_number=SYS_WAIT,.result=42,.validity=SRV_SIGNAL_IO_RESULT_COMPLETED_NATIVE};return 0;
}
long __posix_signal_io_local_begin(struct posix_signal_io *r,uint64_t cookie,int finalize) {
    CHECK(active==r && r->ticket==23 && cookie==71 && finalize && late_info && !guarded && caller_status && *caller_status==0x7777);
    guarded=1;guards++;return 72;
}
void __posix_signal_io_local_end(struct posix_signal_io *r,uint64_t cookie) { CHECK(active==r && guarded && cookie==72 && *caller_status==0x2a00);guarded=0; }
void __posix_signal_io_end(struct posix_signal_io *r) { CHECK(active==r && r->ticket==23 && !guarded);r->ticket=0;active=0;ends++; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) { CHECK(!available && (int64_t)pid==-1 && flags==SRV_WAIT_POSIX);fallback_calls++;*status=0x2a00;return 42; }
static void reset(int next) { mode=next;calls=begins=ends=guards=guarded=legacy_calls=fallback_calls=late_info=0;active=0;error_value=0; }
int main(void) {
    int status=0x7777;caller_status=&status;reset(POSITIVE);depth=1;
    CHECK(waitpid(-1,&status,0)==-1 && errno==ENOTSUP && !begins && status==0x7777);
    depth=0;CHECK(waitpid(-1,&status,0)==42 && status==0x2a00 && begins==1 && calls==1 && guards==1 && ends==1);
    reset(NOHANG_ZERO);status=0x7777;CHECK(waitpid(-1,&status,WNOHANG)==0 && status==0x7777 && !guards && ends==1);
    reset(CHILD_ERROR);CHECK(waitpid(-1,&status,0)==-1 && errno==ECHILD && status==0x7777 && !guards && ends==1);
    reset(FAULT_ERROR);CHECK(waitpid(-1,&status,0)==-1 && errno==EFAULT && status==0x7777 && !guards && ends==1);
    reset(LEGACY_RESTART);CHECK(waitpid(-1,&status,0)==42 && status==0x2a00 && begins==1 && calls==2 && guards==1 && ends==1);
    reset(STICKY_RESTART);status=0x7777;CHECK(waitpid(-1,&status,0)==-1 && errno==EINTR && status==0x7777 && begins==1 && calls==2 && !guards && ends==1);
    reset(POSITIVE);available=0;CHECK(waitpid(-1,&status,0)==42 && status==0x2a00 && fallback_calls==1 && !begins);
    puts("signal-waitpid: ok");return 0;
}
