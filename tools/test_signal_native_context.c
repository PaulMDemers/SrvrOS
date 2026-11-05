#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include <signal.h>
#include <ucontext.h>
#ifdef __cplusplus
#define CHECK static_assert
#define ALIGN alignof
extern "C" {
#else
#define CHECK _Static_assert
#define ALIGN _Alignof
#endif
void __posix_signal_info_invoke(int,const struct srv_signal_info_v1 *,const struct signal_context_snapshot *,void (*)(int,siginfo_t *,void *));
unsigned __posix_signal_info_depth(void);
#ifdef __cplusplus
}
#endif
CHECK(offsetof(siginfo_t,si_signo)==0 && offsetof(siginfo_t,si_status)==40,"old siginfo offsets");
CHECK(offsetof(siginfo_t,__srvros_valid)==48 && offsetof(siginfo_t,__srvros_target_tid)==56 && sizeof(siginfo_t)==64,"extended info");
CHECK(offsetof(mcontext_t,__srvros_snapshot)==256 && sizeof(mcontext_t)==944 && ALIGN(mcontext_t)==16,"mcontext layout");
CHECK(offsetof(ucontext_t,uc_mcontext)==48 && sizeof(ucontext_t)==1008 && ALIGN(ucontext_t)==16,"ucontext layout");
static int saved_errno;
#ifdef __cplusplus
extern "C"
#endif
int *__errno_location(void) { return &saved_errno; }
static struct srv_signal_info_v1 wire_info;
static struct signal_context_snapshot wire_context;
static unsigned visits;
static void handler(int signal,siginfo_t *info,void *raw) {
    ucontext_t *u=(ucontext_t *)raw;unsigned depth=__posix_signal_info_depth();
    assert(depth>=1 && depth<=4 && signal==SIGUSR1 && info->si_signo==SIGUSR1);
    assert(info->__srvros_valid==3 && info->__srvros_target_tid==9 && !info->si_errno && !info->si_code &&
        !info->si_pid && !info->si_uid && !info->si_addr && !info->si_status);
    assert(u->uc_sigmask==wire_context.interrupted_mask && u->uc_mcontext.gregs[REG_R15]==15 &&
        u->uc_mcontext.gregs[REG_RDI]==101 && u->uc_mcontext.gregs[REG_RBP]==102 &&
        u->uc_mcontext.gregs[REG_RSP]==0x12340 && u->uc_mcontext.gregs[REG_RIP]==0x56780 &&
        u->uc_mcontext.gregs[REG_EFL]==0x202 && u->uc_mcontext.gregs[REG_OLDMASK]==32 &&
        !u->uc_mcontext.gregs[REG_CSGSFS] && !u->uc_mcontext.gregs[REG_ERR] &&
        !u->uc_mcontext.gregs[REG_TRAPNO] && !u->uc_mcontext.gregs[REG_CR2]);
    const int indices[]={REG_R8,REG_R9,REG_R10,REG_R11,REG_R12,REG_R13,REG_R14,REG_R15,
        REG_RDI,REG_RSI,REG_RBP,REG_RBX,REG_RDX,REG_RAX,REG_RCX};
    const long expected[]={8,9,10,11,12,13,14,15,101,103,102,104,105,106,107};
    for(unsigned i=0;i<sizeof(indices)/sizeof(indices[0]);i++)assert(u->uc_mcontext.gregs[indices[i]]==expected[i]);
    assert(u->uc_mcontext.fpregs==u->uc_mcontext.__srvros_snapshot.fpu &&
        !memcmp(&u->uc_mcontext.__srvros_snapshot,&wire_context,sizeof(wire_context)));
    visits++;
    if(depth<4) {__posix_signal_info_invoke(SIGUSR1,&wire_info,&wire_context,handler);assert(__posix_signal_info_depth()==depth);}
}
int main(void) {
    wire_info=(struct srv_signal_info_v1){.size=sizeof(wire_info),.version=1,.signo=SIGUSR1,.valid=3,.target_tid=9};
    wire_context=(struct signal_context_snapshot){.size=sizeof(wire_context),.version=1,.token=1,.interrupted_mask=32,
        .r15=15,.r14=14,.r13=13,.r12=12,.r11=11,.r10=10,.r9=9,.r8=8,
        .rbp=102,.rdi=101,.rsi=103,.rdx=105,.rcx=107,.rbx=104,.rax=106,.rip=0x56780,.rsp=0x12340,.rflags=0x202};
    for(unsigned i=0;i<512;i++)wire_context.fpu[i]=(uint8_t)i;
    __posix_signal_info_invoke(SIGUSR1,&wire_info,&wire_context,handler);
    assert(visits==4 && !__posix_signal_info_depth());
    ucontext_t before,other;memset(&before,0xa5,sizeof(before));other=before;
    assert(getcontext(&before)==-1 && errno==ENOSYS && !memcmp(&before,&other,sizeof(before)));
    assert(swapcontext(&before,&other)==-1 && errno==ENOSYS && !memcmp(&before,&other,sizeof(before)));
    makecontext(&before,(void (*)(void))handler,0);assert(errno==ENOSYS && !memcmp(&before,&other,sizeof(before)));
    return 0;
}
