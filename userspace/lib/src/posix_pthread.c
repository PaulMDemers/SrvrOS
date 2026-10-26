#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#define PTHREAD_DEFAULT_STACK_SIZE (2 * 1024 * 1024)
#define SRVROS_MAIN_STACK_TOP 0x1000000ull
#define SRVROS_DYNAMIC_STACK_TOP 0x0ffff000ull
#define PTHREAD_MAX_RECORDS 16
#define PTHREAD_TICKS_PER_SECOND 100
#define PTHREAD_NAME_CAPACITY 16

/* Supplied storage must never be rounded up or replaced by the default size.
 * The x86_64 entry stack is aligned down inside this checked region. */
static int pthread_stack_valid(void *stack, size_t size) {
    uintptr_t base = (uintptr_t)stack;
    return stack != NULL && !(base & 15) && size >= PTHREAD_STACK_MIN &&
           size <= UINTPTR_MAX - base;
}

int __posix_signal_dispatch_pending(void);

struct pthread_key_slot {
    int used;
    uint64_t generation;
    void (*destructor)(void *);
};

struct pthread_start_record {
    void *(*start_routine)(void *);
    void *arg;
    void *stack;
    size_t stack_size;
    int owns_stack;
    int detachstate;
    struct pthread_record *record;
};

struct pthread_record {
    int used;
    int creating;
    int claimed;
    pthread_t tid;
    struct pthread_start_record *start;
    char name[PTHREAD_NAME_CAPACITY];
};

struct pthread_key_value { void *value;uint64_t generation; };
static struct pthread_key_slot pthread_keys[PTHREAD_KEYS_MAX];
static struct pthread_record pthread_records[PTHREAD_MAX_RECORDS];
static _Thread_local struct pthread_key_value key_values[PTHREAD_KEYS_MAX];
/* Owned by this live worker until kernel exit/join permits record cleanup. */
static _Thread_local struct pthread_start_record *self_start;
/* Protects table publication and cleanup claims, never held across OS calls. */
static char main_thread_name[PTHREAD_NAME_CAPACITY] = "srvros-thread";
static unsigned record_lock;
static void lock_records(void) {
    while (__atomic_exchange_n(&record_lock, 1, __ATOMIC_ACQUIRE)) srv_yield();
}
static void unlock_records(void) { __atomic_store_n(&record_lock, 0, __ATOMIC_RELEASE); }
static unsigned key_lock;
static void lock_keys(void) {
    while(__atomic_exchange_n(&key_lock,1,__ATOMIC_ACQUIRE)) srv_yield();
}
static void unlock_keys(void) { __atomic_store_n(&key_lock,0,__ATOMIC_RELEASE); }

static long pthread_tid_value(pthread_t thread) {
    return (long)(uintptr_t)thread;
}

static long pthread_futex_wait_absorbing_signals(uint32_t *address, uint32_t expected, uint64_t timeout_ticks) {
    for (;;) {
        long result = srv_futex_wait(address, expected, timeout_ticks);
        if (result != SRV_ERR_INTR) {
            return result;
        }
        (void)__posix_signal_dispatch_pending();
    }
}

static int pthread_ticks_until(int clock_id, const struct timespec *abstime, uint64_t *ticks_out) {
    struct timespec now;
    if (clock_gettime(clock_id, &now) != 0) {
        return errno != 0 ? errno : EINVAL;
    }
    if (abstime->tv_sec < now.tv_sec ||
        (abstime->tv_sec == now.tv_sec && abstime->tv_nsec <= now.tv_nsec)) {
        return ETIMEDOUT;
    }

    uint64_t sec = (uint64_t)abstime->tv_sec - (uint64_t)now.tv_sec;
    /* Recheck absolute time at least once a second. This bounds the kernel's
     * relative tick deadline and avoids overflow for distant absolute dates. */
    if (sec > 1) {
        *ticks_out = PTHREAD_TICKS_PER_SECOND;
        return 0;
    }
    long nsec = abstime->tv_nsec - now.tv_nsec;
    if (nsec < 0) {
        sec--;
        nsec += 1000000000L;
    }
    uint64_t ticks = sec * PTHREAD_TICKS_PER_SECOND;
    ticks += ((uint64_t)nsec * PTHREAD_TICKS_PER_SECOND + 999999999ull) / 1000000000ull;
    if (ticks > PTHREAD_TICKS_PER_SECOND) ticks = PTHREAD_TICKS_PER_SECOND;
    *ticks_out = ticks == 0 ? 1 : ticks;
    return 0;
}

/* Caller holds record_lock. Only a published, nonzero tid is discoverable. */
static struct pthread_record *pthread_record_find(pthread_t tid) {
    if (!tid) return NULL;
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        if (pthread_records[i].used && pthread_records[i].tid == tid)
            return &pthread_records[i];
    }
    return NULL;
}

static struct pthread_record *pthread_record_alloc(struct pthread_start_record *start) {
    lock_records();
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        if (!pthread_records[i].used) {
            struct pthread_record *record = &pthread_records[i];
            *record = (struct pthread_record){ .used = 1, .creating = 1, .start = start, .name = "srvros-thread" };
            unlock_records();
            return record;
        }
    }
    unlock_records();
    return NULL;
}

/* Caller owns the creation reservation or an exclusive join/reap claim. */
static void pthread_record_cleanup(struct pthread_record *record) {
    lock_records();
    struct pthread_start_record *start = record->start;
    *record = (struct pthread_record){0};
    unlock_records();
    if (start->owns_stack) munmap(start->stack, start->stack_size);
    free(start);
}

static void pthread_record_unclaim(struct pthread_record *record) {
    lock_records();
    record->claimed = 0;
    unlock_records();
}

static void pthread_reap_detached(void) {
    pthread_t self = pthread_self();
    for (int i = 0; i < PTHREAD_MAX_RECORDS; i++) {
        lock_records();
        struct pthread_record *record = &pthread_records[i];
        if (!record->used || record->creating || record->claimed || record->tid == self ||
            __atomic_load_n(&record->start->detachstate, __ATOMIC_ACQUIRE) != PTHREAD_CREATE_DETACHED) {
            unlock_records();
            continue;
        }
        record->claimed = 1;
        pthread_t tid = record->tid;
        unlock_records();
        uint64_t ignored = 0;
        if (srv_thread_status(pthread_tid_value(tid)) == 0 &&
            srv_thread_join(pthread_tid_value(tid), &ignored) == 0) {
            pthread_record_cleanup(record);
        } else {
            /* Failed/interrupted joins retain ownership for a later retry. */
            pthread_record_unclaim(record);
        }
    }
}

static void pthread_run_destructors(void) {
    /* A destructor may reinstall its value. Clear before calling it and make
     * the mandated bounded number of passes, without holding the key lock. */
    for(unsigned pass=0;pass<PTHREAD_DESTRUCTOR_ITERATIONS;pass++) {
        int called=0;
        for(pthread_key_t key=0;key<PTHREAD_KEYS_MAX;key++) {
            lock_keys();
            struct pthread_key_slot *slot=&pthread_keys[key];
            struct pthread_key_value *local=&key_values[key];
            void *value=local->value;
            void (*destructor)(void *)=slot->used && slot->generation==local->generation?slot->destructor:NULL;
            local->value=NULL;
            unlock_keys();
            if(value && destructor) { called=1;destructor(value); }
        }
        if(!called) break;
    }
}

static void pthread_start_trampoline(uint64_t raw_start) {
    struct pthread_start_record *start = (struct pthread_start_record *)(uintptr_t)raw_start;
    self_start = start;
    pthread_t self = pthread_self();
    lock_records();
    start->record->tid = self;
    unlock_records();
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
        detachstate = attr->detachstate;
        if (attr->stackaddr != 0) {
            stack = attr->stackaddr;
            stack_size = attr->stacksize;
            if (!pthread_stack_valid(stack, stack_size)) return EINVAL;
            owns_stack = 0;
        } else {
            stack_size = attr->stacksize > stack_size ? attr->stacksize : stack_size;
        }
    }

    if (stack == 0) {
        if (stack_size > SIZE_MAX - 4095) return EINVAL;
        stack_size = (stack_size + 4095) & ~(size_t)4095;
        stack = mmap(0,
            stack_size,
            PROT_READ | PROT_WRITE,
            MAP_PRIVATE | MAP_ANONYMOUS,
            -1,
            0);
        if (stack == MAP_FAILED) {
            return ENOMEM;
        }
        if (!pthread_stack_valid(stack, stack_size)) {
            munmap(stack, stack_size);
            return EINVAL;
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
        .detachstate = detachstate,
    };

    struct pthread_record *record = pthread_record_alloc(start);
    if (record == 0) {
        free(start);
        if (owns_stack) {
            munmap(stack, stack_size);
        }
        return EAGAIN;
    }

    start->record = record;

    uint64_t stack_top = (((uint64_t)(uintptr_t)stack + stack_size) & ~(uint64_t)0xf) - 8;
    *(uint64_t *)(uintptr_t)stack_top = 0;
    uint64_t flags = 0;
    long tid = srv_thread_create((uint64_t)(uintptr_t)pthread_start_trampoline,
        (uint64_t)(uintptr_t)start,
        stack_top,
        flags);
    if (tid < 0) {
        pthread_record_cleanup(record);
        return EAGAIN;
    }

    lock_records();
    *thread = (pthread_t)(uintptr_t)tid;
    record->tid = (pthread_t)(uintptr_t)tid;
    record->creating = 0;
    /* Do not overwrite detachstate: the child may already have detached/exited.
     * After unlocking a reaper may free the record, so do not touch it again. */
    unlock_records();
    return 0;
}

int pthread_join(pthread_t thread, void **value_ptr) {
    if (thread == pthread_self()) return EDEADLK;
    pthread_reap_detached();
    struct pthread_record *record;
    for (;;) {
        lock_records();
        record = pthread_record_find(thread);
        if (record == NULL || record->claimed ||
            __atomic_load_n(&record->start->detachstate, __ATOMIC_ACQUIRE) == PTHREAD_CREATE_DETACHED) {
            unlock_records();
            return EINVAL;
        }
        if (!record->creating) break;
        unlock_records();
        srv_yield();
    }
    record->claimed = 1;
    unlock_records();

    uint64_t value = 0;
    for (;;) {
        long result = srv_thread_join(pthread_tid_value(thread), &value);
        if (result == 0) {
            break;
        }
        if (result != SRV_ERR_INTR) {
            pthread_record_unclaim(record);
            return EINVAL;
        }
        (void)__posix_signal_dispatch_pending();
    }
    if (value_ptr != 0) {
        *value_ptr = (void *)(uintptr_t)value;
    }

    pthread_record_cleanup(record);
    /* Joining an already-exited worker is still a cooperative signal checkpoint.
     * Its completion can win the kernel wait race over a pending signal. Retire
     * the record before invoking a handler which may create or join threads. */
    (void)__posix_signal_dispatch_pending();
    return 0;
}

int pthread_detach(pthread_t thread) {
    pthread_reap_detached();
    lock_records();
    struct pthread_record *record = pthread_record_find(thread);
    if (record == NULL || record->claimed ||
        __atomic_load_n(&record->start->detachstate, __ATOMIC_ACQUIRE) == PTHREAD_CREATE_DETACHED) {
        unlock_records();
        return EINVAL;
    }
    __atomic_store_n(&record->start->detachstate, PTHREAD_CREATE_DETACHED, __ATOMIC_RELEASE);
    unlock_records();
    pthread_reap_detached();
    return 0;
}

void pthread_exit(void *value_ptr) {
    pthread_run_destructors();
    srv_thread_exit((uint64_t)(uintptr_t)value_ptr);
    _exit(0);
}

pthread_t pthread_self(void) {
    long tid = srv_thread_self();
    return tid > 0 ? (pthread_t)(uintptr_t)tid : (pthread_t)(uintptr_t)1;
}

int pthread_equal(pthread_t left, pthread_t right) {
    return left == right;
}

int pthread_kill(pthread_t thread, int sig) {
    int saved_errno=errno;
    if(sig<0 || sig>=64) return EINVAL;
    long result=srv_thread_signal(pthread_tid_value(thread),(uint64_t)sig);
    if(result==0 && sig && thread==pthread_self())
        (void)__posix_signal_dispatch_pending();
    errno=saved_errno;
    return result<0?(int)-result:0;
}

/* Caller holds record_lock. Tid 1 is the kernel's initial process thread;
 * raw syscall-created workers have no libc record and are not supported here. */
static char *pthread_name_find(pthread_t thread) {
    if (thread == (pthread_t)(uintptr_t)1) return main_thread_name;
    struct pthread_record *record = pthread_record_find(thread);
    return record != NULL ? record->name : NULL;
}

int pthread_setname_np(pthread_t thread, const char *name) {
    if (name == NULL) return EINVAL;
    char copy[PTHREAD_NAME_CAPACITY];
    size_t size = 0;
    do {
        if (size == sizeof(copy)) return ERANGE;
        copy[size] = name[size];
    } while (copy[size++] != '\0');

    lock_records();
    char *stored = pthread_name_find(thread);
    if (stored == NULL) {
        unlock_records();
        return ESRCH;
    }
    for (size_t i = 0; i < size; i++) stored[i] = copy[i];
    unlock_records();
    return 0;
}

int pthread_getname_np(pthread_t thread, char *name, size_t length) {
    if (name == NULL) return EINVAL;
    lock_records();
    const char *stored = pthread_name_find(thread);
    if (stored == NULL) {
        unlock_records();
        return ESRCH;
    }
    size_t needed = 1;
    while (stored[needed - 1] != '\0') needed++;
    if (length < needed) {
        unlock_records();
        return ERANGE;
    }
    for (size_t i = 0; i < needed; i++) name[i] = stored[i];
    unlock_records();
    return 0;
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
    if (attr == 0 || stacksize < PTHREAD_STACK_MIN ||
        (attr->stackaddr ? !pthread_stack_valid(attr->stackaddr, stacksize) :
                          stacksize > SIZE_MAX - 4095)) {
        return EINVAL;
    }
    attr->stacksize = stacksize;
    return 0;
}

int pthread_attr_getstackaddr(const pthread_attr_t *attr, void **stackaddr) {
    if (attr == 0 || stackaddr == 0) {
        return EINVAL;
    }
    *stackaddr = attr->stackaddr;
    return 0;
}

int pthread_attr_setstackaddr(pthread_attr_t *attr, void *stackaddr) {
    if (attr == 0 || !pthread_stack_valid(stackaddr, attr->stacksize)) {
        return EINVAL;
    }
    attr->stackaddr = stackaddr;
    return 0;
}

int pthread_attr_getstack(const pthread_attr_t *attr, void **stackaddr, size_t *stacksize) {
    if (attr == 0 || stackaddr == 0 || stacksize == 0) {
        return EINVAL;
    }
    *stackaddr = attr->stackaddr;
    *stacksize = attr->stacksize;
    return 0;
}

int pthread_attr_setstack(pthread_attr_t *attr, void *stackaddr, size_t stacksize) {
    if (attr == 0 || !pthread_stack_valid(stackaddr, stacksize)) {
        return EINVAL;
    }
    attr->stackaddr = stackaddr;
    attr->stacksize = stacksize;
    return 0;
}

int pthread_getattr_np(pthread_t thread, pthread_attr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    int error = pthread_attr_init(attr);
    if (error != 0) {
        return error;
    }
    if (thread == pthread_self()) {
        /* The child can enter before its parent publishes the pthread record.
         * The trampoline installs this pointer before calling user code. */
        if (self_start != NULL) {
            attr->stackaddr = self_start->stack;
            attr->stacksize = self_start->stack_size;
            attr->detachstate = __atomic_load_n(&self_start->detachstate, __ATOMIC_ACQUIRE);
            return 0;
        }
        /* Kernel-created initial stacks use these two fixed layouts (see
         * process.c: choose_process_stack_top/map_user_stack). Raw syscall
         * threads on other storage have no libc metadata: refuse to guess. */
        uintptr_t frame = (uintptr_t)__builtin_frame_address(0);
        const uintptr_t tops[] = { SRVROS_MAIN_STACK_TOP, SRVROS_DYNAMIC_STACK_TOP };
        for (unsigned i = 0; i < sizeof(tops) / sizeof(tops[0]); i++) {
            if (frame >= tops[i] - PTHREAD_DEFAULT_STACK_SIZE && frame < tops[i]) {
                attr->stacksize = PTHREAD_DEFAULT_STACK_SIZE;
                attr->stackaddr = (void *)(tops[i] - PTHREAD_DEFAULT_STACK_SIZE);
                return 0;
            }
        }
        return ESRCH;
    }
    lock_records();
    struct pthread_record *record = pthread_record_find(thread);
    if (record == NULL) {
        unlock_records();
        return ESRCH;
    }
    attr->detachstate = __atomic_load_n(&record->start->detachstate, __ATOMIC_ACQUIRE);
    attr->stacksize = record->start->stack_size;
    attr->stackaddr = record->start->stack;
    unlock_records();
    return 0;
}

int pthread_mutexattr_init(pthread_mutexattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->type = PTHREAD_MUTEX_DEFAULT;
    return 0;
}

int pthread_mutexattr_destroy(pthread_mutexattr_t *attr) {
    return attr == 0 ? EINVAL : 0;
}

int pthread_mutexattr_gettype(const pthread_mutexattr_t *attr, int *type) {
    if (attr == 0 || type == 0) {
        return EINVAL;
    }
    *type = attr->type;
    return 0;
}

int pthread_mutexattr_settype(pthread_mutexattr_t *attr, int type) {
    if (attr == 0 ||
        (type != PTHREAD_MUTEX_NORMAL &&
            type != PTHREAD_MUTEX_ERRORCHECK &&
            type != PTHREAD_MUTEX_RECURSIVE &&
            type != PTHREAD_MUTEX_DEFAULT)) {
        return EINVAL;
    }
    attr->type = type;
    return 0;
}

int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *attr) {
    if (mutex == 0) {
        return EINVAL;
    }
    mutex->locked = 0;
    mutex->type = attr != 0 ? attr->type : PTHREAD_MUTEX_DEFAULT;
    __atomic_store_n(&mutex->owner, 0, __ATOMIC_RELAXED);
    mutex->depth = 0;
    return 0;
}

int pthread_mutex_destroy(pthread_mutex_t *mutex) {
    return mutex == 0 ? EINVAL : 0;
}

int pthread_mutex_lock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&mutex->locked, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == self) {
        if (mutex->type == PTHREAD_MUTEX_RECURSIVE) {
            mutex->depth++;
            return 0;
        }
        if (mutex->type == PTHREAD_MUTEX_ERRORCHECK) {
            return EDEADLK;
        }
    }
    int expected = 0;
    while (!__atomic_compare_exchange_n(&mutex->locked,
            &expected,
            1,
            0,
            __ATOMIC_ACQUIRE,
            __ATOMIC_RELAXED)) {
        expected = 0;
        (void)pthread_futex_wait_absorbing_signals((uint32_t *)&mutex->locked, 1, 0);
    }
    __atomic_store_n(&mutex->owner, self, __ATOMIC_RELAXED);
    mutex->depth = 1;
    return 0;
}

int pthread_mutex_trylock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&mutex->locked, __ATOMIC_ACQUIRE) != 0 &&
        __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == self &&
        mutex->type == PTHREAD_MUTEX_RECURSIVE) {
        mutex->depth++;
        return 0;
    }
    int expected = 0;
    int locked = __atomic_compare_exchange_n(&mutex->locked,
        &expected,
        1,
        0,
        __ATOMIC_ACQUIRE,
        __ATOMIC_RELAXED);
    if (!locked) {
        return EBUSY;
    }
    __atomic_store_n(&mutex->owner, self, __ATOMIC_RELAXED);
    mutex->depth = 1;
    return 0;
}

int pthread_mutex_unlock(pthread_mutex_t *mutex) {
    if (mutex == 0) {
        return EINVAL;
    }
    pthread_t self = pthread_self();
    if (__atomic_load_n(&mutex->locked, __ATOMIC_ACQUIRE) == 0) {
        return EPERM;
    }
    if (mutex->type != PTHREAD_MUTEX_NORMAL &&
        mutex->type != PTHREAD_MUTEX_DEFAULT &&
        __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) != self) {
        return EPERM;
    }
    if (mutex->type == PTHREAD_MUTEX_RECURSIVE && __atomic_load_n(&mutex->owner, __ATOMIC_RELAXED) == self && mutex->depth > 1) {
        mutex->depth--;
        return 0;
    }
    __atomic_store_n(&mutex->owner, 0, __ATOMIC_RELAXED);
    mutex->depth = 0;
    __atomic_store_n(&mutex->locked, 0, __ATOMIC_RELEASE);
    (void)srv_futex_wake((uint32_t *)&mutex->locked, 1);
    return 0;
}

int pthread_rwlock_init(pthread_rwlock_t *rwlock, const pthread_rwlockattr_t *attr) {
    (void)attr;
    if (rwlock == 0) {
        return EINVAL;
    }
    rwlock->state = 0;
    return 0;
}

int pthread_rwlock_destroy(pthread_rwlock_t *rwlock) {
    if(!rwlock) return EINVAL;
    return __atomic_load_n(&rwlock->state,__ATOMIC_ACQUIRE)?EBUSY:0;
}
int pthread_rwlock_tryrdlock(pthread_rwlock_t *rwlock) {
    if(!rwlock) return EINVAL;
    int state=__atomic_load_n(&rwlock->state,__ATOMIC_ACQUIRE);
    for(;;) {
        if(state<0) return EBUSY;
        if(state==2147483647) return EAGAIN;
        if(__atomic_compare_exchange_n(&rwlock->state,&state,state+1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)) return 0;
    }
}
int pthread_rwlock_trywrlock(pthread_rwlock_t *rwlock) {
    if(!rwlock) return EINVAL;
    int expected=0;
    return __atomic_compare_exchange_n(&rwlock->state,&expected,-1,0,__ATOMIC_ACQUIRE,__ATOMIC_RELAXED)?0:EBUSY;
}
int pthread_rwlock_rdlock(pthread_rwlock_t *rwlock) {
    for(;;) {
        int result=pthread_rwlock_tryrdlock(rwlock);
        if(result!=EBUSY) return result;
        /* Waiting on the observed writer value closes the unlock/wait race. */
        (void)pthread_futex_wait_absorbing_signals((uint32_t *)&rwlock->state,(uint32_t)-1,0);
    }
}
int pthread_rwlock_wrlock(pthread_rwlock_t *rwlock) {
    for(;;) {
        int result=pthread_rwlock_trywrlock(rwlock);
        if(result!=EBUSY) return result;
        int state=__atomic_load_n(&rwlock->state,__ATOMIC_ACQUIRE);
        if(state) (void)pthread_futex_wait_absorbing_signals((uint32_t *)&rwlock->state,(uint32_t)state,0);
    }
}
int pthread_rwlock_unlock(pthread_rwlock_t *rwlock) {
    if(!rwlock) return EINVAL;
    int state=__atomic_load_n(&rwlock->state,__ATOMIC_RELAXED);
    for(;;) {
        if(!state) return EPERM;
        int next=state<0?0:state-1;
        if(__atomic_compare_exchange_n(&rwlock->state,&state,next,0,__ATOMIC_RELEASE,__ATOMIC_RELAXED)) {
            if(!next) (void)srv_futex_wake((uint32_t *)&rwlock->state,UINT64_MAX);
            return 0;
        }
    }
}

int pthread_rwlockattr_init(pthread_rwlockattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->reserved = 0;
    return 0;
}

int pthread_rwlockattr_destroy(pthread_rwlockattr_t *attr) {
    return attr == 0 ? EINVAL : 0;
}

int pthread_condattr_init(pthread_condattr_t *attr) {
    if (attr == 0) {
        return EINVAL;
    }
    attr->clock = CLOCK_REALTIME;
    return 0;
}

int pthread_condattr_destroy(pthread_condattr_t *attr) {
    return attr == 0 ? EINVAL : 0;
}

int pthread_condattr_getclock(const pthread_condattr_t *attr, int *clock_id) {
    if (attr == 0 || clock_id == 0) {
        return EINVAL;
    }
    *clock_id = attr->clock;
    return 0;
}

int pthread_condattr_setclock(pthread_condattr_t *attr, int clock_id) {
    if (attr == 0 || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        return EINVAL;
    }
    attr->clock = clock_id;
    return 0;
}

int pthread_cond_init(pthread_cond_t *cond, const pthread_condattr_t *attr) {
    int clock_id = attr ? attr->clock : CLOCK_REALTIME;
    if (cond == 0 || (clock_id != CLOCK_REALTIME && clock_id != CLOCK_MONOTONIC)) {
        return EINVAL;
    }
    cond->sequence = 0;
    cond->clock = clock_id;
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

static int pthread_cond_wait_until(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    if (cond == 0 || mutex == 0) {
        return EINVAL;
    }
    unsigned int observed = __atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE);
    int unlocked = pthread_mutex_unlock(mutex);
    if (unlocked != 0) return unlocked;
    while (__atomic_load_n(&cond->sequence, __ATOMIC_ACQUIRE) == observed) {
        uint64_t timeout_ticks = 0;
        if (abstime != 0) {
            int error = pthread_ticks_until(cond->clock, abstime, &timeout_ticks);
            if (error != 0) {
                (void)pthread_mutex_lock(mutex);
                return error;
            }
        }
        long result = srv_futex_wait((uint32_t *)&cond->sequence, observed, timeout_ticks);
        if (result == SRV_ERR_INTR) {
            (void)__posix_signal_dispatch_pending();
            continue;
        }
        if (result == -ETIMEDOUT) {
            /* Tick-based wakes can precede the absolute deadline. Retain the
             * caller's clock/deadline across slices, interrupts and early wakes. */
            continue;
        }
    }
    return pthread_mutex_lock(mutex);
}

int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex) {
    return pthread_cond_wait_until(cond, mutex, 0);
}

int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex, const struct timespec *abstime) {
    if (cond == 0 || mutex == 0 || abstime == 0 ||
        abstime->tv_sec < 0 ||
        abstime->tv_nsec < 0 ||
        abstime->tv_nsec >= 1000000000L) {
        return EINVAL;
    }

    return pthread_cond_wait_until(cond, mutex, abstime);
}

int pthread_once(pthread_once_t *once_control, void (*init_routine)(void)) {
    if (once_control == 0 || init_routine == 0) {
        return EINVAL;
    }

    for (;;) {
        int state = __atomic_load_n(once_control, __ATOMIC_ACQUIRE);
        if (state == 2) {
            return 0;
        }
        if (state == 0) {
            int expected = 0;
            if (__atomic_compare_exchange_n(once_control,
                    &expected,
                    1,
                    0,
                    __ATOMIC_ACQ_REL,
                    __ATOMIC_ACQUIRE)) {
                init_routine();
                __atomic_store_n(once_control, 2, __ATOMIC_RELEASE);
                (void)srv_futex_wake((uint32_t *)once_control, UINT64_MAX);
                return 0;
            }
            continue;
        }
        (void)pthread_futex_wait_absorbing_signals((uint32_t *)once_control, 1, 0);
    }
}

int pthread_key_create(pthread_key_t *key, void (*destructor)(void *)) {
    if(!key) return EINVAL;
    lock_keys();
    for(pthread_key_t i=0;i<PTHREAD_KEYS_MAX;i++) if(!pthread_keys[i].used) {
        struct pthread_key_slot *slot=&pthread_keys[i];
        /* Never alias an old generation, even after theoretical wrap. */
        if(slot->generation==UINT64_MAX) continue;
        slot->generation++;slot->destructor=destructor;slot->used=1;*key=i;
        unlock_keys();return 0;
    }
    unlock_keys();return EAGAIN;
}
int pthread_key_delete(pthread_key_t key) {
    lock_keys();
    if(key>=PTHREAD_KEYS_MAX || !pthread_keys[key].used) { unlock_keys();return EINVAL; }
    pthread_keys[key].used=0;pthread_keys[key].destructor=NULL;
    unlock_keys();return 0;
}
void *pthread_getspecific(pthread_key_t key) {
    lock_keys();void *value=NULL;
    if(key<PTHREAD_KEYS_MAX && pthread_keys[key].used &&
        key_values[key].generation==pthread_keys[key].generation) value=key_values[key].value;
    unlock_keys();return value;
}
int pthread_setspecific(pthread_key_t key, const void *value) {
    lock_keys();
    if(key>=PTHREAD_KEYS_MAX || !pthread_keys[key].used) { unlock_keys();return EINVAL; }
    key_values[key]=(struct pthread_key_value){(void *)value,pthread_keys[key].generation};
    unlock_keys();return 0;
}

int sched_yield(void) {
    srv_syscall0(SYS_YIELD);
    (void)__posix_signal_dispatch_pending();
    return 0;
}

int sched_getcpu(void) {
    return 0;
}

int sched_get_priority_min(int policy) {
    if(policy!=SCHED_OTHER) { errno=EINVAL;return -1; }
    return 0;
}

int sched_get_priority_max(int policy) { return sched_get_priority_min(policy); }

int __sched_cpu_count(const cpu_set_t *mask) {
    if (mask == 0) {
        return 0;
    }
    int count = 0;
    for (int i = 0; i < CPU_SETSIZE; i++) {
        if (CPU_ISSET(i, mask)) {
            count++;
        }
    }
    return count;
}

int sched_getaffinity(pid_t pid, size_t cpusetsize, cpu_set_t *mask) {
    if (pid < 0 || mask == 0 || cpusetsize < sizeof(cpu_set_t)) {
        errno = EINVAL;
        return -1;
    }
    CPU_ZERO(mask);
    CPU_SET(0, mask);
    return 0;
}

int sched_setaffinity(pid_t pid, size_t cpusetsize, const cpu_set_t *mask) {
    if (pid < 0 || mask == 0 || cpusetsize < sizeof(cpu_set_t) || !CPU_ISSET(0, mask)) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}
