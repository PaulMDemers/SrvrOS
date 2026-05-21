#include <errno.h>
#include <signal.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <unistd.h>

static struct sigaction signal_actions[64];

static int signal_valid(int signum) {
    return signum > 0 && signum < (int)(sizeof(sigset_t) * 8);
}

static int signal_supported(int signum) {
    return signum == SIGINT || signum == SIGTERM;
}

static sigset_t signal_mask_for(int signum) {
    return (sigset_t)1 << signum;
}

static int signal_apply_action(int signum, sighandler_t handler) {
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
    if (srv_kill_signal((int64_t)pid, (uint64_t)sig) < 0) {
        errno = ESRCH;
        return -1;
    }
    return 0;
}

int raise(int sig) {
    if (!signal_valid(sig) || !signal_supported(sig)) {
        errno = EINVAL;
        return -1;
    }
    sighandler_t handler = signal_actions[sig].sa_handler;
    if (handler == SIG_IGN) {
        return 0;
    }
    if (handler != SIG_DFL && handler != SIG_ERR && handler != 0) {
        handler(sig);
        return 0;
    }
    return kill(getpid(), sig);
}

int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact) {
    if (!signal_valid(signum) || !signal_supported(signum)) {
        errno = EINVAL;
        return -1;
    }
    if (act != 0 && (act->sa_flags & ~SA_RESTART) != 0) {
        errno = EINVAL;
        return -1;
    }

    if (oldact != 0) {
        *oldact = signal_actions[signum];
    }
    if (act != 0) {
        if (signal_apply_action(signum, act->sa_handler) < 0) {
            return -1;
        }
        signal_actions[signum] = *act;
    }
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
    if (srv_signal_mask(srv_how, next, oldset != 0 ? &old_mask : 0) < 0) {
        errno = EINVAL;
        return -1;
    }
    if (oldset != 0) {
        *oldset = (sigset_t)old_mask;
    }
    return 0;
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

    for (;;) {
        uint64_t pending = 0;
        if (srv_signal_pending(&pending) < 0) {
            return EINVAL;
        }
        uint64_t wanted = pending & (uint64_t)(*set);
        for (int signum = 1; signum < 64; signum++) {
            if ((wanted & (uint64_t)signal_mask_for(signum)) != 0) {
                uint64_t polled = 0;
                if (srv_signal_consume((uint64_t)signal_mask_for(signum), &polled) < 0 ||
                    polled != (uint64_t)signum) {
                    return EINVAL;
                }
                *sig = signum;
                return 0;
            }
        }
        srv_yield();
    }
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
