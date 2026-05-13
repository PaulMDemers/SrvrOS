#include <srvros/console.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/vmm.h>

#include <stddef.h>
#include <stdint.h>

#define SCHEDULER_MAX_THREADS 8
#define SCHEDULER_STACK_SIZE 131072
#define SCHEDULER_STACK_MIN_PHYSICAL 0x100000ull
#define SCHEDULER_PREEMPT_QUANTUM_TICKS 2

enum thread_state {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD,
};

struct scheduler_context {
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rsp;
    uint64_t rip;
};

struct thread {
    enum thread_state state;
    const char *name;
    scheduler_thread_fn entry;
    void *arg;
    void *user_context;
    uint64_t address_space;
    uint64_t kernel_stack_top;
    uint8_t *stack;
    struct scheduler_context context;
    uint64_t switches;
};

void scheduler_context_switch(struct scheduler_context *old_context, struct scheduler_context *new_context);
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

static struct thread threads[SCHEDULER_MAX_THREADS];
static uint64_t current_thread;
static bool initialized;
static bool switching;
static uint64_t preempt_tick_accumulator;
static uint64_t preemption_count;
static volatile uint64_t demo_counter;

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

static uint64_t thread_kernel_stack_top(const struct thread *thread) {
    if (thread->kernel_stack_top != 0) {
        return thread->kernel_stack_top;
    }
    return gdt_default_kernel_stack_top();
}

static uint64_t thread_address_space(const struct thread *thread) {
    if (thread->address_space != 0) {
        return thread->address_space;
    }
    return vmm_kernel_address_space();
}

static bool has_ready_thread_except(uint64_t skipped_index) {
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        if (i != skipped_index && threads[i].state == THREAD_READY) {
            return true;
        }
    }
    return false;
}

static void scheduler_trampoline(void) {
    switching = false;
    struct thread *thread = &threads[current_thread];
    thread->entry(thread->arg);
    thread->state = THREAD_DEAD;

    for (;;) {
        scheduler_yield();
    }
}

void scheduler_init(void) {
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        threads[i].state = THREAD_UNUSED;
    }

    current_thread = 0;
    threads[0].state = THREAD_RUNNING;
    threads[0].name = "bootstrap";
    initialized = true;
    console_write("scheduler: timer-preemptive kernel threads ready\n");
}

bool scheduler_spawn(const char *name, scheduler_thread_fn entry, void *arg) {
    if (!initialized || entry == NULL) {
        return false;
    }

    for (uint64_t i = 1; i < SCHEDULER_MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED || threads[i].state == THREAD_DEAD) {
            uint64_t stack_physical = pmm_alloc_frames_above_tagged(
                SCHEDULER_STACK_SIZE / PMM_FRAME_SIZE,
                SCHEDULER_STACK_MIN_PHYSICAL,
                PMM_FRAME_KERNEL_STACK);
            if (stack_physical == 0) {
                return false;
            }
            uint8_t *stack = pmm_phys_to_virt(stack_physical);

            uint64_t stack_top = ((uint64_t)stack + SCHEDULER_STACK_SIZE) & ~0xfull;
            threads[i] = (struct thread) {
                .state = THREAD_READY,
                .name = name,
                .entry = entry,
                .arg = arg,
                .user_context = NULL,
                .address_space = 0,
                .kernel_stack_top = 0,
                .stack = stack,
                .context = {
                    .rsp = stack_top,
                    .rip = (uint64_t)scheduler_trampoline,
                },
                .switches = 0,
            };
            return true;
        }
    }

    return false;
}

void scheduler_yield(void) {
    if (!initialized || switching) {
        return;
    }

    uint64_t old_index = current_thread;
    uint64_t next_index = old_index;

    for (uint64_t i = 1; i <= SCHEDULER_MAX_THREADS; i++) {
        uint64_t candidate = (old_index + i) % SCHEDULER_MAX_THREADS;
        if (threads[candidate].state == THREAD_READY) {
            next_index = candidate;
            break;
        }
    }

    if (next_index == old_index) {
        return;
    }

    switching = true;
    struct thread *old = &threads[old_index];
    struct thread *next = &threads[next_index];

    if (old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
    }
    next->state = THREAD_RUNNING;
    next->switches++;
    current_thread = next_index;

    if (old->address_space != next->address_space) {
        uint64_t next_address_space = thread_address_space(next);
        if (next_address_space != 0) {
            vmm_switch_address_space(next_address_space);
        }
    }
    if (old->kernel_stack_top != next->kernel_stack_top) {
        gdt_set_kernel_stack(thread_kernel_stack_top(next));
    }

    scheduler_context_switch(&old->context, &next->context);
    switching = false;
}

bool scheduler_wait(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg) {
    if (!initialized || queue == NULL || condition == NULL) {
        return false;
    }

    uint64_t flags = irq_save();
    if (condition(arg)) {
        irq_restore(flags);
        return true;
    }

    if (!has_ready_thread_except(current_thread)) {
        irq_restore(flags);
        return false;
    }

    struct thread *thread = &threads[current_thread];
    thread->state = THREAD_BLOCKED;
    queue->waiters |= (1ull << current_thread);
    irq_restore(flags);

    scheduler_yield();
    return true;
}

void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    if (!initialized || queue == NULL) {
        return;
    }

    uint64_t flags = irq_save();
    uint64_t waiters = queue->waiters;
    queue->waiters = 0;

    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        if ((waiters & (1ull << i)) != 0 && threads[i].state == THREAD_BLOCKED) {
            threads[i].state = THREAD_READY;
        }
    }
    irq_restore(flags);
}

void scheduler_preempt_tick(void) {
    if (!initialized || switching) {
        return;
    }

    if (process_should_exit_current()) {
        process_exit(137);
    }

    preempt_tick_accumulator++;
    if (preempt_tick_accumulator < SCHEDULER_PREEMPT_QUANTUM_TICKS) {
        return;
    }
    preempt_tick_accumulator = 0;
    preemption_count++;
    scheduler_yield();
}

void scheduler_set_user_context(void *process, uint64_t address_space, uint64_t kernel_stack_top) {
    if (!initialized) {
        return;
    }

    struct thread *thread = &threads[current_thread];
    thread->user_context = process;
    thread->address_space = address_space;
    thread->kernel_stack_top = kernel_stack_top;

    if (address_space != 0) {
        vmm_refresh_kernel_mappings(address_space);
        vmm_switch_address_space(address_space);
    }
    gdt_set_kernel_stack(thread_kernel_stack_top(thread));
}

void scheduler_clear_user_context(void) {
    if (!initialized) {
        return;
    }

    struct thread *thread = &threads[current_thread];
    thread->user_context = NULL;
    thread->address_space = 0;
    thread->kernel_stack_top = 0;

    uint64_t kernel_address_space = vmm_kernel_address_space();
    if (kernel_address_space != 0) {
        vmm_switch_address_space(kernel_address_space);
    }
    gdt_set_kernel_stack(gdt_default_kernel_stack_top());
}

void *scheduler_current_user_context(void) {
    if (!initialized) {
        return NULL;
    }
    return threads[current_thread].user_context;
}

uint64_t scheduler_thread_count(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        if (threads[i].state != THREAD_UNUSED && threads[i].state != THREAD_DEAD) {
            count++;
        }
    }
    return count;
}

uint64_t scheduler_preemptions(void) {
    return preemption_count;
}

uint64_t scheduler_demo_counter(void) {
    return demo_counter;
}

static void demo_worker(void *arg) {
    (void)arg;

    for (;;) {
        demo_counter++;
        scheduler_yield();
    }
}

bool scheduler_start_demo_worker(void);
bool scheduler_start_demo_worker(void) {
    return scheduler_spawn("demo-worker", demo_worker, NULL);
}
