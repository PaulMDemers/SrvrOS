#include <srvros/console.h>
#include <srvros/scheduler.h>
#include <srvros/workqueue.h>

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define WORKQUEUE_MAX_ITEMS 16

struct work_item {
    bool pending;
    workqueue_fn fn;
    void *arg;
};

static struct work_item items[WORKQUEUE_MAX_ITEMS];
static struct scheduler_wait_queue wait_queue;
static bool initialized;
static bool worker_started;
static uint64_t scheduled_count;
static uint64_t coalesced_count;
static uint64_t dropped_count;
static uint64_t run_count;

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

static bool work_pending(void *arg) {
    (void)arg;

    for (uint64_t i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        if (items[i].pending) {
            return true;
        }
    }
    return false;
}

static bool take_work(workqueue_fn *fn_out, void **arg_out) {
    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        if (items[i].pending) {
            *fn_out = items[i].fn;
            *arg_out = items[i].arg;
            items[i].pending = false;
            items[i].fn = NULL;
            items[i].arg = NULL;
            irq_restore(flags);
            return true;
        }
    }
    irq_restore(flags);
    return false;
}

static void workqueue_worker(void *arg) {
    (void)arg;

    for (;;) {
        workqueue_fn fn = NULL;
        void *work_arg = NULL;
        if (take_work(&fn, &work_arg)) {
            run_count++;
            fn(work_arg);
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
    if (!initialized || fn == NULL) {
        return false;
    }

    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        if (items[i].pending && items[i].fn == fn && items[i].arg == arg) {
            coalesced_count++;
            irq_restore(flags);
            return true;
        }
    }

    for (uint64_t i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        if (!items[i].pending) {
            items[i].pending = true;
            items[i].fn = fn;
            items[i].arg = arg;
            scheduled_count++;
            irq_restore(flags);
            scheduler_wake_all(&wait_queue);
            return true;
        }
    }

    dropped_count++;
    irq_restore(flags);
    return false;
}

void workqueue_print_status(void) {
    uint64_t pending = 0;
    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < WORKQUEUE_MAX_ITEMS; i++) {
        if (items[i].pending) {
            pending++;
        }
    }
    irq_restore(flags);

    console_printf("workqueue: worker=%s scheduled=%u coalesced=%u dropped=%u run=%u pending=%u\n",
        worker_started ? "on" : "off",
        scheduled_count,
        coalesced_count,
        dropped_count,
        run_count,
        pending);
}
