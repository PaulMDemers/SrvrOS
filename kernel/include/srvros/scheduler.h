#ifndef SRVROS_SCHEDULER_H
#define SRVROS_SCHEDULER_H

#include <stdbool.h>
#include <stdint.h>
struct srv_sched_trace;

struct fpu_state;

typedef void (*scheduler_thread_fn)(void *arg);
typedef bool (*scheduler_wait_condition_fn)(void *arg);

struct scheduler_wait_queue {
    uint64_t waiters; /* Count, not a bitmap. Membership is owned by thread records. */
};

struct scheduler_stats {
    uint64_t sampled_ticks, idle_ticks, switches, preemptions;
};

void scheduler_init(void);
bool scheduler_spawn(const char *name, scheduler_thread_fn entry, void *arg);
/* Prepared user-thread entry only: callback must use the initialized process,
 * thread/TLS/FPU context and enter userspace without allocation or cleanup.
 * Publishes that context atomically with READY, preserving normal syscall gates. */
bool scheduler_spawn_user_thread(const char *name,scheduler_thread_fn entry,void *thread,struct fpu_state *fpu,uint64_t fs_base);
/* Service callbacks must not access storage, VFS or user mappings, or wait for
 * ordinary application threads. Eligibility is queried with IRQs masked. */
bool scheduler_spawn_io_service(const char *name, scheduler_thread_fn entry,
    void *arg, scheduler_wait_condition_fn eligible);
void scheduler_enable_io_services(void);
/* Begin/end/yield require IRQs masked. The owner, eligible service threads and
 * audited console/userspace paths in other processes may run until end.
 * All other kernel entries wait; cancellation is deferred over this scope. */
bool scheduler_io_begin(void);
/* IRQ-masked, new private child-image construction only. The caller has copied
 * its path into kernel storage and touches no parent descriptors/mappings or
 * user buffers until end. Retain global serialization/cancellation and exclude
 * the image's process identity; audited caller siblings may service UI. Refuse
 * nested/foreign ownership, absent caller or an image with a scheduler thread.
 * Failure requires the ordinary process service scope, never a relaxed gate. */
bool scheduler_io_begin_process_image(void *image);
bool scheduler_io_active(void);
bool scheduler_io_busy(void); /* Another thread owns storage state. IRQs masked. */
/* IRQ-masked proof for bounded atomic process-private memory work only. Does
 * not acquire/release ownership or permit yielding, file or process teardown.
 * Allocating callers must separately bound and audit IRQ-safe PMM/VMM work.
 * Refuse siblings, worker contexts, image/owner aliases and unbounded scans. */
bool scheduler_io_process_independent(void *process);
/* IRQ-masked bounded atomic retirement by a helper, siblings blocked on the supplied audited wait queue only.
 * No allocation, callbacks, service windows or yield until mutation completes. */
bool scheduler_io_process_helper_memory(void *process,struct scheduler_wait_queue *parked);
/* IRQ-masked proof only: no live scheduler/private-image reference. */
bool scheduler_io_process_retired(void *process);
void scheduler_io_checkpoint(void);
void scheduler_io_end(void);
/* Optional CPU0 diagnostics. Reset refuses active ownership or an unavailable
 * reference clock. Printing copies bounded history with IRQs masked, then emits
 * outside ownership; never changes admission or releases an I/O scope. */
bool scheduler_io_observe_reset(void);
void scheduler_print_io_owners(void);
bool scheduler_io_yield(void);
/* IRQ-masked I/O owner only, recorded address space restored. Retains the I/O
 * gate and DMA ownership while sleeping until condition/wake or next tick.
 * Refuses early boot, missing timer/fallback, foreign owners or longer waits.
 * Returns with IRQs masked; false requires the caller's bounded poll fallback. */
bool scheduler_io_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,void *arg,uint64_t deadline_ticks);
void scheduler_io_wait(void);
/* A nonblocking operation deferred by another I/O owner requests a later
 * quiescent turn without blocking, waking sleepers or widening eligibility. */
void scheduler_io_request_turn(void);
/* Completed userspace syscall only: no mutable operation or lock remains,
 * caller is marked user-safe, IRQs masked. Give one pending gate contender a
 * turn before this caller can submit another operation. Never releases I/O. */
void scheduler_io_handoff(void);
void scheduler_io_set_user_safe(bool safe);
void scheduler_user_enter(void);
void scheduler_account_kernel_enter(void);
void scheduler_account_now(void);
void scheduler_yield(void);
/* CPU0 fatal service boundary, IRQs already masked. Ordinary thread boundary
 * or a terminal IRQ exit hook AFTER EOI, every completion callback and registry
 * bookkeeping have drained. NEVER from a handler or completion callback.
 * Atomically retain current+peer kernel service slots/stacks until reboot.
 * Refuses userspace, idle, stale identities, a storage owner or no idle fallback.
 * On success only IRQ-masked terminal bookkeeping may precede yielding; caller
 * must never re-enable IRQs or return on the retained stack.
 * Does not release device resources, locks or memory. No resume/free operation. */
bool scheduler_quarantine_service_pair(uint64_t caller, uint64_t peer);
/* CPU0, IRQs masked. Retain two READY/BLOCKED kernel services, neither current
 * nor a storage owner. Safe at an IRQ entry guard before acquiring service locks;
 * leaves the interrupted task untouched and MUST return through normal IRQ/EOI
 * completion. No switching, cleanup or callback unwinding is performed. */
bool scheduler_quarantine_noncurrent_services(uint64_t first, uint64_t second);
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
/* Bounded timestamped per-lifetime counters; diagnostic capture only. */
void scheduler_print_activity(void);
void scheduler_snapshot(struct scheduler_stats *out);
bool scheduler_trace(unsigned operation,struct srv_sched_trace *out);
bool scheduler_start_idle_worker(void);

#endif
