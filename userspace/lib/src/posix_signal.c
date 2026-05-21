#include <errno.h>
#include <signal.h>
#include <srvros/sys.h>
#include <unistd.h>

static sighandler_t signal_handlers[64];

static int signal_valid(int signum) {
    return signum > 0 && signum < (int)(sizeof(sigset_t) * 8);
}

static int signal_supported(int signum) {
    return signum == SIGINT || signum == SIGTERM;
}

sighandler_t signal(int signum, sighandler_t handler) {
    if (!signal_valid(signum) || !signal_supported(signum)) {
        errno = EINVAL;
        return SIG_ERR;
    }

    uint64_t action = SRV_SIGNAL_DEFAULT;
    if (handler == SIG_IGN) {
        action = SRV_SIGNAL_IGNORE;
    } else if (handler != SIG_DFL) {
        action = SRV_SIGNAL_CATCH;
    }
    if (srv_signal_config((uint64_t)signum, action) < 0) {
        errno = EINVAL;
        return SIG_ERR;
    }
    sighandler_t previous = signal_handlers[signum];
    signal_handlers[signum] = handler;
    return previous;
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
    sighandler_t handler = signal_handlers[sig];
    if (handler == SIG_IGN) {
        return 0;
    }
    if (handler != SIG_DFL && handler != SIG_ERR && handler != 0) {
        handler(sig);
        return 0;
    }
    return kill(getpid(), sig);
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
