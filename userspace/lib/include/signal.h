#ifndef SRVROS_POSIX_SIGNAL_H
#define SRVROS_POSIX_SIGNAL_H

#include <sys/types.h>

typedef int sig_atomic_t;
typedef unsigned long sigset_t;
typedef void (*sighandler_t)(int);

typedef struct {
    void *ss_sp;
    int ss_flags;
    size_t ss_size;
} stack_t;

typedef struct {
    int si_signo;
    int si_errno;
    int si_code;
    pid_t si_pid;
    uid_t si_uid;
    void *si_addr;
    int si_status;
} siginfo_t;

#define SIGINT 2
#define SIGQUIT 3
#define SIGKILL 9
#define SIGUSR1 10
#define SIGSEGV 11
#define SIGUSR2 12
#define SIGPIPE 13
#define SIGALRM 14
#define SIGTERM 15
#define SIGCHLD 17
#define SIGCONT 18
#define SIGSTOP 19
#define SIGTSTP 20
#define SIGTTOU 22
#define SIGXFSZ 25
#define SIGPROF 27
#define SIG_DFL ((sighandler_t)0)
#define SIG_IGN ((sighandler_t)1)
#define SIG_ERR ((sighandler_t)-1)

#define SIG_BLOCK 0
#define SIG_UNBLOCK 1
#define SIG_SETMASK 2

#define SA_RESTART 0x10000000
#define SA_SIGINFO 0x00000004
#define SA_RESETHAND 0x80000000
#define SA_ONSTACK 0x08000000

#ifdef __cplusplus
extern "C" {
#endif

struct sigaction {
    union {
        sighandler_t sa_handler;
        void (*sa_sigaction)(int, siginfo_t *, void *);
    };
    sigset_t sa_mask;
    int sa_flags;
};

sighandler_t signal(int signum, sighandler_t handler);
int sigaction(int signum, const struct sigaction *act, struct sigaction *oldact);
int sigprocmask(int how, const sigset_t *set, sigset_t *oldset);
int pthread_sigmask(int how, const sigset_t *set, sigset_t *oldset);
int sigsuspend(const sigset_t *mask);
int sigpending(sigset_t *set);
int sigwait(const sigset_t *set, int *sig);
int kill(pid_t pid, int sig);
int pthread_kill(void *thread, int sig);
int raise(int sig);
int sigemptyset(sigset_t *set);
int sigfillset(sigset_t *set);
int sigaddset(sigset_t *set, int signum);
int sigdelset(sigset_t *set, int signum);
int sigismember(const sigset_t *set, int signum);

#ifdef __cplusplus
}
#endif

#endif
