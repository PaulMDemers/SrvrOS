#include <errno.h>
#include <stdarg.h>
#include <string.h>
#include <ucontext.h>

int getcontext(ucontext_t *ucp) {
    if (ucp == 0) {
        errno = EINVAL;
        return -1;
    }
    memset(ucp, 0, sizeof(*ucp));
    return 0;
}

int setcontext(const ucontext_t *ucp) {
    (void)ucp;
    errno = ENOSYS;
    return -1;
}

void makecontext(ucontext_t *ucp, void (*func)(void), int argc, ...) {
    (void)ucp;
    (void)func;
    va_list args;
    va_start(args, argc);
    va_end(args);
}

int swapcontext(ucontext_t *oucp, const ucontext_t *ucp) {
    if (oucp != 0) {
        (void)getcontext(oucp);
    }
    (void)ucp;
    errno = ENOSYS;
    return -1;
}
