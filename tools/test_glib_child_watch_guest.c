/* Native child-watch prerequisite, not a GLib implementation or test. */
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <spawn.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define CHECK(phase, condition) do { \
    if (!(condition)) { \
        printf("glib-child-watch: failure %s line=%d errno=%d\n", (phase), __LINE__, errno); \
        return 1; \
    } \
} while (0)

static int wake_pipe[2], done_pipe[2];
static atomic_int ready, watched_pid, worker_error, worker_wakes, worker_status;
static volatile sig_atomic_t handler_count, handler_error;

static void child_signal(int signum) {
    int saved = errno;
    const char byte = 's';
    if (signum != SIGCHLD || write(wake_pipe[1], &byte, 1) != 1) handler_error = 1;
    handler_count++;
    errno = saved;
}

static int64_t now_ms(void) {
    struct timespec now;
    if (clock_gettime(CLOCK_MONOTONIC, &now) != 0) return -1;
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static int deadline_reached(int64_t deadline) {
    int64_t now = now_ms();
    return now < 0 || now >= deadline;
}

static void *worker(void *unused) {
    (void)unused;
    sigset_t mask;
    if (pthread_sigmask(SIG_SETMASK, NULL, &mask) != 0 || sigismember(&mask, SIGCHLD) != 1) {
        atomic_store(&worker_error, 1);
        atomic_store(&ready, 1);
        return NULL;
    }
    atomic_store(&ready, 1);
    int64_t deadline = now_ms();
    if (deadline < 0) { atomic_store(&worker_error, 8); return NULL; }
    deadline += 5000;
    int pid;
    unsigned attempts = 0;
    while ((pid = atomic_load(&watched_pid)) == 0) {
        if (++attempts > 1000) { atomic_store(&worker_error, 9); return NULL; }
        if (deadline_reached(deadline)) { atomic_store(&worker_error, 2); return NULL; }
        struct timespec pause = {0, 10000000};
        (void)nanosleep(&pause, NULL);
    }
    attempts = 0;
    while (!deadline_reached(deadline) && ++attempts <= 1000) {
        struct pollfd p = { .fd = wake_pipe[0], .events = POLLIN };
        int polled = poll(&p, 1, 100);
        if (polled < 0 && errno == EINTR) continue;
        if (polled < 0 || (p.revents & (POLLERR | POLLHUP | POLLNVAL))) {
            atomic_store(&worker_error, 3); return NULL;
        }
        if (polled == 0) continue;
        char bytes[32];
        ssize_t n = read(wake_pipe[0], bytes, sizeof(bytes));
        if (n <= 0) { atomic_store(&worker_error, 4); return NULL; }
        atomic_fetch_add(&worker_wakes, 1);
        int status = 0;
        pid_t result;
        do {
            result = waitpid((pid_t)pid, &status, WNOHANG);
        } while (result < 0 && errno == EINTR && !deadline_reached(deadline));
        if (result < 0) { atomic_store(&worker_error, 5); return NULL; }
        if (result == (pid_t)pid) {
            atomic_store(&worker_status, status);
            if (write(done_pipe[1], "d", 1) != 1) atomic_store(&worker_error, 6);
            return NULL;
        }
    }
    atomic_store(&worker_error, 7);
    return NULL;
}

static int child(const char *arg) {
    int status;
    struct timespec delay;
    if (strcmp(arg, "17") == 0) { status = 17; delay = (struct timespec){0, 200000000}; }
    else if (strcmp(arg, "42") == 0) { status = 42; delay = (struct timespec){0, 100000000}; }
    else return 90;
    while (nanosleep(&delay, &delay) != 0) if (errno != EINTR) return 91;
    return status;
}

int main(int argc, char **argv) {
    if (argc == 2) return child(argv[1]);
    CHECK("argc", argc == 1);
    /* The pseudo-eventfd implementation takes a mutex, so use a real pipe for
     * the signal handler's nonblocking write rather than claim signal safety. */
    CHECK("wake-pipe", pipe2(wake_pipe, O_NONBLOCK | O_CLOEXEC) == 0);
    CHECK("done-pipe", pipe2(done_pipe, O_NONBLOCK | O_CLOEXEC) == 0);
    struct sigaction action = {0}, old_action = {0}, observed = {0};
    action.sa_handler = child_signal;
    CHECK("signal-mask-init", sigemptyset(&action.sa_mask) == 0);
    action.sa_flags = SA_RESTART | SA_ONSTACK;
    errno = 0;
    CHECK("unsupported-flags", sigaction(SIGCHLD, &action, NULL) == -1 && errno == EINVAL);
    action.sa_flags = SA_RESTART;
    CHECK("signal-install", sigaction(SIGCHLD, &action, &old_action) == 0);
    CHECK("signal-query", sigaction(SIGCHLD, NULL, &observed) == 0 &&
        observed.sa_handler == child_signal && (observed.sa_flags & SA_RESTART));

    sigset_t all, previous;
    CHECK("fill-mask", sigfillset(&all) == 0);
    CHECK("block-for-worker", pthread_sigmask(SIG_SETMASK, &all, &previous) == 0);
    CHECK("caller-initial-mask", sigismember(&previous, SIGCHLD) == 0);
    pthread_t thread;
    CHECK("worker-create", pthread_create(&thread, NULL, worker, NULL) == 0);
    CHECK("restore-mask", pthread_sigmask(SIG_SETMASK, &previous, NULL) == 0);
    int64_t deadline = now_ms();
    CHECK("clock-ready", deadline >= 0);
    deadline += 5000;
    unsigned attempts = 0;
    while (!atomic_load(&ready) && !deadline_reached(deadline) && ++attempts <= 1000) {
        struct timespec pause = {0, 10000000};
        (void)nanosleep(&pause, NULL);
    }
    CHECK("worker-ready", atomic_load(&ready) && !atomic_load(&worker_error));

    char *watched_args[] = {"/glib-child-watch", "17", NULL};
    char *unwatched_args[] = {"/glib-child-watch", "42", NULL};
    char *environment[] = {NULL};
    pid_t watched = 0, unwatched = 0;
    CHECK("spawn-unwatched", posix_spawn(&unwatched, unwatched_args[0], NULL, NULL, unwatched_args, environment) == 0 && unwatched > 0);
    deadline = now_ms();
    CHECK("clock-unwatched-exit", deadline >= 0);
    deadline += 1500;
    while (handler_count == 0 && !deadline_reached(deadline))
        (void)poll(NULL, 0, 20);
    CHECK("unwatched-exited-before-watch", handler_count > 0 && !handler_error);
    CHECK("spawn-watched", posix_spawn(&watched, watched_args[0], NULL, NULL, watched_args, environment) == 0 &&
        watched > 0 && watched != unwatched);
    atomic_store(&watched_pid, watched);
    deadline = now_ms();
    CHECK("clock-watched", deadline >= 0);
    deadline += 5000;
    int done = 0;
    attempts = 0;
    while (!done && !deadline_reached(deadline) && ++attempts <= 1000) {
        struct pollfd p = { .fd = done_pipe[0], .events = POLLIN };
        int polled = poll(&p, 1, 100); /* Unmasked caller reaches signal checkpoints. */
        if (polled < 0 && errno == EINTR) continue;
        CHECK("done-poll", polled >= 0 && !(p.revents & (POLLERR | POLLHUP | POLLNVAL)));
        if (polled > 0) {
            char byte = 0;
            CHECK("done-read", read(done_pipe[0], &byte, 1) == 1 && byte == 'd');
            done = 1;
        }
    }
    CHECK("worker-complete", done && pthread_join(thread, NULL) == 0 && !atomic_load(&worker_error));
    CHECK("handler-wakeup", handler_count > 0 && !handler_error && atomic_load(&worker_wakes) > 0);
    int status = atomic_load(&worker_status);
    CHECK("watched-status", WIFEXITED(status) && WEXITSTATUS(status) == 17);
    errno = 0;
    CHECK("watched-reaped", waitpid(watched, &status, WNOHANG) == -1 && errno == ECHILD);

    deadline = now_ms();
    CHECK("clock-unwatched", deadline >= 0);
    deadline += 5000;
    pid_t result = 0;
    attempts = 0;
    while (result == 0 && !deadline_reached(deadline) && ++attempts <= 1000) {
        result = waitpid(unwatched, &status, WNOHANG);
        if (result < 0 && errno == EINTR) result = 0;
        if (result == 0) { struct pollfd p = { .fd = -1 }; (void)poll(&p, 0, 20); }
    }
    CHECK("unwatched-status", result == unwatched && WIFEXITED(status) && WEXITSTATUS(status) == 42);
    CHECK("restore-action", sigaction(SIGCHLD, &old_action, NULL) == 0);
    CHECK("close", close(wake_pipe[0]) == 0 && close(wake_pipe[1]) == 0 &&
        close(done_pipe[0]) == 0 && close(done_pipe[1]) == 0);
    puts("glib-child-watch: ok");
    return 0;
}
