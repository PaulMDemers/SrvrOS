#include <errno.h>
#include <signal.h>

sighandler_t signal(int signum, sighandler_t handler) {
    (void)signum;
    (void)handler;
    errno = ENOSYS;
    return SIG_ERR;
}

static int signal_valid(int signum) {
    return signum > 0 && signum < (int)(sizeof(sigset_t) * 8);
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
