#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include <srvros/sys.h>
#include <time.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: sync-contracts line=%d errno=%d\n", __LINE__, errno); return -1; } } while (0)
struct stack_probe { uintptr_t base; size_t size; };
static void *stack_worker(void *arg) {
    struct stack_probe *probe = arg;
    volatile unsigned char local[128]; local[0] = 7;
    uintptr_t address = (uintptr_t)&local[0];
    return address >= probe->base && address + sizeof(local) <= probe->base + probe->size && local[0] == 7 ? NULL : (void *)1;
}
static int live_stack(size_t size) {
    size_t rounded = (size + 4095) & ~(size_t)4095;
    unsigned char *mapping = mmap(NULL, rounded + 8192, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(mapping != MAP_FAILED);
    CHECK(mprotect(mapping, 4096, PROT_NONE) == 0);
    CHECK(mprotect(mapping + 4096 + rounded, 4096, PROT_NONE) == 0);
    unsigned char *base = mapping + 4096;
    memset(base + size, 0xa5, rounded - size);
    pthread_attr_t attr; pthread_t worker;
    CHECK(pthread_attr_init(&attr) == 0 && pthread_attr_setstack(&attr, base, size) == 0);
    struct stack_probe probe = {.base = (uintptr_t)base, .size = size};
    CHECK(pthread_create(&worker, &attr, stack_worker, &probe) == 0);
    void *result = (void *)1;
    CHECK(pthread_join(worker, &result) == 0 && !result);
    for (size_t i = size; i < rounded; i++) CHECK(base[i] == 0xa5);
    CHECK(pthread_attr_destroy(&attr) == 0 && munmap(mapping, rounded + 8192) == 0);
    return 0;
}
static pthread_mutex_t lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t condition;
static unsigned ready;
static void *signal_worker(void *unused) {
    (void)unused;
    if (pthread_mutex_lock(&lock)) return (void *)1;
    ready = 1;
    int result = pthread_cond_signal(&condition);
    result |= pthread_mutex_unlock(&lock);
    return result ? (void *)1 : NULL;
}
static struct timespec after_ms(struct timespec now, long ms) {
    now.tv_nsec += ms * 1000000;
    now.tv_sec += now.tv_nsec / 1000000000;
    now.tv_nsec %= 1000000000;
    return now;
}
static int live_condition(int clock_id) {
    pthread_condattr_t attr;
    CHECK(pthread_condattr_init(&attr) == 0 && pthread_condattr_setclock(&attr, clock_id) == 0);
    CHECK(pthread_cond_init(&condition, &attr) == 0);
    struct timespec now, end;
    CHECK(clock_gettime(clock_id, &now) == 0);
    struct timespec deadline = after_ms(now, 35);
    CHECK(pthread_mutex_lock(&lock) == 0);
    CHECK(pthread_cond_timedwait(&condition, &lock, &deadline) == ETIMEDOUT);
    CHECK(clock_gettime(clock_id, &end) == 0);
    CHECK(end.tv_sec > deadline.tv_sec || (end.tv_sec == deadline.tv_sec && end.tv_nsec >= deadline.tv_nsec));
    CHECK(pthread_mutex_trylock(&lock) == EBUSY && pthread_mutex_unlock(&lock) == 0);
    ready = 0; pthread_t worker;
    CHECK(pthread_mutex_lock(&lock) == 0);
    CHECK(pthread_create(&worker, NULL, signal_worker, NULL) == 0);
    CHECK(clock_gettime(clock_id, &now) == 0); deadline = after_ms(now, 2000);
    while (!ready) CHECK(pthread_cond_timedwait(&condition, &lock, &deadline) == 0);
    CHECK(pthread_mutex_unlock(&lock) == 0);
    void *result = (void *)1; CHECK(pthread_join(worker, &result) == 0 && !result);
    CHECK(pthread_cond_destroy(&condition) == 0 && pthread_condattr_destroy(&attr) == 0);
    return 0;
}
static sem_t near_limit;
static unsigned post_gate, successes, overflows, unexpected;
static void *post_worker(void *unused) {
    (void)unused;
    while (!__atomic_load_n(&post_gate, __ATOMIC_ACQUIRE)) sched_yield();
    if (sem_post(&near_limit) == 0) __atomic_add_fetch(&successes, 1, __ATOMIC_RELAXED);
    else if (errno == EOVERFLOW) __atomic_add_fetch(&overflows, 1, __ATOMIC_RELAXED);
    else __atomic_add_fetch(&unexpected, 1, __ATOMIC_RELAXED);
    return NULL;
}
static int live_semaphore(void) {
    int value;
    CHECK(sem_init(&near_limit, 0, (unsigned)SEM_VALUE_MAX + 1) == -1 && errno == EINVAL);
    CHECK(sem_init(&near_limit, 0, SEM_VALUE_MAX) == 0);
    CHECK(sem_post(&near_limit) == -1 && errno == EOVERFLOW);
    CHECK(sem_getvalue(&near_limit, &value) == 0 && value == SEM_VALUE_MAX);
    CHECK(sem_trywait(&near_limit) == 0 && sem_post(&near_limit) == 0);
    CHECK(sem_destroy(&near_limit) == 0);
    for (unsigned round = 0; round < 12; round++) {
        CHECK(sem_init(&near_limit, 0, SEM_VALUE_MAX - 1) == 0);
        post_gate = successes = overflows = unexpected = 0;
        pthread_t workers[3];
        for (unsigned i = 0; i < 3; i++) CHECK(pthread_create(&workers[i], NULL, post_worker, NULL) == 0);
        __atomic_store_n(&post_gate, 1, __ATOMIC_RELEASE);
        for (unsigned i = 0; i < 3; i++) CHECK(pthread_join(workers[i], NULL) == 0);
        CHECK(successes == 1 && overflows == 2 && unexpected == 0);
        CHECK(sem_getvalue(&near_limit, &value) == 0 && value == SEM_VALUE_MAX);
        CHECK(sem_destroy(&near_limit) == 0);
    }
    return 0;
}
int posix_sync_contract_test(void) {
    CHECK(live_stack(PTHREAD_STACK_MIN) == 0);
    CHECK(live_stack(PTHREAD_STACK_MIN + 1) == 0);
    CHECK(live_stack(2 * 1024 * 1024 + 1) == 0);
    CHECK(live_condition(CLOCK_MONOTONIC) == 0);
    CHECK(live_condition(CLOCK_REALTIME) == 0);
    CHECK(live_semaphore() == 0);
    puts("threadstress: sync-contracts ok");
    return 0;
}

/* Inspect immediately on entry, then again after a parent-side detach. */
struct inspection_probe {
    unsigned ready, go, done;
    pthread_attr_t first, second;
    int error;
    uintptr_t local;
};
static void *inspection_worker(void *arg) {
    struct inspection_probe *probe = arg;
    volatile char local = 1;
    probe->local = (uintptr_t)&local;
    probe->error = pthread_getattr_np(pthread_self(), &probe->first);
    __atomic_store_n(&probe->ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&probe->go, __ATOMIC_ACQUIRE)) sched_yield();
    probe->error |= pthread_getattr_np(pthread_self(), &probe->second);
    __atomic_store_n(&probe->done, 1, __ATOMIC_RELEASE);
    return NULL;
}
static void *inspection_noop(void *arg) { return arg; }
static int inspect_stack(size_t size, int supplied, int detached, int detach_later) {
    void *stack = NULL;
    if (supplied) {
        stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
        CHECK(stack != MAP_FAILED);
    }
    pthread_attr_t attr, parent; pthread_t worker;
    struct inspection_probe probe = {0};
    CHECK(pthread_attr_init(&attr) == 0);
    CHECK(pthread_attr_setstacksize(&attr, size) == 0);
    if (supplied) CHECK(pthread_attr_setstack(&attr, stack, size) == 0);
    CHECK(pthread_attr_setdetachstate(&attr, detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE) == 0);
    CHECK(pthread_create(&worker, &attr, inspection_worker, &probe) == 0);
    while (!__atomic_load_n(&probe.ready, __ATOMIC_ACQUIRE)) sched_yield();
    CHECK(!probe.error && pthread_getattr_np(worker, &parent) == 0);
    CHECK(parent.stackaddr == probe.first.stackaddr && parent.stacksize == probe.first.stacksize);
    CHECK(parent.detachstate == attr.detachstate && probe.first.detachstate == attr.detachstate);
    size_t actual_size = supplied ? size : ((size < 2097152 ? 2097152 : size) + 4095) & ~(size_t)4095;
    CHECK(parent.stacksize == actual_size);
    if (supplied) CHECK(parent.stackaddr == stack);
    CHECK(probe.local >= (uintptr_t)parent.stackaddr && probe.local < (uintptr_t)parent.stackaddr + parent.stacksize);
    if (detach_later) CHECK(pthread_detach(worker) == 0);
    __atomic_store_n(&probe.go, 1, __ATOMIC_RELEASE);
    if (detached || detach_later) {
        while (!__atomic_load_n(&probe.done, __ATOMIC_ACQUIRE)) sched_yield();
        while (srv_thread_status((uint64_t)(uintptr_t)worker) > 0) sched_yield();
        /* Reap the exited detached worker before its caller-owned storage goes away. */
        pthread_t cleanup;
        CHECK(pthread_create(&cleanup, NULL, inspection_noop, NULL) == 0);
        CHECK(pthread_join(cleanup, NULL) == 0);
    } else CHECK(pthread_join(worker, NULL) == 0);
    CHECK(!probe.error && probe.second.stackaddr == parent.stackaddr && probe.second.stacksize == parent.stacksize);
    CHECK(probe.second.detachstate == ((detached || detach_later) ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE));
    CHECK(pthread_getattr_np(worker, &attr) == ESRCH);
    if (supplied) CHECK(munmap(stack, size) == 0);
    return 0;
}
int posix_stack_inspection_test(void) {
    pthread_attr_t initial;
    volatile char local = 1;
    CHECK(pthread_getattr_np(pthread_self(), &initial) == 0);
    CHECK(initial.stacksize == 2097152 && initial.detachstate == PTHREAD_CREATE_JOINABLE);
    CHECK((uintptr_t)&local >= (uintptr_t)initial.stackaddr &&
          (uintptr_t)&local < (uintptr_t)initial.stackaddr + initial.stacksize);
    CHECK(pthread_getattr_np(pthread_self(), NULL) == EINVAL);
    CHECK(pthread_getattr_np((pthread_t)(uintptr_t)-1, &initial) == ESRCH);
    CHECK(inspect_stack(PTHREAD_STACK_MIN, 1, 0, 0) == 0);
    CHECK(inspect_stack(PTHREAD_STACK_MIN + 1, 1, 0, 0) == 0);
    CHECK(inspect_stack(65536, 1, 0, 1) == 0);
    CHECK(inspect_stack(2097153, 1, 1, 0) == 0);
    CHECK(inspect_stack(PTHREAD_STACK_MIN, 0, 0, 0) == 0);
    CHECK(inspect_stack(2097153, 0, 1, 0) == 0);
    puts("threadstress: stack-inspection ok");
    return 0;
}

/* Three independent creators exercise shared reaping and table reuse. */
struct detach_probe { unsigned done; int error; };
static void *detach_worker(void *arg) {
    struct detach_probe *probe = arg;
    pthread_attr_t attr;
    probe->error = pthread_join(pthread_self(), NULL) != EDEADLK;
    probe->error |= pthread_detach(pthread_self()) != 0;
    probe->error |= pthread_getattr_np(pthread_self(), &attr) != 0;
    probe->error |= attr.detachstate != PTHREAD_CREATE_DETACHED;
    probe->error |= pthread_detach(pthread_self()) != EINVAL;
    __atomic_store_n(&probe->done, 1, __ATOMIC_RELEASE);
    return NULL;
}
static void *detach_creator(void *unused) {
    (void)unused;
    const size_t size = 65536;
    void *stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (stack == MAP_FAILED) return (void *)1;
    int failed = 0;
    for (unsigned round = 0; round < 32; round++) {
        pthread_attr_t attr; pthread_t worker;
        struct detach_probe probe = {0};
        if (pthread_attr_init(&attr) ||
            ((round & 1) && pthread_attr_setstack(&attr, stack, size)) ||
            pthread_create(&worker, &attr, detach_worker, &probe)) { failed = 1; break; }
        while (!__atomic_load_n(&probe.done, __ATOMIC_ACQUIRE)) sched_yield();
        /* A concurrent creator may already have reaped this exited worker. */
        while (srv_thread_status((uint64_t)(uintptr_t)worker) > 0) sched_yield();
        if (probe.error || pthread_join(worker, NULL) != EINVAL) { failed = 1; break; }
    }
    if (munmap(stack, size)) failed = 1;
    return failed ? (void *)1 : NULL;
}
int posix_self_detach_test(void) {
    pthread_t creators[3];
    for (unsigned i = 0; i < 3; i++) CHECK(pthread_create(&creators[i], NULL, detach_creator, NULL) == 0);
    for (unsigned i = 0; i < 3; i++) {
        void *result = (void *)1;
        CHECK(pthread_join(creators[i], &result) == 0 && result == NULL);
    }
    /* Reclaim any final detached worker and prove a fresh create/join works. */
    pthread_t final;
    CHECK(pthread_create(&final, NULL, inspection_noop, NULL) == 0);
    CHECK(pthread_join(final, NULL) == 0);
    puts("threadstress: self-detach ok");
    return 0;
}
