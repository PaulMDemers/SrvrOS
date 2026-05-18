#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PTHREAD_DEFAULT_STACK_SIZE 65536
#define PTHREAD_MAX_RECORDS 16
#define PTHREAD_TLS_THREADS 16
#define PTHREAD_TICKS_PER_SECOND 100

struct pthread_key_slot {
    int used;
    void (*destructor)(void *);
};

struct pthread_start_record {
    void *(*start_routine)(void *);
    void *arg;
    void *stack;
    size_t stack_size;
    int owns_stack;
};

struct pthread_record {
    int used;
    int detached;
    pthread_t tid;
    struct pthread_start_record *start;
};

struct pthread_tls_slot {
    int used;
    pthread_t tid;
    void *values[PTHREAD_KEYS_MAX];
};

static struct pthread_key_slot pthread_keys[PTHREAD_KEYS_MAX];
static struct pthread_record pthread_records[PTHREAD_MAX_RECORDS];
static struct pthread_tls_slot pthread_tls_slots[PTHREAD_TLS_THREADS];

static struct pthread_record *pthread_record_find(pthread_t tid) {
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        if (pthread_records[i].used && pthread_records[i].tid == tid) {
            return &pthread_records[i];
        }
    }
    return 0;
}

static struct pthread_record *pthread_record_alloc(void) {
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        int expected = 0;
        if (__atomic_compare_exchange_n(&pthread_records[i].used,
                &expected,
                1,
                0,
                __ATOMIC_ACQ_REL,
                __ATOMIC_ACQUIRE)) {
            pthread_records[i].tid = 0;
            pthread_records[i].detached = 0;
            pthread_records[i].start = 0;
            return &pthread_records[i];
        }
    }
    return 0;
}

static void pthread_record_release(struct pthread_record *record) {
    if (record == 0) {
        return;
    }
    record->tid = 0;
    record->detached = 0;
    record->start = 0;
    __atomic_store_n(&record->used, 0, __ATOMIC_RELEASE);
}

static void pthread_record_cleanup(struct pthread_record *record) {
    struct pthread_start_record *start = record != 0 ? record->start : 0;
    pthread_record_release(record);
    if (start == 0) {
        return;
    }
    if (start->owns_stack) {
        munmap(start->stack, start->stack_size);
    }
    free(start);
}

static void pthread_reap_detached(void) {
    pthread_t self = pthread_self();
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        struct pthread_record *record = &pthread_records[i];
        if (!record->used || !record->detached || record->tid == self) {
            continue;
        }
        long status = srv_thread_status(record->tid);
        if (status == 0) {
            uint64_t ignored = 0;
            if (srv_thread_join(record->tid, &ignored) == 0) {
                pthread_record_cleanup(record);
            }
        } else if (status < 0) {
            pthread_record_release(record);
        }
    }
}

static struct pthread_tls_slot *pthread_tls_slot_for(pthread_t tid, int create) {
    struct pthread_tls_slot *free_slot = 0;
    for (int i = 0; i < PTHREAD_TLS_THREADS; i++) {
        if (pthread_tls_slots[i].used && pthread_tls_slots[i].tid == tid) {
            return &pthread_tls_slots[i];
        }
        if (!pthread_tls_slots[i].used && free_slot == 0) {
            free_slot = &pthread_tls_slots[i];
        }
    }
    if (!create || free_slot == 0) {
        return 0;
    }

    free_slot->tid = tid;
    for (int i = 0; i < PTHREAD_KEYS_MAX; i++) {
        free_slot->values[i] = 0;
    }
    __atomic_store_n(&free_slot->used, 1, __ATOMIC_RELEASE);
    return free_slot;
}

static void pthread_run_destructors(void) {
    struct pthread_tls_slot *slot = pthread_tls_slot_for(pthread_self(), 0);
    if (slot == 0) {
        return;
    }

    for (pthread_key_t i = 0; i < PTHREAD_KEYS_MAX; i++) {
        void *value = slot->values[i];
        void (*destructor)(void *) = pthread_keys[i].destructor;
        slot->values[i] = 0;
        if (value != 0 && pthread_keys[i].used && destructor != 0) {
            destructor(value);
        }
    }
    __atomic_store_n(&slot->used, 0, __ATOMIC_RELEASE);
}

static void pthread_start_trampoline(uint64_t raw_start) {
    struct pthread_start_record *start = (struct pthread_start_record *)(uintptr_t)raw_start;
    void *result = start->start_routine(start->arg);
    pthread_exit(result);
}

int pthread_create(pthread_t *thread,
    const pthread_attr_t *attr,
    void *(*start_routine)(void *),
    void *arg) {
    if (thread == 0 || start_routine == 0) {
        return EINVAL;
    }
    pthread_reap_detached();

    size_t stack_size = PTHREAD_DEFAULT_STACK_SIZE;
    int detachstate = PTHREAD_CREATE_JOINABLE;
    void *stack = 0;
    int owns_stack = 1;
    if (attr != 0) {
        if (attr->stacksize < PTHREAD_STACK_MIN ||
            (attr->detachstate != PTHREAD_CREATE_JOINABLE &&
                attr->detachstate != PTHREAD_CREATE_DETACHED)) {
            return EINVAL;
        }
        stack_size = attr->stacksize > stack_size ? attr->stacksize : stack_size;
        detachstate = attr->detachstate;
        if (attr->stackaddr != 0) {
            stack = attr->stackaddr;
            owns_stack = 0;
        }
    }

    if ((stack_size & 0xfff) != 0) {
        stack_size = (stack_size + 0xfff) & ~(size_t)0xfff;
    }
    if (stack == 0) {
        stack = mmap(0,
            stack_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);
        if (stack == MAP_FAILED) {
            return ENOMEM;
        }
    }

    struct pthread_start_record *start = malloc(sizeof(*start));
    if (start == 0) {
        if (owns_stack) {
            munmap(stack, stack_size);
        }
        return ENOMEM;
    }
    *start = (struct pthread_start_record) {
        .start_routine = start_routine,
        .arg = arg,
        .stack = stack,
        .stack_size = stack_size,
        .owns_stack = owns_stack,
    };

    struct pthread_record *record = pthread_record_alloc();
    if (record == 0) {
        free(start);
        if (owns_stack) {
            munmap(stack, stack_size);
        }
        return EAGAIN;
    }

    uint64_t stack_top = (((uint64_t)(uintptr_t)stack + stack_size) & ~(uint64_t)0xf) - 8;
    *(uint64_t *)(uintptr_t)stack_top = 0;
    uint64_t flags = 0;
    long tid = srv_thread_create((uint64_t)(uintptr_t)pthread_start_trampoline,
        (uint64_t)(uintptr_t)start,
        stack_top,
        flags);
    if (tid < 0) {
        pthread_record_release(record);
        free(start);
        if (owns_stack) {
            munmap(stack, stack_size);
        }
        return EAGAIN;
    }

    *thread = (pthread_t)tid;
    record->tid = (pthread_t)tid;
    record->detached = detachstate == PTHREAD_CREATE_DETACHED;
    record->start = start;
    return 0;
}

int pthread_join(pthread_t thread, void **value_ptr) {
    pthread_reap_detached();
    struct pthread_record *record = pthread_record_find(thread);
    if (record == 0 || record->detached) {
        return EINVAL;
    }

    uint64_t value = 0;
    if (srv_thread_join(thread, &value) < 0) {
        return EINVAL;
    }
    if (value_ptr != 0) {
        *value_ptr = (void *)(uintptr_t)value;
    }

    pthread_record_cleanup(record);
    return 0;
}

int pthread_detach(pthread_t thread) {
    pthread_reap_detached();
    struct pthread_record *record = pthread_record_find(thread);
    if (record == 0 || record->detached) {
        return EINVAL;
    }
    record->detached = 1;
    if (srv_thread_status(thread) == 0) {
        uint64_t ignored = 0;
        if (srv_thread_join(thread, &ignored) == 0) {
            pthread_record_cleanup(record);
        }
    }
    return 0;
}

void pthread_exit(void *value_ptr) {
    pthread_run_destructors();
    srv_thread_exit((uint64_t)(uintptr_t)value_ptr);
    _exit(0);
}

pthread_t pthread_self(void) {
    long tid = srv_thread_self();
    return tid > 0 ? (pthread_t)tid : 1;
}

int pthread_equal(pthread_t left, pthread_t right) {
    return left == right;
}

int pthread_attr_init(pthread_attr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->detachstate = PTHREAD_CREATE_JOINABLE;
    attr->stacksize = PTHREAD_DEFAULT_STACK_SIZE;
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
    int expected = 0;
    while (!__atomic_compare_exchange_n(&mutex->locked,
            &expected,
            1,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED)) {
        expected = 0;
        (void)srv_futex_wait((uint32_t *)&mutex->locked, 1, 0);
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
    (void)srv_futex_wake((uint32_t *)&mutex->locked, 1);
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
    (void)srv_futex_wake((uint32_t *)&cond->sequence, 1);
    return 0;
}

int pthread_cond_broadcast(pthread_cond_t *cond) {
    if (cond == 0) {
        return EINVAL;
    }
    __atomic_add_fetch(&cond->sequence, 1, __ATOMIC_RELEASE);
    (void)srv_futex_wake((uint32_t *)&cond->sequence, UINT64_MAX);
    return 0;
}

static int pthread_cond_wait_ticks(pthread_cond_t *cond, pthread_mutex_t *mutex, uint64_t timeout_ticks) {
    if (cond == 0 || mutex == 0) {
        return EINVAL;
    }
    unsigned int observed = __atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE);
    pthread_mutex_unlock(mutex);
    while (__atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE) == observed) {
        long result = srv_futex_wait((uint32_t *)&cond->sequence, observed, timeout_ticks);
        if (result == -ETIMEDOUT) {
            (void)pthread_mutex_lock(mutex);
            return ETIMEDOUT;
        }
    }
    return pthread_mutex_lock(mutex);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return pthread_cond_wait_ticks(cond, mutex, 0);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    if (cond == 0 || mutex == 0 || abstime == 0 ||
        abstime->tv_sec < 0 ||
        abstime->tv_nsec < 0 ||
        abstime->tv_nsec >= 1000000000L) {
        return EINVAL;
    }

    struct timespec now;
    if (clock_gettime(CLOCK_REALTIME, &now) != 0) {
        return errno;
    }
    if (abstime->tv_sec < now.tv_sec ||
        (abstime->tv_sec == now.tv_sec && abstime->tv_nsec <= now.tv_nsec)) {
        return ETIMEDOUT;
    }

    uint64_t sec = (uint64_t)(abstime->tv_sec - now.tv_sec);
    long nsec = abstime->tv_nsec - now.tv_nsec;
    if (nsec < 0) {
        sec--;
        nsec += 1000000000L;
    }
    uint64_t ticks = sec * PTHREAD_TICKS_PER_SECOND;
    ticks += ((uint64_t)nsec * PTHREAD_TICKS_PER_SECOND + 999999999ull) / 1000000000ull;
    if (ticks == 0) {
        ticks = 1;
    }
    return pthread_cond_wait_ticks(cond, mutex, ticks);
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
    for (int i = 0; i < PTHREAD_TLS_THREADS; i++) {
        if (pthread_tls_slots[i].used) {
            pthread_tls_slots[i].values[key] = 0;
        }
    }
    pthread_keys[key].destructor = 0;
    __atomic_store_n(&pthread_keys[key].used, 0, __ATOMIC_RELEASE);
    return 0;
}

void *pthread_getspecific(pthread_key_t key) {
    if (key >= PTHREAD_KEYS_MAX || !pthread_keys[key].used) {
        return 0;
    }
    struct pthread_tls_slot *slot = pthread_tls_slot_for(pthread_self(), 0);
    return slot != 0 ? slot->values[key] : 0;
}

int pthread_setspecific(pthread_key_t key, const void *value) {
    if (key >= PTHREAD_KEYS_MAX || !pthread_keys[key].used) {
        return EINVAL;
    }
    struct pthread_tls_slot *slot = pthread_tls_slot_for(pthread_self(), 1);
    if (slot == 0) {
        return EAGAIN;
    }
    slot->values[key] = (void *)value;
    return 0;
}

int sched_yield(void) {
    srv_syscall0(SYS_YIELD);
    return 0;
}
