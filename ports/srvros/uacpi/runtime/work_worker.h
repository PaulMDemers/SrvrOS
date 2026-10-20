#ifndef SRVROS_ACPI_WORK_WORKER_H
#define SRVROS_ACPI_WORK_WORKER_H

#include "work_queue.h"
#include <srvros/scheduler.h>

/* CPU0 only. Zero-initialize before first start; owner serializes lifecycle.
 * Uses an ordinary kernel worker unless a caller explicitly supplies an audited
 * I/O eligibility predicate. The predicate and context must outlive retirement. */
struct acpi_work_worker {
    struct acpi_work_queue work;
    struct scheduler_wait_queue ready, progress;
    uint64_t identity, completed;
    scheduler_wait_condition_fn io_eligible;
    void *io_context;
    bool started, stopping, stopped;
};

bool acpi_worker_start(struct acpi_work_worker *);
/* Caller must audit every callback/dependency for kernel-only I/O coexistence.
 * The predicate runs with IRQs masked; it must not allocate, block or access
 * devices or storage.
 * Returning false excludes even in-flight work; do not revoke eligibility until
 * that work can safely pause. NULL is refused, not a blanket exemption. */
bool acpi_worker_start_io(struct acpi_work_worker *, scheduler_wait_condition_fn,
    void *context);
/* Allocation-free, IRQ-safe. Item lifetime follows work_queue.h. */
bool acpi_worker_enqueue(struct acpi_work_worker *, struct acpi_work_item *,
    uacpi_work_type, uacpi_work_handler, uacpi_handle);
/* Caller must first synchronize its IRQ producers. No IRQ/source management is
 * implicit here. Drain includes callback-scheduled work; it never cancels it.
 * Finite millisecond timeout; zero is a poll. Cannot wait from this worker. */
bool acpi_worker_drain(struct acpi_work_worker *, uint64_t timeout_ms);
/* Requires an idle queue before closing enqueue admission. Owner must exclude
 * other callers/producers before freeing or restarting. On timeout the stop
 * remains requested: retry until success before freeing worker storage. */
bool acpi_worker_stop(struct acpi_work_worker *, uint64_t timeout_ms);

#endif
