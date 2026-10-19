#ifndef SRVROS_ACPI_WORK_QUEUE_H
#define SRVROS_ACPI_WORK_QUEUE_H

#include <stdbool.h>
#include <stdint.h>
#include <uacpi/kernel_api.h>

/* Runtime foundation, not yet bound to uACPI's kernel callbacks. Zero-initialize
 * queues and items. The adapter must serialize every operation (including idle
 * checks) with the same IRQ-safe lock. No operation allocates or invokes AML.
 * One CPU0 worker will serve both work types; scheduling can occur in an IRQ. */
struct acpi_work_item {
    struct acpi_work_item *next;
    uacpi_work_handler handler;
    uacpi_handle context;
    uacpi_work_type type;
    bool pending;
};

struct acpi_work_queue {
    struct acpi_work_item *head, *tail;
    uint64_t in_flight;
};

/* Detached invocation: the callback may requeue or free its original item.
 * The worker must not retain/access that item after taking the invocation. */
struct acpi_work_call {
    uacpi_work_handler handler;
    uacpi_handle context;
    uacpi_work_type type;
};

bool acpi_work_enqueue(struct acpi_work_queue *, struct acpi_work_item *,
    uacpi_work_type, uacpi_work_handler, uacpi_handle);
bool acpi_work_take(struct acpi_work_queue *, struct acpi_work_call *);
/* Call exactly once after each detached invocation returns, even if it queues
 * more work. The callback itself runs outside the queue's lock. */
bool acpi_work_complete(struct acpi_work_queue *);
bool acpi_work_idle(const struct acpi_work_queue *);

/* Freeing an item while pending is forbidden. Before draining this queue during
 * teardown, disable/uninstall and synchronize its interrupt producers. Queue
 * idleness alone is not uacpi_kernel_wait_for_work_completion's full contract. */
#endif
