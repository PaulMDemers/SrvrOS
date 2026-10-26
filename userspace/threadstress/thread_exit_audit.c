/* Explicit known-failure audit, excluded from the positive all-tests suite. */
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <time.h>

static unsigned exit_worker_ready;
static void *exit_worker(void *unused) {
    (void)unused;
    __atomic_store_n(&exit_worker_ready, 1, __ATOMIC_RELEASE);
    struct timespec delay = {.tv_sec = 1};
    if (nanosleep(&delay, NULL)) return (void *)1;
    puts("threadstress: main-exit-audit worker survived");
    puts("threadstress: main-exit-audit ok\nthreadstress: ok");
    return NULL;
}
int posix_main_exit_audit(void) {
    pthread_t worker;
    if (pthread_create(&worker, NULL, exit_worker, NULL)) return 1;
    while (!__atomic_load_n(&exit_worker_ready, __ATOMIC_ACQUIRE)) sched_yield();
    puts("threadstress: main-exit-audit initial thread exiting with live worker");
    pthread_exit(NULL);
}
