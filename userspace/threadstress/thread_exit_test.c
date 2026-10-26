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
static unsigned main_destroyed, workers_destroyed;
static _Thread_local unsigned tls_sentinel;
enum { EMPTY, EMPTY_VALUE, JOINABLE, DETACHED, DESCENDANT, PREEXITED,
       EXPLICIT_EXIT, IMMEDIATE_EXIT, FATAL_SIGNAL, MASKED_SIGNAL, CASES };

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
    unsigned expected = mode == DESCENDANT ? 2 :
        (mode == JOINABLE || mode == DETACHED || mode == PREEXITED || mode == MASKED_SIGNAL);
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

int posix_main_exit_child(int argc, char **argv) {
    if (argc != 4) return 90;
    mode = atoi(argv[2]); report_fd = atoi(argv[3]);
    CHILD_CHECK(mode >= 0 && mode < CASES);
    shared_heap = strdup("process storage survives");
    CHILD_CHECK(shared_heap != NULL && atexit(process_cleanup) == 0);
    CHILD_CHECK(pthread_key_create(&exit_key, destroy_key) == 0);
    tls_sentinel = 101;
    CHILD_CHECK(pthread_setspecific(exit_key, (void *)1) == 0);
    if (mode >= JOINABLE) {
        pthread_attr_t attr; pthread_t worker;
        CHILD_CHECK(pthread_attr_init(&attr) == 0);
        if (mode == DETACHED)
            CHILD_CHECK(pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED) == 0);
        CHILD_CHECK(pthread_create(&worker, &attr, exit_worker, NULL) == 0);
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
    unsigned workers = test == DESCENDANT ? 2 : (test >= JOINABLE && test != PREEXITED);
    unsigned destructors = test == DESCENDANT ? 2 :
        (test == JOINABLE || test == DETACHED || test == PREEXITED || test == MASKED_SIGNAL);
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
        for (int test = 0; test < CASES; test++) if (run_exit_case(test)) return -1;
    puts("threadstress: main-exit ok");
    return 0;
}

/* Separate known-failure audit; not part of the positive all-tests suite. */
static void *join_initial_worker(void *initial) {
    void *value = NULL;
    int error = pthread_join((pthread_t)initial, &value);
    printf("threadstress: main-join-audit join=%d value=%p expected=0/0x9876\n", error, value);
    if (error || value != (void *)(uintptr_t)0x9876) {
        puts("threadstress: main-join-audit failed");
        _exit(1);
    }
    puts("threadstress: main-join-audit ok\nthreadstress: ok");
    return NULL;
}
int posix_main_join_audit(void) {
    pthread_t worker;
    if (pthread_create(&worker, NULL, join_initial_worker, pthread_self())) return 1;
    pthread_exit((void *)(uintptr_t)0x9876);
}
