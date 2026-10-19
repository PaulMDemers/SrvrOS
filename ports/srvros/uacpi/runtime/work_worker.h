#ifndef SRVROS_ACPI_WORK_WORKER_H
#define SRVROS_ACPI_WORK_WORKER_H

#include "work_queue.h"
#include <srvros/scheduler.h>

/* CPU0 only. Zero-initialize before first start; owner serializes lifecycle.
 * Uses an ordinary kernel worker, never the storage I/O service exemption. */
struct acpi_work_worker {
    struct acpi_work_queue work;
    struct scheduler_wait_queue ready, progress;
    uint64_t identity, completed;
    bool started, stopping, stopped;
};

bool acpi_worker_start(struct acpi_work_worker *);
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
