#include <errno.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
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

/* Explicit audit mode, excluded from the passing all-tests gate. */
struct inspection_probe { unsigned go; void *reported; size_t size; int error; };
static void *inspection_worker(void *arg) {
    struct inspection_probe *probe = arg;
    while (!__atomic_load_n(&probe->go, __ATOMIC_ACQUIRE)) sched_yield();
    pthread_attr_t attr;
    probe->error = pthread_getattr_np(pthread_self(), &attr);
    if (!probe->error) probe->error = pthread_attr_getstack(&attr, &probe->reported, &probe->size);
    return NULL;
}
int posix_stack_inspection_audit(void) {
    size_t size = 65536;
    void *stack = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    CHECK(stack != MAP_FAILED);
    pthread_attr_t attr; pthread_t worker; struct inspection_probe probe = {0};
    CHECK(pthread_attr_init(&attr) == 0 && pthread_attr_setstack(&attr, stack, size) == 0);
    CHECK(pthread_create(&worker, &attr, inspection_worker, &probe) == 0);
    __atomic_store_n(&probe.go, 1, __ATOMIC_RELEASE); /* parent record is published */
    CHECK(pthread_join(worker, NULL) == 0);
    int mismatch = probe.error || probe.reported != stack || probe.size != size;
    printf("threadstress: stack-inspection requested=%p/%lu reported=%p/%lu error=%d mismatch=%d\n",
           stack, (unsigned long)size, probe.reported, (unsigned long)probe.size, probe.error, mismatch);
    CHECK(munmap(stack, size) == 0);
    if (mismatch) { puts("threadstress: stack-inspection-audit failed"); return 1; }
    puts("threadstress: stack-inspection-audit ok\nthreadstress: ok"); return 0;
}
