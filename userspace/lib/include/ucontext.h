#ifndef SRVROS_POSIX_UCONTEXT_H
#define SRVROS_POSIX_UCONTEXT_H

#include <signal.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef long greg_t;
typedef greg_t gregset_t[23];

enum {
    REG_R8 = 0,
    REG_R9 = 1,
    REG_R10 = 2,
    REG_R11 = 3,
    REG_R12 = 4,
    REG_R13 = 5,
    REG_R14 = 6,
    REG_R15 = 7,
    REG_RDI = 8,
    REG_RSI = 9,
    REG_RBP = 10,
    REG_RBX = 11,
    REG_RDX = 12,
    REG_RAX = 13,
    REG_RCX = 14,
    REG_RSP = 15,
    REG_RIP = 16,
    REG_EFL = 17,
    REG_CSGSFS = 18,
    REG_ERR = 19,
    REG_TRAPNO = 20,
    REG_OLDMASK = 21,
    REG_CR2 = 22
};

typedef struct {
    gregset_t gregs;
    void *fpregs;
    uint64_t __reserved1[8];
} mcontext_t;

typedef struct ucontext {
    unsigned long uc_flags;
    struct ucontext *uc_link;
    stack_t uc_stack;
    mcontext_t uc_mcontext;
    sigset_t uc_sigmask;
} ucontext_t;

int getcontext(ucontext_t *ucp);
int setcontext(const ucontext_t *ucp);
void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...);
int swapcontext(ucontext_t *oucp, const ucontext_t *ucp);

#ifdef __cplusplus
}
#endif

#endif
