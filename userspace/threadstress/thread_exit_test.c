/* Subprocesses observe initial/last-thread retirement, process status, atexit,
 * TLS destructors and descriptor lifetime without ending the test runner. */
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <sched.h>
#include <signal.h>
#include <spawn.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>
#include <srvros/sys.h>

#define CHILD_CHECK(x) do { if (!(x)) _exit(91); } while (0)
static int report_fd, mode;
static char *shared_heap;
static pthread_key_t exit_key;
static unsigned main_destroyed, workers_destroyed, join_started, claim_checked, signal_seen;
static pthread_t joining_worker;
static _Thread_local unsigned tls_sentinel;
enum { EMPTY, EMPTY_VALUE, JOINABLE, DETACHED, DESCENDANT, PREEXITED,
       EXPLICIT_EXIT, IMMEDIATE_EXIT, FATAL_SIGNAL, MASKED_SIGNAL, EXIT_CASES,
       JOIN_WAIT = EXIT_CASES, JOIN_LATE, JOIN_NULL, DETACH_SELF, DETACH_EARLY,
       DETACH_LATE, JOIN_INTERRUPTED, JOIN_CLAIM, CASES };

static void report(char event) { CHILD_CHECK(write(report_fd, &event, 1) == 1); }
static void destroy_key(void *value) {
    if (value == (void *)1) {
        CHILD_CHECK(tls_sentinel == 101);
        __atomic_add_fetch(&main_destroyed, 1, __ATOMIC_RELEASE);
        report('M');
    } else {
        CHILD_CHECK(value == (void *)2 && tls_sentinel == 202);
        __atomic_add_fetch(&workers_destroyed, 1, __ATOMIC_RELEASE);
        report('D');
    }
}
static void process_cleanup(void) {
    unsigned expected = mode == DESCENDANT || mode == JOIN_CLAIM ? 2 :
        (mode >= JOIN_WAIT || mode == JOINABLE || mode == DETACHED || mode == PREEXITED || mode == MASKED_SIGNAL);
    CHILD_CHECK(__atomic_load_n(&main_destroyed, __ATOMIC_ACQUIRE) == 1);
    CHILD_CHECK(__atomic_load_n(&workers_destroyed, __ATOMIC_ACQUIRE) == expected);
    CHILD_CHECK(!strcmp(shared_heap, "process storage survives"));
    free(shared_heap);
    report('A');
}
static void *exit_worker(void *arg) {
    tls_sentinel = 202;
    CHILD_CHECK(pthread_setspecific(exit_key, (void *)2) == 0);
    if (mode == PREEXITED) return (void *)77;
    /* Observe kernel retirement rather than hoping a sleep orders it. */
    while (srv_thread_status(1) == 1) sched_yield();
    CHILD_CHECK(srv_thread_status(1) == 0);
    CHILD_CHECK(__atomic_load_n(&main_destroyed, __ATOMIC_ACQUIRE) == 1);
    CHILD_CHECK(!strcmp(shared_heap, "process storage survives"));
    CHILD_CHECK(pthread_kill((pthread_t)1, SIGTERM) == 0);
    report('W');
    if (mode == IMMEDIATE_EXIT) _exit(37);
    if (mode == EXPLICIT_EXIT) exit(23);
    if (mode == FATAL_SIGNAL) {
        CHILD_CHECK(kill(getpid(), SIGTERM) == 0);
        for (;;) sched_yield();
    }
    if (mode == MASKED_SIGNAL) {
        sigset_t blocked, pending;
        sigemptyset(&blocked); sigaddset(&blocked, SIGTERM);
        CHILD_CHECK(pthread_sigmask(SIG_BLOCK, &blocked, NULL) == 0);
        CHILD_CHECK(kill(getpid(), SIGTERM) == 0);
        CHILD_CHECK(sigpending(&pending) == 0 && sigismember(&pending, SIGTERM) == 1);
    }
    if (mode == DESCENDANT && arg == NULL) {
        pthread_t leaf;
        pthread_attr_t attr;
        CHILD_CHECK(pthread_attr_init(&attr) == 0);
        CHILD_CHECK(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
        CHILD_CHECK(pthread_create(&leaf, &attr, exit_worker, (void *)1) == 0);
        CHILD_CHECK(pthread_attr_destroy(&attr) == 0);
    } else if (mode == DESCENDANT) {
        /* Leave the newly created detached worker as the last live thread. */
        while (!__atomic_load_n(&workers_destroyed, __ATOMIC_ACQUIRE)) sched_yield();
        CHILD_CHECK(usleep(20000) == 0);
    }
    if (mode == JOINABLE) return (void *)(uintptr_t)0x1234;
    pthread_exit((void *)(uintptr_t)0x1234);
}

static void join_signal_handler(int signo) {
    CHILD_CHECK(signo == SIGUSR1);
    void *unchanged = (void *)77;
    CHILD_CHECK(pthread_join((pthread_t)1, &unchanged) == EINVAL && unchanged == (void *)77);
    CHILD_CHECK(pthread_detach((pthread_t)1) == EINVAL);
    __atomic_store_n(&signal_seen, 1, __ATOMIC_RELEASE);
}
static void *main_join_worker(void *arg) {
    tls_sentinel = 202;
    CHILD_CHECK(pthread_setspecific(exit_key, (void *)2) == 0);
    if (mode == JOIN_CLAIM && arg) {
        while (!__atomic_load_n(&join_started, __ATOMIC_ACQUIRE)) sched_yield();
        CHILD_CHECK(usleep(20000) == 0);
        void *value = (void *)77;
        CHILD_CHECK(pthread_join((pthread_t)1, &value) == EINVAL && value == (void *)77);
        CHILD_CHECK(pthread_detach((pthread_t)1) == EINVAL);
        __atomic_store_n(&claim_checked, 1, __ATOMIC_RELEASE);
        report('W');
        return NULL;
    }
    if (mode == DETACH_EARLY) {
        CHILD_CHECK(pthread_detach((pthread_t)1) == 0);
        CHILD_CHECK(pthread_detach((pthread_t)1) == EINVAL);
        __atomic_store_n(&join_started, 1, __ATOMIC_RELEASE);
    }
    if (mode == JOIN_LATE || mode == DETACH_SELF || mode == DETACH_EARLY || mode == DETACH_LATE) {
        while (srv_thread_status(1) == 1) sched_yield();
        CHILD_CHECK(srv_thread_status(1) == 0);
    }
    if (mode == DETACH_LATE) CHILD_CHECK(pthread_detach((pthread_t)1) == 0);
    if (mode == DETACH_SELF || mode == DETACH_EARLY || mode == DETACH_LATE) {
        CHILD_CHECK(pthread_join((pthread_t)1, NULL) == EINVAL); /* Also lazily reaps. */
    } else {
        __atomic_store_n(&join_started, 1, __ATOMIC_RELEASE);
        void *value = (void *)77;
        CHILD_CHECK(pthread_join((pthread_t)1, mode == JOIN_NULL ? NULL : &value) == 0);
        CHILD_CHECK(mode == JOIN_NULL ? value == (void *)77 : value == (void *)(uintptr_t)0x9876);
    }
    CHILD_CHECK(__atomic_load_n(&main_destroyed, __ATOMIC_ACQUIRE) == 1);
    CHILD_CHECK(!strcmp(shared_heap, "process storage survives"));
    CHILD_CHECK(pthread_join((pthread_t)1, NULL) == EINVAL);
    CHILD_CHECK(pthread_detach((pthread_t)1) == EINVAL);
    CHILD_CHECK(pthread_kill((pthread_t)1, 0) == ESRCH);
    char name[16];
    CHILD_CHECK(pthread_getname_np((pthread_t)1, name, sizeof(name)) == ESRCH);
    CHILD_CHECK(srv_thread_status(1) < 0);
    report('W');
    return (void *)42;
}

int posix_main_exit_child(int argc, char **argv) {
    if (argc != 4) return 90;
    mode = atoi(argv[2]); report_fd = atoi(argv[3]);
    CHILD_CHECK(mode >= 0 && mode < CASES);
    shared_heap = strdup("process storage survives");
    CHILD_CHECK(shared_heap != NULL && atexit(process_cleanup) == 0);
    CHILD_CHECK(pthread_key_create(&exit_key, destroy_key) == 0);
    tls_sentinel = 101;
    CHILD_CHECK(pthread_setspecific(exit_key, (void *)1) == 0);
    CHILD_CHECK(pthread_join(pthread_self(), NULL) == EDEADLK);
    if (mode == DETACH_SELF) {
        CHILD_CHECK(pthread_detach(pthread_self()) == 0);
        pthread_attr_t current;
        CHILD_CHECK(pthread_getattr_np(pthread_self(), &current) == 0);
        CHILD_CHECK(current.detachstate == PTHREAD_CREATE_DETACHED);
    }
    if (mode == JOIN_INTERRUPTED) CHILD_CHECK(signal(SIGUSR1, join_signal_handler) != SIG_ERR);
    if (mode >= JOINABLE) {
        pthread_attr_t attr; pthread_t worker;
        CHILD_CHECK(pthread_attr_init(&attr) == 0);
        if (mode == DETACHED)
            CHILD_CHECK(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
        CHILD_CHECK(pthread_create(&worker, &attr, mode >= JOIN_WAIT ? main_join_worker : exit_worker, NULL) == 0);
        joining_worker = worker;
        if (mode == JOIN_CLAIM) {
            pthread_t other;
            CHILD_CHECK(pthread_create(&other, NULL, main_join_worker, (void *)1) == 0);
            while (!__atomic_load_n(&claim_checked, __ATOMIC_ACQUIRE)) sched_yield();
        }
        if (mode == JOIN_WAIT || mode == JOIN_NULL || mode == JOIN_INTERRUPTED || mode == DETACH_EARLY) {
            while (!__atomic_load_n(&join_started, __ATOMIC_ACQUIRE)) sched_yield();
            CHILD_CHECK(usleep(20000) == 0);
        }
        if (mode == JOIN_INTERRUPTED) {
            CHILD_CHECK(pthread_kill(joining_worker, SIGUSR1) == 0);
            while (!__atomic_load_n(&signal_seen, __ATOMIC_ACQUIRE)) sched_yield();
        }
        CHILD_CHECK(pthread_attr_destroy(&attr) == 0);
        if (mode == PREEXITED) {
            while (srv_thread_status((uint64_t)(uintptr_t)worker) == 1) sched_yield();
            /* Deliberately leave a completed joinable record unreaped. */
        }
    }
    pthread_exit(mode == EMPTY ? NULL : (void *)(uintptr_t)0x9876);
}

static int run_exit_case(int test) {
    int pipefd[2], status = -1;
    if (pipe(pipefd)) return -1;
    char mode_text[16], fd_text[16];
    snprintf(mode_text, sizeof(mode_text), "%d", test);
    snprintf(fd_text, sizeof(fd_text), "%d", pipefd[1]);
    char *args[] = {"threadstress", "main-exit-child", mode_text, fd_text, NULL};
    posix_spawn_file_actions_t actions;
    if (posix_spawn_file_actions_init(&actions)) { close(pipefd[0]); close(pipefd[1]); return -1; }
    if (posix_spawn_file_actions_addclose(&actions, pipefd[0])) {
        posix_spawn_file_actions_destroy(&actions); close(pipefd[0]); close(pipefd[1]); return -1;
    }
    pid_t pid;
    int error = posix_spawn(&pid, "/fat/bin/threadstress", &actions, NULL, args, environ);
    posix_spawn_file_actions_destroy(&actions);
    close(pipefd[1]);
    if (error) { close(pipefd[0]); return -1; }
    unsigned events[256] = {0}; char last = 0;
    for (;;) {
        struct pollfd pfd = {.fd = pipefd[0], .events = POLLIN};
        int ready = poll(&pfd, 1, 5000);
        if (ready < 0 && errno == EINTR) continue;
        if (ready <= 0) { kill(pid, SIGKILL); error = 1; break; }
        unsigned char event;
        ssize_t count = read(pipefd[0], &event, 1);
        if (count == 0) break;
        if (count < 0 && errno == EINTR) continue;
        if (count != 1 || ++events[event] > 8) { kill(pid, SIGKILL); error = 1; break; }
        last = (char)event;
    }
    close(pipefd[0]);
    if (waitpid(pid, &status, 0) != pid) return -1;
    unsigned workers = test == DESCENDANT || test == JOIN_CLAIM ? 2 : (test >= JOINABLE && test != PREEXITED);
    unsigned destructors = test == DESCENDANT || test == JOIN_CLAIM ? 2 :
        (test >= JOIN_WAIT || test == JOINABLE || test == DETACHED || test == PREEXITED || test == MASKED_SIGNAL);
    unsigned cleanup = test != IMMEDIATE_EXIT && test != FATAL_SIGNAL;
    int status_ok = test == FATAL_SIGNAL ? (WIFSIGNALED(status) && WTERMSIG(status) == SIGTERM) :
        (WIFEXITED(status) && WEXITSTATUS(status) ==
            (test == EXPLICIT_EXIT ? 23 : test == IMMEDIATE_EXIT ? 37 : 0));
    if (error || !status_ok || events['M'] != 1 || events['W'] != workers ||
        events['D'] != destructors || events['A'] != cleanup || (cleanup && last != 'A')) {
        printf("threadstress: main-exit case=%d status=%d M=%u W=%u D=%u A=%u error=%d\n",
            test, status, events['M'], events['W'], events['D'], events['A'], error);
        return -1;
    }
    return 0;
}
int posix_main_exit_test(void) {
    for (unsigned round = 0; round < 3; round++)
        for (int test = 0; test < EXIT_CASES; test++) if (run_exit_case(test)) return -1;
    puts("threadstress: main-exit ok");
    return 0;
}

int posix_main_join_test(void) {
    for (unsigned round = 0; round < 3; round++)
        for (int test = JOIN_WAIT; test < CASES; test++) if (run_exit_case(test)) return -1;
    puts("threadstress: main-join ok");
    return 0;
}
