/* Private INT3 signal-test kernel only. No public signal ABI is advertised. */
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>
#include <srvros/sys.h>
#include "../kernel/src/process_signal.h"
#define MAGIC UINT64_C(0x5349475452415031)
#define PAGE 4096u
#define MAX_TRACKED 65536u
volatile unsigned test_ready,test_done;
_Alignas(16) unsigned char test_fx_before[512],test_fx_after[512],test_fx_saved[512];
uint64_t test_caller_rsp,test_probe_rsp,test_probe_rsp_after,test_fs_before,test_fs_after,test_registers[15],test_redzone[16];
uint64_t test_expected_pointer,test_probe_flags,test_handler_df;
extern const unsigned char test_xmm_pattern[256];
extern const unsigned char test_loop_begin[],test_loop_end[];
extern void test_probe(void),test_trampoline(void);
static volatile uint64_t target_tid;
static volatile unsigned sender_started,sender_release;
static int sender_result,worker_result,handler_result;
static __thread volatile uint64_t cookie;
static volatile unsigned depth_mode;
static __thread volatile unsigned handler_depth,depth_count,depth_peak,depth_deferred,depth_final,depth_error;
static __thread volatile uint64_t depth_last_token;
struct native_object { volatile uint64_t magic; };
static struct native_object *native_object;

static long hook(uint64_t operation,uint64_t a,uint64_t b,uint64_t c,uint64_t d) {
    register uint64_t r8 __asm__("r8")=d;uint64_t rax=MAGIC;
    __asm__ volatile("int3" : "+a"(rax),"+r"(r8) : "D"(operation),"S"(a),"d"(b),"c"(c) : "r10","r11","cc","memory");
    return (long)rax;
}
static void handler(uint64_t signum,struct process_signal_private_info *info,struct signal_context_snapshot *context) {
    int bad=0;
    if(signum!=SRV_SIGNAL_USR1 || info->size!=sizeof(*info) || info->version!=1 ||
       info->signo!=SRV_SIGNAL_USR1 || info->code ||
       info->valid!=(PROCESS_SIGNAL_INFO_SIGNO|PROCESS_SIGNAL_INFO_TARGET_TID) || info->target_tid!=target_tid) bad=1;
    for(unsigned i=0;i<4;i++)if(info->reserved[i])bad=2;
    if(depth_mode) {
        unsigned depth=++handler_depth;depth_count++;
        if(depth>depth_peak)depth_peak=depth;
        if(context->size!=sizeof(*context) || context->version!=SIGNAL_CONTEXT_VERSION || context->flags ||
           !context->token || context->token<=depth_last_token || depth>4 || test_handler_df) bad=7;
        depth_last_token=context->token;
        if(bad)depth_error=(unsigned)bad;
        if(!depth_final && depth<=4) {
            if(depth==4)depth_final=1;
            if(srv_thread_signal(target_tid,SRV_SIGNAL_USR1)<0 ||
               srv_signal_mask(SRV_SIGNAL_UNBLOCK,1ull<<SRV_SIGNAL_USR1,NULL)<0) depth_error=8;
            if(depth==4 && depth_count==4)depth_deferred=1;
        }
        memset(context,0xd3,sizeof(*context));
        handler_depth--;
        return;
    }
    if(context->size!=sizeof(*context) || context->version!=SIGNAL_CONTEXT_VERSION || context->flags ||
       !context->token || (context->interrupted_mask&(1ull<<SRV_SIGNAL_USR1)) ||
       context->rip<(uint64_t)(uintptr_t)test_loop_begin || context->rip>=(uint64_t)(uintptr_t)test_loop_end ||
       context->rsp!=test_probe_rsp || !(context->rflags&0x400) || test_handler_df ||
       cookie!=UINT64_C(0x1122334455667788)) bad=3;
    uint64_t values[]={context->rax,context->rbx,context->rcx,context->rdx,context->rsi,context->rdi,
        context->rbp,context->r8,context->r9,context->r10,context->r11,context->r12,context->r13,context->r14,context->r15};
    for(unsigned i=0;i<14;i++)if(values[i]!=i+1)bad=4;
    if(values[14]!=test_expected_pointer || !native_object || native_object->magic!=UINT64_C(0x9876543212345678))bad=4;
    if(memcmp(context->fpu,test_fx_before,512))bad=5;
    uint16_t cw;uint32_t mxcsr;memcpy(&cw,context->fpu,2);memcpy(&mxcsr,context->fpu+24,4);
    unsigned tags=0;for(unsigned i=0;i<8;i++)tags+=(context->fpu[4]>>i)&1u;
    if(cw!=0x077f || mxcsr!=0x3f80 || tags!=2 || memcmp(context->fpu+160,test_xmm_pattern,256))bad=6;
    handler_result=bad;
    /* Deliberately corrupt the entire public snapshot; private sigreturn must
     * restore the saved kernel-owned record rather than these user bytes. */
    memset(context,0xd3,sizeof(*context));
    __atomic_store_n(&test_done,1,__ATOMIC_RELEASE);
}
static int install(void) {
    return hook(1,(uint64_t)(uintptr_t)handler,(uint64_t)(uintptr_t)test_trampoline,0,0)==0;
}
static int reset_probe(void) {
    free(native_object);native_object=malloc(sizeof(*native_object));
    if(!native_object)return 0;
    native_object->magic=UINT64_C(0x9876543212345678);test_expected_pointer=(uint64_t)(uintptr_t)native_object;
    __atomic_store_n(&test_ready,0,__ATOMIC_RELAXED);__atomic_store_n(&test_done,0,__ATOMIC_RELAXED);
    test_handler_df=0;handler_result=0;sender_result=0;cookie=UINT64_C(0x1122334455667788);
    return 1;
}
static int verify_probe(void) {
    if(handler_result || test_probe_rsp!=test_probe_rsp_after || test_fs_before!=test_fs_after || cookie!=UINT64_C(0x1122334455667788) ||
       !(test_probe_flags&0x400) || test_handler_df || !native_object ||
       native_object->magic!=UINT64_C(0x9876543212345678) || memcmp(test_fx_before,test_fx_after,512))return 0;
    for(unsigned i=0;i<14;i++)if(test_registers[i]!=i+1)return 0;
    if(test_registers[14]!=test_expected_pointer)return 0;
    for(unsigned i=0;i<16;i++)if(test_redzone[i]!=UINT64_C(0x6162636465666768))return 0;
    return 1;
}
static void *sender(void *unused) {
    (void)unused;__atomic_store_n(&sender_started,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&test_ready,__ATOMIC_ACQUIRE))srv_yield();
    sender_result=(int)srv_thread_signal(target_tid,SRV_SIGNAL_USR1);
    if(sender_result)__atomic_store_n(&test_done,1,__ATOMIC_RELEASE);
    while(!__atomic_load_n(&sender_release,__ATOMIC_ACQUIRE))srv_yield();
    return NULL;
}
static void *worker_target(void *unused) {
    (void)unused;target_tid=(uint64_t)srv_thread_self();cookie=UINT64_C(0x1122334455667788);
    test_probe();worker_result=verify_probe()?0:1;return NULL;
}
static int pages(uint64_t *count) {
    struct srv_process_info info={.abi_version=SRV_ABI_VERSION,.struct_size=sizeof(info)};
    long result=srv_proc_list(SRV_PROC_LIST_BY_PID|(uint64_t)getpid(),&info);
    if(result<=0 || info.pid!=(uint64_t)getpid() || info.abi_version!=SRV_ABI_VERSION ||
       info.struct_size!=sizeof(info) || info.user_mapped_bytes%PAGE)return 0;
    *count=info.user_mapped_bytes/PAGE;return 1;
}
static int main_target(int maximum) {
    pthread_t thread;if(!reset_probe())return 1;target_tid=(uint64_t)srv_thread_self();sender_started=sender_release=0;
    if(pthread_create(&thread,NULL,sender,NULL))return 1;
    while(!__atomic_load_n(&sender_started,__ATOMIC_ACQUIRE))srv_yield();
    void *filler=NULL;size_t bytes=0;uint64_t count=0;
    if(maximum) {
        if(!pages(&count) || count>=MAX_TRACKED)return 2;
        bytes=(size_t)(MAX_TRACKED-count)*PAGE;
        filler=mmap(NULL,bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
        if(filler==MAP_FAILED || !pages(&count) || count!=MAX_TRACKED)return 3;
        if(hook(3,1,0,0,0))return 4;
    }
    test_probe();int ok=verify_probe() && !sender_result;
    if(maximum && (!pages(&count) || count!=MAX_TRACKED))ok=0;
    __atomic_store_n(&sender_release,1,__ATOMIC_RELEASE);
    if(pthread_join(thread,NULL))ok=0;
    if(maximum && munmap(filler,bytes))ok=0;
    return ok?0:5;
}
static int worker_target_test(void) {
    pthread_t thread;if(!reset_probe())return 1;worker_result=1;
    if(pthread_create(&thread,NULL,worker_target,NULL))return 1;
    while(!__atomic_load_n(&test_ready,__ATOMIC_ACQUIRE))srv_yield();
    int result=(int)srv_thread_signal(target_tid,SRV_SIGNAL_USR1);
    if(result)__atomic_store_n(&test_done,1,__ATOMIC_RELEASE);
    if(pthread_join(thread,NULL))return 2;
    return result || worker_result?3:0;
}
static int depth_test(void) {
    target_tid=(uint64_t)srv_thread_self();test_handler_df=0;
    handler_depth=depth_count=depth_peak=depth_deferred=depth_final=depth_error=0;depth_last_token=0;
    depth_mode=1;
    if(srv_signal_mask(SRV_SIGNAL_UNBLOCK,1ull<<SRV_SIGNAL_USR1,NULL)<0 ||
       srv_thread_signal(target_tid,SRV_SIGNAL_USR1)<0) {depth_mode=0;return 1;}
    struct timespec now;if(clock_gettime(CLOCK_MONOTONIC,&now)) {depth_mode=0;return 2;}
    uint64_t deadline=(uint64_t)now.tv_sec*1000000000ull+(uint64_t)now.tv_nsec+15000000000ull;
    while(depth_count<5 || handler_depth) {
        if(clock_gettime(CLOCK_MONOTONIC,&now) ||
           (uint64_t)now.tv_sec*1000000000ull+(uint64_t)now.tv_nsec>=deadline) {depth_mode=0;return 3;}
        srv_yield();
    }
    int ok=depth_count==5 && depth_peak==4 && depth_deferred && depth_final && !depth_error && !test_handler_df;
    if(!ok)printf("signal-delivery: depth failed count=%u peak=%u deferred=%u error=%u df=%llu\n",
        depth_count,depth_peak,depth_deferred,depth_error,(unsigned long long)test_handler_df);
    depth_mode=0;return ok?0:4;
}
static void *depth_worker(void *unused) { (void)unused;return (void *)(uintptr_t)depth_test(); }
int main(void) {
    if(!install())return 10;
    if(main_target(0) || worker_target_test() || main_target(0))return 11;
    puts("signal-delivery: main worker private restore ok");
    if(depth_test())return 13;
    pthread_t depth_thread;void *depth_result=NULL;
    if(pthread_create(&depth_thread,NULL,depth_worker,NULL) || pthread_join(depth_thread,&depth_result) || depth_result) return 14;
    puts("signal-delivery: nested depth and pending recovery ok");
    if(main_target(1))return 12;
    puts("signal-delivery: maximum mapping transaction ok");
    free(native_object);native_object=NULL;
    return 0;
}
