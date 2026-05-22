#include <errno.h>
#include <sched.h>
#include <semaphore.h>

int sem_init(sem_t *sem, int pshared, unsigned int value) {
    (void)pshared;
    if (sem == 0) {
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

int sem_wait(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
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
    if (value == 0 || !__atomic_compare_exchange_n(&sem->__value, &value, value - 1, 0, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
        errno = EAGAIN;
        return -1;
    }
    return 0;
}

int sem_post(sem_t *sem) {
    if (sem == 0) {
        errno = EINVAL;
        return -1;
    }
    __atomic_add_fetch(&sem->__value, 1, __ATOMIC_RELEASE);
    return 0;
}

int sem_getvalue(sem_t *sem, int *sval) {
    if (sem == 0 || sval == 0) {
        errno = EINVAL;
        return -1;
    }
    *sval = (int)__atomic_load_n(&sem->__value, __ATOMIC_ACQUIRE);
    return 0;
}
