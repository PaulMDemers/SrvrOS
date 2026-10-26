/* Real libc and modeled OS services; a native shim forces early child exit. */
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
static pthread_mutex_t *clock_mutex_probe;
static int clock_probes, clock_calls, wake_calls;
static int creates, mappings, unmappings, fail_mapping;
static uint64_t submitted_stack, largest_ticks;
static int inspect_early, expected_detach, lifecycle, model_detach = 1;
static long model_tid = 1;
static int model_exited, model_joined, joins, status_error, join_error, reenter;
static int initial_exit, initial_destroyed, initial_handoff;
static int main_lifecycle, main_status, main_join_calls, main_join_failures;
int sync_host_is_worker(void);
int sync_host_run_worker(uint64_t entry, uint64_t arg);
_Noreturn void sync_host_exit_worker(void);
static void *expected_stack;
static size_t expected_size;
static struct timespec now[2];
int *__errno_location(void) { return &error_value; }
long srv_thread_self(void) { return main_lifecycle ? 2 : sync_host_is_worker() ? model_tid : 1; }
long srv_thread_status(uint64_t tid) {
    if (main_lifecycle && tid == 1) return main_status;
    if (!lifecycle || tid != (uint64_t)model_tid || model_joined) return -ESRCH;
    if (status_error) { status_error = 0; return -EIO; }
    if (reenter) {
        reenter = 0;
        /* Another API entry tries to reap while this record is already claimed. */
        CHECK(pthread_detach((pthread_t)999) == EINVAL);
        CHECK(!model_joined);
    }
    return model_exited ? 0 : 1;
}
long srv_thread_join(uint64_t tid, uint64_t *value) {
    if (main_lifecycle && tid == 1) {
        void *unchanged = (void *)77;
        main_join_calls++;
        /* A competing API entry cannot collect or detach the claimed record. */
        CHECK(pthread_join((pthread_t)1, &unchanged) == EINVAL && unchanged == (void *)77);
        CHECK(pthread_detach((pthread_t)1) == EINVAL);
        if (main_join_failures) return --main_join_failures ? -EIO : SRV_ERR_INTR;
        *value = 0x9876;
        return 0;
    }
    if (!lifecycle || tid != (uint64_t)model_tid || model_joined) return -ESRCH;
    if (join_error) { int error = join_error; join_error = 0; return error; }
    CHECK(model_exited); model_joined = 1; joins++; *value = 0; return 0;
}
long srv_thread_create(uint64_t entry, uint64_t arg, uint64_t stack, uint64_t flags) {
    (void)flags;
    creates++; submitted_stack = stack;
    if (lifecycle) {
        model_tid++; model_exited = model_joined = 0;
        int before = unmappings, joined = joins;
        CHECK(pthread_setname_np((pthread_t)1, "parent") == 0);
        CHECK(sync_host_run_worker(entry, arg) == 0);
        char name[16];
        CHECK(pthread_getname_np((pthread_t)model_tid, name, sizeof(name)) == 0 && !strcmp(name, "early-worker"));
        CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == 0 && !strcmp(name, "from-worker"));
        CHECK(pthread_setname_np((pthread_t)model_tid, "after-exit") == 0);
        CHECK(model_exited);
        pthread_attr_t attr;
        CHECK(pthread_getattr_np((pthread_t)model_tid, &attr) == 0);
        CHECK(attr.detachstate == (model_detach ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE));
        CHECK(pthread_detach((pthread_t)999) == EINVAL);
        CHECK(unmappings == before && joins == joined); /* Creator still pins it. */
        return model_tid;
    }
    if (inspect_early) {
        ((void (*)(uint64_t))(uintptr_t)entry)(arg);
        abort();
    }
    return -EAGAIN; /* Exercise creation failure and cleanup, without a worker. */
}
long srv_thread_exit(uint64_t value) {
    if (initial_exit) {
        CHECK(value == 0x1234 && initial_destroyed == 1);
        initial_handoff = 1;
        return 0; /* Kernel's last-thread cleanup handoff, modeled only. */
    }
    if (lifecycle) { CHECK(value == 0); model_exited = 1; sync_host_exit_worker(); }
    CHECK(inspect_early && value == 0);
    puts("sync-contracts: child self-inspection before parent publication PASS");
    exit(0); /* Dedicated process; never return to parent publication. */
}
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
long srv_futex_wake(uint32_t *address, uint64_t count) { (void)address; (void)count; wake_calls++; return 0; }
int clock_gettime(int clock_id, struct timespec *out) {
    CHECK(clock_id == CLOCK_REALTIME || clock_id == CLOCK_MONOTONIC);
    clock_calls++;
    if (clock_mutex_probe) {
        /* Exercise the real mutex: clock/deadline evaluation must happen after
         * releasing it, even for an already-expired absolute deadline. */
        CHECK(pthread_mutex_trylock(clock_mutex_probe) == 0);
        CHECK(pthread_mutex_unlock(clock_mutex_probe) == 0);
        clock_probes++;
    }
    last_clock = clock_id;
    if (clock_error) { errno = EIO; return -1; }
    *out = now[clock_id]; return 0;
}
void *mmap(void *address, size_t size, int protection, int flags, int fd, off_t offset) {
    (void)address; (void)protection; (void)flags; (void)fd; (void)offset;
    mappings++;
    if (fail_mapping) return MAP_FAILED;
    CHECK(size >= PTHREAD_STACK_MIN && !(size & 4095));
    void *memory = malloc(size); CHECK(memory != NULL);
    if (inspect_early || lifecycle) { expected_stack = memory; expected_size = size; }
    return memory;
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

static void absolute_deadline_tests(void) {
    pthread_mutexattr_t mutex_attr;
    pthread_mutex_t mutex;
    CHECK(pthread_mutexattr_init(&mutex_attr) == 0);
    CHECK(pthread_mutexattr_settype(&mutex_attr, PTHREAD_MUTEX_ERRORCHECK) == 0);
    CHECK(pthread_mutex_init(&mutex, &mutex_attr) == 0);
    const struct timespec expired[] = {
        {.tv_sec = -1}, {.tv_sec = -1, .tv_nsec = 999999999},
        {.tv_sec = INT64_MIN}, {.tv_sec = INT64_MIN, .tv_nsec = 999999999}, {0}
    };
    for (int kind = 0; kind < 3; kind++) {
        int clock_id = kind == 2 ? CLOCK_MONOTONIC : CLOCK_REALTIME;
        pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
        if (kind) {
            pthread_condattr_t attr;
            CHECK(pthread_condattr_init(&attr) == 0 && pthread_condattr_setclock(&attr, clock_id) == 0);
            CHECK(pthread_cond_init(&cond, &attr) == 0 && pthread_condattr_destroy(&attr) == 0);
        }
        for (unsigned i = 0; i < sizeof(expired) / sizeof(*expired); i++) {
            reset_wait(0); clock_probes = clock_calls = 0;
            CHECK(pthread_mutex_lock(&mutex) == 0);
            clock_mutex_probe = &mutex; errno = EDOM;
            CHECK(pthread_cond_timedwait(&cond, &mutex, &expired[i]) == ETIMEDOUT);
            clock_mutex_probe = NULL;
            CHECK(clock_probes == 1 && clock_calls == 1 && last_clock == clock_id && futex_calls == 0);
            CHECK(errno == EDOM && pthread_mutex_lock(&mutex) == EDEADLK);
            CHECK(pthread_mutex_unlock(&mutex) == 0);
        }
        const long invalid[] = {-1, 1000000000L};
        for (unsigned i = 0; i < sizeof(invalid) / sizeof(*invalid); i++) {
            struct timespec deadline = {.tv_sec = -1, .tv_nsec = invalid[i]};
            CHECK(pthread_mutex_lock(&mutex) == 0);
            clock_calls = wake_calls = 0; errno = EDOM;
            CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == EINVAL);
            CHECK(clock_calls == 0 && wake_calls == 0 && errno == EDOM);
            CHECK(pthread_mutex_lock(&mutex) == EDEADLK && pthread_mutex_unlock(&mutex) == 0);
        }
        /* A negative absolute value can also be in the future. Include a
         * nanosecond borrow and a signed-seconds boundary crossing. */
        for (int crossing = 0; crossing < 2; crossing++) {
            reset_wait(0); now[clock_id] = (struct timespec){.tv_sec = crossing ? -1 : -3, .tv_nsec = 990000000};
            struct timespec deadline = {.tv_sec = crossing ? 0 : -2, .tv_nsec = 20000000};
            CHECK(pthread_mutex_lock(&mutex) == 0);
            clock_mutex_probe = &mutex; clock_probes = 0;
            CHECK(pthread_cond_timedwait(&cond, &mutex, &deadline) == ETIMEDOUT);
            clock_mutex_probe = NULL;
            CHECK(futex_calls == 1 && largest_ticks == 3 && clock_probes == 2);
            CHECK(pthread_mutex_lock(&mutex) == EDEADLK && pthread_mutex_unlock(&mutex) == 0);
        }
        reset_wait(3); now[clock_id] = (struct timespec){.tv_sec = INT64_MIN};
        struct timespec distant = {.tv_sec = INT64_MAX};
        CHECK(pthread_mutex_lock(&mutex) == 0);
        CHECK(pthread_cond_timedwait(&cond, &mutex, &distant) == 0 && largest_ticks == 100);
        CHECK(pthread_mutex_unlock(&mutex) == 0 && pthread_cond_destroy(&cond) == 0);
    }
    CHECK(pthread_mutex_destroy(&mutex) == 0 && pthread_mutexattr_destroy(&mutex_attr) == 0);
    puts("sync-contracts: signed absolute deadlines, invalid nanoseconds and mutex transition PASS");
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
static void *inspect_worker(void *arg) {
    (void)arg;
    pthread_attr_t attr;
    CHECK(pthread_getattr_np(pthread_self(), &attr) == 0);
    CHECK(attr.stackaddr == expected_stack && attr.stacksize == expected_size);
    CHECK(attr.detachstate == expected_detach);
    if (lifecycle) {
        char name[16];
        CHECK(pthread_getname_np(pthread_self(), name, sizeof(name)) == 0 && !strcmp(name, "srvros-thread"));
        CHECK(pthread_setname_np(pthread_self(), "early-worker") == 0);
        CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == 0 && !strcmp(name, "parent"));
        CHECK(pthread_setname_np((pthread_t)1, "from-worker") == 0);
        CHECK(pthread_join(pthread_self(), NULL) == EDEADLK);
        if (model_detach) {
            CHECK(pthread_detach(pthread_self()) == 0);
            CHECK(pthread_detach(pthread_self()) == EINVAL);
            CHECK(pthread_getattr_np(pthread_self(), &attr) == 0);
            CHECK(attr.detachstate == PTHREAD_CREATE_DETACHED);
            CHECK(pthread_join(pthread_self(), NULL) == EDEADLK);
        }
    }
    return NULL;
}
static void lifecycle_tests(void) {
    lifecycle = 1;
    for (int i = 0; i < 40; i++) {
        pthread_attr_t attr; pthread_t thread;
        CHECK(pthread_attr_init(&attr) == 0);
        int supplied = i & 1;
        if (supplied) {
            expected_size = PTHREAD_STACK_MIN + 1;
            expected_stack = malloc(expected_size); CHECK(expected_stack != NULL);
            CHECK(pthread_attr_setstack(&attr, expected_stack, expected_size) == 0);
        }
        expected_detach = PTHREAD_CREATE_JOINABLE;
        int before = unmappings, joined = joins;
        CHECK(pthread_create(&thread, &attr, inspect_worker, NULL) == 0);
        char name[16];
        CHECK(pthread_getname_np(thread, name, sizeof(name)) == 0 && !strcmp(name, "after-exit"));
        CHECK(pthread_getattr_np(thread, &attr) == 0 && attr.detachstate == PTHREAD_CREATE_DETACHED);
        /* Neither transient status errors nor interrupted reaps discard ownership. */
        status_error = 1;
        CHECK(pthread_detach((pthread_t)999) == EINVAL);
        CHECK(joins == joined && unmappings == before);
        join_error = SRV_ERR_INTR;
        CHECK(pthread_detach((pthread_t)999) == EINVAL);
        CHECK(joins == joined && unmappings == before);
        reenter = 1;
        CHECK(pthread_join(thread, NULL) == EINVAL); /* Reaps; cannot join detached. */
        CHECK(joins == joined + 1 && unmappings == before + !supplied);
        CHECK(pthread_getattr_np(thread, &attr) == ESRCH);
        CHECK(pthread_getname_np(thread, name, sizeof(name)) == ESRCH);
        CHECK(pthread_setname_np(thread, "stale") == ESRCH);
        CHECK(pthread_detach((pthread_t)999) == EINVAL);
        CHECK(joins == joined + 1 && unmappings == before + !supplied);
        if (supplied) free(expected_stack);
    }
    /* A failed ordinary join releases its claim; interruption retries it. */
    model_detach = 0;
    pthread_t thread; pthread_attr_t attr;
    int before = unmappings, joined = joins;
    CHECK(pthread_create(&thread, NULL, inspect_worker, NULL) == 0);
    join_error = -EIO;
    CHECK(pthread_join(thread, NULL) == EINVAL);
    CHECK(joins == joined && unmappings == before);
    CHECK(pthread_getattr_np(thread, &attr) == 0 && attr.detachstate == PTHREAD_CREATE_JOINABLE);
    join_error = SRV_ERR_INTR;
    void *value = (void *)1;
    CHECK(pthread_join(thread, &value) == 0 && value == NULL);
    CHECK(joins == joined + 1 && unmappings == before + 1);
    CHECK(pthread_getattr_np(thread, &attr) == ESRCH);
    puts("sync-contracts: early self-detach, parent resume, reap ownership, 40 lifetimes and join retry PASS");
}
static void name_tests(void) {
    char name[32]; pthread_t self = pthread_self();
    errno = EDOM;
    CHECK(pthread_getname_np(self, name, sizeof(name)) == 0 && !strcmp(name, "srvros-thread"));
    CHECK(pthread_setname_np(self, "audit-worker") == 0);
    CHECK(pthread_getname_np(self, name, sizeof(name)) == 0 && !strcmp(name, "audit-worker"));
    CHECK(pthread_setname_np(self, "123456789abcdef") == 0);
    memset(name, 'x', sizeof(name));
    CHECK(pthread_getname_np(self, name, 16) == 0 && !strcmp(name, "123456789abcdef") && name[16] == 'x');
    CHECK(pthread_setname_np(self, "123456789abcdefg") == ERANGE);
    char unterminated[16]; memset(unterminated, 'x', sizeof(unterminated));
    CHECK(pthread_setname_np(self, unterminated) == ERANGE);
    CHECK(pthread_getname_np(self, name, sizeof(name)) == 0 && !strcmp(name, "123456789abcdef"));
    memset(name, 'x', sizeof(name));
    CHECK(pthread_getname_np(self, name, 15) == ERANGE);
    CHECK(pthread_getname_np(self, name, 0) == ERANGE);
    for (unsigned i = 0; i < sizeof(name); i++) CHECK(name[i] == 'x');
    CHECK(pthread_setname_np(self, "") == 0);
    CHECK(pthread_getname_np(self, name, 1) == 0 && name[0] == 0 && name[1] == 'x');
    CHECK(pthread_setname_np(self, NULL) == EINVAL);
    CHECK(pthread_getname_np(self, NULL, sizeof(name)) == EINVAL);
    CHECK(pthread_setname_np((pthread_t)0, "missing") == ESRCH);
    CHECK(pthread_getname_np((pthread_t)999, name, sizeof(name)) == ESRCH);
    CHECK(errno == EDOM);
    puts("sync-contracts: thread names, exact limits, unchanged errors and errno PASS");
}
static void initial_destructor(void *value) {
    CHECK(value == (void *)1 && !initial_handoff);
    initial_destroyed++;
}
static void initial_cleanup(void) {
    CHECK(initial_destroyed == 1 && initial_handoff == 1);
    puts("sync-contracts: initial exit destructors and process cleanup PASS");
}
static void main_lifecycle_tests(const char *mode) {
    main_lifecycle = 1;
    main_status = 1; /* status: running */
    char name[16];
    CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == 0);
    if (!strcmp(mode, "initial-join")) {
        void *value = (void *)77;
        main_join_failures = 2;
        CHECK(pthread_join((pthread_t)1, &value) == EINVAL && value == (void *)77);
        CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == 0);
        CHECK(pthread_join((pthread_t)1, &value) == 0 && value == (void *)0x9876);
        CHECK(main_join_calls == 3 && dispatches >= 2);
    } else if (!strcmp(mode, "initial-join-null")) {
        CHECK(pthread_join((pthread_t)1, NULL) == 0 && main_join_calls == 1);
    } else {
        if (!strcmp(mode, "initial-detach-late")) main_status = 0;
        CHECK(pthread_detach((pthread_t)1) == 0);
        if (main_status) {
            CHECK(main_join_calls == 0);
            CHECK(pthread_detach((pthread_t)1) == EINVAL);
            main_status = 0;
            CHECK(pthread_detach((pthread_t)999) == EINVAL); /* Triggers lazy reap. */
        }
        CHECK(main_join_calls == 1);
    }
    CHECK(pthread_join((pthread_t)1, NULL) == EINVAL);
    CHECK(pthread_detach((pthread_t)1) == EINVAL);
    CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == ESRCH);
    CHECK(pthread_setname_np((pthread_t)1, "gone") == ESRCH);
    CHECK(mappings == 0 && unmappings == 0);
    puts("sync-contracts: initial join/detach ownership and static storage PASS");
}
int main(int argc, char **argv) {
    if (argc > 1 && (!strcmp(argv[1], "initial-join") || !strcmp(argv[1], "initial-join-null") ||
        !strcmp(argv[1], "initial-detach") || !strcmp(argv[1], "initial-detach-late"))) {
        main_lifecycle_tests(argv[1]); return 0;
    }
    if (argc > 1 && !strcmp(argv[1], "initial-exit")) {
        pthread_key_t key;
        initial_exit = 1;
        CHECK(pthread_key_create(&key, initial_destructor) == 0);
        CHECK(pthread_setspecific(key, (void *)1) == 0);
        CHECK(atexit(initial_cleanup) == 0);
        pthread_exit((void *)(uintptr_t)0x1234);
    }
    if (argc > 1 && !strcmp(argv[1], "thread-names")) { name_tests(); return 0; }
    if (argc == 1) {
        stack_tests(); condition_tests(); absolute_deadline_tests(); semaphore_tests();
        pthread_attr_t attr;
        CHECK(pthread_getattr_np(pthread_self(), &attr) == ESRCH); /* Unregistered host stack. */
        CHECK(pthread_getattr_np((pthread_t)999, &attr) == ESRCH);
        CHECK(pthread_getattr_np(pthread_self(), NULL) == EINVAL);
        return 0;
    }
    if (!strcmp(argv[1], "lifecycle")) { lifecycle_tests(); return 0; }
    inspect_early = 1;
    pthread_attr_t attr; pthread_t thread;
    CHECK(pthread_attr_init(&attr) == 0);
    expected_detach = !strcmp(argv[1], "detached") ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE;
    CHECK(pthread_attr_setdetachstate(&attr, expected_detach) == 0);
    if (strcmp(argv[1], "owned")) {
        expected_size = PTHREAD_STACK_MIN + 1;
        expected_stack = malloc(expected_size); CHECK(expected_stack != NULL);
        CHECK(pthread_attr_setstack(&attr, expected_stack, expected_size) == 0);
    }
    CHECK(pthread_create(&thread, &attr, inspect_worker, NULL) == 0);
    abort(); /* The modeled child exits this process before create returns. */
}
