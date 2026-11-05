/* Compile with the matched userspace profile; run from a disposable guest.
 * Inline INT avoids imposing the C caller-saved XMM contract on this test. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>

static unsigned done;
static int failure;
static __thread volatile uint64_t tls_cookie;

static long checked_call(long number, long a, long b, long c, long d) {
    const uint64_t pattern[2]={UINT64_C(0x1122334455667788),UINT64_C(0x8877665544332211)};
    uint64_t before_fs,after_fs,before_sp,after_sp,out[2];
    volatile uint64_t canary=UINT64_C(0xfeed0123456789ab);
    __asm__ volatile(
        "movq %%fs:0, %%r10\nmovq %%r10, %1\nmovq %%rsp, %3\n"
        "movdqu %6, %%xmm15\nint $0x80\nmovdqu %%xmm15, %5\n"
        "movq %%rsp, %4\nmovq %%fs:0, %%r10\nmovq %%r10, %2"
        : "+a"(number),"=m"(before_fs),"=m"(after_fs),"=m"(before_sp),"=m"(after_sp),"=m"(out)
        : "m"(pattern),"D"(a),"S"(b),"d"(c),"c"(d)
        : "memory","cc","xmm15","r10");
    if(before_fs!=after_fs || before_sp!=after_sp ||
       canary!=UINT64_C(0xfeed0123456789ab) || tls_cookie!=UINT64_C(0xaabbccddeeff0099) ||
       out[0]!=pattern[0] || out[1]!=pattern[1]) failure=10;
    return number;
}

static void *worker(void *unused) {
    (void)unused;
    tls_cookie=UINT64_C(0xaabbccddeeff0099);
    const char *path="/fat/bin/echo",*args="worker-child",*redirect="/fat/worker-refused.txt";
    char *argv[]={"echo","worker-child",NULL};
    struct srv_exec_request request={.path=path,.argv=argv,.stdin_fd=0,.stdout_fd=1,.stderr_fd=2};
    if(checked_call(SYS_SPAWN,(long)path,0,0,0)!=SRV_ERR_NOTSUP) failure=11;
    if(checked_call(SYS_SPAWN_ARGS,(long)path,(long)args,0,0)!=SRV_ERR_NOTSUP) failure=12;
    if(checked_call(SYS_SPAWN_ARGS_REDIRECT,(long)path,(long)args,(long)redirect,0)!=SRV_ERR_NOTSUP) failure=13;
    if(checked_call(SYS_EXEC,(long)&request,0,0,0)!=SRV_ERR_NOTSUP) failure=14;
    request.flags=SRV_EXEC_BACKGROUND;
    request.path="/fat/bin/worker-spawn-missing";
    if(checked_call(SYS_EXEC,(long)&request,0,0,0)!=SRV_ERR_NOENT) failure=15;
    request.path=path;
    struct srv_spawn_file_action action={.type=SRV_SPAWN_FILE_ACTION_DUP2,.fd=999,.new_fd=1};
    request.file_actions=&action;request.file_action_count=1;
    if(checked_call(SYS_EXEC,(long)&request,0,0,0)!=SRV_ERR_BADF) failure=16;
    request.file_actions=NULL;request.file_action_count=0;
    long child=checked_call(SYS_EXEC,(long)&request,0,0,0);
    if(child<=0) failure=17;
    else {
        uint64_t status=UINT64_MAX;
        if(srv_wait((uint64_t)child,&status,0)<0 || status) failure=18;
    }
    __atomic_store_n(&done,1,__ATOMIC_RELEASE);
    return NULL;
}

int main(void) {
    pthread_t thread;
    if(pthread_create(&thread,NULL,worker,NULL)) return 1;
    struct timespec now;clock_gettime(CLOCK_MONOTONIC,&now);
    uint64_t end=(uint64_t)now.tv_sec*1000000000+now.tv_nsec+UINT64_C(10000000000);
    while(!__atomic_load_n(&done,__ATOMIC_ACQUIRE)) {
        clock_gettime(CLOCK_MONOTONIC,&now);
        if((uint64_t)now.tv_sec*1000000000+now.tv_nsec>=end) {
            puts("binding-probe: worker did not return");return 3;
        }
        usleep(1000);
    }
    if(pthread_join(thread,NULL)) return 4;
    if(failure) {printf("binding-probe: failed check=%d\n",failure);return failure;}
    if(srv_stat("/fat/worker-refused.txt",&(struct srv_stat){0})!=SRV_ERR_NOENT) return 5;
    puts("binding-probe: foreground refusal, detached failure/success, TLS/FPU/stack ok");
    return 0;
}
