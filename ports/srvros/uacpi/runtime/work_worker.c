#include "work_worker.h"
#include <srvros/timer.h>

static uint64_t irq_save(void) {
#ifdef SRVROS_ACPI_WORKER_HOST_TEST
    extern uint64_t acpi_worker_test_flags;
    uint64_t flags = acpi_worker_test_flags;
    acpi_worker_test_flags &= ~(1ull << 9);
    return flags;
#else
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_ACPI_WORKER_HOST_TEST
    extern uint64_t acpi_worker_test_flags;
    acpi_worker_test_flags = flags;
#else
    if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
#endif
}

static bool work_ready(void *arg) {
    struct acpi_work_worker *worker = arg;
    return worker->stopping || worker->work.head;
}

static void worker_main(void *arg) {
    struct acpi_work_worker *worker = arg;
    uint64_t flags = irq_save();
    worker->identity = scheduler_current_thread_id();
    irq_restore(flags);
    for (;;) {
        struct acpi_work_call call;
        flags = irq_save();
        if (acpi_work_take(&worker->work, &call)) {
            irq_restore(flags);
            call.handler(call.context); /* May requeue or free original item. */
            flags = irq_save();
            acpi_work_complete(&worker->work);
            if (worker->completed != UINT64_MAX) worker->completed++;
            scheduler_wake_all(&worker->progress);
            irq_restore(flags);
            scheduler_yield(); /* Even a self-rescheduling callback yields. */
            continue;
        }
        if (worker->stopping) {
            worker->stopped = true;
            scheduler_wake_all(&worker->progress);
            /* Return to the scheduler trampoline with IRQs still masked. An I/O
             * eligibility callback holds this worker pointer until the thread
             * is dead; do not let a waiter reclaim it between publication and
             * the trampoline's terminal exit. No worker-state access follows. */
            return;
        }
        irq_restore(flags);
        if (!scheduler_wait(&worker->ready, work_ready, worker)) scheduler_yield();
    }
}

static bool io_ready(void *arg) {
    struct acpi_work_worker *worker=arg;
    return worker->started && !worker->stopped && worker->io_eligible &&
        worker->io_eligible(worker->io_context);
}

static bool start(struct acpi_work_worker *worker,
    scheduler_wait_condition_fn eligible, void *context) {
    if (!worker || !scheduler_current_thread_id()) return false;
    uint64_t flags = irq_save();
    if (worker->started && !worker->stopped) {
        irq_restore(flags);
        return false;
    }
    *worker = (struct acpi_work_worker){.started = true,
        .io_eligible=eligible,.io_context=context};
    bool ok = eligible ? scheduler_spawn_io_service("acpi-work",worker_main,worker,io_ready) :
        scheduler_spawn("acpi-work", worker_main, worker);
    if (!ok) *worker = (struct acpi_work_worker){0};
    irq_restore(flags);
    return ok;
}

bool acpi_worker_start(struct acpi_work_worker *worker) {
    return start(worker,NULL,NULL);
}
bool acpi_worker_start_io(struct acpi_work_worker *worker,
    scheduler_wait_condition_fn eligible, void *context) {
    return eligible && start(worker,eligible,context);
}

bool acpi_worker_enqueue(struct acpi_work_worker *worker, struct acpi_work_item *item,
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle context) {
    if (!worker) return false;
    uint64_t flags = irq_save();
    bool ok = worker->started && !worker->stopping &&
        acpi_work_enqueue(&worker->work, item, type, handler, context);
    if (ok) scheduler_wake_all(&worker->ready);
    irq_restore(flags);
    return ok;
}

static bool is_idle(void *arg) {
    return acpi_work_idle(&((struct acpi_work_worker *)arg)->work);
}

static bool is_stopped(void *arg) {
    return ((struct acpi_work_worker *)arg)->stopped;
}

static bool wait_until(struct acpi_work_worker *worker,
    scheduler_wait_condition_fn ready, uint64_t timeout_ms) {
    uint64_t flags = irq_save();
    if (!worker->started || (worker->identity &&
        worker->identity == scheduler_current_thread_id())) {
        irq_restore(flags);
        return false;
    }
    if (ready(worker)) { irq_restore(flags); return true; }
    if (!timeout_ms || !(flags & (1ull << 9)) || !timer_reference_ready()) {
        irq_restore(flags);
        return false;
    }
    uint64_t now = timer_monotonic_us();
    if (timeout_ms > UINT64_MAX / 1000 || now > UINT64_MAX - timeout_ms * 1000) {
        irq_restore(flags);
        return false;
    }
    uint64_t end = now + timeout_ms * 1000;
    for (;;) {
        if (ready(worker)) { irq_restore(flags); return true; }
        now = timer_monotonic_us();
        if (now >= end) { irq_restore(flags); return false; }
        uint64_t left = end - now;
        uint64_t ticks = left / 10000 + (left % 10000 != 0) + 1;
        uint64_t due = timer_ticks();
        due = ticks > UINT64_MAX - due ? UINT64_MAX : due + ticks;
        irq_restore(flags);
        if (!scheduler_wait_timeout(&worker->progress, ready, worker, due)) return false;
        flags = irq_save();
    }
}

bool acpi_worker_drain(struct acpi_work_worker *worker, uint64_t timeout_ms) {
    return worker && wait_until(worker, is_idle, timeout_ms);
}

bool acpi_worker_stop(struct acpi_work_worker *worker, uint64_t timeout_ms) {
    if (!worker) return false;
    uint64_t flags = irq_save();
    if (!worker->started || !is_idle(worker) || (worker->identity &&
        worker->identity == scheduler_current_thread_id())) {
        irq_restore(flags);
        return false;
    }
    worker->stopping = true;
    scheduler_wake_all(&worker->ready);
    irq_restore(flags);
    return wait_until(worker, is_stopped, timeout_ms);
}
