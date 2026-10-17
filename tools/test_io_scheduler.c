#include <assert.h>
#include <stdio.h>
#define SRVROS_SCHEDULER_HOST_TEST
#include "../kernel/src/scheduler.c"
static uint64_t actual_space=1;
uint64_t vmm_current_address_space(void) { return actual_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
static bool ready(void *arg) { return arg != NULL; }
int main(void) {
    assert(!scheduler_io_begin());
    initialized = true;
    assert(!scheduler_io_begin()); /* Early boot must not enable IRQs. */
    scheduler_enable_io_services();
    current_thread = 3;
    threads[3].state = THREAD_RUNNING;
    threads[4].state = THREAD_READY;
    threads[4].io_eligible = ready;
    threads[4].arg = (void *)1;
    threads[5].state = THREAD_READY; /* Ordinary application. */
    threads[6].state = THREAD_READY;
    threads[6].io_eligible = ready;
    threads[6].arg = (void *)1;
    threads[6].user_context = (void *)1; /* Never treat userspace as a service. */
    assert(scheduler_io_begin() && scheduler_io_active());
    threads[3].address_space=2;
    assert(!scheduler_io_yield()); /* A temporary CR3 cannot cross a context switch. */
    actual_space=2; assert(scheduler_io_yield());
    assert(io_thread_allowed(3) && io_thread_allowed(4));
    assert(!io_thread_allowed(5) && !io_thread_allowed(6));
    assert(has_ready_thread_except(3));
    threads[4].arg = NULL;
    assert(!has_ready_thread_except(3));
    assert(scheduler_io_begin() && io_depth == 2);
    current_thread = 4;
    assert(!scheduler_io_begin()); scheduler_io_end(); assert(io_depth == 2);
    current_thread = 3;
    scheduler_io_end(); assert(scheduler_io_active());
    scheduler_io_end(); assert(!scheduler_io_active() && io_thread_allowed(5));
    scheduler_io_end(); assert(io_depth == 0);
    switching = true; assert(!scheduler_io_begin());
    puts("io-scheduler-host: PASS (boot gating, owner/nesting, service eligibility, task isolation)");
}
