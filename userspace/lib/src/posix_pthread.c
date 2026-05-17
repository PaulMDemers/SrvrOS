#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <srvros/sys.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

struct pthread_key_slot {
    int used;
    void *value;
    void (*destructor)(void *);
};

static struct pthread_key_slot pthread_keys[PTHREAD_KEYS_MAX];

int pthread_create(pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg) {
    (void)thread;
    (void)attr;
    (void)start_routine;
    (void)arg;
    return ENOSYS;
}

int pthread_join(pthread_t thread, void **value_ptr) {
    (void)thread;
    (void)value_ptr;
    return ENOSYS;
}

int pthread_detach(pthread_t thread) {
    (void)thread;
    return ENOSYS;
}

void pthread_exit(void *value_ptr) {
    (void)value_ptr;
    exit(0);
}

pthread_t pthread_self(void) {
    pid_t pid = getpid();
    return pid > 0 ? (pthread_t)pid : 1;
}

int pthread_equal(pthread_t left, pthread_t right) {
    return left == right;
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->stacksize = PTHREAD_STACK_MIN;
    attr->stackaddr = 0;
    return 0;
}

int pthread_attr_destroy(pthread_attr_t *attr) {
    return attr == 0 ? EINVAL : 0;
}

int pthread_attr_getdetachstate(const pthread_attr_t *attr, int *detachstate) {
    if (attr == 0 || detachstate == 0) {
        return EINVAL;
    }
    *detachstate = attr->detachstate;
    return 0;
}

int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate) {
    if (attr == 0 ||
        (detachstate != PTHREAD_CREATE_JOINABLE && detachstate != PTHREAD_CREATE_DETACHED)) {
        return EINVAL;
    }
    attr->detachstate = detachstate;
    return 0;
}

int pthread_attr_getstacksize(const pthread_attr_t *attr, size_t *stacksize) {
    if (attr == 0 || stacksize == 0) {
        return EINVAL;
    }
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize) {
    if (attr == 0 || stacksize < PTHREAD_STACK_MIN) {
        return EINVAL;
    }
    attr->stacksize = stacksize;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const void *attr) {
    (void)attr;
    if (mutex == 0) {
        return EINVAL;
    }
    mutex->locked = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return mutex == 0 ? EINVAL : 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    while (__atomic_exchange_n(&mutex->locked, 1, __ATOMIC_ACQUIRE) != 0) {
        sched_yield();
    }
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    int expected = 0;
    return __atomic_compare_exchange_n(&mutex->locked,
        &expected,
        1,
        0,
        __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED) ? 0 : EBUSY;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    __atomic_store_n(&mutex->locked, 0, __ATOMIC_RELEASE);
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const void *attr) {
    (void)attr;
    if (cond == 0) {
        return EINVAL;
    }
    cond->sequence = 0;
    return 0;
}

int pthread_cond_destroy(pthread_cond_t *cond) {
    return cond == 0 ? EINVAL : 0;
}

int pthread_cond_signal(pthread_cond_t *cond) {
    if (cond == 0) {
        return EINVAL;
    }
    __atomic_add_fetch(&cond->sequence, 1, __ATOMIC_RELEASE);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    return pthread_cond_signal(cond);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    if (cond == 0 || mutex == 0) {
        return EINVAL;
    }
    unsigned int observed = __atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(mutex);
    while (__atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE) == observed) {
        sched_yield();
        break;
    }
    return pthread_mutex_lock(mutex);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    (void)abstime;
    return pthread_cond_wait(cond, mutex);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (once_control == 0 || init_routine == 0) {
        return EINVAL;
    }
    int expected = 0;
    if (__atomic_compare_exchange_n(once_control,
            &expected,
            1,
            0,
            __ATOMIC_ACQ_REL,
            __ATOMIC_ACQUIRE)) {
        init_routine();
    }
    return 0;
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if (key == 0) {
        return EINVAL;
    }
    for (pthread_key_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&pthread_keys[i].used,
                &expected,
                1,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            pthread_keys[i].value = 0;
            pthread_keys[i].destructor = destructor;
            *key = i;
            return 0;
        }
    }
    return EAGAIN;
}

int pthread_key_delete(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !pthread_keys[key].used) {
        return EINVAL;
    }
    pthread_keys[key].value = 0;
    pthread_keys[key].destructor = 0;
    __atomic_store_n(&pthread_keys[key].used, 0, __ATOMIC_RELEASE);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !pthread_keys[key].used) {
        return 0;
    }
    return pthread_keys[key].value;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= PTHREAD_KEYS_MAX || !pthread_keys[key].used) {
        return EINVAL;
    }
    pthread_keys[key].value = (void *)value;
    return 0;
}

int sched_yield(void) {
    srv_syscall0(SYS_YIELD);
    return 0;
}
