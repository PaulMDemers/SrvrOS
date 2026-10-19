#include "../ports/srvros/uacpi/runtime/work_queue.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

static unsigned calls;
static void count(void *context) { calls += *(unsigned *)context; }

struct self_work {
    struct acpi_work_item item;
    struct acpi_work_queue *queue;
    unsigned remaining;
};

static void requeue_and_free(void *context) {
    struct self_work *work = context;
    assert(!work->item.pending && !acpi_work_idle(work->queue));
    calls++;
    if (--work->remaining) {
        assert(acpi_work_enqueue(work->queue, &work->item,
            UACPI_WORK_NOTIFICATION, requeue_and_free, work));
    } else {
        /* A worker that touches the original item after invoking its callback
         * will be caught by ASan. Completion uses only the queue. */
        free(work);
    }
}

int main(void) {
    struct acpi_work_queue queue = {0}, other = {0};
    struct acpi_work_item items[3] = {{0}};
    struct acpi_work_call call;
    unsigned values[] = {1, 10, 100};
    assert(acpi_work_idle(&queue) && !acpi_work_idle(NULL));
    assert(!acpi_work_take(&queue, &call) && !acpi_work_complete(&queue));
    assert(!acpi_work_enqueue(NULL, items, UACPI_WORK_NOTIFICATION, count, values));
    assert(!acpi_work_enqueue(&queue, NULL, UACPI_WORK_NOTIFICATION, count, values));
    assert(!acpi_work_enqueue(&queue, items, UACPI_WORK_NOTIFICATION, NULL, values));
    assert(!acpi_work_enqueue(&queue, items, (uacpi_work_type)99, count, values));
    for (unsigned i = 0; i < 3; i++) {
        assert(acpi_work_enqueue(&queue, &items[i],
            i == 1 ? UACPI_WORK_GPE_EXECUTION : UACPI_WORK_NOTIFICATION,
            count, &values[i]));
        assert(!acpi_work_enqueue(&queue, &items[i], UACPI_WORK_NOTIFICATION, count, values));
        assert(!acpi_work_enqueue(&other, &items[i], UACPI_WORK_NOTIFICATION, count, values));
    }
    assert(!acpi_work_take(&queue, NULL) && queue.head == &items[0]);
    queue.in_flight = UINT64_MAX;
    assert(!acpi_work_take(&queue, &call) && items[0].pending);
    queue.in_flight = 0;
    for (unsigned i = 0; i < 3; i++) {
        assert(acpi_work_take(&queue, &call));
        assert(call.context == &values[i] && !items[i].pending);
        assert(call.type == (i == 1 ? UACPI_WORK_GPE_EXECUTION : UACPI_WORK_NOTIFICATION));
        call.handler(call.context);
    }
    assert(calls == 111 && queue.in_flight == 3 && !queue.head && !queue.tail);
    assert(!acpi_work_idle(&queue));
    for (unsigned i = 0; i < 3; i++) assert(acpi_work_complete(&queue));
    assert(acpi_work_idle(&queue) && !acpi_work_complete(&queue));

    /* Reuse an item while its previous invocation is in flight. Both calls
     * retain their original context even though the item gets overwritten. */
    assert(acpi_work_enqueue(&queue, items, UACPI_WORK_NOTIFICATION, count, &values[0]));
    assert(acpi_work_take(&queue, &call));
    assert(acpi_work_enqueue(&queue, items, UACPI_WORK_GPE_EXECUTION, count, &values[1]));
    call.handler(call.context);
    assert(acpi_work_complete(&queue) && !acpi_work_idle(&queue));
    assert(acpi_work_take(&queue, &call) && call.type == UACPI_WORK_GPE_EXECUTION);
    call.handler(call.context);
    assert(acpi_work_complete(&queue) && acpi_work_idle(&queue) && calls == 122);

    struct self_work *work = calloc(1, sizeof(*work));
    assert(work); work->queue = &queue; work->remaining = 1000;
    assert(acpi_work_enqueue(&queue, &work->item, UACPI_WORK_GPE_EXECUTION,
        requeue_and_free, work));
    while (acpi_work_take(&queue, &call)) {
        call.handler(call.context);
        assert(acpi_work_complete(&queue));
    }
    assert(calls == 1122 && acpi_work_idle(&queue));
    puts("acpi-work: PASS FIFO/type retention, duplicate/cross-queue refusal, detached invocation, active drain, requeue and callback-owned free");
}
