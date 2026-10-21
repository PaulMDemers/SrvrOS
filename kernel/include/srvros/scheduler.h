#ifndef SRVROS_SCHEDULER_H
#define SRVROS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>

struct fpu_state;

typedef void (*scheduler_thread_fn)(void *arg);
typedef bool (*scheduler_wait_condition_fn)(void *arg);

struct scheduler_wait_queue {
    uint64_t waiters;
};

struct scheduler_stats {
    uint64_t sampled_ticks, idle_ticks, switches, preemptions;
};

void scheduler_init(void);
bool scheduler_spawn(const char *name, scheduler_thread_fn entry, void *arg);
/* Service callbacks must not access storage, VFS or user mappings, or wait for
 * ordinary application threads. Eligibility is queried with IRQs masked. */
bool scheduler_spawn_io_service(const char *name, scheduler_thread_fn entry,
    void *arg, scheduler_wait_condition_fn eligible);
void scheduler_enable_io_services(void);
/* Begin/end/yield require IRQs masked. The owner, eligible service threads and
 * audited console/userspace paths in other processes may run until end.
 * All other kernel entries wait; cancellation is deferred over this scope. */
bool scheduler_io_begin(void);
bool scheduler_io_active(void);
bool scheduler_io_busy(void); /* Another thread owns storage state. IRQs masked. */
void scheduler_io_checkpoint(void);
void scheduler_io_end(void);
bool scheduler_io_yield(void);
void scheduler_io_wait(void);
void scheduler_io_set_user_safe(bool safe);
void scheduler_user_enter(void);
void scheduler_yield(void);
/* CPU0 fatal service boundary only, IRQs already masked, NOT IRQ context.
 * Atomically retain current+peer kernel service slots/stacks until reboot.
 * Refuses userspace, idle, stale identities, a storage owner or no idle fallback.
 * On success only IRQ-masked terminal bookkeeping may precede yielding; caller
 * must never re-enable IRQs or return on the retained stack.
 * Does not release device resources, locks or memory. No resume/free operation. */
bool scheduler_quarantine_service_pair(uint64_t caller, uint64_t peer);
bool scheduler_service_pair_quarantined(uint64_t caller, uint64_t peer);
void scheduler_exit_current(void) __attribute__((noreturn));
void scheduler_preempt_tick(void);
void scheduler_kill_user_threads(void *process, void *except_user_thread_context);
bool scheduler_wait(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg);
bool scheduler_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg,
    uint64_t deadline_ticks);
void scheduler_wake_all(struct scheduler_wait_queue *queue);
uint64_t scheduler_wake_count(struct scheduler_wait_queue *queue, uint64_t max_count);
void scheduler_set_user_context(void *process, uint64_t address_space, uint64_t kernel_stack_top);
void scheduler_set_user_context_fpu(void *process,
    uint64_t address_space,
    uint64_t kernel_stack_top,
    struct fpu_state *user_fpu);
void scheduler_clear_user_context(void);
void *scheduler_current_user_context(void);
void scheduler_set_user_thread_context(void *context);
void *scheduler_current_user_thread_context(void);
/* Nonzero boot-lifetime identity; zero before scheduler initialization. */
uint64_t scheduler_current_thread_id(void);
uint64_t scheduler_thread_count(void);
uint64_t scheduler_preemptions(void);
void scheduler_print_status(void);
void scheduler_snapshot(struct scheduler_stats *out);
bool scheduler_start_idle_worker(void);

#endif
