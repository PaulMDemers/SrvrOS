#include <srvros/console.h>
#include <srvros/latency.h>
#include <srvros/fpu.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

#include <stddef.h>
#include <stdint.h>

#define SCHEDULER_MAX_THREADS 32
#define SCHEDULER_STACK_SIZE 131072
#define SCHEDULER_STACK_MIN_PHYSICAL 0x100000ull
#define SCHEDULER_PREEMPT_QUANTUM_TICKS 2

enum thread_state {
    THREAD_UNUSED,
    THREAD_READY,
    THREAD_RUNNING,
    THREAD_BLOCKED,
    THREAD_DEAD,
    THREAD_QUARANTINED,
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
    uint64_t identity;
    scheduler_thread_fn entry;
    scheduler_wait_condition_fn io_eligible;
    void *arg;
    void *user_context;
    bool io_user_safe;
    void *user_thread_context;
    struct fpu_state *user_fpu;
    uint64_t user_fs_base;
    uint64_t address_space;
    uint64_t kernel_stack_top;
    uint8_t *stack;
    uint64_t stack_physical;
    struct fpu_state kernel_fpu;
    struct scheduler_context context;
    struct scheduler_wait_queue *blocked_queue;
    uint64_t blocked_deadline_ticks;
    uint64_t switches;
    uint64_t sampled_ticks;
    uint64_t timeout_wakes, signal_wakes;
};

void scheduler_context_switch(struct scheduler_context *old_context, struct scheduler_context *new_context);
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

static struct thread threads[SCHEDULER_MAX_THREADS];
static uint64_t current_thread;
static uint64_t next_identity=2;
static bool initialized;
static bool switching;
static bool io_services_enabled;
static uint64_t io_owner, io_depth, io_service_at;
static struct scheduler_wait_queue io_waiters;
static uint64_t preempt_tick_accumulator;
static uint64_t preemption_count;
static uint64_t sampled_ticks;
static uint64_t context_switches;
static uint64_t idle_thread=SCHEDULER_MAX_THREADS;

static uint64_t irq_save(void) {
#ifdef SRVROS_SCHEDULER_HOST_TEST
    return 0;
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    return flags;
#endif
}

static void irq_restore(uint64_t flags) {
#ifdef SRVROS_SCHEDULER_HOST_TEST
    (void)flags;
#else
    if ((flags & (1ull << 9)) != 0) {
        __asm__ volatile ("sti" : : : "memory");
    }
#endif
}

static uint64_t thread_kernel_stack_top(const struct thread *thread) {
    if (thread->kernel_stack_top != 0) {
        return thread->kernel_stack_top;
    }
    if (thread->stack != NULL) {
        return ((uint64_t)thread->stack + SCHEDULER_STACK_SIZE) & ~0xfull;
    }
    return gdt_default_kernel_stack_top();
}

static uint64_t thread_address_space(const struct thread *thread) {
    if (thread->address_space != 0) {
        return thread->address_space;
    }
    return vmm_kernel_address_space();
}

static uint64_t read_fs_base(void) {
#ifdef SRVROS_SCHEDULER_HOST_TEST
    return 0;
#else
    uint32_t low;
    uint32_t high;
    __asm__ volatile ("rdmsr" : "=a"(low), "=d"(high) : "c"(0xC0000100u));
    return ((uint64_t)high << 32) | low;
#endif
}

static void write_fs_base(uint64_t value) {
#ifdef SRVROS_SCHEDULER_HOST_TEST
    (void)value;
#else
    uint32_t low = (uint32_t)value;
    uint32_t high = (uint32_t)(value >> 32);
    __asm__ volatile ("wrmsr" : : "c"(0xC0000100u), "a"(low), "d"(high) : "memory");
#endif
}

static struct fpu_state *thread_user_fpu(struct thread *thread) {
    if (thread == NULL || thread->user_context == NULL) {
        return NULL;
    }
    return thread->user_fpu;
}

static bool io_thread_allowed(uint64_t index) {
    struct thread *thread = &threads[index];
    return io_depth == 0 || index == io_owner ||
        (thread->user_context != NULL && thread->io_user_safe &&
         thread->user_context != threads[io_owner].user_context) ||
        (thread->user_context == NULL && thread->io_eligible != NULL &&
         thread->io_eligible(thread->arg));
}

void scheduler_enable_io_services(void) {
    uint64_t flags = irq_save();
    io_services_enabled = initialized;
    irq_restore(flags);
}

bool scheduler_io_active(void) { return io_depth != 0; }
bool scheduler_io_busy(void) { return io_depth != 0 && io_owner != current_thread; }

bool scheduler_io_begin(void) {
    if (!initialized || !io_services_enabled || switching) return false;
    if (io_depth != 0 && io_owner != current_thread) return false;
    if (io_depth++ == 0) { io_owner = current_thread; io_service_at=timer_monotonic_us(); latency_owner_begin(); }
    return true;
}

void scheduler_io_end(void) {
    if (io_depth != 0 && io_owner == current_thread && --io_depth == 0) {
        latency_owner_end(); scheduler_wake_all(&io_waiters);
    }
}

void scheduler_io_set_user_safe(bool safe) {
    uint64_t flags=irq_save();
    if (initialized) threads[current_thread].io_user_safe=safe;
    irq_restore(flags);
}

void scheduler_user_enter(void) { scheduler_io_set_user_safe(true); }

static bool io_available(void *arg) {
    (void)arg;
    return io_depth == 0 || io_owner == current_thread;
}

/* Gate before inspecting/mutating filesystem, descriptor or process state.
 * Waiting preserves the syscall's arguments and does not turn contention into
 * an I/O error. Nested owner operations retain their existing scope. */
void scheduler_io_wait(void) {
    uint64_t flags=irq_save();
    while (!io_available(NULL)) {
        if (!scheduler_wait(&io_waiters,io_available,NULL)) scheduler_yield();
    }
    irq_restore(flags);
}

bool scheduler_io_yield(void) {
    if (io_depth == 0 || io_owner != current_thread ||
        vmm_current_address_space() != thread_address_space(&threads[current_thread])) return false;
    io_service_at=timer_monotonic_us();
    /* The caller restored its scheduler-recorded address space before entry.
     * Service IRQs even when there is no other eligible runnable thread. */
#ifndef SRVROS_SCHEDULER_HOST_TEST
    __asm__ volatile ("sti; nop" : : : "memory");
    scheduler_yield();
    __asm__ volatile ("cli" : : : "memory");
#endif
    return true;
}

struct io_completion_wait {
    scheduler_wait_condition_fn condition;
    void *arg;
    uint64_t deadline;
};
static bool io_completion_ready(void *arg) {
    struct io_completion_wait *wait=arg;
    return wait->condition(wait->arg) || timer_ticks()>=wait->deadline;
}
bool scheduler_io_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,void *arg,uint64_t deadline_ticks) {
    uint64_t now=timer_ticks();
    if(!initialized || !io_services_enabled || switching || !queue || !condition ||
       !io_depth || io_owner!=current_thread || !now || !timer_reference_ready() ||
       deadline_ticks<=now || deadline_ticks-now>1 ||
       vmm_current_address_space()!=thread_address_space(&threads[current_thread])) return false;
    io_service_at=timer_monotonic_us();
#ifndef SRVROS_SCHEDULER_HOST_TEST
    __asm__ volatile("sti":::"memory");
#endif
    struct io_completion_wait wait={condition,arg,deadline_ticks};
    bool waited=scheduler_wait_timeout(queue,io_completion_ready,&wait,deadline_ticks);
#ifndef SRVROS_SCHEDULER_HOST_TEST
    __asm__ volatile("cli":::"memory");
#endif
    return waited;
}

/* Per-owner budget spans commands: a sequence of individually fast transfers
 * must not avoid service windows by restarting a timer on every transfer. */
void scheduler_io_checkpoint(void) {
    if(io_depth && io_owner==current_thread && timer_monotonic_us()-io_service_at>=4000)
        (void)scheduler_io_yield();
}

static bool has_ready_thread_except(uint64_t skipped_index) {
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        /* A retired service may already have released its callback context. */
        if (i != skipped_index &&
            (threads[i].state == THREAD_READY || threads[i].state == THREAD_RUNNING) &&
            io_thread_allowed(i)) {
            return true;
        }
    }
    return false;
}

static void scheduler_trampoline(void) {
    switching = false;
    __asm__ volatile ("sti" : : : "memory");
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
    threads[0].identity = 1;
    next_identity = 2;
    fpu_init_state(&threads[0].kernel_fpu);
    fpu_set_current_kernel_state(&threads[0].kernel_fpu);
    initialized = true;
    console_write("scheduler: timer-preemptive kernel threads ready\n");
}

/* Context switch jumps into the C trampoline rather than calling it. Provide
 * the return-address slot required by SysV AMD64: RSP is 8 mod 16 at entry. */
static uint64_t initial_thread_stack(uint64_t top) {
    uint64_t rsp=(top & ~0xfull)-8;
    *(uint64_t *)rsp=0; /* The trampoline must never return. */
    return rsp;
}

static bool spawn_thread(const char *name, scheduler_thread_fn entry, void *arg,
    scheduler_wait_condition_fn io_eligible) {
    if (!initialized || entry == NULL) {
        return false;
    }

    uint64_t flags=irq_save();
    if(next_identity==UINT64_MAX) { irq_restore(flags);return false; }
    for (uint64_t i = 1; i < SCHEDULER_MAX_THREADS; i++) {
        if (threads[i].state == THREAD_UNUSED || threads[i].state == THREAD_DEAD) {
            if (threads[i].state == THREAD_DEAD && threads[i].stack_physical != 0) {
                pmm_free_frame_range(threads[i].stack_physical, SCHEDULER_STACK_SIZE / PMM_FRAME_SIZE);
                threads[i].stack_physical = 0;
                threads[i].stack = NULL;
            }
            uint64_t stack_physical = pmm_alloc_frames_above_tagged(
                SCHEDULER_STACK_SIZE / PMM_FRAME_SIZE,
                SCHEDULER_STACK_MIN_PHYSICAL,
                PMM_FRAME_KERNEL_STACK);
            if (stack_physical == 0) {
                irq_restore(flags);return false;
            }
            uint8_t *stack = pmm_phys_to_virt(stack_physical);

            uint64_t stack_top = ((uint64_t)stack + SCHEDULER_STACK_SIZE) & ~0xfull;
            threads[i] = (struct thread) {
                .state = THREAD_READY,
                .name = name,
                .identity = next_identity++,
                .entry = entry,
                .io_eligible = io_eligible,
                .arg = arg,
                .user_context = NULL,
                .user_thread_context = NULL,
                .user_fpu = NULL,
                .user_fs_base = 0,
                .address_space = 0,
                .kernel_stack_top = 0,
                .stack = stack,
                .stack_physical = stack_physical,
                .context = {
                    .rsp = initial_thread_stack(stack_top),
                    .rip = (uint64_t)scheduler_trampoline,
                },
                .switches = 0,
            };
            fpu_init_state(&threads[i].kernel_fpu);
            irq_restore(flags);return true;
        }
    }

    irq_restore(flags);return false;
}

bool scheduler_spawn(const char *name, scheduler_thread_fn entry, void *arg) {
    return spawn_thread(name, entry, arg, NULL);
}

bool scheduler_spawn_io_service(const char *name, scheduler_thread_fn entry,
    void *arg, scheduler_wait_condition_fn eligible) {
    if (eligible == NULL) return false;
    return spawn_thread(name, entry, arg, eligible);
}

static uint64_t next_ready_thread(uint64_t old_index) {
    for (uint64_t i = 1; i <= SCHEDULER_MAX_THREADS; i++) {
        uint64_t candidate = (old_index + i) % SCHEDULER_MAX_THREADS;
        if (candidate != old_index && threads[candidate].state == THREAD_RUNNING) {
            threads[candidate].state = THREAD_READY;
        }
        if (candidate!=idle_thread && threads[candidate].state == THREAD_READY && io_thread_allowed(candidate))
            return candidate;
    }
    /* Idle is a fallback, never a peer that steals a runnable worker's turn. */
    if(old_index!=idle_thread && threads[old_index].state==THREAD_RUNNING) return old_index;
    if(idle_thread<SCHEDULER_MAX_THREADS &&
       (threads[idle_thread].state==THREAD_READY || threads[idle_thread].state==THREAD_RUNNING)) return idle_thread;
    return old_index;
}

/* Identities are boot-monotonic; an old slot index is not authority. */
static uint64_t identity_index(uint64_t identity) {
    if(identity)for(uint64_t i=0;i<SCHEDULER_MAX_THREADS;i++)
        if(threads[i].identity==identity)return i;
    return SCHEDULER_MAX_THREADS;
}
bool scheduler_service_pair_quarantined(uint64_t caller,uint64_t peer) {
    uint64_t flags=irq_save(),a=identity_index(caller),b=identity_index(peer);
    bool ok=initialized && a<SCHEDULER_MAX_THREADS && b<SCHEDULER_MAX_THREADS && a!=b &&
        threads[a].state==THREAD_QUARANTINED && threads[b].state==THREAD_QUARANTINED;
    irq_restore(flags);return ok;
}
static bool quarantine_pair(uint64_t caller,uint64_t peer,bool external) {
    /* The caller owns IRQ exclusion and its explicit recovery boundary. Keep
     * validation entirely before mutation; failure retains both original slots. */
    uint64_t a=identity_index(caller),b=identity_index(peer);
    if(!initialized || switching || current_thread>=SCHEDULER_MAX_THREADS ||
       threads[current_thread].state!=THREAD_RUNNING || a==0 ||
       a>=SCHEDULER_MAX_THREADS || b>=SCHEDULER_MAX_THREADS || b==0 || a==b ||
       a==idle_thread || b==idle_thread || idle_thread>=SCHEDULER_MAX_THREADS ||
       (threads[idle_thread].state!=THREAD_READY && threads[idle_thread].state!=THREAD_RUNNING) ||
       (external ? (a==current_thread || b==current_thread ||
         (threads[a].state!=THREAD_READY && threads[a].state!=THREAD_BLOCKED)) :
         (a!=current_thread || threads[a].state!=THREAD_RUNNING)) ||
       (threads[b].state!=THREAD_READY && threads[b].state!=THREAD_BLOCKED) ||
       (io_depth && (io_owner==a || io_owner==b)))return false;
    for(unsigned j=0;j<2;j++) {
        struct thread *t=&threads[j?b:a];
        if(t->user_context || t->user_thread_context || t->address_space ||
           !t->stack || !t->stack_physical || !t->io_eligible)return false;
    }
    for(unsigned j=0;j<2;j++) {
        uint64_t i=j?b:a;struct thread *t=&threads[i];
        if(t->blocked_queue)t->blocked_queue->waiters&=~(1ull<<i);
        t->blocked_queue=NULL;t->blocked_deadline_ticks=0;
        t->io_eligible=NULL;t->state=THREAD_QUARANTINED;
    }
    return true;
}

bool scheduler_quarantine_service_pair(uint64_t caller,uint64_t peer) {
    return quarantine_pair(caller,peer,false);
}
bool scheduler_quarantine_noncurrent_services(uint64_t first,uint64_t second) {
    return quarantine_pair(first,second,true);
}

void scheduler_yield(void) {
    uint64_t flags = irq_save();
    if (!initialized || switching) {
        irq_restore(flags);
        return;
    }

    uint64_t old_index = current_thread;
    uint64_t next_index = next_ready_thread(old_index);

    if (next_index == old_index) {
        irq_restore(flags);
        return;
    }

    switching = true;
    struct thread *old = &threads[old_index];
    struct thread *next = &threads[next_index];
    if (old->user_context != NULL) {
        old->user_fs_base = read_fs_base();
    }

    if (old->state == THREAD_RUNNING) {
        old->state = THREAD_READY;
    }
    next->state = THREAD_RUNNING;
    next->switches++;
    context_switches++;
    current_thread = next_index;

    if (old->address_space != next->address_space) {
        uint64_t next_address_space = thread_address_space(next);
        if (next_address_space != 0) {
            vmm_switch_address_space(next_address_space);
        }
    }
    uint64_t old_stack_top = thread_kernel_stack_top(old);
    uint64_t next_stack_top = thread_kernel_stack_top(next);
    if (old_stack_top != next_stack_top) {
        gdt_set_kernel_stack(next_stack_top);
    }
    fpu_switch_kernel_state(&old->kernel_fpu, &next->kernel_fpu, thread_user_fpu(next));
    write_fs_base(next->user_context != NULL ? next->user_fs_base : 0);

    scheduler_context_switch(&old->context, &next->context);

    switching = false;
    irq_restore(flags);
}

void scheduler_exit_current(void) {
    if (!initialized) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    threads[current_thread].state = THREAD_DEAD;
    for (;;) {
        scheduler_yield();
        __asm__ volatile ("hlt");
    }
}

void scheduler_kill_user_threads(void *process, void *except_user_thread_context) {
    if (!initialized || process == NULL) {
        return;
    }

    uint64_t flags = irq_save();
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        struct thread *thread = &threads[i];
        if (thread->state == THREAD_UNUSED ||
            thread->state == THREAD_DEAD ||
            thread->user_context != process ||
            thread->user_thread_context == NULL ||
            thread->user_thread_context == except_user_thread_context) {
            continue;
        }
        if (thread->blocked_queue != NULL) {
            thread->blocked_queue->waiters &= ~(1ull << i);
        }
        thread->blocked_queue = NULL;
        thread->blocked_deadline_ticks = 0;
        thread->user_context = NULL;
        thread->user_thread_context = NULL;
        thread->user_fpu = NULL;
        thread->user_fs_base = 0;
        thread->address_space = 0;
        thread->kernel_stack_top = 0;
        thread->state = THREAD_DEAD;
    }
    irq_restore(flags);
}

bool scheduler_wait_timeout(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg,
    uint64_t deadline_ticks) {
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
    thread->blocked_queue = queue;
    thread->blocked_deadline_ticks = deadline_ticks;
    queue->waiters |= (1ull << current_thread);
    irq_restore(flags);

    scheduler_yield();
    return true;
}

bool scheduler_wait(struct scheduler_wait_queue *queue,
    scheduler_wait_condition_fn condition,
    void *arg) {
    return scheduler_wait_timeout(queue, condition, arg, 0);
}

uint64_t scheduler_wake_count(struct scheduler_wait_queue *queue, uint64_t max_count) {
    if (!initialized || queue == NULL || max_count == 0) {
        return 0;
    }

    uint64_t flags = irq_save();
    uint64_t waiters = queue->waiters;
    uint64_t woken = 0;

    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        if ((waiters & (1ull << i)) == 0) {
            continue;
        }
        if (threads[i].state != THREAD_BLOCKED) {
            queue->waiters &= ~(1ull << i);
            continue;
        }
        if (woken < max_count) {
            queue->waiters &= ~(1ull << i);
            threads[i].state = THREAD_READY;
            threads[i].signal_wakes++;
            threads[i].blocked_queue = NULL;
            threads[i].blocked_deadline_ticks = 0;
            woken++;
        }
    }
    irq_restore(flags);
    return woken;
}

void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    (void)scheduler_wake_count(queue, UINT64_MAX);
}

static void wake_expired_waits(void) {
    uint64_t now = timer_ticks();
    for (uint64_t i = 0; i < SCHEDULER_MAX_THREADS; i++) {
        struct thread *thread = &threads[i];
        if (thread->state != THREAD_BLOCKED ||
            thread->blocked_deadline_ticks == 0 ||
            now < thread->blocked_deadline_ticks) {
            continue;
        }
        if (thread->blocked_queue != NULL) {
            thread->blocked_queue->waiters &= ~(1ull << i);
        }
        thread->blocked_queue = NULL;
        thread->blocked_deadline_ticks = 0;
        thread->state = THREAD_READY;
        thread->timeout_wakes++;
    }
}

void scheduler_preempt_tick(void) {
    if (!initialized || switching) {
        return;
    }

    sampled_ticks++;
    threads[current_thread].sampled_ticks++;

    wake_expired_waits();

    if (io_depth == 0 && process_should_exit_current()) {
        process_exit(process_current_kill_status());
    }

    preempt_tick_accumulator++;
    if (preempt_tick_accumulator < SCHEDULER_PREEMPT_QUANTUM_TICKS) {
        return;
    }
    preempt_tick_accumulator = 0;
    preemption_count++;
    scheduler_yield();
}

void scheduler_set_user_context_fpu(void *process,
    uint64_t address_space,
    uint64_t kernel_stack_top,
    struct fpu_state *user_fpu) {
    if (!initialized) {
        return;
    }

    struct thread *thread = &threads[current_thread];
    thread->user_context = process;
    thread->io_user_safe = false;
    thread->address_space = address_space;
    thread->kernel_stack_top = kernel_stack_top;
    thread->user_fpu = user_fpu;
    thread->user_fs_base = process_fs_base(process);
    fpu_set_current_user_state(user_fpu);
    write_fs_base(thread->user_fs_base);

    if (address_space != 0) {
        vmm_refresh_kernel_mappings(address_space);
        vmm_switch_address_space(address_space);
    }
    gdt_set_kernel_stack(thread_kernel_stack_top(thread));
}

void scheduler_set_user_context(void *process, uint64_t address_space, uint64_t kernel_stack_top) {
    scheduler_set_user_context_fpu(process, address_space, kernel_stack_top, process_fpu_state(process));
}

void scheduler_clear_user_context(void) {
    if (!initialized) {
        return;
    }

    struct thread *thread = &threads[current_thread];
    thread->user_context = NULL;
    thread->io_user_safe = false;
    thread->user_thread_context = NULL;
    thread->user_fpu = NULL;
    thread->user_fs_base = 0;
    thread->address_space = 0;
    thread->kernel_stack_top = 0;
    fpu_set_current_user_state(NULL);
    write_fs_base(0);

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

void scheduler_set_user_thread_context(void *context) {
    if (!initialized) {
        return;
    }
    threads[current_thread].user_thread_context = context;
}

void *scheduler_current_user_thread_context(void) {
    if (!initialized) {
        return NULL;
    }
    return threads[current_thread].user_thread_context;
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

void scheduler_print_status(void) {
    /* Copy with IRQs masked, then print outside the critical section. These
     * timer samples show where the boot CPU spends time, not process CPU time
     * or an energy estimate. Short tasks can fall between samples. */
    struct sample { const char *name; unsigned state; uint64_t ticks, switches; } sample[SCHEDULER_MAX_THREADS];
    uint64_t flags=irq_save(),total=sampled_ticks,preemptions=preemption_count;
    for(unsigned i=0;i<SCHEDULER_MAX_THREADS;i++) sample[i]=(struct sample){
        threads[i].name,threads[i].state,threads[i].sampled_ticks,threads[i].switches};
    irq_restore(flags);
    console_printf("scheduler: threads=%u preemptions=%u sampled_ticks=%u\n",
        scheduler_thread_count(),preemptions,total);
    for(unsigned i=0;i<SCHEDULER_MAX_THREADS;i++) if(sample[i].state!=THREAD_UNUSED)
        console_printf("scheduler: thread=%u name=%s state=%u sampled_ticks=%u switches=%u\n",
            (uint64_t)i,sample[i].name?sample[i].name:"unknown",(uint64_t)sample[i].state,
            sample[i].ticks,sample[i].switches);
}

/* Diagnostic-only, one bounded CPU0 snapshot; no output while IRQs are masked.
 * Copy names as well as counters: a process may retire before printing ends.
 * Identity, not the reusable slot or name, identifies a thread lifetime. */
struct activity_sample {
    char name[32];
    uint64_t slot, identity, state, ticks, switches, timeouts, signals;
};
static unsigned activity_snapshot(struct activity_sample *out, uint64_t *at) {
    unsigned count=0;
    uint64_t flags=irq_save();
    *at=timer_monotonic_us();
    if(initialized) for(unsigned i=0;i<SCHEDULER_MAX_THREADS;i++) {
        const struct thread *t=&threads[i];
        if(t->state==THREAD_UNUSED) continue;
        struct activity_sample *v=&out[count++];
        *v=(struct activity_sample){.slot=i,.identity=t->identity,.state=t->state,
            .ticks=t->sampled_ticks,.switches=t->switches,
            .timeouts=t->timeout_wakes,.signals=t->signal_wakes};
        const char *name=t->name?t->name:"unknown";
        unsigned j=0;
        for(;j<sizeof(v->name)-1 && name[j];j++) {
            unsigned char c=(unsigned char)name[j];
            v->name[j]=(c>32 && c<127)?(char)c:'_';
        }
        v->name[j]=0;
    }
    irq_restore(flags);
    return count;
}
void scheduler_print_activity(void) {
    struct activity_sample rows[SCHEDULER_MAX_THREADS];
    uint64_t at;
    unsigned count=activity_snapshot(rows,&at);
    console_printf("worker-snapshot: us=%u count=%u version=1\n",at,(uint64_t)count);
    for(unsigned i=0;i<count;i++) {
        const struct activity_sample *v=&rows[i];
        console_printf("worker: us=%u slot=%u id=%u name=%s state=%u ticks=%u switches=%u timeouts=%u signals=%u\n",
            at,v->slot,v->identity,v->name,v->state,v->ticks,v->switches,v->timeouts,v->signals);
    }
    console_printf("worker-snapshot-end: us=%u count=%u\n",at,(uint64_t)count);
}

void scheduler_snapshot(struct scheduler_stats *out) {
    if(!out) return;
    uint64_t flags=irq_save();
    *out=(struct scheduler_stats){sampled_ticks,
        idle_thread<SCHEDULER_MAX_THREADS?threads[idle_thread].sampled_ticks:0,
        context_switches,preemption_count};
    irq_restore(flags);
}

static bool idle_eligible(void *arg) { (void)arg;return true; }

static void idle_worker(void *arg) {
    (void)arg;
    for (;;) {
        uint64_t flags=irq_save();
        if(has_ready_thread_except(current_thread)) {
            irq_restore(flags);scheduler_yield();continue;
        }
        /* STI's interrupt shadow makes the readiness check + HLT atomic with
         * respect to interrupt wakeups. The idle thread touches no I/O state
         * and can also run while a storage owner is waiting for service. */
#ifndef SRVROS_SCHEDULER_HOST_TEST
        __asm__ volatile("sti; hlt; cli":::"memory");
#endif
        irq_restore(flags);
    }
}

bool scheduler_start_idle_worker(void) {
    if(idle_thread<SCHEDULER_MAX_THREADS) return true;
    if(!scheduler_spawn_io_service("idle",idle_worker,NULL,idle_eligible)) return false;
    for(uint64_t i=1;i<SCHEDULER_MAX_THREADS;i++) if(threads[i].entry==idle_worker) {
        idle_thread=i;return true;
    }
    return false;
}

/* Monotonic lifetime identity; a recycled worker slot receives a fresh ID. */
uint64_t scheduler_current_thread_id(void) {
    uint64_t flags=irq_save();
    uint64_t id=initialized?threads[current_thread].identity:0;
    irq_restore(flags);return id;
}
