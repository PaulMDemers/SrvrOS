#ifndef SRVROS_SEMAPHORE_H
#define SRVROS_SEMAPHORE_H
#include <srvros/scheduler.h>
#include <stdbool.h>
#include <stdint.h>
/* CPU0 counting semaphore. All state is serialized against local IRQs.
 * Not an SMP primitive. Signal is allocation-free and safe in IRQ context.
 * Wait is for ordinary kernel workers, never an IRQ, storage service callback
 * or cancellable user task (which would need cancellation cleanup). */
struct kernel_semaphore {
    struct scheduler_wait_queue queue;
    uint64_t units;
    uint32_t waiters;
    bool closed;
};
enum semaphore_result { SEMAPHORE_OK, SEMAPHORE_TIMEOUT, SEMAPHORE_CLOSED, SEMAPHORE_UNAVAILABLE };
#define SEMAPHORE_WAIT_FOREVER UINT64_MAX
void semaphore_init(struct kernel_semaphore *,uint64_t units);
enum semaphore_result semaphore_wait(struct kernel_semaphore *,uint64_t timeout_ms);
bool semaphore_signal(struct kernel_semaphore *);
void semaphore_close(struct kernel_semaphore *);
/* After excluding new callers and closing, wait until this is true before
 * freeing owner storage. Close wakes waiters; it does not free the object. */
bool semaphore_drained(struct kernel_semaphore *);
void semaphore_test_run(void); /* Explicit diagnostic, not a firmware service. */
#endif
