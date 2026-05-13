#include <srvros/gui.h>
#include <srvros/process.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GUI_MAX_QUEUES 16
#define GUI_QUEUE_DEPTH 64

struct gui_queue {
    bool used;
    uint64_t pid;
    uint64_t head;
    uint64_t count;
    struct gui_message messages[GUI_QUEUE_DEPTH];
};

static struct gui_queue queues[GUI_MAX_QUEUES];
static uint64_t server_pid;

static uint64_t irq_save(void) {
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
}

static void irq_restore(uint64_t flags) {
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
}

static struct gui_queue *queue_for_pid(uint64_t pid, bool create) {
    struct gui_queue *free_queue = NULL;
    for (uint64_t i = 0; i < GUI_MAX_QUEUES; i++) {
        if (queues[i].used && queues[i].pid == pid) {
            return &queues[i];
        }
        if (!queues[i].used && free_queue == NULL) {
            free_queue = &queues[i];
        }
    }

    if (!create || free_queue == NULL) {
        return NULL;
    }

    free_queue->used = true;
    free_queue->pid = pid;
    free_queue->head = 0;
    free_queue->count = 0;
    return free_queue;
}

static int64_t push_message(uint64_t target_pid, const struct gui_message *message) {
    struct gui_queue *queue = queue_for_pid(target_pid, true);
    if (queue == NULL || queue->count >= GUI_QUEUE_DEPTH) {
        return -1;
    }

    uint64_t index = (queue->head + queue->count) % GUI_QUEUE_DEPTH;
    queue->messages[index] = *message;
    queue->count++;
    return 0;
}

int64_t gui_register_server(void) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result;
    if (process == NULL) {
        return -1;
    }

    flags = irq_save();
    server_pid = process_pid(process);
    result = queue_for_pid(server_pid, true) != NULL ? 0 : -1;
    irq_restore(flags);
    return result;
}

int64_t gui_send(const struct gui_message *message) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result;
    if (process == NULL || message == NULL) {
        return -1;
    }

    uint64_t source_pid = process_pid(process);
    uint64_t target_pid = source_pid == server_pid ? message->target_pid : server_pid;
    if (target_pid == 0) {
        return -1;
    }

    struct gui_message copy = *message;
    copy.source_pid = source_pid;
    copy.target_pid = target_pid;
    flags = irq_save();
    result = push_message(target_pid, &copy);
    irq_restore(flags);
    return result;
}

int64_t gui_recv(struct gui_message *message) {
    struct process *process = process_current();
    uint64_t flags;
    int64_t result = 1;
    if (process == NULL || message == NULL) {
        return -1;
    }

    flags = irq_save();
    struct gui_queue *queue = queue_for_pid(process_pid(process), false);
    if (queue == NULL || queue->count == 0) {
        irq_restore(flags);
        return 0;
    }

    *message = queue->messages[queue->head];
    queue->head = (queue->head + 1) % GUI_QUEUE_DEPTH;
    queue->count--;
    irq_restore(flags);
    return result;
}
