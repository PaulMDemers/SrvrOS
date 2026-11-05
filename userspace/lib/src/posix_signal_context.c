#include <signal.h>
#include <ucontext.h>
#include <srvros/signal_abi.h>
#include <srvros/syscall_numbers.h>
#include <stddef.h>
#include <stdint.h>

/* Only SIGUSR1 INFO is admitted; these facts match the private wire contract. */
#define INFO_SIGNO 1u
#define INFO_TARGET_TID 2u
static _Thread_local volatile sig_atomic_t info_depth;

static __attribute__((noreturn)) void invalid_context(void) {
    register long number __asm__("rax")=SYS_EXIT;
    register long status __asm__("rdi")=128+SIGABRT;
    __asm__ volatile("int $0x80; ud2" : "+a"(number) : "D"(status) : "memory");
    __builtin_unreachable();
}

unsigned __posix_signal_info_depth(void) { return (unsigned)info_depth; }

void __posix_signal_info_invoke(int signum,const struct srv_signal_info_v1 *info,
    const struct signal_context_snapshot *snapshot,void (*handler)(int,siginfo_t *,void *)) {
    if (!info || !snapshot || !handler || signum!=SIGUSR1 ||
        info->size!=sizeof(*info) || info->version!=1 || info->signo!=(uint32_t)signum || info->code ||
        info->valid!=(INFO_SIGNO|INFO_TARGET_TID) || !info->target_tid ||
        info->reserved[0] || info->reserved[1] || info->reserved[2] || info->reserved[3] ||
        snapshot->size!=sizeof(*snapshot) || snapshot->version!=SIGNAL_CONTEXT_VERSION ||
        snapshot->flags || !snapshot->token || info_depth<0 || info_depth>=4) invalid_context();
    info_depth++;
    siginfo_t native_info={0};
    ucontext_t context={0};
    native_info.si_signo=signum;
    native_info.__srvros_valid=info->valid;
    native_info.__srvros_target_tid=info->target_tid;
    context.uc_sigmask=(sigset_t)snapshot->interrupted_mask;
    context.uc_mcontext.__srvros_snapshot=*snapshot;
    context.uc_mcontext.fpregs=context.uc_mcontext.__srvros_snapshot.fpu;
    greg_t *g=context.uc_mcontext.gregs;
    g[REG_R8]=snapshot->r8;g[REG_R9]=snapshot->r9;g[REG_R10]=snapshot->r10;g[REG_R11]=snapshot->r11;
    g[REG_R12]=snapshot->r12;g[REG_R13]=snapshot->r13;g[REG_R14]=snapshot->r14;g[REG_R15]=snapshot->r15;
    g[REG_RDI]=snapshot->rdi;g[REG_RSI]=snapshot->rsi;g[REG_RBP]=snapshot->rbp;g[REG_RBX]=snapshot->rbx;
    g[REG_RDX]=snapshot->rdx;g[REG_RAX]=snapshot->rax;g[REG_RCX]=snapshot->rcx;
    g[REG_RSP]=snapshot->rsp;g[REG_RIP]=snapshot->rip;g[REG_EFL]=snapshot->rflags;
    g[REG_OLDMASK]=snapshot->interrupted_mask;
    handler(signum,&native_info,&context);
    info_depth--;
}
