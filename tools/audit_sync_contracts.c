/* Audit probe, not a passing conformance test. The runner prefixes libc names.
 * Compile the real SrvrOS implementations; only OS services are modeled here. */
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <srvros/sys.h>

static int error_value, last_clock = -1, futex_calls;
static uint64_t submitted_stack;
int *__errno_location(void) { return &error_value; }
long srv_thread_self(void) { return 1; }
long srv_thread_status(uint64_t tid) { (void)tid; return -ESRCH; }
long srv_thread_join(uint64_t tid, uint64_t *value) {
    (void)tid; (void)value; return -ESRCH;
}
long srv_thread_create(uint64_t entry, uint64_t arg, uint64_t stack, uint64_t flags) {
    (void)entry; (void)arg; (void)flags;
    submitted_stack = stack;
    return -EAGAIN; /* No host/guest thread is started; caller cleans its record. */
}
long srv_thread_exit(uint64_t value) { (void)value; abort(); }
long srv_thread_signal(uint64_t tid, uint64_t signal) {
    (void)tid; (void)signal; abort();
}
long srv_syscall0(long number) { (void)number; abort(); }
void srv_yield(void) { abort(); }
int __posix_signal_dispatch_pending(void) { return 0; }
long srv_futex_wait(uint32_t *address, uint32_t expected, uint64_t ticks) {
    (void)address; (void)expected; (void)ticks;
    futex_calls++;
    return -ETIMEDOUT;
}
long srv_futex_wake(uint32_t *address, uint64_t count) {
    (void)address; (void)count; return 0;
}
int clock_gettime(int clock_id, struct timespec *out) {
    last_clock = clock_id;
    out->tv_sec = clock_id == CLOCK_MONOTONIC ? 100 : 10000;
    out->tv_nsec = 0;
    return 0;
}
void *mmap(void *address, size_t size, int protection, int flags, int fd, off_t offset) {
    (void)address; (void)size; (void)protection; (void)flags; (void)fd; (void)offset;
    abort(); /* This probe supplies its own guarded stack storage. */
}
int munmap(void *address, size_t size) { (void)address; (void)size; abort(); }

static void require(int condition) {
    if (!condition) { puts("probe setup/control failed"); exit(2); }
}
static void *never_started(void *arg) { return arg; }

static int supplied_stack(size_t declared) {
    const size_t reserve = 3 * 1024 * 1024;
    unsigned char *allocation = malloc(reserve + 4096);
    require(allocation != NULL);
    unsigned char *base = (unsigned char *)(((uintptr_t)allocation + 4095) & ~(uintptr_t)4095);
    memset(base, 0xa5, reserve);
    pthread_attr_t attr;
    pthread_t thread;
    require(pthread_attr_init(&attr) == 0);
    require(pthread_attr_setstack(&attr, base, declared) == 0);
    submitted_stack = 0;
    require(pthread_create(&thread, &attr, never_started, NULL) == EAGAIN);
    require(submitted_stack >= (uintptr_t)base);
    size_t offset = (size_t)(submitted_stack - (uintptr_t)base);
    require(offset + sizeof(uint64_t) <= reserve);
    uint64_t written;
    memcpy(&written, base + offset, sizeof(written));
    int escaped = offset + sizeof(written) > declared && written == 0;
    printf("stack declared=%zu submitted_offset=%zu out_of_contract_write=%d\n",
           declared, offset, escaped);
    free(allocation);
    return escaped;
}

static int condition_clock(void) {
    pthread_condattr_t attr;
    pthread_cond_t cond;
    pthread_mutex_t mutex;
    require(pthread_mutex_init(&mutex, NULL) == 0);
    require(pthread_mutex_lock(&mutex) == 0);
    require(pthread_condattr_init(&attr) == 0);
    require(pthread_condattr_setclock(&attr, CLOCK_MONOTONIC) == 0);
    require(pthread_cond_init(&cond, &attr) == 0);
    struct timespec deadline = { .tv_sec = 101 };
    last_clock = -1; futex_calls = 0;
    int result = pthread_cond_timedwait(&cond, &mutex, &deadline);
    int wrong_clock = last_clock == CLOCK_REALTIME && result == ETIMEDOUT && !futex_calls;
    printf("condition monotonic_selected=1 realtime_queried=%d futex_calls=%d early_timeout=%d\n",
           last_clock == CLOCK_REALTIME, futex_calls, wrong_clock);
    require(pthread_mutex_unlock(&mutex) == 0);
    return wrong_clock;
}

static int semaphore_overflow(void) {
    sem_t sem;
    int value;
    /* Positive control before exercising the accepted upper bound. */
    require(sem_init(&sem, 0, 1) == 0);
    require(sem_trywait(&sem) == 0);
    require(sem_trywait(&sem) == -1 && errno == EAGAIN);
    require(sem_destroy(&sem) == 0);
    int initialized = sem_init(&sem, 0, UINT32_MAX);
    int posted = initialized == 0 ? sem_post(&sem) : -1;
    require(initialized != 0 || sem_getvalue(&sem, &value) == 0);
    int wrapped = initialized == 0 && posted == 0 && value == 0;
    printf("semaphore init_uint_max=%d post=%d silent_wrap=%d\n", initialized, posted, wrapped);
    if (initialized == 0) require(sem_destroy(&sem) == 0);
    return wrapped;
}

int main(int argc, char **argv) {
    int expected = argc == 2 && !strcmp(argv[1], "--expect-known-defects");
    if (argc != 1 && !expected) return 2;
    /* Exact default-sized stack is a positive control; padding cases are not. */
    require(!supplied_stack(2 * 1024 * 1024));
    int findings = supplied_stack(PTHREAD_STACK_MIN);
    findings += supplied_stack(2 * 1024 * 1024 + 1);
    findings += condition_clock();
    findings += semaphore_overflow();
    printf("audit: %d known-defect observations; this is not a conformance PASS\n", findings);
    return expected ? (findings == 4 ? 0 : 2) : (findings ? 1 : 0);
}
