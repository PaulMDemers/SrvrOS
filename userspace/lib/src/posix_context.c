#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <ucontext.h>

int getcontext(ucontext_t *ucp) {
    (void)ucp;
    errno = ENOSYS;
    return -1;
}

int setcontext(const ucontext_t *ucp) {
    (void)ucp;
    errno = ENOSYS;
    return -1;
}

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...) {
    (void)ucp; (void)func; (void)argc;
    errno = ENOSYS;
}

int swapcontext(ucontext_t *oucp, const ucontext_t *ucp) {
    (void)oucp; (void)ucp;
    errno = ENOSYS;
    return -1;
}
