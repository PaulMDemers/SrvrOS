/* Source-preparation probe for the experimental public signal kernel only.
 * Link with tools/test_signal_delivery_guest.S for its existing register loop.
 * The external harness must enforce a bounded guest timeout. */
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <ucontext.h>
#include <srvros/sys.h>
volatile unsigned test_ready,test_done;
_Alignas(16) unsigned char test_fx_before[512],test_fx_after[512],test_fx_saved[512];
uint64_t test_caller_rsp,test_probe_rsp,test_probe_rsp_after,test_fs_before,test_fs_after,test_registers[15],test_redzone[16];
uint64_t test_expected_pointer,test_probe_flags,test_handler_df;
extern const unsigned char test_xmm_pattern[256],test_loop_begin[],test_loop_end[];
extern void test_probe(void);
static volatile uint64_t target_tid;
static volatile unsigned phase,handshake,failed;
static __thread volatile uint64_t tls_cookie;
static uint64_t native_object=UINT64_C(0x9876543212345678);
static void handler(int signo,siginfo_t *info,void *opaque) {
    if(signo!=SIGUSR1 || !info || info->si_signo!=SIGUSR1 || info->__srvros_valid!=3 || info->__srvros_target_tid!=target_tid || !opaque) {failed=1;return;}
    if(handshake) {
        unsigned observed=__atomic_load_n(&phase,__ATOMIC_ACQUIRE);
        if(observed==2) {__atomic_store_n(&phase,3,__ATOMIC_RELEASE);return;}
        if(observed!=1) {failed=2;return;}
        __atomic_store_n(&phase,2,__ATOMIC_RELEASE);
        sigset_t mask;if(sigfillset(&mask) || sigdelset(&mask,SIGUSR1) || sigsuspend(&mask)!=-1 || errno!=EINTR) {failed=3;return;}
        if(__atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=3)failed=4;
        __atomic_store_n(&phase,4,__ATOMIC_RELEASE);return;
    }
    ucontext_t *uc=opaque;const struct signal_context_snapshot *snap=&uc->uc_mcontext.__srvros_snapshot;
    if(snap->size!=688 || snap->version!=1 || snap->rip<(uint64_t)(uintptr_t)test_loop_begin ||
       snap->rip>=(uint64_t)(uintptr_t)test_loop_end || snap->rsp!=test_probe_rsp || !(snap->rflags&0x400) ||
       tls_cookie!=UINT64_C(0x1122334455667788) || native_object!=UINT64_C(0x9876543212345678) ||
       uc->uc_mcontext.fpregs!=snap->fpu || memcmp(snap->fpu,test_fx_before,512) ||
       memcmp(snap->fpu+160,test_xmm_pattern,256))failed=5;
    uint16_t cw;uint32_t mxcsr;memcpy(&cw,snap->fpu,2);memcpy(&mxcsr,snap->fpu+24,4);
    unsigned tags=0;for(unsigned i=0;i<8;i++)tags+=(snap->fpu[4]>>i)&1u;
    if(cw!=0x077f || mxcsr!=0x3f80 || tags!=2)failed=5;
    const uint64_t values[]={snap->rax,snap->rbx,snap->rcx,snap->rdx,snap->rsi,snap->rdi,snap->rbp,
        snap->r8,snap->r9,snap->r10,snap->r11,snap->r12,snap->r13,snap->r14,snap->r15};
    for(unsigned i=0;i<14;i++)if(values[i]!=i+1)failed=6;
    const int indices[]={REG_RAX,REG_RBX,REG_RCX,REG_RDX,REG_RSI,REG_RDI,REG_RBP,
        REG_R8,REG_R9,REG_R10,REG_R11,REG_R12,REG_R13,REG_R14,REG_R15};
    for(unsigned i=0;i<15;i++)if((uint64_t)uc->uc_mcontext.gregs[indices[i]]!=values[i])failed=7;
    if(values[14]!=test_expected_pointer || (uint64_t)uc->uc_mcontext.gregs[REG_RSP]!=snap->rsp ||
       (uint64_t)uc->uc_mcontext.gregs[REG_RIP]!=snap->rip || (uint64_t)uc->uc_mcontext.gregs[REG_EFL]!=snap->rflags)failed=7;
    /* Mutating public context cannot change the private authoritative restore. */
    memset(uc,0xa5,sizeof(*uc));__atomic_store_n(&test_done,1,__ATOMIC_RELEASE);
}
static void *sender(void *unused) {
    (void)unused;
    while(!__atomic_load_n(&test_ready,__ATOMIC_ACQUIRE))srv_yield();
    if(srv_thread_signal(target_tid,SIGUSR1)<0){failed=8;__atomic_store_n(&test_done,1,__ATOMIC_RELEASE);}
    if(handshake) {
        while(__atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=2 && !failed)srv_yield();
        if(!failed && srv_thread_signal(target_tid,SIGUSR1)<0)failed=9;
    }
    return NULL;
}
static int run_probe(void) {
    pthread_t t;target_tid=(uint64_t)srv_thread_self();tls_cookie=UINT64_C(0x1122334455667788);
    test_expected_pointer=(uint64_t)(uintptr_t)&native_object;test_ready=test_done=failed=0;handshake=0;
    if(pthread_create(&t,NULL,sender,NULL))return 1;
    test_probe();if(pthread_join(t,NULL))return 2;
    if(failed || test_probe_rsp!=test_probe_rsp_after || test_fs_before!=test_fs_after ||
       tls_cookie!=UINT64_C(0x1122334455667788) || !(test_probe_flags&0x400) || memcmp(test_fx_before,test_fx_after,512))return 3;
    for(unsigned i=0;i<14;i++)if(test_registers[i]!=i+1)return 4;
    if(test_registers[14]!=test_expected_pointer)return 5;
    for(unsigned i=0;i<16;i++)if(test_redzone[i]!=UINT64_C(0x6162636465666768))return 6;
    return 0;
}
static void *worker_probe(void *unused) {(void)unused;return (void *)(uintptr_t)run_probe();}
static int run_handshake(void) {
    pthread_t t;target_tid=(uint64_t)srv_thread_self();test_ready=failed=0;handshake=1;phase=1;
    if(pthread_create(&t,NULL,sender,NULL))return 1;
    __atomic_store_n(&test_ready,1,__ATOMIC_RELEASE);
    while(__atomic_load_n(&phase,__ATOMIC_ACQUIRE)!=4 && !failed)srv_yield();
    if(pthread_join(t,NULL))return 2;handshake=0;return failed?3:0;
}
int main(void) {
    struct sigaction a={.sa_sigaction=handler,.sa_flags=SA_SIGINFO};
    if(sigemptyset(&a.sa_mask) || sigaction(SIGUSR1,&a,NULL))return 10;
    if(run_probe())return 11;
    pthread_t t;void *result=NULL;if(pthread_create(&t,NULL,worker_probe,NULL) || pthread_join(t,&result) || result)return 12;
    puts("signal-public: native main and worker context ok");
    if(run_handshake())return 13;
    puts("signal-public: nested suspend resume handshake ok");return 0;
}
