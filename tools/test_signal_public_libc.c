/* Actual libc signal/runtime sources with an explicit kernel model. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define sigaction test_sigaction
#define sigprocmask test_sigprocmask
#define pthread_sigmask test_pthread_sigmask
#define sigsuspend test_sigsuspend
#define sigpending test_sigpending
#define sigwait test_sigwait
#define signal test_signal
#define kill test_kill
#define raise test_raise
#define sigemptyset test_sigemptyset
#define sigfillset test_sigfillset
#define sigaddset test_sigaddset
#define sigdelset test_sigdelset
#define sigismember test_sigismember
#include "../userspace/lib/src/signal_runtime.c"
#include "../userspace/lib/src/posix_signal.c"
static int err,depth,features_enabled,feature_queries,config_calls,suspend_finishes,unwind_calls,exit_calls,pending_queries;
static void *jmpbuf[5];static struct sigaction model_action;static sigset_t model_mask;
static void *expected_output;static uint64_t last_unwind;
int *__errno_location(void) {return &err;}
unsigned __posix_signal_info_depth(void) {return (unsigned)depth;}
void __posix_signal_info_trampoline(void) {}
long srv_signal_config(uint64_t sig,uint64_t op) {(void)sig;config_calls++;return op==SRV_SIGNAL_QUERY?SRV_SIGNAL_DEFAULT:0;}
long srv_signal_pending(uint64_t *out) {pending_queries++;*out=0;return 0;}
long srv_signal_mask(uint64_t how,uint64_t bits,uint64_t *out) {(void)how;(void)bits;if(out)*out=model_mask;return 0;}
long srv_signal_consume(uint64_t mask,uint64_t *out) {(void)mask;*out=0;return 0;}
long srv_thread_self(void) {return 1;}
long srv_thread_signal(uint64_t tid,uint64_t sig) {(void)tid;(void)sig;return 0;}
long srv_kill_signal(int64_t pid,uint64_t sig) {(void)pid;(void)sig;return 0;}
void srv_yield(void) {}
long srv_syscall6(long number,long a,long b,long c,long d,long e,long f) {
    assert(!e && (!f || (number==SYS_SIGNAL_IO && f==1)));
    if(number==SYS_EXIT) {exit_calls++;__builtin_longjmp(jmpbuf,1);}
    if(number==SYS_SIGNAL_SUSPEND) {
        if(a==SRV_SIGNAL_SUSPEND_BEGIN) {assert(b==0x40 && c && !d && !f);return 31;}
        assert(a==SRV_SIGNAL_SUSPEND_FINISH && b==31 && !c && !d && !f);
        return ++suspend_finishes==1?31:SRV_ERR_INTR;
    }
    if(number==SYS_SIGNAL_IO) {
        const struct srv_signal_io_v1 *io=(void *)a;
        assert(b==128 && !c && !d && !f && io->operation==SRV_SIGNAL_IO_UNWIND);
        unwind_calls++;last_unwind=io->args[0];return 0;
    }
    assert(number==SYS_SIGNAL_CONTROL && b==96 && !f);
    const struct srv_signal_control_v1 *w=(void *)a;
    if(w->operation==SRV_SIGNAL_CONTROL_FEATURES) {
        struct srv_signal_features_v1 *features=(void *)c;assert(d==64);feature_queries++;
        *features=(struct srv_signal_features_v1){.size=64,.version=1,.capabilities=features_enabled?1:0,
            .info_signals=1ull<<SIGUSR1,.context_bytes=688,.info_bytes=64,.max_depth=4};return 0;
    }
    if(w->operation==SRV_SIGNAL_CONTROL_QUERY_ACTION || w->operation==SRV_SIGNAL_CONTROL_EXCHANGE_ACTION) {
        assert(w->signum==SIGUSR1 && (void *)c==expected_output);
        if((w->action.flags&SA_SIGINFO) && (w->action.flags&SA_RESTART))return SRV_ERR_NOTSUP;
        struct sigaction old=model_action;
        if(w->operation==SRV_SIGNAL_CONTROL_EXCHANGE_ACTION) {
            model_action.sa_handler=(sighandler_t)(uintptr_t)w->action.handler;
            model_action.sa_mask=w->action.mask;model_action.sa_flags=(int)w->action.flags;
        }
        if(c) {assert(d==24);memcpy((void *)c,&old,24);}return 0;
    }
    if(w->operation==SRV_SIGNAL_CONTROL_MASK) {
        assert((void *)c==expected_output);sigset_t old=model_mask;
        model_mask=w->mask_how==SRV_SIGNAL_BLOCK?old|w->mask:w->mask_how==SRV_SIGNAL_UNBLOCK?old&~w->mask:w->mask;
        if(c){assert(d==8);memcpy((void *)c,&old,8);}return 0;
    }
    assert(0);return -1;
}
static void handler(int sig) {(void)sig;}
int main(void) {
    features_enabled=0;public_available=0;assert(!__posix_signal_public_available() && feature_queries==1);
    struct sigaction act={.sa_handler=handler},old={0};assert(test_sigaction(SIGUSR1,&act,&old)==0 && config_calls);
    features_enabled=1;public_available=0;assert(__posix_signal_public_available());
    model_action=(struct sigaction){.sa_handler=SIG_IGN,.sa_mask=4,.sa_flags=0};
    expected_output=&act;act=(struct sigaction){.sa_handler=handler,.sa_mask=8};
    assert(test_sigaction(SIGUSR1,&act,&act)==0 && act.sa_handler==SIG_IGN && model_action.sa_handler==handler && model_action.sa_mask==8);
    old=(struct sigaction){0};expected_output=&old;assert(test_sigaction(SIGUSR1,NULL,&old)==0 && old.sa_handler==handler);
    struct sigaction bad={.sa_handler=handler,.sa_flags=SA_SIGINFO|SA_RESTART};expected_output=NULL;
    assert(test_sigaction(SIGUSR1,&bad,NULL)==-1 && errno==ENOTSUP && model_action.sa_handler==handler);
    sigset_t set=0x40,prior=0;expected_output=&prior;model_mask=0x20;
    assert(test_sigprocmask(SIG_SETMASK,&set,&prior)==0 && prior==0x20 && model_mask==0x40);
    depth=1;suspend_finishes=0;int old_pending_queries=pending_queries;assert(test_sigsuspend(&set)==-1 && errno==EINTR && suspend_finishes==2 && pending_queries==old_pending_queries);
    if(!__builtin_setjmp(jmpbuf)) {__posix_signal_unwind(0x1234);assert(0);}
    assert(exit_calls==1 && unwind_calls==0);depth=0;__posix_signal_unwind(0x5678);
    assert(unwind_calls==1 && last_unwind==0x5678);
    signal_dispatching=1;signal_dispatch_anchor=0x8000;
    __posix_signal_unwind(0x7000);assert(signal_dispatching && signal_dispatch_anchor==0x8000);
    __posix_signal_unwind(0x9000);assert(!signal_dispatching && !signal_dispatch_anchor && unwind_calls==3);
    puts("signal-public-libc: actual-source staged signal model PASS");return 0;
}
