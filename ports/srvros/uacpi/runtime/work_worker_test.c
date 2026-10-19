#include "work_worker.h"
#include <srvros/acpi_work_test.h>
#include <srvros/console.h>
#include <srvros/heap.h>
#include <srvros/irq_handlers.h>
#include <srvros/semaphore.h>
#include <srvros/timer.h>

static struct {
    struct acpi_work_worker worker;
    struct acpi_work_item *item;
    struct kernel_semaphore entered, gate;
    uint64_t calls, interrupts, queued, owner;
    bool busy, ok;
} test;

static void callback(uacpi_handle context) {
    test.ok &= context == &test && scheduler_current_thread_id() != test.owner;
    test.ok &= !acpi_worker_drain(&test.worker, 0);
    test.ok &= !acpi_worker_stop(&test.worker, 0);
    test.calls++;
    if (test.calls == 1) {
        semaphore_signal(&test.entered);
        test.ok &= semaphore_wait(&test.gate, 2000) == SEMAPHORE_OK;
    }
    if (test.calls < 1000) {
        test.ok &= acpi_worker_enqueue(&test.worker, test.item,
            test.calls & 1 ? UACPI_WORK_NOTIFICATION : UACPI_WORK_GPE_EXECUTION,
            callback, &test);
    } else {
        kfree(test.item); /* Worker must not touch the item after callback. */
        test.item = NULL;
    }
}

static bool interrupt_producer(void *context) {
    uint64_t flags;
    __asm__ volatile("pushfq; popq %0" : "=r"(flags));
    test.ok &= context == &test && !(flags & (1ull << 9));
    test.interrupts++;
    if (acpi_worker_enqueue(&test.worker, test.item,
        UACPI_WORK_GPE_EXECUTION, callback, &test)) test.queued++;
    return true;
}

void acpi_work_test_run(void) {
    if (test.busy) { console_write("acpi-work-test: FAIL previous worker not drained\n"); return; }
    test.busy = true;
    test.ok = true;
    test.calls = test.interrupts = test.queued = 0;
    test.owner = scheduler_current_thread_id();
    semaphore_init(&test.entered, 0);
    semaphore_init(&test.gate, 0);
    test.item = kcalloc(1, sizeof(*test.item));
    bool started = test.item && acpi_worker_start(&test.worker);
    uint64_t token = started ? irq_handler_register(15, interrupt_producer, &test) : 0;
    bool ok = token && !acpi_worker_start(&test.worker);
    if (ok) {
        /* Synthetic IRQ entry tests the actual ISR/registry path with IF clear.
         * No I/O APIC route is changed and no firmware event is enabled. */
        uint64_t flags;
        __asm__ volatile("pushfq; popq %0; cli" : "=r"(flags) :: "memory");
        for (unsigned i = 0; i < 8; i++) __asm__ volatile("int $0x2f" ::: "memory");
        if (flags & (1ull << 9)) __asm__ volatile("sti" ::: "memory");
        ok = semaphore_wait(&test.entered, 1000) == SEMAPHORE_OK;
    }
    uint64_t begin = timer_monotonic_us();
    if (ok) ok = !acpi_worker_drain(&test.worker, 15) &&
        timer_monotonic_us() - begin >= 15000 && !acpi_worker_stop(&test.worker, 0);
    bool removed = !token || irq_handler_remove(token); /* Synthetic producer quiesced. */
    semaphore_signal(&test.gate);
    bool drained = !started || acpi_worker_drain(&test.worker, 5000);
    uint64_t identity = test.worker.identity, completed = test.worker.completed;
    bool stopped = !started || (drained && acpi_worker_stop(&test.worker, 1000));
    ok = ok && removed && drained && stopped && test.ok && test.calls == 1000 &&
        test.interrupts == 8 && test.queued == 1 && completed == 1000 &&
        !acpi_worker_enqueue(&test.worker, NULL, UACPI_WORK_NOTIFICATION, callback, &test);
    if (stopped && removed) {
        if (test.item) kfree(test.item);
        test.item = NULL;
        test.busy = false;
    }
    console_printf("acpi-work-test: %s owner=%u worker=%u calls=%u interrupts=%u queued=%u stopped=%s\n",
        ok ? "PASS" : "FAIL", test.owner, identity, test.calls, test.interrupts,
        test.queued, stopped ? "yes" : "NO");
}
