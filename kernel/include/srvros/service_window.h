#ifndef SRVROS_SERVICE_WINDOW_H
#define SRVROS_SERVICE_WINDOW_H
#include <srvros/scheduler.h>
#include <stdint.h>
/* CPU0 audited private-buffer work. Keep cancellation and unsafe entries gated
 * until the caller has released all temporary resources. Checkpoint only after
 * restoring the scheduler-recorded address space and completing a bounded unit.
 * A foreign owner or early boot retains the existing synchronous fallback: do
 * not yield a private allocation across someone else's gate being retired. */
struct service_window { uint64_t flags;bool held; };
#ifdef SRVROS_SERVICE_WINDOW_HOST_TEST
uint64_t service_window_test_irq_save(void);
void service_window_test_irq_restore(uint64_t);
#endif
static inline struct service_window service_window_begin(void) {
    uint64_t flags;
#ifdef SRVROS_SERVICE_WINDOW_HOST_TEST
    flags=service_window_test_irq_save();
#else
    __asm__ volatile("pushfq; popq %0; cli":"=r"(flags)::"memory");
#endif
    return (struct service_window){flags,scheduler_io_begin()};
}
static inline void service_window_checkpoint(struct service_window *window) {
    if(window->held)scheduler_io_checkpoint();
}
static inline void service_window_end(struct service_window window) {
    if(window.held)scheduler_io_end();
#ifdef SRVROS_SERVICE_WINDOW_HOST_TEST
    service_window_test_irq_restore(window.flags);
#else
    if(window.flags&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
#endif
