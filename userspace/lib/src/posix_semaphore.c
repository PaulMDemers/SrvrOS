#include <errno.h>
#include <sched.h>
#include <semaphore.h>
#include <srvros/sys.h>
#include "signal_runtime.h"

int __posix_signal_dispatch_pending_io(int *restartable);

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    if (sem == 0 || value > SEM_VALUE_MAX) {
        errno = EINVAL;
        return -1;
    }
    sem->__value = value;
    return 0;
}

int sem_destroy(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

__attribute__((noinline)) int sem_wait(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
    }
    if (__posix_signal_info_depth()) { errno = EOPNOTSUPP; return -1; }
    if (__posix_signal_public_available()) {
        struct posix_signal_io root;
        int status = __posix_signal_io_begin(&root, SRV_SIGNAL_ROOT_SEM_WAIT, SYS_YIELD, 0,
            (uint64_t)(uintptr_t)__builtin_frame_address(0), 0, 0, 0, 0, 0);
        if (status < 0) { errno = -status; return -1; }
        for (;;) {
            long guard = __posix_signal_io_local_begin(&root, 0, 0);
            if (guard < 0) { __posix_signal_io_end(&root); errno = (int)-guard; return -1; }
            unsigned int value = __atomic_load_n(&sem->__value, __ATOMIC_ACQUIRE);
            int consumed = 0;
            for (unsigned attempts = 0; value && attempts < 32; ++attempts) {
                if (__atomic_compare_exchange_n(&sem->__value, &value, value - 1, 0,
                                                __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                    consumed = 1;
                    break;
                }
            }
            __posix_signal_io_local_end(&root, (uint64_t)guard);
            if (consumed) { __posix_signal_io_end(&root); return 0; }
            int restartable = 0;
            int dispatched = __posix_signal_dispatch_pending_io(&restartable);
            if (dispatched && !restartable) {
                __posix_signal_io_end(&root); errno = EINTR; return -1;
            }
            const uint64_t args[6] = {0};
            long result = __posix_signal_io_call(&root, SYS_YIELD, args, 0, 0);
            restartable = 0;
            dispatched = __posix_signal_dispatch_pending_io(&restartable);
            if ((result < 0 && !(result == SRV_ERR_INTR && dispatched && restartable))
                || (dispatched && !restartable)) {
                __posix_signal_io_end(&root);
                errno = result < 0 ? (int)-result : EINTR;
                return -1;
            }
        }
    }
    for (;;) {
        unsigned int value = __atomic_load_n(&sem->__value, __ATOMIC_ACQUIRE);
        if (value > 0 && __atomic_compare_exchange_n(&sem->__value, &value, value - 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            return 0;
        }
        sched_yield();
    }
}

int sem_trywait(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned int value = __atomic_load_n(&sem->__value, __ATOMIC_ACQUIRE);
    for (;;) {
        if (value == 0) {
            errno = EAGAIN;
            return -1;
        }
        if (__atomic_compare_exchange_n(&sem->__value, &value, value - 1, 0,
                                        __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
            return 0;
        }
        /* A concurrent post/consume updates value. Only an observed zero
         * means the operation would block; CAS contention is not EAGAIN. */
    }
}

int sem_post(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
    }
    unsigned int value = __atomic_load_n(&sem->__value, __ATOMIC_RELAXED);
    for (;;) {
        if (value >= SEM_VALUE_MAX) {
            errno = EOVERFLOW;
            return -1;
        }
        if (__atomic_compare_exchange_n(&sem->__value, &value, value + 1, 0,
                                        __ATOMIC_RELEASE, __ATOMIC_RELAXED)) {
            return 0;
        }
    }
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (sem == 0 || sval == 0) {
        errno = EINVAL;
        return -1;
    }
    *sval = (int)__atomic_load_n(&sem->__value, __ATOMIC_ACQUIRE);
    return 0;
}
