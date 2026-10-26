/* Real libc, modeled OS services. No actual thread is created in this fixture. */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <srvros/sys.h>

#define CHECK(x) do { if (!(x)) { printf("sync-contracts: line %d failed: %s\n", __LINE__, #x); exit(1); } } while (0)
static int error_value, last_clock, futex_calls, dispatches, wait_mode, clock_error;
static int creates, mappings, unmappings, fail_mapping;
static uint64_t submitted_stack, largest_ticks;
static struct timespec now[2];
int *__errno_location(void) { return &error_value; }
long srv_thread_self(void) { return 1; }
long srv_thread_status(uint64_t tid) { (void)tid; return -ESRCH; }
long srv_thread_join(uint64_t tid, uint64_t *value) { (void)tid; (void)value; return -ESRCH; }
long srv_thread_create(uint64_t entry, uint64_t arg, uint64_t stack, uint64_t flags) {
    (void)entry; (void)arg; (void)flags;
    creates++; submitted_stack = stack;
    return -EAGAIN; /* Exercise creation failure and cleanup, without a worker. */
}
long srv_thread_exit(uint64_t value) { (void)value; abort(); }
long srv_thread_signal(uint64_t tid, uint64_t signal) { (void)tid; (void)signal; abort(); }
long srv_syscall0(long number) { (void)number; abort(); }
void srv_yield(void) { abort(); }
int __posix_signal_dispatch_pending(void) { dispatches++; return 0; }
long srv_futex_wait(uint32_t *address, uint32_t expected, uint64_t ticks) {
    CHECK(++futex_calls < 10);
    if (ticks > largest_ticks) largest_ticks = ticks;
    if (wait_mode == 1 && futex_calls == 1) return -ETIMEDOUT; /* early tick wake */
    if (wait_mode == 2 && futex_calls == 1) return SRV_ERR_INTR;
    if (wait_mode == 3) { *address = expected + 1; return 0; }
    CHECK(ticks > 0 && ticks <= 100);
    now[last_clock].tv_nsec += (long)ticks * 10000000L;
    now[last_clock].tv_sec += now[last_clock].tv_nsec / 1000000000L;
    now[last_clock].tv_nsec %= 1000000000L;
    return -ETIMEDOUT;
}
long srv_futex_wake(uint32_t *address, uint64_t count) { (void)address; (void)count; return 0; }
int clock_gettime(int clock_id, struct timespec *out) {
    CHECK(clock_id == CLOCK_REALTIME || clock_id == CLOCK_MONOTONIC);
    last_clock = clock_id;
    if (clock_error) { errno = EIO; return -1; }
    *out = now[clock_id]; return 0;
}
void *mmap(void *address, size_t size, int protection, int flags, int fd, off_t offset) {
    (void)address; (void)protection; (void)flags; (void)fd; (void)offset;
    mappings++;
    if (fail_mapping) return MAP_FAILED;
    CHECK(size >= PTHREAD_STACK_MIN && !(size & 4095));
    void *memory = malloc(size); CHECK(memory != NULL); return memory;
}
int munmap(void *address, size_t size) { (void)size; unmappings++; free(address); return 0; }
static void *never_started(void *arg) { return arg; }

static void supplied_stack(size_t declared) {
    const size_t reserve = 3 * 1024 * 1024;
    unsigned char *allocation = malloc(reserve + 4096); CHECK(allocation != NULL);
    unsigned char *base = (unsigned char *)(((uintptr_t)allocation + 4095) & ~(uintptr_t)4095);
    memset(base, 0xa5, reserve);
    pthread_attr_t attr; pthread_t thread = (pthread_t)99;
    CHECK(pthread_attr_init(&attr) == 0);
    CHECK(pthread_attr_setstack(&attr, base, declared) == 0);
    int before = creates, maps = mappings, unmaps = unmappings;
    CHECK(pthread_create(&thread, &attr, never_started, NULL) == EAGAIN);
    CHECK(creates == before + 1 && mappings == maps && unmappings == unmaps);
    CHECK(thread == (pthread_t)99);
    CHECK(submitted_stack >= (uintptr_t)base);
    size_t offset = (size_t)(submitted_stack - (uintptr_t)base);
    CHECK(offset + sizeof(uint64_t) <= declared && (submitted_stack & 15) == 8);
    for (size_t i = 0; i < reserve; i++)
        CHECK(base[i] == (i >= offset && i < offset + 8 ? 0 : 0xa5));
    void *reported; size_t reported_size;
    CHECK(pthread_attr_getstack(&attr, &reported, &reported_size) == 0 &&
          reported == base && reported_size == declared);
    free(allocation);
}

static void stack_tests(void) {
    supplied_stack(PTHREAD_STACK_MIN);
    supplied_stack(PTHREAD_STACK_MIN + 1);
    supplied_stack(2 * 1024 * 1024);
    supplied_stack(2 * 1024 * 1024 + 1);
    pthread_attr_t attr; pthread_t thread;
    CHECK(pthread_attr_init(&attr) == 0);
    CHECK(pthread_attr_setstack(&attr, (void *)0x1000, PTHREAD_STACK_MIN - 1) == EINVAL);
    CHECK(pthread_attr_setstack(&attr, NULL, PTHREAD_STACK_MIN) == EINVAL);
    CHECK(pthread_attr_setstack(&attr, (void *)0x1001, PTHREAD_STACK_MIN) == EINVAL);
    CHECK(pthread_attr_setstack(&attr, (void *)(UINTPTR_MAX & ~(uintptr_t)15), PTHREAD_STACK_MIN) == EINVAL);
    CHECK(pthread_attr_setstacksize(&attr, SIZE_MAX) == EINVAL);
    CHECK(pthread_attr_setstackaddr(&attr, (void *)0x1001) == EINVAL);
    int before = creates, maps = mappings;
    attr.stackaddr = (void *)(UINTPTR_MAX & ~(uintptr_t)15);
    attr.stacksize = PTHREAD_STACK_MIN;
    CHECK(pthread_create(&thread, &attr, never_started, NULL) == EINVAL);
    attr.stackaddr = NULL; attr.stacksize = SIZE_MAX;
    CHECK(pthread_create(&thread, &attr, never_started, NULL) == EINVAL);
    CHECK(creates == before && mappings == maps);
    CHECK(pthread_attr_init(&attr) == 0);
    CHECK(pthread_attr_setstacksize(&attr, PTHREAD_STACK_MIN + 1) == 0);
    fail_mapping = 1;
    CHECK(pthread_create(&thread, &attr, never_started, NULL) == ENOMEM);
    fail_mapping = 0;
    int unmaps = unmappings;
    for (int i = 0; i < 20; i++) CHECK(pthread_create(&thread, &attr, never_started, NULL) == EAGAIN);
    CHECK(creates == before + 20 && unmappings == unmaps + 20);
    puts("sync-contracts: stack bounds, invalid ranges, overflow and failure cleanup PASS");
}

static void reset_wait(int mode) {
    now[CLOCK_MONOTONIC] = (struct timespec){.tv_sec = 100};
    now[CLOCK_REALTIME] = (struct timespec){.tv_sec = 10000};
    wait_mode = mode; last_clock = -1; futex_calls = dispatches = clock_error = 0; largest_ticks = 0;
}
static void condition_tests(void) {
    pthread_mutex_t mutex; pthread_cond_t cond;
    CHECK(pthread_mutex_init(&mutex, NULL) == 0);
    for (int clock_id = CLOCK_REALTIME; clock_id <= CLOCK_MONOTONIC; clock_id++) {
        pthread_condattr_t attr;
        CHECK(pthread_condattr_init(&attr) == 0 && pthread_condattr_setclock(&attr, clock_id) == 0);
        CHECK(pthread_cond_init(&cond, &attr) == 0);
        for (int mode = 0; mode <= 2; mode++) {
            reset_wait(mode);
            struct timespec deadline = now[clock_id]; deadline.tv_nsec = 25000000;
            CHECK(pthread_mutex_lock(&mutex) == 0);
            CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == ETIMEDOUT);
            CHECK(last_clock == clock_id && futex_calls == (mode ? 2 : 1));
            CHECK(now[clock_id].tv_sec > deadline.tv_sec || now[clock_id].tv_nsec >= deadline.tv_nsec);
            CHECK(dispatches == (mode == 2));
            CHECK(pthread_mutex_trylock(&mutex) == EBUSY && pthread_mutex_unlock(&mutex) == 0);
        }
        reset_wait(3); /* wake without timeout */
        struct timespec deadline = {.tv_sec = INT64_MAX};
        CHECK(pthread_mutex_lock(&mutex) == 0);
        CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == 0 && largest_ticks == 100);
        CHECK(pthread_mutex_unlock(&mutex) == 0);
        reset_wait(0); deadline = now[clock_id];
        CHECK(pthread_mutex_lock(&mutex) == 0);
        CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == ETIMEDOUT && futex_calls == 0);
        CHECK(pthread_mutex_unlock(&mutex) == 0);
        reset_wait(0); clock_error = 1;
        CHECK(pthread_mutex_lock(&mutex) == 0);
        CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == EIO);
        CHECK(pthread_mutex_unlock(&mutex) == 0);
    }
    pthread_condattr_t bad = {.clock = 123};
    CHECK(pthread_cond_init(&cond, &bad) == EINVAL);
    cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
    reset_wait(0); struct timespec deadline = now[CLOCK_REALTIME];
    CHECK(pthread_mutex_lock(&mutex) == 0);
    CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == ETIMEDOUT && last_clock == CLOCK_REALTIME);
    deadline.tv_nsec = 1000000000;
    CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == EINVAL);
    CHECK(pthread_mutex_unlock(&mutex) == 0);
    puts("sync-contracts: selected/default clocks, early wakes, signals, expiry and relocking PASS");
}
static void semaphore_tests(void) {
    sem_t sem; int value;
    CHECK(sem_init(&sem, 0, 1) == 0 && sem_trywait(&sem) == 0);
    CHECK(sem_trywait(&sem) == -1 && errno == EAGAIN);
    CHECK(sem_post(&sem) == 0 && sem_getvalue(&sem, &value) == 0 && value == 1);
    CHECK(sem_destroy(&sem) == 0);
    sem.__value = 7;
    CHECK(sem_init(&sem, 0, (unsigned)SEM_VALUE_MAX + 1) == -1 && errno == EINVAL && sem.__value == 7);
    CHECK(sem_init(&sem, 0, UINT32_MAX) == -1 && errno == EINVAL && sem.__value == 7);
    CHECK(sem_init(&sem, 0, SEM_VALUE_MAX - 1) == 0);
    CHECK(sem_post(&sem) == 0 && sem_getvalue(&sem, &value) == 0 && value == SEM_VALUE_MAX);
    CHECK(sem_post(&sem) == -1 && errno == EOVERFLOW);
    CHECK(sem_getvalue(&sem, &value) == 0 && value == SEM_VALUE_MAX);
    CHECK(sem_trywait(&sem) == 0 && sem_post(&sem) == 0);
    CHECK(sem_post(NULL) == -1 && errno == EINVAL);
    puts("sync-contracts: semaphore bounds, unchanged overflow and recovery PASS");
}
int main(void) { stack_tests(); condition_tests(); semaphore_tests(); return 0; }
