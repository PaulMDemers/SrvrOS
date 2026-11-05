#include <errno.h>
#include <signal.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <unistd.h>
#include "signal_runtime.h"

extern int __posix_signal_dispatch_pending(void);
static struct sigaction signal_actions[64];
static _Thread_local volatile sig_atomic_t signal_dispatching;
static _Thread_local uintptr_t signal_dispatch_anchor;
extern void __posix_signal_info_trampoline(void);
static unsigned actions_lock;
static void lock_actions(void) {
    while(__atomic_exchange_n(&actions_lock,1,__ATOMIC_ACQUIRE)) srv_yield();
}
static void unlock_actions(void) { __atomic_store_n(&actions_lock,0,__ATOMIC_RELEASE); }

static int signal_valid(int signum) {
    return signum > 0 && signum < (int)(sizeof(sigset_t) * 8);
}

static int signal_supported(int signum) {
    return signum == SIGINT ||
        signum == SIGQUIT || signum == SIGABRT || signum == SIGSEGV || signum == SIGPROF ||
        signum == SIGUSR1 ||
        signum == SIGUSR2 ||
        signum == SIGPIPE || signum == SIGXFSZ ||
        signum == SIGALRM ||
        signum == SIGTERM ||
        signum == SIGCHLD || signum == SIGHUP || signum == SIGWINCH;
}

static sigset_t signal_mask_for(int signum) {
    return (sigset_t)1 << signum;
}

static int signal_action_is_handler(sighandler_t handler) {
    return handler != SIG_DFL && handler != SIG_IGN && handler != SIG_ERR && handler != 0;
}

static int signal_apply_action(int signum, sighandler_t handler) {
    if (!signal_supported(signum)) {
        errno = EINVAL;
        return -1;
    }
    uint64_t action = SRV_SIGNAL_DEFAULT;
    if (handler == SIG_IGN) {
        action = SRV_SIGNAL_IGNORE;
    } else if (handler != SIG_DFL) {
        action = SRV_SIGNAL_CATCH;
    }
    if (srv_signal_config((uint64_t)signum, action) < 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

sighandler_t signal(int signum, sighandler_t handler) {
    struct sigaction action = {
        .sa_handler = handler,
        .sa_mask = 0,
        .sa_flags = 0,
    };
    struct sigaction old_action;
    if (sigaction(signum, &action, &old_action) < 0) {
        return SIG_ERR;
    }
    return old_action.sa_handler;
}

int kill(pid_t pid, int sig) {
    if (sig < 0 || sig >= 64) {
        errno = EINVAL;
        return -1;
    }
    long result=srv_kill_signal((int64_t)pid,(uint64_t)sig);
    if (result < 0) {
        errno = result < -1 ? (int)-result : ESRCH;
        return -1;
    }
    (void)__posix_signal_dispatch_pending();
    return 0;
}

int raise(int sig) {
    if(!signal_valid(sig)) { errno=EINVAL;return -1; }
    long result=srv_thread_signal((uint64_t)srv_thread_self(),(uint64_t)sig);
    if(result<0) { errno=(int)-result;return -1; }
    (void)__posix_signal_dispatch_pending();
    return 0;
}

static __attribute__((noinline)) int signal_dispatch_pending(int *restartable_out) {
    if (signal_dispatching || __posix_signal_info_depth()) {
        if (restartable_out != 0) {
            *restartable_out = 0;
        }
        return 0;
    }

    uint64_t pending = 0;
    uint64_t blocked = 0;
    if (srv_signal_pending(&pending) < 0 ||
        srv_signal_mask(SRV_SIGNAL_BLOCK, 0, &blocked) < 0) {
        return 0;
    }

    int dispatched = 0;
    int restartable = 1;
    signal_dispatching = 1;
    signal_dispatch_anchor=(uintptr_t)__builtin_frame_address(0);
    for (int signum = 1; signum < 64; signum++) {
        uint64_t mask = (uint64_t)signal_mask_for(signum);
        if ((pending & mask) == 0 || (blocked & mask) != 0 ||
            !signal_supported(signum)) {
            continue;
        }

        if(signum==SIGUSR1 && __posix_signal_public_available()) {
            struct srv_signal_control_v1 request={.size=96,.version=1,
                .operation=SRV_SIGNAL_CONTROL_LEGACY_TAKE,.signum=SIGUSR1};
            struct srv_signal_legacy_take_v1 selected={0};
            long result=__posix_signal_control(&request,&selected,sizeof(selected));
            if(result<0 || !selected.signum)continue;
            if(selected.size!=64 || selected.version!=1 || selected.signum!=SIGUSR1 ||
                selected.action.handler<=1 || (selected.action.flags&SRV_SIGNAL_NATIVE_INFO))__posix_signal_runtime_failure();
            ((sighandler_t)(uintptr_t)selected.action.handler)(SIGUSR1);
            struct srv_signal_control_v1 restore={.size=96,.version=1,.operation=SRV_SIGNAL_CONTROL_MASK,
                .mask_how=SRV_SIGNAL_SETMASK,.mask=selected.previous_mask};
            if(__posix_signal_control(&restore,NULL,0)<0)__posix_signal_runtime_failure();
            if(!(selected.action.flags&SRV_SIGNAL_NATIVE_RESTART))restartable=0;
            dispatched++;continue;
        }

        lock_actions();
        struct sigaction action = signal_actions[signum];
        if(!signal_action_is_handler(action.sa_handler)) { unlock_actions();continue; }
        uint64_t polled = 0;
        if (srv_signal_consume(mask, &polled) < 0 || polled != (uint64_t)signum) {
            unlock_actions();continue;
        }
        if (action.sa_flags & SA_RESETHAND) {
            signal_actions[signum]=(struct sigaction){.sa_handler=SIG_DFL};
            (void)signal_apply_action(signum,SIG_DFL);
        }
        unlock_actions();
        uint64_t handler_mask = (uint64_t)(action.sa_mask | signal_mask_for(signum));
        uint64_t old_mask = 0;
        if (srv_signal_mask(SRV_SIGNAL_BLOCK, handler_mask, &old_mask) < 0) {
            old_mask = blocked;
        }
        action.sa_handler(signum);
        (void)srv_signal_mask(SRV_SIGNAL_SETMASK, old_mask, 0);
        if ((action.sa_flags & SA_RESTART) == 0) {
            restartable = 0;
        }
        dispatched++;
    }
    signal_dispatching = 0;
    signal_dispatch_anchor=0;
    if (restartable_out != 0) {
        *restartable_out = dispatched != 0 && restartable;
    }
    return dispatched;
}

/* Blocking synthetic I/O needs both whether a handler ran and its restart
 * decision: SYS_POLL may return zero when it observes a pending signal. */
int __posix_signal_dispatch_pending_io(int *restartable) {
    return signal_dispatch_pending(restartable);
}

int __posix_signal_dispatch_pending(void) {
    return signal_dispatch_pending(0);
}

int __posix_signal_dispatch_pending_restartable(void) {
    int restartable = 0;
    return signal_dispatch_pending(&restartable) != 0 && restartable;
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (!signal_valid(signum)) {
        errno = EINVAL;
        return -1;
    }
    if(signum==SIGUSR1 && __posix_signal_public_available()) {
        struct srv_signal_control_v1 request={.size=96,.version=1,.signum=SIGUSR1,
            .operation=act?SRV_SIGNAL_CONTROL_EXCHANGE_ACTION:SRV_SIGNAL_CONTROL_QUERY_ACTION};
        if(!act && !oldact){errno=EINVAL;return -1;}
        if(act) {
            struct sigaction replacement=*act;
            request.action=(struct srv_signal_action_v1){(uint64_t)(uintptr_t)replacement.sa_handler,
                replacement.sa_mask,(uint32_t)replacement.sa_flags,0};
            if(replacement.sa_flags&SA_SIGINFO)request.trampoline=(uint64_t)(uintptr_t)__posix_signal_info_trampoline;
        }
        long result=__posix_signal_control(&request,oldact,oldact?sizeof(*oldact):0);
        if(result<0){errno=result==-1?ENOSYS:(int)-result;return -1;}
        return 0;
    }
    if(__posix_signal_info_depth()){errno=ENOTSUP;return -1;}
    /* A successful registration must describe delivery we actually implement.
     * Alternate stacks and siginfo/context frames need a kernel trampoline. */
    if (act != 0 && (signum==SIGKILL || signum==SIGSTOP ||
        (act->sa_flags & ~(SA_RESTART | SA_RESETHAND)) != 0 || act->sa_handler==SIG_ERR)) {
        errno = EINVAL;
        return -1;
    }

    struct sigaction replacement;
    if(act) replacement=*act; /* act and oldact may alias. */
    lock_actions();
    struct sigaction previous=signal_actions[signum];
    /* A fresh image has no libc handler table, but ignored dispositions survive. */
    if(srv_signal_config((uint64_t)signum,SRV_SIGNAL_QUERY)==SRV_SIGNAL_IGNORE)
        previous.sa_handler=SIG_IGN;
    if(act && signal_apply_action(signum,replacement.sa_handler)<0) {
        unlock_actions();return -1;
    }
    if(act) signal_actions[signum]=replacement;
    if(oldact) *oldact=previous;
    unlock_actions();
    return 0;
}

int sigprocmask(int how, const sigset_t *set, sigset_t *oldset) {
    uint64_t old_mask = 0;
    uint64_t srv_how;
    if (how == SIG_BLOCK) {
        srv_how = SRV_SIGNAL_BLOCK;
    } else if (how == SIG_UNBLOCK) {
        srv_how = SRV_SIGNAL_UNBLOCK;
    } else if (how == SIG_SETMASK) {
        srv_how = SRV_SIGNAL_SETMASK;
    } else {
        errno = EINVAL;
        return -1;
    }
    uint64_t next = set != 0 ? (uint64_t)(*set) : 0;
    if (set == 0) {
        srv_how = SRV_SIGNAL_BLOCK;
        next = 0;
    }
    if(__posix_signal_public_available()) {
        struct srv_signal_control_v1 request={.size=96,.version=1,.operation=SRV_SIGNAL_CONTROL_MASK,
            .mask_how=srv_how,.mask=next};
        long result=__posix_signal_control(&request,oldset,oldset?sizeof(*oldset):0);
        if(result<0){errno=result==-1?ENOSYS:(int)-result;return -1;}
        if(set && how!=SIG_BLOCK)(void)__posix_signal_dispatch_pending();
        return 0;
    }
    if (srv_signal_mask(srv_how, next, oldset != 0 ? &old_mask : 0) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (oldset != 0) {
        *oldset = (sigset_t)old_mask;
    }
    if (set != 0 && how != SIG_BLOCK) {
        (void)__posix_signal_dispatch_pending();
    }
    return 0;
}

__attribute__((noinline)) int sigsuspend(const sigset_t *mask) {
    if (mask == 0) {
        errno = EINVAL;
        return -1;
    }
    if(__posix_signal_public_available()) {
        if(signal_dispatching && !__posix_signal_info_depth()){errno=ENOTSUP;return -1;}
        uint64_t temporary=*mask;
        long ticket=srv_syscall6(SYS_SIGNAL_SUSPEND,SRV_SIGNAL_SUSPEND_BEGIN,temporary,
            (long)__builtin_frame_address(0),0,0,0);
        if(ticket<0){errno=ticket==-1?ENOSYS:(int)-ticket;return -1;}
        if(!ticket)__posix_signal_runtime_failure();
        for(;;) {
            int ran=__posix_signal_dispatch_pending();
            long result=srv_syscall6(SYS_SIGNAL_SUSPEND,SRV_SIGNAL_SUSPEND_FINISH,ticket,ran!=0,0,0,0);
            if(result==SRV_ERR_INTR){errno=EINTR;return -1;}
            if(result!=ticket)__posix_signal_runtime_failure();
        }
    }

    uint64_t old_mask = 0;
    if (srv_signal_mask(SRV_SIGNAL_SETMASK, (uint64_t)(*mask), &old_mask) < 0) {
        errno = EINVAL;
        return -1;
    }
    while (__posix_signal_dispatch_pending() == 0) {
        srv_yield();
    }
    (void)srv_signal_mask(SRV_SIGNAL_SETMASK, old_mask, 0);
    errno = EINTR;
    return -1;
}

int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset) {
    int saved_errno = errno;
    if (sigprocmask(how, set, oldset) < 0) {
        int error = errno != 0 ? errno : EINVAL;
        errno = saved_errno;
        return error;
    }
    errno = saved_errno;
    return 0;
}

int sigpending(sigset_t *set) {
    if (set == 0) {
        errno = EINVAL;
        return -1;
    }

    uint64_t pending = 0;
    uint64_t blocked = 0;
    if (srv_signal_pending(&pending) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (srv_signal_mask(SRV_SIGNAL_BLOCK, 0, &blocked) < 0) {
        errno = EINVAL;
        return -1;
    }
    *set = (sigset_t)pending & (sigset_t)blocked;
    return 0;
}

int sigwait(const sigset_t *set, int *sig) {
    if (set == 0 || sig == 0) {
        return EINVAL;
    }
    if(__posix_signal_info_depth())return ENOTSUP;
    if(__posix_signal_public_available()) {
        struct srv_signal_control_v1 request={.size=96,.version=1,
            .operation=SRV_SIGNAL_CONTROL_BLOCKED_TAKE,.mask=*set};
        for(;;) {
            long result=__posix_signal_control(&request,sig,sizeof(*sig));
            if(result<0)return result==-1?ENOSYS:(int)-result;
            if(*sig)return 0;
            srv_yield();
        }
    }

    for (;;) {
        uint64_t pending = 0;
        if (srv_signal_pending(&pending) < 0) {
            return EINVAL;
        }
        uint64_t wanted = pending & (uint64_t)(*set);
        for (int signum = 1; signum < 64; signum++) {
            if ((wanted & (uint64_t)signal_mask_for(signum)) != 0) {
                uint64_t polled = 0;
                if (srv_signal_consume((uint64_t)signal_mask_for(signum), &polled) < 0) return EINVAL;
                if(polled != (uint64_t)signum) continue;
                *sig = signum;
                return 0;
            }
        }
        srv_yield();
    }
}

void __posix_signal_unwind(uint64_t target_stack) {
    if(__posix_signal_info_depth())__posix_signal_runtime_failure();
    if(__posix_signal_public_available()) {
        struct srv_signal_io_v1 request={.size=128,.version=1,.operation=SRV_SIGNAL_IO_UNWIND,
            .args={target_stack,0,0,0,0,0}};
        if(srv_syscall6(SYS_SIGNAL_IO,(long)&request,128,0,0,0,0)<0)__posix_signal_runtime_failure();
    }
    if(signal_dispatching && target_stack>signal_dispatch_anchor){signal_dispatching=0;signal_dispatch_anchor=0;}
}

int sigemptyset(sigset_t *set) {
    if (set == 0) {
        errno = EINVAL;
        return -1;
    }
    *set = 0;
    return 0;
}

int sigfillset(sigset_t *set) {
    if (set == 0) {
        errno = EINVAL;
        return -1;
    }
    *set = ~((sigset_t)0);
    return 0;
}

int sigaddset(sigset_t *set, int signum) {
    if (set == 0 || !signal_valid(signum)) {
        errno = EINVAL;
        return -1;
    }
    *set |= (sigset_t)1 << signum;
    return 0;
}

int sigdelset(sigset_t *set, int signum) {
    if (set == 0 || !signal_valid(signum)) {
        errno = EINVAL;
        return -1;
    }
    *set &= ~((sigset_t)1 << signum);
    return 0;
}

int sigismember(const sigset_t *set, int signum) {
    if (set == 0 || !signal_valid(signum)) {
        errno = EINVAL;
        return -1;
    }
    return ((*set & ((sigset_t)1 << signum)) != 0) ? 1 : 0;
}
