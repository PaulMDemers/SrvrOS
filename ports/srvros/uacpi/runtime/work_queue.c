#include "work_queue.h"

bool acpi_work_enqueue(struct acpi_work_queue *queue, struct acpi_work_item *item,
    uacpi_work_type type, uacpi_work_handler handler, uacpi_handle context) {
    if (!queue || !item || !handler || item->pending ||
        (type != UACPI_WORK_GPE_EXECUTION && type != UACPI_WORK_NOTIFICATION)) {
        return false;
    }
    item->next = NULL;
    item->handler = handler;
    item->context = context;
    item->type = type;
    item->pending = true;
    if (queue->tail) queue->tail->next = item;
    else queue->head = item;
    queue->tail = item;
    return true;
}

bool acpi_work_take(struct acpi_work_queue *queue, struct acpi_work_call *call) {
    if (!queue || !call || !queue->head || queue->in_flight == UINT64_MAX) {
        return false;
    }
    struct acpi_work_item *item = queue->head;
    *call = (struct acpi_work_call){item->handler, item->context, item->type};
    queue->head = item->next;
    if (!queue->head) queue->tail = NULL;
    *item = (struct acpi_work_item){0};
    queue->in_flight++;
    return true;
}

bool acpi_work_complete(struct acpi_work_queue *queue) {
    if (!queue || !queue->in_flight) return false;
    queue->in_flight--;
    return true;
}

bool acpi_work_idle(const struct acpi_work_queue *queue) {
    return queue && !queue->head && !queue->in_flight;
}
