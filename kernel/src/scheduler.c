#include <srvros/ap_probe.h>
#include <srvros/console.h>
#include <srvros/latency.h>
#include <srvros/fpu.h>
#include <srvros/pmm.h>
#include <srvros/process.h>
#include <srvros/scheduler.h>
#include <srvros/idle_audit.h>
#include <srvros/scheduler_abi.h>
#include <srvros/timer.h>
#include <srvros/vmm.h>

#include <stddef.h>
#include <stdint.h>

#define SCHEDULER_INITIAL_THREADS 32
#define SCHEDULER_STORAGE_BUDGET (32ull * 1024 * 1024)
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
    bool io_gate_waiting;
    void *user_thread_context;
    struct fpu_state *user_fpu;
    uint64_t user_fs_base;
    uint64_t address_space;
    uint64_t resume_address_space;
    uint64_t kernel_stack_top;
    uint8_t *stack;
    uint64_t stack_physical;
    struct fpu_state kernel_fpu;
    struct scheduler_context context;
    struct scheduler_wait_queue *blocked_queue;
    uint64_t blocked_deadline_ticks;
    uint64_t switches;
    uint64_t sampled_ticks;
    uint64_t account_at;
    bool account_user;
    uint64_t timeout_wakes, signal_wakes;
    struct {
        struct srv_sched_trace out;
        uint64_t since,deadline,wake_tick,reason,exclusions;
        unsigned active,state; /* 0 running, 1 blocked, 2 ready */
    } trace;
};

void scheduler_context_switch(struct scheduler_context *old_context, struct scheduler_context *new_context);
uint64_t gdt_default_kernel_stack_top(void);
void gdt_set_kernel_stack(uint64_t stack_top);

/* Stable records: context/FPU pointers survive growth and suspended switches.
 * Only spawn allocates, under CPU0 IRQ exclusion like kernel-stack allocation.
 * Dead stacks remain charged until reuse; quarantined stacks are never reused.
 * Dynamic record chunks stay resident at the high-water mark until reboot. */
#define SCHEDULER_CHUNKS ((SCHEDULER_STORAGE_BUDGET / SCHEDULER_STACK_SIZE + SCHEDULER_INITIAL_THREADS - 1) / SCHEDULER_INITIAL_THREADS)
#define SCHEDULER_MAX_THREADS (SCHEDULER_CHUNKS * SCHEDULER_INITIAL_THREADS)
#define SCHEDULER_CHUNK_FRAMES ((sizeof(struct thread) * SCHEDULER_INITIAL_THREADS + PMM_FRAME_SIZE - 1) / PMM_FRAME_SIZE)
static struct thread threads[SCHEDULER_INITIAL_THREADS];
static struct thread *thread_chunks[SCHEDULER_CHUNKS] = { threads };
static uint64_t thread_capacity=SCHEDULER_INITIAL_THREADS;
static uint64_t thread_storage_bytes;
static uint64_t thread_storage_budget=SCHEDULER_STORAGE_BUDGET;
static struct thread *thread_at(uint64_t index) {
    return &thread_chunks[index/SCHEDULER_INITIAL_THREADS][index%SCHEDULER_INITIAL_THREADS];
}
static void detach_wait(struct thread *thread) {
    if(thread->blocked_queue && thread->blocked_queue->waiters)
        thread->blocked_queue->waiters--;
    thread->blocked_queue=NULL;
    thread->blocked_deadline_ticks=0;
}
static uint64_t current_thread;
static uint64_t next_identity=2;
static bool initialized;
static bool switching;
static bool io_services_enabled;
static uint64_t io_owner, io_depth, io_service_at;
static void *io_private_image;
static struct scheduler_wait_queue io_waiters;
static uint64_t io_gate_pending;
static uint64_t io_gate_cursor;
static void yield_to(uint64_t preferred);
static void io_gate_request(struct thread *thread) {
    if(!thread->io_gate_waiting) { thread->io_gate_waiting=true;io_gate_pending++; }
}
static void io_gate_leave(struct thread *thread) {
    if(thread->io_gate_waiting) { thread->io_gate_waiting=false;io_gate_pending--; }
}
static uint64_t preempt_tick_accumulator;
static uint64_t preemption_count;
static uint64_t sampled_ticks;
static uint64_t context_switches;
static uint64_t idle_thread=UINT64_MAX;

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

/* Opt-in observations only: never influence eligibility, wakeups or selection. */
static void trace_ready(struct thread *t,uint64_t reason) {
    if(!t->trace.active)return;
    uint64_t now=timer_monotonic_us();
    if(now<t->trace.since) {t->trace.out.invalid++;return;}
    if(t->trace.state==1)t->trace.out.blocked_us+=now-t->trace.since;
    else if(t->trace.state!=0) {t->trace.out.invalid++;return;}
    t->trace.state=2;t->trace.since=now;t->trace.reason=reason;
    t->trace.wake_tick=timer_ticks();t->trace.exclusions=0;
    if(reason==1)t->trace.out.signals++;
    if(reason==2) {
        t->trace.out.timeouts++;
        uint64_t late=t->trace.wake_tick>=t->trace.deadline?t->trace.wake_tick-t->trace.deadline:0;
        if(late>t->trace.out.late_ticks)t->trace.out.late_ticks=late;
    }
}
static void trace_run(struct thread *t,uint64_t now) {
    if(!t->trace.active || !t->trace.state)return;
    if(t->trace.state!=2 || now<t->trace.since) {t->trace.out.invalid++;return;}
    uint64_t ready=now-t->trace.since;
    t->trace.out.ready_us+=ready;t->trace.out.dispatches++;
    if(ready>t->trace.out.max_ready_us) {
        t->trace.out.max_ready_us=ready;t->trace.out.wake_us=t->trace.since;
        t->trace.out.run_us=now;t->trace.out.wake_tick=t->trace.wake_tick;
        t->trace.out.deadline_tick=t->trace.deadline;t->trace.out.reason=t->trace.reason;
        t->trace.out.ready_exclusions=t->trace.exclusions;
    }
    t->trace.state=0;t->trace.since=now;t->trace.deadline=0;
}
bool scheduler_trace(unsigned operation,struct srv_sched_trace *out) {
    if(!out || operation>SRV_SCHED_TRACE_END)return false;
    uint64_t flags=irq_save();
    if(!initialized || switching || thread_at(current_thread)->state!=THREAD_RUNNING) {
        irq_restore(flags);return false;
    }
    struct thread *t=thread_at(current_thread);
    if(operation==SRV_SCHED_TRACE_BEGIN) {
        if(t->trace.active) {irq_restore(flags);return false;}
        t->trace.out=(struct srv_sched_trace){.version=SRV_SCHED_TRACE_VERSION,
            .start_us=timer_monotonic_us(),.start_tick=timer_ticks()};
        t->trace.active=1;t->trace.state=0;t->trace.since=t->trace.out.start_us;
        t->trace.deadline=t->trace.wake_tick=t->trace.reason=t->trace.exclusions=0;
    } else {
        if(!t->trace.active) {irq_restore(flags);return false;}
        if(t->trace.state)t->trace.out.invalid++;
        t->trace.out.end_us=timer_monotonic_us();t->trace.out.end_tick=timer_ticks();
        if(t->trace.out.end_us<t->trace.out.start_us || t->trace.out.end_tick<t->trace.out.start_tick)
            t->trace.out.invalid++;
        t->trace.active=0;
    }
    *out=t->trace.out;irq_restore(flags);return true;
}

static void account_current_at(uint64_t now) {
    struct thread *thread=thread_at(current_thread);
    if(thread->user_context && now>=thread->account_at)
        process_account_cpu(thread->user_context,thread->user_thread_context,
            thread->account_user,now-thread->account_at);
    thread->account_at=now;
}
void scheduler_account_now(void) {
    uint64_t flags=irq_save();
    if(initialized) account_current_at(timer_monotonic_us());
    irq_restore(flags);
}
static void account_transition(bool user) {
    uint64_t flags=irq_save();
    if(initialized) {
        account_current_at(timer_monotonic_us());
        thread_at(current_thread)->account_user=user;
    }
    irq_restore(flags);
}
void scheduler_account_kernel_enter(void) { account_transition(false); }

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
    struct thread *thread = thread_at(index);
    bool allowed=io_depth == 0 || index == io_owner ||
        (thread->user_context != NULL && thread->io_user_safe &&
         thread->user_context != (io_private_image?io_private_image:thread_at(io_owner)->user_context)) ||
        (thread->user_context == NULL && thread->io_eligible != NULL &&
         thread->io_eligible(thread->arg));
    /* Fresh process threads and post-exit kernel continuations have no syscall
     * waiter. Remember their actual READY exclusion too; otherwise a continuous
     * writer can reacquire the gate before their first/last kernel turn. */
    if(!allowed && thread->state==THREAD_READY) {
        io_gate_request(thread);
        if(thread->trace.active) {thread->trace.out.exclusions++;thread->trace.exclusions++;}
    }
    return allowed;
}

void scheduler_enable_io_services(void) {
    uint64_t flags = irq_save();
    io_services_enabled = initialized;
    irq_restore(flags);
}

bool scheduler_io_active(void) { return io_depth != 0; }
bool scheduler_io_busy(void) { return io_depth != 0 && io_owner != current_thread; }

void scheduler_io_request_turn(void) {
    uint64_t flags=irq_save();
    if(initialized && io_services_enabled && !switching && scheduler_io_busy() &&
       thread_at(current_thread)->state==THREAD_RUNNING)
        io_gate_request(thread_at(current_thread));
    irq_restore(flags);
}

bool scheduler_io_begin(void) {
    if (!initialized || !io_services_enabled || switching) return false;
    if (io_depth != 0 && io_owner != current_thread) return false;
    if (io_depth++ == 0) {
        io_owner = current_thread;io_private_image=NULL;
        io_service_at=timer_monotonic_us();latency_owner_begin();
    }
    return true;
}

bool scheduler_io_begin_process_image(void *image) {
    if(!initialized || !io_services_enabled || switching || io_depth || !image ||
       !thread_at(current_thread)->user_context || image==thread_at(current_thread)->user_context ||
       thread_at(current_thread)->state!=THREAD_RUNNING ||
       vmm_current_address_space()!=thread_address_space(thread_at(current_thread)))return false;
    for(uint64_t i=0;i<thread_capacity;i++) {
        struct thread *t=thread_at(i);
        if(t->state!=THREAD_UNUSED && t->state!=THREAD_DEAD && t->user_context==image)return false;
    }
    if(!scheduler_io_begin())return false;
    io_private_image=image;return true;
}

void scheduler_io_end(void) {
    if (io_depth != 0 && io_owner == current_thread && --io_depth == 0) {
        io_private_image=NULL;latency_owner_end(); scheduler_wake_all(&io_waiters);
    }
}

void scheduler_io_set_user_safe(bool safe) {
    uint64_t flags=irq_save();
    if (initialized) thread_at(current_thread)->io_user_safe=safe;
    irq_restore(flags);
}

void scheduler_user_enter(void) { account_transition(true);scheduler_io_set_user_safe(true); }

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
        struct thread *thread=thread_at(current_thread);
        io_gate_request(thread);
        if (!scheduler_wait(&io_waiters,io_available,NULL)) scheduler_yield();
    }
    io_gate_leave(thread_at(current_thread));
    irq_restore(flags);
}

void scheduler_io_handoff(void) {
    uint64_t flags=irq_save();
    if(initialized && io_services_enabled && !switching && !io_depth && io_gate_pending &&
       thread_at(current_thread)->state==THREAD_RUNNING && thread_at(current_thread)->io_user_safe) {
        /* Includes awakened waiters, READY kernel entries excluded by the gate,
         * and nonblocking reap retries. Prefer them only at this fully unwound
         * boundary, never during ordinary GUI/audio service turns. */
        /* Resume after the last admitted waiter, not after this writer.
         * A nearby recurring nohang waiter must not win every gap ahead of
         * a farther startup/exit continuation. Admission remains quiescent. */
        for(uint64_t n=1;n<=thread_capacity;n++) {
            uint64_t i=(io_gate_cursor+n)%thread_capacity;
            if(thread_at(i)->io_gate_waiting && thread_at(i)->state==THREAD_READY) {
                yield_to(i);break;
            }
        }
    }
    irq_restore(flags);
}

bool scheduler_io_yield(void) {
    if (io_depth == 0 || io_owner != current_thread ||
        vmm_current_address_space() != thread_address_space(thread_at(current_thread))) return false;
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
       vmm_current_address_space()!=thread_address_space(thread_at(current_thread))) return false;
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
    for (uint64_t i = 0; i < thread_capacity; i++) {
        /* A retired service may already have released its callback context. */
        if (i != skipped_index &&
            (thread_at(i)->state == THREAD_READY || thread_at(i)->state == THREAD_RUNNING) &&
            io_thread_allowed(i)) {
            return true;
        }
    }
    return false;
}

static void scheduler_trampoline(void) {
    switching = false;
    __asm__ volatile ("sti" : : : "memory");
    struct thread *thread = thread_at(current_thread);
    thread->entry(thread->arg);
    io_gate_leave(thread);
    thread->state = THREAD_DEAD;

    for (;;) {
        scheduler_yield();
    }
}

void scheduler_init(void) {
    for (uint64_t i = 0; i < thread_capacity; i++) {
        thread_at(i)->state = THREAD_UNUSED;
        thread_at(i)->io_gate_waiting=false;
    }
    io_gate_pending=io_gate_cursor=0;io_private_image=NULL;

    current_thread = 0;
    thread_at(0)->state = THREAD_RUNNING;
    thread_at(0)->name = "bootstrap";
    thread_at(0)->identity = 1;
    next_identity = 2;
    fpu_init_state(&thread_at(0)->kernel_fpu);
    fpu_set_current_kernel_state(&thread_at(0)->kernel_fpu);
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
    uint64_t i;
    for(i=1;i<thread_capacity;i++)
        if(i!=current_thread && (thread_at(i)->state==THREAD_UNUSED || thread_at(i)->state==THREAD_DEAD))break;
    bool grow=i==thread_capacity;
    if(grow && thread_capacity==SCHEDULER_MAX_THREADS) { irq_restore(flags);return false; }
    struct thread *slot=grow?NULL:thread_at(i);
    if(slot && slot->stack_physical) {
        pmm_free_frame_range(slot->stack_physical,SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE);
        thread_storage_bytes-=SCHEDULER_STACK_SIZE;
        slot->stack_physical=0;slot->stack=NULL;
    }
    uint64_t metadata=grow?SCHEDULER_CHUNK_FRAMES*PMM_FRAME_SIZE:0;
    uint64_t needed=SCHEDULER_STACK_SIZE+metadata;
    if(thread_storage_bytes>thread_storage_budget || needed>thread_storage_budget-thread_storage_bytes) {
        irq_restore(flags);return false;
    }
    uint64_t physical=pmm_alloc_frames_above_tagged(SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE,
        SCHEDULER_STACK_MIN_PHYSICAL,PMM_FRAME_KERNEL_STACK);
    if(!physical) { irq_restore(flags);return false; }
    struct thread *chunk=NULL;
    if(grow) {
        uint64_t record_physical=pmm_alloc_frames_above_tagged(SCHEDULER_CHUNK_FRAMES,
            SCHEDULER_STACK_MIN_PHYSICAL,PMM_FRAME_GENERAL);
        if(!record_physical) {
            pmm_free_frame_range(physical,SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE);
            irq_restore(flags);return false;
        }
        chunk=pmm_phys_to_virt(record_physical);
        for(unsigned n=0;n<SCHEDULER_INITIAL_THREADS;n++)chunk[n]=(struct thread){0};
        slot=&chunk[0];
    }
    uint8_t *stack=pmm_phys_to_virt(physical);
    *slot=(struct thread){.state=THREAD_READY,.name=name,.identity=next_identity++,
        .entry=entry,.io_eligible=io_eligible,.arg=arg,.stack=stack,.stack_physical=physical,
        .context={.rsp=initial_thread_stack((uint64_t)stack+SCHEDULER_STACK_SIZE),
                  .rip=(uint64_t)scheduler_trampoline}};
    fpu_init_state(&slot->kernel_fpu);
    if(grow) {
        thread_chunks[thread_capacity/SCHEDULER_INITIAL_THREADS]=chunk;
        thread_capacity+=SCHEDULER_INITIAL_THREADS;
    }
    thread_storage_bytes+=needed;
    irq_restore(flags);return true;
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
    for (uint64_t i = 1; i <= thread_capacity; i++) {
        uint64_t candidate = (old_index + i) % thread_capacity;
        if (candidate != old_index && thread_at(candidate)->state == THREAD_RUNNING) {
            thread_at(candidate)->state = THREAD_READY;
        }
        if (candidate!=idle_thread && thread_at(candidate)->state == THREAD_READY && io_thread_allowed(candidate))
            return candidate;
    }
    /* Idle is a fallback, never a peer that steals a runnable worker's turn. */
    if(old_index!=idle_thread && thread_at(old_index)->state==THREAD_RUNNING) return old_index;
    if(idle_thread<thread_capacity &&
       (thread_at(idle_thread)->state==THREAD_READY || thread_at(idle_thread)->state==THREAD_RUNNING)) return idle_thread;
    return old_index;
}

/* Identities are boot-monotonic; an old slot index is not authority. */
static uint64_t identity_index(uint64_t identity) {
    if(identity)for(uint64_t i=0;i<thread_capacity;i++)
        if(thread_at(i)->identity==identity)return i;
    return thread_capacity;
}
bool scheduler_service_pair_quarantined(uint64_t caller,uint64_t peer) {
    uint64_t flags=irq_save(),a=identity_index(caller),b=identity_index(peer);
    bool ok=initialized && a<thread_capacity && b<thread_capacity && a!=b &&
        thread_at(a)->state==THREAD_QUARANTINED && thread_at(b)->state==THREAD_QUARANTINED;
    irq_restore(flags);return ok;
}
static bool quarantine_pair(uint64_t caller,uint64_t peer,bool external) {
    /* The caller owns IRQ exclusion and its explicit recovery boundary. Keep
     * validation entirely before mutation; failure retains both original slots. */
    uint64_t a=identity_index(caller),b=identity_index(peer);
    if(!initialized || switching || current_thread>=thread_capacity ||
       thread_at(current_thread)->state!=THREAD_RUNNING || a==0 ||
       a>=thread_capacity || b>=thread_capacity || b==0 || a==b ||
       a==idle_thread || b==idle_thread || idle_thread>=thread_capacity ||
       (thread_at(idle_thread)->state!=THREAD_READY && thread_at(idle_thread)->state!=THREAD_RUNNING) ||
       (external ? (a==current_thread || b==current_thread ||
         (thread_at(a)->state!=THREAD_READY && thread_at(a)->state!=THREAD_BLOCKED)) :
         (a!=current_thread || thread_at(a)->state!=THREAD_RUNNING)) ||
       (thread_at(b)->state!=THREAD_READY && thread_at(b)->state!=THREAD_BLOCKED) ||
       (io_depth && (io_owner==a || io_owner==b)))return false;
    for(unsigned j=0;j<2;j++) {
        struct thread *t=thread_at(j?b:a);
        if(t->user_context || t->user_thread_context || t->address_space ||
           !t->stack || !t->stack_physical || !t->io_eligible)return false;
    }
    for(unsigned j=0;j<2;j++) {
        uint64_t i=j?b:a;struct thread *t=thread_at(i);
        detach_wait(t);io_gate_leave(t);
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

static void yield_to(uint64_t preferred) {
    uint64_t flags = irq_save();
    if (!initialized || switching) {
        irq_restore(flags);
        return;
    }

    uint64_t old_index = current_thread;
    uint64_t next_index = preferred<thread_capacity && preferred!=old_index &&
        thread_at(preferred)->state==THREAD_READY && io_thread_allowed(preferred)
        ? preferred : next_ready_thread(old_index);

    /* A request is satisfied by a turn with no owner, including ordinary
     * scheduling. An allowed service turn during owned I/O does not consume it.
     * If preempted and excluded again, eligibility records a fresh request. */
    if(!io_depth && thread_at(next_index)->io_gate_waiting) {
        io_gate_cursor=next_index;
        io_gate_leave(thread_at(next_index));
    }

    if (next_index == old_index) {
        if(thread_at(old_index)->trace.active)trace_run(thread_at(old_index),timer_monotonic_us());
        irq_restore(flags);
        return;
    }

    uint64_t account_now=timer_monotonic_us();
    account_current_at(account_now);
    switching = true;
    struct thread *old = thread_at(old_index);
    struct thread *next = thread_at(next_index);
    old->resume_address_space=vmm_current_address_space();
    if (old->user_context != NULL) {
        old->user_fs_base = read_fs_base();
    }

    if (old->state == THREAD_RUNNING) {
        trace_ready(old,0);
        old->state = THREAD_READY;
    }
    if(next->trace.active)trace_run(next,timer_monotonic_us());
    next->state = THREAD_RUNNING;
    next->switches++;
    context_switches++;
    next->account_at=account_now;
    current_thread = next_index;

    /* A kernel continuation can temporarily use the kernel CR3 while still
     * belonging to a userspace process. Preserve the actual suspended CR3;
     * comparing only process identity can return a sibling into unmapped code. */
    uint64_t next_address_space=next->resume_address_space
        ? next->resume_address_space : thread_address_space(next);
    if(next_address_space) vmm_switch_address_space(next_address_space);
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

void scheduler_yield(void) { yield_to(thread_capacity); }

void scheduler_exit_current(void) {
    if (!initialized) {
        for (;;) {
            __asm__ volatile ("hlt");
        }
    }

    io_gate_leave(thread_at(current_thread));
    thread_at(current_thread)->state = THREAD_DEAD;
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
    for (uint64_t i = 0; i < thread_capacity; i++) {
        struct thread *thread = thread_at(i);
        if (thread->state == THREAD_UNUSED ||
            thread->state == THREAD_DEAD ||
            thread->user_context != process ||
            thread->user_thread_context == NULL ||
            thread->user_thread_context == except_user_thread_context) {
            continue;
        }
        detach_wait(thread);io_gate_leave(thread);
        if(i==current_thread) account_current_at(timer_monotonic_us());
        thread->account_user=false;
        thread->user_context = NULL;
        thread->user_thread_context = NULL;
        thread->user_fpu = NULL;
        thread->user_fs_base = 0;
        thread->address_space = 0;
        thread->resume_address_space=0;
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

    struct thread *thread = thread_at(current_thread);
    if(thread->trace.active) {
        if(thread->trace.state)thread->trace.out.invalid++;
        thread->trace.state=1;thread->trace.since=timer_monotonic_us();
        thread->trace.deadline=deadline_ticks;thread->trace.out.blocks++;
    }
    thread->state = THREAD_BLOCKED;
    thread->blocked_queue = queue;
    thread->blocked_deadline_ticks = deadline_ticks;
    queue->waiters++;
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
    if(!queue->waiters) { irq_restore(flags);return 0; }
    uint64_t woken=0,remaining=0;
    /* Membership belongs to the stable thread record, not a slot-sized bitmap.
     * Never let a stale queue wake a reused slot waiting on another queue. */
    for(uint64_t i=0;i<thread_capacity;i++) {
        struct thread *t=thread_at(i);
        if(t->blocked_queue!=queue)continue;
        if(t->state!=THREAD_BLOCKED) { detach_wait(t);continue; }
        if(woken<max_count) {
            trace_ready(t,1);detach_wait(t);t->state=THREAD_READY;t->signal_wakes++;woken++;
        } else remaining++;
    }
    queue->waiters=remaining;
    irq_restore(flags);
    return woken;
}

void scheduler_wake_all(struct scheduler_wait_queue *queue) {
    (void)scheduler_wake_count(queue, UINT64_MAX);
}

static void wake_expired_waits(void) {
    uint64_t now = timer_ticks();
    for (uint64_t i = 0; i < thread_capacity; i++) {
        struct thread *thread = thread_at(i);
        if (thread->state != THREAD_BLOCKED ||
            thread->blocked_deadline_ticks == 0 ||
            now < thread->blocked_deadline_ticks) {
            continue;
        }
        trace_ready(thread,2);detach_wait(thread);
        thread->state = THREAD_READY;
        thread->timeout_wakes++;
    }
}

void scheduler_preempt_tick(void) {
    if (!initialized || switching) {
        return;
    }

    sampled_ticks++;
    thread_at(current_thread)->sampled_ticks++;

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

    struct thread *thread = thread_at(current_thread);
    account_current_at(timer_monotonic_us());
    thread->account_user=false;
    thread->user_context = process;
    thread->io_user_safe = false;
    thread->address_space = address_space;
    thread->resume_address_space=address_space;
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

    struct thread *thread = thread_at(current_thread);
    account_current_at(timer_monotonic_us());
    thread->account_user=false;
    thread->user_context = NULL;
    thread->io_user_safe = false;
    thread->user_thread_context = NULL;
    thread->user_fpu = NULL;
    thread->user_fs_base = 0;
    thread->address_space = 0;
    thread->resume_address_space=0;
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
    return thread_at(current_thread)->user_context;
}

void scheduler_set_user_thread_context(void *context) {
    if (!initialized) {
        return;
    }
    thread_at(current_thread)->user_thread_context = context;
}

void *scheduler_current_user_thread_context(void) {
    if (!initialized) {
        return NULL;
    }
    return thread_at(current_thread)->user_thread_context;
}

uint64_t scheduler_thread_count(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < thread_capacity; i++) {
        if (thread_at(i)->state != THREAD_UNUSED && thread_at(i)->state != THREAD_DEAD) {
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
    struct sample { char name[32]; unsigned state; uint64_t ticks, switches; };
    static struct sample sample[SCHEDULER_MAX_THREADS];
    static bool busy; /* Diagnostic scratch is never placed on a 16 KiB entry stack. */
    uint64_t flags=irq_save(),total=sampled_ticks,preemptions=preemption_count,capacity=thread_capacity;
    if(busy) { irq_restore(flags);return; }busy=true;
    uint64_t storage=thread_storage_bytes,budget=thread_storage_budget;
    for(unsigned i=0;i<capacity;i++) {
        const struct thread *t=thread_at(i);sample[i]=(struct sample){.state=t->state,.ticks=t->sampled_ticks,.switches=t->switches};
        const char *name=t->name?t->name:"unknown";
        unsigned j=0;for(;j<sizeof(sample[i].name)-1 && name[j];j++)sample[i].name[j]=name[j];
        sample[i].name[j]=0;
    }
    irq_restore(flags);
    console_printf("scheduler: capacity=%u storage_bytes=%u budget_bytes=%u\n",capacity,storage,budget);
    console_printf("scheduler: threads=%u preemptions=%u sampled_ticks=%u\n",
        scheduler_thread_count(),preemptions,total);
    for(unsigned i=0;i<capacity;i++) if(sample[i].state!=THREAD_UNUSED)
        console_printf("scheduler: thread=%u name=%s state=%u sampled_ticks=%u switches=%u\n",
            (uint64_t)i,sample[i].name,(uint64_t)sample[i].state,
            sample[i].ticks,sample[i].switches);
    flags=irq_save();busy=false;irq_restore(flags);
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
    if(initialized) for(unsigned i=0;i<thread_capacity;i++) {
        const struct thread *t=thread_at(i);
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
    static struct activity_sample rows[SCHEDULER_MAX_THREADS];
    static bool busy;
    uint64_t flags=irq_save();
    if(busy) { irq_restore(flags);return; }busy=true;irq_restore(flags);
    uint64_t at;
    unsigned count=activity_snapshot(rows,&at);
    console_printf("worker-snapshot: us=%u count=%u version=2\n",at,(uint64_t)count);
    for(unsigned i=0;i<count;i++) {
        const struct activity_sample *v=&rows[i];
        console_printf("worker: us=%u slot=%u id=%u name=%s state=%u ticks=%u switches=%u timeouts=%u signals=%u\n",
            at,v->slot,v->identity,v->name,v->state,v->ticks,v->switches,v->timeouts,v->signals);
    }
    console_printf("worker-snapshot-end: us=%u count=%u\n",at,(uint64_t)count);
    flags=irq_save();busy=false;irq_restore(flags);
}

void scheduler_snapshot(struct scheduler_stats *out) {
    if(!out) return;
    uint64_t flags=irq_save();
    *out=(struct scheduler_stats){sampled_ticks,
        idle_thread<thread_capacity?thread_at(idle_thread)->sampled_ticks:0,
        context_switches,preemption_count};
    irq_restore(flags);
}

static bool idle_eligible(void *arg) { (void)arg;return true; }

static struct scheduler_idle_audit idle_audit;
static bool idle_audit_active;
bool scheduler_idle_audit_start(void) {
    uint64_t flags=irq_save();
    if(idle_audit_active || !initialized || idle_thread>=thread_capacity || idle_audit.epoch==UINT64_MAX) {
        irq_restore(flags);return false;
    }
    idle_audit=(struct scheduler_idle_audit){.epoch=idle_audit.epoch+1};
    idle_audit_active=true;irq_restore(flags);return true;
}
void scheduler_idle_audit_snapshot(struct scheduler_idle_audit *out) {
    uint64_t flags=irq_save();*out=idle_audit;irq_restore(flags);
}
void scheduler_idle_audit_stop(struct scheduler_idle_audit *out) {
    uint64_t flags=irq_save();idle_audit_active=false;*out=idle_audit;irq_restore(flags);
}
static uint64_t idle_audit_enter(void) {
    if(!idle_audit_active) return 0;
    idle_audit.enters++;return idle_audit.epoch;
}
static void idle_audit_return(uint64_t epoch) {
    if(idle_audit_active && epoch && epoch==idle_audit.epoch) idle_audit.returns++;
}
/* Dedicated WB kernel storage, isolated from counters/other writers. CPU0's
 * interrupt is the wake source in this experiment; this is not an AP protocol. */
static _Alignas(64) volatile unsigned char mwait_line[64];
static struct scheduler_mwait_test mwait_test;
static bool mwait_active;
_Static_assert(SCHEDULER_MWAIT_LIMIT_US==SCHEDULER_PREEMPT_QUANTUM_TICKS*10000,
    "wakeup ceiling must remain one existing scheduler quantum");
bool scheduler_mwait_test_start(uint32_t hint) {
    uint64_t flags=irq_save();
    bool ok=(hint==0 || hint==0x10) && !mwait_active && idle_audit_active && initialized &&
        idle_thread<thread_capacity && timer_reference_ready();
    if(ok) { mwait_test=(struct scheduler_mwait_test){.epoch=idle_audit.epoch,.hint=hint};mwait_active=true; }
    irq_restore(flags);return ok;
}
void scheduler_mwait_test_stop(struct scheduler_mwait_test *out) {
    uint64_t flags=irq_save();mwait_active=false;*out=mwait_test;irq_restore(flags);
}
/* These fixed read-only registers are reachable only after the power service's
 * exact 06_45/invariant-TSC gate. No inferred wrap or residency on invalid data. */
static uint64_t mwait_read(unsigned reg) {
#if defined(SRVROS_MWAIT_HOST_TEST)
    extern uint64_t scheduler_test_mwait_read(unsigned);
    return scheduler_test_mwait_read(reg);
#elif defined(SRVROS_SCHEDULER_HOST_TEST)
    (void)reg;return 0;
#else
    uint32_t a,d;__asm__ volatile("rdmsr":"=a"(a),"=d"(d):"c"(reg));return a|((uint64_t)d<<32);
#endif
}
static bool mwait_step(void) {
    (void)mwait_line;
    if(!mwait_active || !idle_audit_active || mwait_test.epoch!=idle_audit.epoch) return false;
    if(mwait_test.fault || mwait_test.attempts>=SCHEDULER_MWAIT_ATTEMPTS) {
        if(mwait_test.hlt<SCHEDULER_MWAIT_ATTEMPTS)mwait_test.hlt++;
        return false;
    }
#if defined(SRVROS_MWAIT_HOST_TEST)
    extern void scheduler_test_monitor(void *);
    scheduler_test_monitor((void *)mwait_line);
#elif !defined(SRVROS_SCHEDULER_HOST_TEST)
    __asm__ volatile("monitor"::"a"(mwait_line),"c"(0),"d"(0):"memory");
#endif
    /* Recheck after MONITOR with IRQs still masked. Pending interrupts break
     * MWAIT via ECX=1, including the check-to-wait window. No unowned wake word. */
    if(has_ready_thread_except(current_thread)) {
        mwait_test.rechecks++;return true;
    }
    if(!mwait_test.attempts)mwait_test.tick_begin=timer_ticks();
    bool deep=mwait_test.hint==0x10;
#ifdef SRVROS_AP_PROBE
    bool coordinated=deep && ap_c3_active();
    uint64_t coord_start=coordinated?timer_monotonic_us():0;
    if(coordinated && !ap_c3_prepare(mwait_test.epoch,mwait_test.attempts+1,coord_start)) {
        mwait_test.fault=4;return false;
    }
#endif
    uint64_t t0=0,c0=0,p0=0,dt=0,dc=0,dp=0;
    if(deep) { t0=mwait_read(0x10);c0=mwait_read(0x3fc);p0=mwait_read(0x3f8); }
    uint64_t before=timer_monotonic_us();mwait_test.attempts++;
#if defined(SRVROS_MWAIT_HOST_TEST)
    extern void scheduler_test_mwait(uint32_t);
    scheduler_test_mwait((uint32_t)mwait_test.hint);
#elif !defined(SRVROS_SCHEDULER_HOST_TEST)
    __asm__ volatile("mwait"::"a"((uint32_t)mwait_test.hint),"c"(1):"memory"); /* Exact gated C1 or Haswell C3. */
#endif
    uint64_t after=timer_monotonic_us();mwait_test.returns++;
    mwait_test.tick_end=timer_ticks();
    if(deep) {
        uint64_t p1=mwait_read(0x3f8),c1=mwait_read(0x3fc),t1=mwait_read(0x10);
        if(t1<=t0 || c1<c0 || p1<p0 || c1-c0>t1-t0 || p1-p0>t1-t0 ||
           UINT64_MAX-mwait_test.tsc_ticks<t1-t0 ||
           UINT64_MAX-mwait_test.core_c3_ticks<c1-c0 || UINT64_MAX-mwait_test.package_c3_ticks<p1-p0)
            mwait_test.fault=3;
        else { dt=t1-t0;dc=c1-c0;dp=p1-p0;mwait_test.tsc_ticks+=dt;mwait_test.core_c3_ticks+=dc;mwait_test.package_c3_ticks+=dp; }
    }
    if(after<before) mwait_test.fault=1;
    else {
        uint64_t elapsed=after-before;
        if(elapsed>mwait_test.max_us)mwait_test.max_us=elapsed;
        if(elapsed>SCHEDULER_MWAIT_LIMIT_US)mwait_test.fault=2;
    }
#ifdef SRVROS_AP_PROBE
    if(coordinated && !ap_c3_finish(mwait_test.attempts,coord_start,dt,dc,dp,!mwait_test.fault) && !mwait_test.fault)
        mwait_test.fault=4;
#endif
    /* Measure before pending IRQ dispatch/preemption; then let that IRQ run.
     * There are no accesses to trial state after this scheduling boundary. */
#ifndef SRVROS_SCHEDULER_HOST_TEST
    __asm__ volatile("sti; nop; cli":::"memory");
#endif
    return true;
}
static void idle_step(void) {
    uint64_t flags=irq_save();
    if(has_ready_thread_except(current_thread)) {
        if(idle_audit_active) idle_audit.ready++;
        irq_restore(flags);scheduler_yield();return;
    }
    if(mwait_step()) { irq_restore(flags);return; }
    /* The default/fallback diagnostic does not change the readiness check or STI shadow.
     * No clocks, I/O, allocation or printing in this IRQ-masked path. */
    uint64_t epoch=idle_audit_enter();
#ifndef SRVROS_SCHEDULER_HOST_TEST
    __asm__ volatile("sti; hlt; cli":::"memory");
#endif
    idle_audit_return(epoch);
    irq_restore(flags);
}
static void idle_worker(void *arg) {
    (void)arg;
    for(;;) idle_step();
}

bool scheduler_start_idle_worker(void) {
    if(idle_thread<thread_capacity) return true;
    if(!scheduler_spawn_io_service("idle",idle_worker,NULL,idle_eligible)) return false;
    for(uint64_t i=1;i<thread_capacity;i++) if(thread_at(i)->entry==idle_worker) {
        idle_thread=i;return true;
    }
    return false;
}

/* Monotonic lifetime identity; a recycled worker slot receives a fresh ID. */
uint64_t scheduler_current_thread_id(void) {
    uint64_t flags=irq_save();
    uint64_t id=initialized?thread_at(current_thread)->identity:0;
    irq_restore(flags);return id;
}
