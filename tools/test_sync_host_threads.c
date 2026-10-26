/* Compile separately with HOST headers: isolate native pthread ABI from SrvrOS. */
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>

struct host_call { uint64_t entry, arg; };
static _Thread_local int worker;
int sync_host_is_worker(void) { return worker; }
static void *host_entry(void *arg) {
    struct host_call *call = arg;
    worker = 1;
    ((void (*)(uint64_t))(uintptr_t)call->entry)(call->arg);
    abort();
}
int sync_host_run_worker(uint64_t entry, uint64_t arg) {
    struct host_call call = {entry, arg};
    pthread_t thread;
    int error = pthread_create(&thread, NULL, host_entry, &call);
    return error ? error : pthread_join(thread, NULL);
}
_Noreturn void sync_host_exit_worker(void) { pthread_exit(NULL); abort(); }
