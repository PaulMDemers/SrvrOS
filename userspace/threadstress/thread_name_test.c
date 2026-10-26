#include <errno.h>
#include <pthread.h>
#include <sched.h>
#include <srvros/sys.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define CHECK(x) do { if (!(x)) { printf("threadstress: thread-names line=%d\n", __LINE__); return -1; } } while (0)
struct name_probe { pthread_t main; unsigned ready, go, done; int error; };
static void *named_worker(void *arg) {
    struct name_probe *probe = arg;
    char name[16];
    probe->error = pthread_getname_np(pthread_self(), name, sizeof(name)) != 0 || strcmp(name, "srvros-thread");
    probe->error |= pthread_setname_np(pthread_self(), "early-worker") != 0;
    probe->error |= pthread_getname_np(probe->main, name, sizeof(name)) != 0 || strcmp(name, "main-before");
    probe->error |= pthread_setname_np(probe->main, "from-worker") != 0;
    __atomic_store_n(&probe->ready, 1, __ATOMIC_RELEASE);
    while (!__atomic_load_n(&probe->go, __ATOMIC_ACQUIRE)) sched_yield();
    probe->error |= pthread_getname_np(pthread_self(), name, sizeof(name)) != 0 || strcmp(name, "from-parent");
    __atomic_store_n(&probe->done, 1, __ATOMIC_RELEASE);
    return NULL;
}
static int name_lifetimes(void) {
    char name[16];
    for (unsigned round = 0; round < 24; round++) {
        int detached = round & 1;
        pthread_attr_t attr; pthread_t worker;
        struct name_probe probe = {.main = pthread_self()};
        CHECK(pthread_setname_np(probe.main, "main-before") == 0);
        CHECK(pthread_attr_init(&attr) == 0);
        CHECK(pthread_attr_setdetachstate(&attr, detached ? PTHREAD_CREATE_DETACHED : PTHREAD_CREATE_JOINABLE) == 0);
        CHECK(pthread_create(&worker, &attr, named_worker, &probe) == 0);
        while (!__atomic_load_n(&probe.ready, __ATOMIC_ACQUIRE)) sched_yield();
        CHECK(!probe.error);
        CHECK(pthread_getname_np(worker, name, sizeof(name)) == 0 && !strcmp(name, "early-worker"));
        CHECK(pthread_getname_np(probe.main, name, sizeof(name)) == 0 && !strcmp(name, "from-worker"));
        CHECK(pthread_setname_np(worker, "from-parent") == 0);
        __atomic_store_n(&probe.go, 1, __ATOMIC_RELEASE);
        while (!__atomic_load_n(&probe.done, __ATOMIC_ACQUIRE)) sched_yield();
        while (srv_thread_status((uint64_t)(uintptr_t)worker) > 0) sched_yield();
        CHECK(!probe.error);
        /* Exited records remain valid until libc joins/reaps them. */
        CHECK(pthread_getname_np(worker, name, sizeof(name)) == 0 && !strcmp(name, "from-parent"));
        CHECK(pthread_setname_np(worker, "after-exit") == 0);
        CHECK(pthread_getname_np(worker, name, sizeof(name)) == 0 && !strcmp(name, "after-exit"));
        CHECK(pthread_join(worker, NULL) == (detached ? EINVAL : 0));
        memset(name, 'x', sizeof(name));
        CHECK(pthread_getname_np(worker, name, sizeof(name)) == ESRCH && name[0] == 'x');
        CHECK(pthread_setname_np(worker, "stale") == ESRCH);
    }
    return 0;
}
static pthread_t name_target;
static unsigned name_gate;
static const char *name_a = "aaaaaaaaaaaaaaa", *name_b = "bbbbbbbbbbbbbbb";
static void *name_racer(void *arg) {
    uintptr_t role = (uintptr_t)arg;
    while (!__atomic_load_n(&name_gate, __ATOMIC_ACQUIRE)) sched_yield();
    for (unsigned i = 0; i < 200; i++) {
        if (role < 2) {
            if (pthread_setname_np(name_target, role ? name_b : name_a)) return (void *)1;
        } else {
            char name[16];
            if (pthread_getname_np(name_target, name, sizeof(name)) ||
                (strcmp(name, name_a) && strcmp(name, name_b))) return (void *)1;
        }
        sched_yield();
    }
    return NULL;
}
int posix_thread_name_test(void) {
    char saved[16], name[32]; pthread_t self = pthread_self();
    CHECK(pthread_getname_np(self, saved, sizeof(saved)) == 0);
    errno = EDOM;
    CHECK(pthread_setname_np(self, "123456789abcdef") == 0);
    CHECK(pthread_setname_np(self, "123456789abcdefg") == ERANGE);
    memset(name, 'x', sizeof(name));
    CHECK(pthread_getname_np(self, name, 15) == ERANGE);
    CHECK(pthread_getname_np(self, name, 0) == ERANGE);
    for (unsigned i = 0; i < sizeof(name); i++) CHECK(name[i] == 'x');
    CHECK(pthread_getname_np(self, name, 16) == 0 && !strcmp(name, "123456789abcdef") && name[16] == 'x');
    CHECK(pthread_setname_np(self, "") == 0);
    CHECK(pthread_getname_np(self, name, 1) == 0 && name[0] == 0);
    CHECK(pthread_setname_np(self, NULL) == EINVAL);
    CHECK(pthread_getname_np(self, NULL, 16) == EINVAL);
    CHECK(pthread_setname_np((pthread_t)(uintptr_t)-1, "missing") == ESRCH);
    CHECK(pthread_getname_np((pthread_t)0, name, 16) == ESRCH);
    CHECK(errno == EDOM);
    CHECK(name_lifetimes() == 0);
    name_target = self; name_gate = 0;
    CHECK(pthread_setname_np(self, name_a) == 0);
    pthread_t racers[3];
    for (unsigned i = 0; i < 3; i++) CHECK(pthread_create(&racers[i], NULL, name_racer, (void *)(uintptr_t)i) == 0);
    __atomic_store_n(&name_gate, 1, __ATOMIC_RELEASE);
    for (unsigned i = 0; i < 3; i++) {
        void *result = (void *)1;
        CHECK(pthread_join(racers[i], &result) == 0 && result == NULL);
    }
    CHECK(pthread_setname_np(self, saved) == 0);
    puts("threadstress: thread-names ok");
    return 0;
}
