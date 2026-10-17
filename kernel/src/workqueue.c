#include <srvros/console.h>
#include <srvros/scheduler.h>
#include <srvros/workqueue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORKQUEUE_MAX_ITEMS 16

struct work_item {
    workqueue_fn fn;
    void *arg;
};

static struct work_item items[WORKQUEUE_MAX_ITEMS];
static struct scheduler_wait_queue wait_queue;
static uint64_t queue_head, queue_count;
static bool initialized;
static bool worker_started;
static uint64_t scheduled_count;
static uint64_t coalesced_count;
static uint64_t dropped_count;
static uint64_t run_count;

static uint64_t irq_save(void) {
#ifdef SRVROS_WORKQUEUE_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_WORKQUEUE_HOST_TEST
    (void)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
}

static bool work_pending(void *arg) {
    (void)arg;

    return queue_count != 0;
}

static bool take_work(workqueue_fn *fn_out, void **arg_out) {
    uint64_t flags = irq_save();
    if (queue_count == 0) { irq_restore(flags); return false; }
    *fn_out = items[queue_head].fn;
    *arg_out = items[queue_head].arg;
    items[queue_head] = (struct work_item){0};
    queue_head = (queue_head + 1) % WORKQUEUE_MAX_ITEMS;
    queue_count--;
    irq_restore(flags);
    return true;
}

static void workqueue_worker(void *arg) {
    (void)arg;

    for (;;) {
        workqueue_fn fn = NULL;
        void *work_arg = NULL;
        if (take_work(&fn, &work_arg)) {
            run_count++;
            fn(work_arg);
            /* A callback that requeues itself must also give other threads a turn. */
            scheduler_yield();
            continue;
        }

        if (!scheduler_wait(&wait_queue, work_pending, NULL)) {
            scheduler_yield();
        }
    }
}

void workqueue_init(void) {
    if (initialized) {
        return;
    }

    initialized = true;
    worker_started = scheduler_spawn("workqueue", workqueue_worker, NULL);
    console_printf("workqueue: %s slots=%u\n",
        worker_started ? "ready" : "failed",
        (uint64_t)WORKQUEUE_MAX_ITEMS);
}

bool workqueue_schedule(workqueue_fn fn, void *arg) {
    if (!initialized || !worker_started || fn == NULL) {
        return false;
    }

    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < queue_count; i++) {
        struct work_item *item = &items[(queue_head + i) % WORKQUEUE_MAX_ITEMS];
        if (item->fn == fn && item->arg == arg) {
            coalesced_count++;
            irq_restore(flags);
            return true;
        }
    }

    if (queue_count < WORKQUEUE_MAX_ITEMS) {
        uint64_t tail = (queue_head + queue_count) % WORKQUEUE_MAX_ITEMS;
        items[tail] = (struct work_item){.fn = fn, .arg = arg};
        queue_count++;
        scheduled_count++;
        irq_restore(flags);
        scheduler_wake_all(&wait_queue);
        return true;
    }

    dropped_count++;
    irq_restore(flags);
    return false;
}

void workqueue_print_status(void) {
    uint64_t flags = irq_save();
    uint64_t pending = queue_count;
    uint64_t scheduled = scheduled_count, coalesced = coalesced_count;
    uint64_t dropped = dropped_count, ran = run_count;
    irq_restore(flags);

    console_printf("workqueue: worker=%s scheduled=%u coalesced=%u dropped=%u run=%u pending=%u\n",
        worker_started ? "on" : "off",
        scheduled,
        coalesced,
        dropped,
        ran,
        pending);
}
