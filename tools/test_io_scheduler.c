#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <setjmp.h>
#include <stdbool.h>
#include <stdint.h>
/* Real fixture memory remains writable host backing; this logical HHDM view
 * makes the production extent predicate testable without mapping kernel VAs. */
static bool invalid_stack_view;
static uint64_t fixture_fs;
static uint64_t fixture_irq_flags;
static unsigned fixture_fs_writes;
static uint64_t scheduler_test_irq_save(void) { uint64_t flags=fixture_irq_flags;fixture_irq_flags&=~0x200ull;return flags; }
static void scheduler_test_irq_restore(uint64_t flags) { fixture_irq_flags=flags; }
static uint64_t scheduler_test_read_fs(void) { return fixture_fs; }
static void scheduler_test_write_fs(uint64_t value) { fixture_fs=value;fixture_fs_writes++; }
static uint64_t scheduler_test_stack_address(uint64_t physical,const void *backing) {
    (void)backing;
    return invalid_stack_view ? 0x1000 : (0xffff800000000000ull | (physical & 0x00007ffffffff000ull));
}
static uint64_t scheduler_test_kernel_address(uint64_t address) {
    return 0xffff800000000000ull | address;
}
#define SRVROS_SCHEDULER_HOST_TEST
#define SRVROS_AP_PROBE
#define SRVROS_MWAIT_HOST_TEST
#include "../kernel/src/scheduler.c"
static uint64_t actual_space=1,clock_us,kernel_space=1;
static unsigned clock_calls;
static bool peer_enabled,peer_prepare_fail,peer_finish_fail;
static unsigned peer_prepares,peer_finishes;
bool ap_c3_active(void) { return peer_enabled; }
bool ap_c3_prepare(uint64_t epoch,uint64_t seq,uint64_t start) {
    assert(epoch && seq && seq<=32 && start==clock_us);peer_prepares++;return !peer_prepare_fail;
}
bool ap_c3_finish(uint64_t seq,uint64_t start,uint64_t tsc,uint64_t core,uint64_t package,bool waited) {
    assert(seq && seq<=32 && clock_us>=start);peer_finishes++;
    if(waited)assert(tsc && core<=tsc && package<=tsc);
    return waited && !peer_finish_fail;
}

static _Alignas(16) uint8_t fake_stack[SCHEDULER_STACK_SIZE];
static unsigned frees;
static unsigned stack_attempts;
static bool growing_allocations,fail_stack,fail_records;
static unsigned live_allocations;

uint64_t pmm_alloc_frames_above_tagged(uint64_t count,uint64_t minimum,enum pmm_frame_tag tag) {
    if(tag==PMM_FRAME_KERNEL_STACK)stack_attempts++;
    assert(minimum==SCHEDULER_STACK_MIN_PHYSICAL);
    if(!growing_allocations) { assert(count==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE && tag==PMM_FRAME_KERNEL_STACK);return 4096; }
    assert((tag==PMM_FRAME_KERNEL_STACK && count==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE) || (tag==PMM_FRAME_GENERAL && count==SCHEDULER_CHUNK_FRAMES));
    if((tag==PMM_FRAME_KERNEL_STACK && fail_stack) || (tag==PMM_FRAME_GENERAL && fail_records))return 0;
    void *p=aligned_alloc(PMM_FRAME_SIZE,count*PMM_FRAME_SIZE);assert(p);memset(p,0xa5,count*PMM_FRAME_SIZE);live_allocations++;return (uintptr_t)p;
}
void pmm_free_frame_range(uint64_t physical,uint64_t count) { if(growing_allocations) { assert(physical && live_allocations);free((void *)(uintptr_t)physical);live_allocations--; }
    else { assert(physical==4096 && count==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE); }
    frees++; }
void *pmm_phys_to_virt(uint64_t physical) { if(growing_allocations)return (void *)(uintptr_t)physical;assert(physical==4096);return fake_stack; }
void fpu_init_state(struct fpu_state *state) { if(state)state->active_user_trap=NULL; }
void fpu_invalidate_user_trap(struct fpu_state *state) { if(state)state->active_user_trap=NULL; }
static struct process_user_binding resolved_binding,resolved_parent_binding;
static void *fixture_parent_worker;
static bool resolver_ok=true,prepare_ok=true;
static unsigned fpu_set_calls,tss_set_calls;
static uint64_t fixture_tss;
bool process_resolve_user_binding(void *p,void *worker,uint64_t generation,uint64_t as,struct process_user_binding *out) {
    (void)p;(void)generation;(void)as;if(!resolver_ok)return false;
    *out=worker && worker!=fixture_parent_worker ? resolved_binding : resolved_parent_binding;return true;
}
bool process_admit_user_worker(void *p,void *worker,uint64_t generation,uint64_t as,struct process_user_binding *out) {
    return process_resolve_user_binding(p,worker,generation,as,out);
}
bool vmm_prepare_user_kernel_mappings(uint64_t as) { (void)as;return prepare_ok; }
static jmp_buf bind_failure;
static unsigned bind_diagnostics;
void serial_write(const char *s) { assert(!(fixture_irq_flags&0x200) && !strcmp(s,"scheduler: invalid user binding\n"));bind_diagnostics++; }
__attribute__((noreturn)) void halt_forever(void) { longjmp(bind_failure,1); }
static bool preempt_cancel;
static jmp_buf preempt_cancel_exit;
bool process_should_exit_current(void) { return preempt_cancel; }
uint64_t process_current_kill_status(void) { if(preempt_cancel)return 130;assert(!"unexpected cancellation status");abort(); }
__attribute__((noreturn)) void process_exit(uint64_t status) {
    if(preempt_cancel) {assert(status==130 && !io_depth && !threads[current_thread].io_user_safe);longjmp(preempt_cancel_exit,1);}
    (void)status;assert(!"unexpected process exit");abort();
}
static void empty_worker(void *arg) {(void)arg;}

uint64_t timer_monotonic_us(void) { clock_calls++; return clock_us; }
uint64_t timer_ticks(void) { return clock_us/10000; }
uint64_t vmm_current_address_space(void) { return actual_space; }
uint64_t vmm_kernel_address_space(void) { return kernel_space; }
void vmm_switch_address_space(uint64_t space) { actual_space=space; }
uint64_t gdt_default_kernel_stack_top(void) { return 0x1000; }
void gdt_set_kernel_stack(uint64_t stack) { fixture_tss=stack;tss_set_calls++; }
void fpu_switch_kernel_state(struct fpu_state *old,struct fpu_state *next,struct fpu_state *user) {
    (void)old;(void)next;(void)user;
}
static unsigned switches;
static bool return_timer_test,return_timer_publish,return_timer_allowed;
static unsigned return_timer_turns;
static bool foreign_timer_test;
static unsigned foreign_timer_owner_turns, foreign_timer_service_turns;
static bool address_space_test;
static uint64_t expected_space,return_space;
static bool completion_wait_test,completion_signal,completion_ready,reference=true;
bool timer_reference_ready(void) { return reference; }
static struct scheduler_wait_queue completion_queue;
static bool high_wait_test;
static bool handoff_test;
static bool fair_handoff_test;
static unsigned fair_handoff_turns[2];
static bool retry_turn_test,ready_edge_test;
static unsigned ready_edge_turns;
static unsigned trace_test;
void fpu_set_current_user_state(struct fpu_state *state) { (void)state;fpu_set_calls++; }
static unsigned handoff_admissions;
static struct scheduler_wait_queue high_wait;
static bool completion_condition(void *arg) { assert(arg==&completion_ready);return completion_ready; }
void scheduler_context_switch(struct scheduler_context *old,struct scheduler_context *next) {
    if(return_timer_test) {
        assert(old==&threads[4].context && next==&threads[3].context && current_thread==3);
        assert(io_depth==1 && io_owner==3 && !threads[4].account_user);
        assert(threads[4].io_user_safe==return_timer_publish);
        assert(io_thread_allowed(4)==return_timer_allowed);
        if(!return_timer_allowed)scheduler_io_end();
        else assert(next_ready_thread(3)==4); /* Selection under the same owner. */
        return_timer_turns++;
        threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;current_thread=4;
        return;
    }
    if(foreign_timer_test) {
        assert(threads[4].state==THREAD_READY && !threads[4].io_user_safe && !threads[4].account_user);
        assert(io_depth==1 && io_owner==3 && !io_thread_allowed(4));
        if(current_thread==5) {
            assert(old==&threads[4].context && next==&threads[5].context);
            assert(threads[5].io_user_safe && io_thread_allowed(5));
            foreign_timer_service_turns++;
            switching=false; /* Model execution after the service's switch returns. */
            scheduler_yield(); /* Actual scheduler selects owner 3 while 4 is excluded. */
            assert(current_thread==4 && !io_depth && !threads[4].io_user_safe);
            return;
        }
        assert(old==&threads[5].context && next==&threads[3].context && current_thread==3);
        foreign_timer_owner_turns++;
        scheduler_io_end(); /* The selected owner completes its own I/O. */
        assert(!io_depth && io_thread_allowed(4) && !threads[4].io_user_safe);
        threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;current_thread=4;switches+=2;
        return;
    }
    if(ready_edge_test) {
        assert(old==&threads[3].context && next==&threads[6].context && current_thread==6 && !io_depth);
        assert(!threads[6].io_gate_waiting);ready_edge_turns++;
        threads[6].state=THREAD_READY;threads[3].state=THREAD_RUNNING;current_thread=3;return;
    }
    if(fair_handoff_test) {
        assert(old==&threads[3].context && (current_thread==6 || current_thread==7));
        assert(next==&threads[current_thread].context && !io_depth);
        assert(!threads[current_thread].io_gate_waiting);
        fair_handoff_turns[current_thread-6]++;
        threads[current_thread].state=THREAD_READY;
        threads[3].state=THREAD_RUNNING;current_thread=3;
        return;
    }
    if(trace_test) {
        if(current_thread==1) {
            assert(old==&threads[0].context && threads[0].state==THREAD_BLOCKED);
            assert(threads[0].trace.out.blocks==1);
            assert(threads[0].memory_park_queue==threads[0].blocked_queue && threads[0].memory_park_queue);
            switching=false;clock_us+=10000;wake_expired_waits();
            assert(threads[0].state==THREAD_READY && threads[0].trace.out.timeouts==1);
            assert(threads[0].memory_park_queue && !threads[0].blocked_queue);
            io_depth=1;io_owner=1;
            assert(!io_thread_allowed(0)); /* Recording must not admit the unsafe thread. */
            clock_us+=520000;assert(!io_thread_allowed(0));
            io_depth=0;scheduler_yield();
            assert(current_thread==0);return;
        }
        assert(current_thread==0 && old==&threads[1].context && next==&threads[0].context);
        assert(!threads[0].memory_park_queue);
        return;
    }
    if(retry_turn_test) {
        assert(old==&threads[3].context && next==&threads[6].context && current_thread==6);
        assert(threads[6].io_gate_waiting==(io_depth!=0));
        assert(io_gate_pending==(io_depth!=0));
        threads[6].state=THREAD_READY;threads[3].state=THREAD_RUNNING;current_thread=3;
        return;
    }
    if(handoff_test) {
        assert(old==&threads[3].context && next==&threads[6].context);
        assert(current_thread==6 && !io_depth && !threads[6].io_gate_waiting);
        assert(threads[4].state==THREAD_READY); /* Do not merely yield to the next slot. */
        switching=false; /* The resumed scheduler_yield continuation clears it. */
        scheduler_io_wait();
        assert(!threads[6].io_gate_waiting && !io_gate_pending);
        assert(scheduler_io_begin()); /* App enters before writer's next operation. */
        assert(!io_thread_allowed(4)); /* Unsafe peers still cannot overlap it. */
        io_gate_leave(&threads[4]); /* This mock peer does not run a continuation. */
        scheduler_io_end();handoff_admissions++;
        threads[6].state=THREAD_READY;threads[3].state=THREAD_RUNNING;current_thread=3;
        return;
    }
    if(high_wait_test) {
        assert(old==&thread_at(95)->context && thread_at(95)->state==THREAD_BLOCKED);
        assert(high_wait.waiters==1 && thread_at(95)->blocked_queue==&high_wait);
        assert(scheduler_wake_count(&high_wait,1)==1 && !high_wait.waiters);
        thread_at(current_thread)->state=THREAD_READY;current_thread=95;thread_at(95)->state=THREAD_RUNNING;
        return;
    }
    if(address_space_test) {
        assert(old==&threads[0].context && next==&threads[1].context);
        assert(current_thread==1 && actual_space==expected_space);
        current_thread=0;actual_space=return_space;threads[0].state=THREAD_RUNNING;threads[1].state=THREAD_READY;
        return;
    }
    if(completion_wait_test) {
        assert(old==&threads[3].context && next==&threads[1].context);
        assert(current_thread==1 && threads[3].state==THREAD_BLOCKED);
        assert(io_depth==2 && io_owner==3 && !io_thread_allowed(5));
        assert(completion_queue.waiters==1);
        if(completion_signal) { completion_ready=true;scheduler_wake_all(&completion_queue); }
        else { clock_us+=10000;wake_expired_waits(); }
        assert(threads[3].state==THREAD_READY && !completion_queue.waiters);
        threads[1].state=THREAD_READY;threads[3].state=THREAD_RUNNING;
        current_thread=3;actual_space=2;switches++;return;
    }
    assert(old==&threads[4].context && next==&threads[3].context);
    assert(current_thread==3 && threads[4].state==THREAD_BLOCKED);
    assert(io_waiters.waiters==1);
    scheduler_io_end(); /* Simulate owner completing while caller sleeps. */
    assert(threads[4].state==THREAD_READY && !io_waiters.waiters);
    threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;
    current_thread=4;switches++;
}
static bool retired_context(void *arg) { (void)arg;assert(!"retired eligibility callback");return false; }
static bool ready(void *arg) { return arg != NULL; }
static void test_wake_policy(void) {
    memset(threads,0,sizeof(threads));initialized=true;
    struct scheduler_wait_queue queue={0},other_queue={0};
    for(unsigned i=1;i<8;i++) {
        threads[i]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&queue};queue.waiters++;
    }
    threads[8]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&other_queue};other_queue.waiters=1;
    assert(!scheduler_wake_count(&queue,0) && queue.waiters==7);
    /* A finite cohort drains exactly once, without waking another queue. */
    for(unsigned i=1;i<8;i++) {
        assert(scheduler_wake_count(&queue,1)==1 && queue.waiters==7-i);
        assert(threads[i].state==THREAD_READY && !threads[i].blocked_queue && threads[i].signal_wakes==1);
    }
    assert(!scheduler_wake_count(&queue,1) && other_queue.waiters==1);
    /* Selection is slot-ordered, not FIFO: re-parking a low slot can repeatedly
     * precede an older high-slot waiter. Document this, do not promise fairness. */
    threads[7].state=THREAD_BLOCKED;threads[7].blocked_queue=&queue;queue.waiters=1;
    for(unsigned i=0;i<16;i++) {
        threads[1].state=THREAD_BLOCKED;threads[1].blocked_queue=&queue;queue.waiters++;
        assert(scheduler_wake_count(&queue,1)==1 && threads[1].state==THREAD_READY);
        assert(threads[7].state==THREAD_BLOCKED && queue.waiters==1);
    }
    scheduler_wake_all(&queue);assert(!queue.waiters && threads[7].state==THREAD_READY);
    scheduler_wake_all(&other_queue);assert(!other_queue.waiters);
    memset(threads,0,sizeof(threads));initialized=false;
    puts("scheduler-wake-policy: PASS bounded cohort, queue isolation, counts and slot-order limit");
}
static void test_ready_gate_admission(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    current_thread=3;idle_thread=1;io_depth=io_gate_pending=0;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)3,.io_user_safe=true};
    threads[4]=(struct thread){.state=THREAD_READY};
    threads[6]=(struct thread){.state=THREAD_READY}; /* Fresh process, not in io_waiters. */
    assert(scheduler_io_begin());
    assert(!io_thread_allowed(6));
    assert(threads[6].io_gate_waiting && io_gate_pending==1 && !io_waiters.waiters);
    assert(!io_thread_allowed(6) && io_gate_pending==1); /* Repeated exclusion is idempotent. */
    scheduler_io_end();handoff_test=true;
    scheduler_io_handoff();assert(handoff_admissions==1 && !io_gate_pending);
    /* The same exclusion can happen after user context is cleared for retirement. */
    threads[6].user_context=(void*)42;threads[6].io_user_safe=true;
    threads[6].identity=6;
    current_thread=6;threads[6].state=THREAD_RUNNING;threads[3].state=THREAD_READY;
    kernel_space=0x1000;
    scheduler_clear_user_context();
    kernel_space=1;
    current_thread=3;threads[3].state=THREAD_RUNNING;threads[6].state=THREAD_READY;
    assert(scheduler_io_begin() && !io_thread_allowed(6));
    assert(threads[6].io_gate_waiting && !threads[6].user_context);
    scheduler_io_end();scheduler_io_handoff();assert(handoff_admissions==2 && !io_gate_pending);
    handoff_test=false;handoff_admissions=0;
    /* Nonblocking pipe retries/reaping stay runnable during owned I/O, but their request
     * survives those service turns and is consumed only at quiescence. */
    threads[6].user_context=(void*)42;threads[6].io_user_safe=true;
    assert(scheduler_io_begin());
    scheduler_io_request_turn();assert(!io_gate_pending); /* Owner is not deferred. */
    current_thread=6;threads[6].state=THREAD_RUNNING;threads[3].state=THREAD_READY;
    for(unsigned n=0;n<64;n++)scheduler_io_request_turn();
    assert(io_gate_pending==1 && threads[6].state==THREAD_RUNNING && !threads[6].blocked_queue);
    assert(io_thread_allowed(6)); /* No service eligibility was narrowed or widened. */
    current_thread=3;threads[3].state=THREAD_RUNNING;threads[6].state=THREAD_READY;
    retry_turn_test=true;
    for(unsigned n=0;n<64;n++) { yield_to(6);assert(io_gate_pending==1); }
    scheduler_io_end();scheduler_io_handoff();assert(!io_gate_pending);
    scheduler_io_request_turn();assert(!io_gate_pending); /* No owner, no request. */
    retry_turn_test=false;
    threads[6].user_context=NULL;threads[6].state=THREAD_BLOCKED;
    assert(scheduler_io_begin() && !io_thread_allowed(6) && !io_gate_pending);
    scheduler_io_end(); /* Sleeping threads are neither requested nor woken. */
    memset(threads,0,sizeof(threads));current_thread=0;idle_thread=UINT64_MAX;initialized=false;
    puts("io-admission: PASS startup/retirement exclusion, idempotent nonblocking retry, owned service retention, quiescent consumption and no sleeper wake");
}
static void test_gate_handoff(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    current_thread=3;idle_thread=1;io_depth=io_gate_pending=0;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)3,.io_user_safe=true};
    threads[4]=(struct thread){.state=THREAD_READY};
    handoff_test=true;
    for(unsigned round=0;round<64;round++) {
        assert(scheduler_io_begin() && scheduler_io_begin());
        threads[6]=(struct thread){.state=THREAD_BLOCKED,.io_gate_waiting=true,.blocked_queue=&io_waiters};
        io_waiters.waiters=io_gate_pending=1;
        scheduler_io_handoff();assert(handoff_admissions==round && io_depth==2);
        scheduler_io_end();scheduler_io_handoff();assert(handoff_admissions==round && io_depth==1);
        scheduler_io_end();assert(threads[6].state==THREAD_READY && !io_waiters.waiters && io_gate_pending==1);
        /* Returning to userspace is required; a partial kernel operation may
         * not hand off even after dropping its innermost I/O scope. */
        threads[3].io_user_safe=false;scheduler_io_handoff();assert(handoff_admissions==round);
        threads[3].io_user_safe=true;scheduler_io_handoff();assert(handoff_admissions==round+1);
        scheduler_io_handoff();assert(handoff_admissions==round+1);
    }
    handoff_test=false;
    /* Forced retirement must not leave a phantom admission or reuse its slot. */
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.io_gate_waiting=true,
        .blocked_queue=&io_waiters,.user_context=(void*)42,.user_thread_context=(void*)43};
    io_gate_pending=io_waiters.waiters=1;
    scheduler_kill_user_threads((void*)42,NULL);
    assert(!io_gate_pending && !io_waiters.waiters && !threads[6].io_gate_waiting && threads[6].state==THREAD_DEAD);
    memset(threads,0,sizeof(threads));current_thread=0;idle_thread=UINT64_MAX;initialized=false;
    puts("io-handoff: PASS 64 continuous writer rounds, awakened app admission, nested ownership, unsafe caller refusal and canceled waiter retirement");
}
static void ready_edge_setup(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    current_thread=3;idle_thread=UINT64_MAX;io_depth=io_gate_pending=io_gate_cursor=0;
    actual_space=1;clock_us=1000;ready_edge_turns=0;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)3,.io_user_safe=true};
}
static void ready_edge_handoff(void) {
    ready_edge_test=true;scheduler_io_handoff();ready_edge_test=false;
    assert(ready_edge_turns==1 && !io_gate_pending && io_gate_cursor==6);
}
static void test_ready_edge_admission(void) {
    ready_edge_setup();
    for(unsigned i=0;i<6;i++)if(i!=3)threads[i].state=THREAD_READY;
    assert(scheduler_spawn("new child",empty_worker,NULL));
    /* No timer selection, I/O refusal or syscall wait has happened yet. */
    ready_edge_handoff();
    /* Preemption/explicit yield of an unsafe continuation is another READY
     * transition; no owned-I/O selection needs to have refused it first. */
    ready_edge_setup();threads[3].user_context=NULL;
    threads[6]=(struct thread){.state=THREAD_READY,.user_context=(void *)1,.io_user_safe=true};
    ready_edge_test=true;scheduler_yield();ready_edge_test=false;
    assert(ready_edge_turns==1 && threads[3].io_gate_waiting && io_gate_pending==1);
    io_gate_leave(&threads[3]);
    ready_edge_setup();threads[3].user_context=(void *)1;
    threads[6]=(struct thread){.state=THREAD_READY,.user_context=(void *)2,.io_user_safe=true};
    ready_edge_test=true;scheduler_yield();ready_edge_test=false;assert(!io_gate_pending);
    ready_edge_setup();struct scheduler_wait_queue q={.waiters=1};
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&q};
    assert(scheduler_wake_count(&q,1)==1 && !q.waiters);
    assert(!scheduler_wake_count(&q,1));ready_edge_handoff();
    ready_edge_setup();q=(struct scheduler_wait_queue){.waiters=1};
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&q,.blocked_deadline_ticks=2};
    assert(scheduler_io_begin() && scheduler_io_begin());clock_us=20000;wake_expired_waits();
    assert(!q.waiters && threads[6].state==THREAD_READY && io_gate_pending==1);
    scheduler_io_handoff();assert(!ready_edge_turns && io_depth==2);
    scheduler_io_end();scheduler_io_handoff();assert(!ready_edge_turns && io_depth==1);
    scheduler_io_end();ready_edge_handoff();
    /* Safe userspace and explicitly audited service workers do not request a
     * privileged turn merely because they wake. Existing selection stays intact. */
    ready_edge_setup();q=(struct scheduler_wait_queue){.waiters=2};
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&q,.user_context=(void *)1,.io_user_safe=true};
    threads[7]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&q,.io_eligible=idle_eligible};
    scheduler_wake_all(&q);assert(!io_gate_pending);
    ready_edge_setup();q=(struct scheduler_wait_queue){.waiters=1};
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.blocked_queue=&q,.user_context=(void *)42,.user_thread_context=(void *)43};
    scheduler_wake_all(&q);assert(io_gate_pending==1);
    scheduler_kill_user_threads((void *)42,NULL);
    assert(!io_gate_pending && threads[6].state==THREAD_DEAD);
    scheduler_io_handoff();assert(!ready_edge_turns);
    /* A transport completion can wake the current I/O owner. It is already
     * eligible and must not reserve an unnecessary later quiescent turn. */
    ready_edge_setup();q=(struct scheduler_wait_queue){.waiters=1};
    assert(scheduler_io_begin());
    threads[3].state=THREAD_BLOCKED;threads[3].blocked_queue=&q;threads[3].io_user_safe=false;
    scheduler_wake_all(&q);assert(!io_gate_pending && threads[3].state==THREAD_READY);
    threads[3].state=THREAD_RUNNING;scheduler_io_end();
    ready_edge_setup();assert(scheduler_spawn_io_service("audited",empty_worker,NULL,idle_eligible));
    assert(!io_gate_pending);
    ready_edge_setup();io_services_enabled=false;
    assert(scheduler_spawn("boot worker",empty_worker,NULL) && !io_gate_pending);
    initialized=io_services_enabled=false;memset(threads,0,sizeof(threads));current_thread=0;
    puts("ready-edge-admission: PASS creation and unsafe signal/timeout wake request the first quiescent boundary; no active-owner bypass");
}

static void test_gate_handoff_fairness(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    current_thread=3;idle_thread=1;io_depth=io_gate_pending=0;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)3,.io_user_safe=true};
    threads[6]=(struct thread){.state=THREAD_READY};
    threads[7]=(struct thread){.state=THREAD_READY};
    memset(fair_handoff_turns,0,sizeof(fair_handoff_turns));fair_handoff_test=true;
    for(unsigned round=0;round<32;round++) {
        assert(scheduler_io_begin());
        /* Both contenders request every gap, as repeated nohang reap and
         * unsafe exit/worker continuations can do under continuous writes. */
        io_gate_request(&threads[6]);io_gate_request(&threads[7]);
        scheduler_io_end();scheduler_io_handoff();
        assert(fair_handoff_turns[0]+fair_handoff_turns[1]==round+1);
    }
    fair_handoff_test=false;
    assert(fair_handoff_turns[0]==16 && fair_handoff_turns[1]==16);
    io_gate_leave(&threads[6]);io_gate_leave(&threads[7]);assert(!io_gate_pending);
    puts("io-handoff-fairness: PASS recurring waiter cannot starve a second contender");
}
static void test_completion_wait(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    clock_us=100000;current_thread=3;idle_thread=1;actual_space=2;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.address_space=2};
    threads[1]=(struct thread){.state=THREAD_READY,.io_eligible=idle_eligible};
    threads[5]=(struct thread){.state=THREAD_READY};io_owner=3;io_depth=2;
    completion_wait_test=true;
    assert(!scheduler_io_wait_timeout(NULL,completion_condition,&completion_ready,11));
    assert(!scheduler_io_wait_timeout(&completion_queue,NULL,&completion_ready,11));
    assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,10));
    assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,12));
    actual_space=1;assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));actual_space=2;
    io_owner=5;assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));io_owner=3;
    reference=false;assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));reference=true;
    io_services_enabled=false;assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));io_services_enabled=true;
    threads[1].state=THREAD_BLOCKED;
    assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));
    threads[1].state=THREAD_READY;
    completion_ready=true;unsigned before=switches;
    assert(scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11) && switches==before);
    completion_ready=false;completion_signal=true;
    assert(scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));
    assert(switches==before+1 && threads[3].signal_wakes==1 && io_depth==2);
    completion_ready=completion_signal=false;
    assert(scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,11));
    assert(clock_us==110000 && threads[3].timeout_wakes==1 && io_depth==2);
    struct io_completion_wait expired={completion_condition,&completion_ready,11};assert(io_completion_ready(&expired));
    clock_us=0;assert(!scheduler_io_wait_timeout(&completion_queue,completion_condition,&completion_ready,1));
    completion_wait_test=false;io_depth=0;
    puts("io-completion-wait: PASS owner/address/timer/bound refusal, ready/race, early wake, expiry, gate retained and idle fallback");
}
static uint64_t user_charged,system_charged;
static bool check_accounting;
void process_account_cpu(void *process,void *thread,bool user,uint64_t elapsed) {
    if(!check_accounting) return;
    assert(process==(void*)42 && thread==(void*)43);
    if(user) user_charged+=elapsed;else system_charged+=elapsed;
}
static void test_cpu_accounting(void) {
    memset(threads,0,sizeof(threads));initialized=true;current_thread=0;clock_us=100;
    threads[0].user_context=(void*)42;threads[0].user_thread_context=(void*)43;
    threads[0].account_at=100;check_accounting=true;
    clock_us=140;scheduler_user_enter();assert(system_charged==40 && user_charged==0);
    clock_us=240;scheduler_account_kernel_enter();assert(user_charged==100);
    clock_us=250;scheduler_account_now();assert(system_charged==50);
    /* Model switch-out / switch-in: only the actual running interval is charged. */
    threads[0].account_at=10000;clock_us=10020;scheduler_account_now();assert(system_charged==70);
    clock_us=10010;scheduler_account_now();assert(system_charged==70); /* No wrap on bad clock. */
    check_accounting=false;memset(threads,0,sizeof(threads));initialized=false;clock_us=0;
    puts("cpu-accounting: PASS user/kernel intervals, blocked-time exclusion, monotonic guard");
}
static void test_resume_address_space(void) {
    memset(threads,0,sizeof(threads));initialized=true;current_thread=0;actual_space=1;
    threads[0]=(struct thread){.state=THREAD_RUNNING,.address_space=2};
    threads[1]=(struct thread){.state=THREAD_READY,.address_space=2};
    threads[0].kernel_fpu.active_user_trap=(void *)0x123;
    threads[1].kernel_fpu.active_user_trap=(void *)0x456;
    address_space_test=true;expected_space=2;return_space=1;
    scheduler_yield();assert(threads[0].resume_address_space==1 && actual_space==1);
    /* Resume the sibling's temporary kernel continuation, even though its
     * process identity still names address space two. */
    actual_space=2;threads[1].resume_address_space=1;expected_space=1;return_space=2;
    scheduler_yield();assert(threads[0].resume_address_space==2 && actual_space==2);
    assert(threads[0].kernel_fpu.active_user_trap==(void *)0x123 && threads[1].kernel_fpu.active_user_trap==(void *)0x456);
    address_space_test=false;memset(threads,0,sizeof(threads));initialized=false;actual_space=1;
    puts("scheduler-address-space: PASS same-process switch and temporary kernel CR3 preservation");
}
static void test_growth(void) {
    memset(threads,0,sizeof(threads));thread_chunks[0]=threads;thread_capacity=SCHEDULER_INITIAL_THREADS;
    thread_storage_bytes=0;thread_storage_budget=SCHEDULER_STORAGE_BUDGET;
    current_thread=0;idle_thread=UINT64_MAX;initialized=true;switching=false;io_depth=0;
    threads[0].state=THREAD_RUNNING;threads[0].identity=1;next_identity=2;growing_allocations=true;
    struct thread *stable=&threads[1];
    for(unsigned i=1;i<32;i++)assert(scheduler_spawn("growth",empty_worker,NULL));
    uint64_t bytes=thread_storage_bytes,identity=next_identity;unsigned live=live_allocations;
    fail_stack=true;assert(!scheduler_spawn("fail stack",empty_worker,NULL));fail_stack=false;
    fail_records=true;assert(!scheduler_spawn("fail records",empty_worker,NULL));fail_records=false;
    assert(thread_capacity==32 && thread_storage_bytes==bytes && live_allocations==live && next_identity==identity);
    for(unsigned i=32;i<96;i++)assert(scheduler_spawn("growth",empty_worker,NULL));
    assert(thread_capacity==96 && scheduler_thread_count()==96 && thread_at(1)==stable);
    for(unsigned i=1;i<96;i++)assert(thread_at(i)->identity==i+1 && thread_at(i)->stack && thread_at(i)->context.rsp%16==8);
    threads[0].state=THREAD_READY;current_thread=95;thread_at(95)->state=THREAD_RUNNING;
    high_wait_test=true;completion_ready=false;
    assert(scheduler_wait_timeout(&high_wait,completion_condition,&completion_ready,123));
    assert(!high_wait.waiters && thread_at(95)->signal_wakes==1 && !thread_at(95)->blocked_queue);
    high_wait_test=false;current_thread=0;threads[0].state=THREAD_RUNNING;thread_at(95)->state=THREAD_READY;
    struct scheduler_wait_queue a={.waiters=2},b={.waiters=1},stale={.waiters=1};
    thread_at(64)->state=THREAD_BLOCKED;thread_at(64)->blocked_queue=&a;
    thread_at(65)->state=THREAD_BLOCKED;thread_at(65)->blocked_queue=&a;
    thread_at(95)->state=THREAD_BLOCKED;thread_at(95)->blocked_queue=&b;
    assert(!scheduler_wake_count(&stale,1) && !stale.waiters);
    assert(scheduler_wake_count(&a,1)==1 && a.waiters==1 && thread_at(64)->state==THREAD_READY && thread_at(65)->state==THREAD_BLOCKED && b.waiters==1);
    thread_at(65)->blocked_deadline_ticks=1;clock_us=20000;wake_expired_waits();assert(!a.waiters && !thread_at(65)->blocked_queue && thread_at(65)->timeout_wakes==1);
    thread_at(95)->user_context=(void *)55;thread_at(95)->user_thread_context=(void *)56;
    uint64_t retired=thread_at(95)->identity;struct thread *reused=thread_at(95);
    scheduler_kill_user_threads((void *)55,NULL);assert(!b.waiters && reused->state==THREAD_DEAD);
    assert(scheduler_spawn("reuse high",empty_worker,NULL) && thread_at(95)==reused && reused->identity!=retired && reused->signal_wakes==0);
    thread_at(64)->state=THREAD_QUARANTINED;
    while(scheduler_spawn("budget",empty_worker,NULL)) {}
    bytes=thread_storage_bytes;identity=next_identity;live=live_allocations;
    assert(bytes<=thread_storage_budget && scheduler_thread_count()>200);
    assert(!scheduler_spawn("budget refused",empty_worker,NULL) && thread_storage_bytes==bytes && next_identity==identity && live_allocations==live);
    uint64_t retained=thread_at(64)->identity;thread_at(65)->state=THREAD_DEAD;
    assert(scheduler_spawn("reuse at budget",empty_worker,NULL) && thread_storage_bytes==bytes);
    assert(thread_at(64)->state==THREAD_QUARANTINED && thread_at(64)->identity==retained);
    for(unsigned i=1;i<thread_capacity;i++)if(thread_at(i)->stack_physical)pmm_free_frame_range(thread_at(i)->stack_physical,SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE);
    for(unsigned i=1;i<thread_capacity/SCHEDULER_INITIAL_THREADS;i++)pmm_free_frame_range((uintptr_t)thread_chunks[i],SCHEDULER_CHUNK_FRAMES);
    assert(!live_allocations);
    puts("scheduler-growth: PASS stable records, 95+ admission, high-slot waits/wake/timeout/kill, allocation rollback, budget refusal/reuse and quarantine retention");
}

static bool trace_false(void *arg) {(void)arg;return false;}
static void test_wait_trace(void) {
    struct srv_sched_trace out;
    assert(!scheduler_trace(SRV_SCHED_TRACE_END,&out));
    initialized=true;current_thread=0;clock_us=1000;
    threads[0]=(struct thread){.state=THREAD_RUNNING};
    threads[1]=(struct thread){.state=THREAD_READY};
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    assert(!scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    struct scheduler_wait_queue q={0};trace_test=1;
    assert(scheduler_wait_timeout(&q,trace_false,NULL,1));trace_test=0;
    assert(!q.waiters && scheduler_trace(SRV_SCHED_TRACE_END,&out));
    assert(out.version==1 && out.start_us==1000 && out.end_us==531000);
    assert(out.blocked_us==10000 && out.ready_us==520000 && out.max_ready_us==520000);
    assert(out.blocks==1 && out.timeouts==1 && !out.signals && out.dispatches==1);
    assert(out.exclusions==2 && out.ready_exclusions==2 && out.reason==2 && !out.invalid);
    assert(out.wake_us==11000 && out.run_us==531000 && out.deadline_tick==1 && !out.late_ticks);
    assert(!scheduler_trace(SRV_SCHED_TRACE_END,&out));
    /* Unrelated signal wakes retain their original deadline; a second blocked
     * interval accumulates without erasing the longest ready interval. */
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    threads[0].trace.state=1;threads[0].trace.since=clock_us;threads[0].trace.deadline=100;
    threads[0].trace.out.blocks=1;threads[0].state=THREAD_BLOCKED;
    threads[0].blocked_queue=&q;q.waiters=1;
    clock_us+=4000;assert(scheduler_wake_count(&q,1)==1);
    clock_us+=2000;trace_run(&threads[0],clock_us);threads[0].state=THREAD_RUNNING;
    assert(scheduler_trace(SRV_SCHED_TRACE_END,&out));
    assert(out.signals==1 && !out.timeouts && out.blocked_us==4000 && out.ready_us==2000 && out.reason==1);
    assert(!out.invalid && out.deadline_tick==100);
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    uint64_t deadline=timer_ticks()+1;
    threads[0].trace.state=1;threads[0].trace.since=clock_us;threads[0].trace.deadline=deadline;
    threads[0].trace.out.blocks=1;threads[0].state=THREAD_BLOCKED;
    threads[0].blocked_queue=&q;threads[0].blocked_deadline_ticks=deadline;q.waiters=1;
    clock_us+=50000;wake_expired_waits();
    assert(threads[0].state==THREAD_READY);
    clock_us+=3000;trace_run(&threads[0],clock_us);threads[0].state=THREAD_RUNNING;
    assert(scheduler_trace(SRV_SCHED_TRACE_END,&out));
    assert(out.late_ticks==4 && out.timeouts==1 && out.blocked_us==50000 && out.ready_us==3000);
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    trace_ready(&threads[0],0);clock_us+=1000;trace_run(&threads[0],clock_us);
    assert(scheduler_trace(SRV_SCHED_TRACE_END,&out));
    assert(out.ready_us==1000 && !out.blocks && !out.signals && !out.timeouts && !out.reason && !out.deadline_tick);
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&out));
    clock_us--;trace_ready(&threads[0],0);assert(scheduler_trace(SRV_SCHED_TRACE_END,&out));assert(out.invalid);
    memset(threads,0,sizeof(threads));initialized=false;switching=false;io_depth=io_gate_pending=0;clock_us=0;current_thread=0;
    puts("scheduler-wait-trace: PASS blocked versus ready, unchanged exclusion, timeout/signal, reset/refusal and invalid clock");
}

static void test_independent_process_memory(void) {
    void *process=(void *)0x10,*other=(void *)0x20;
    initialized=io_services_enabled=true;current_thread=3;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=process,.io_user_safe=true,.address_space=1};
    assert(scheduler_io_process_independent(process));
    assert(!scheduler_io_process_independent(NULL) && !scheduler_io_process_independent(other));
    threads[3].io_user_safe=false;assert(!scheduler_io_process_independent(process));threads[3].io_user_safe=true;
    threads[3].user_thread_context=process;assert(!scheduler_io_process_independent(process));threads[3].user_thread_context=NULL;
    actual_space=2;assert(!scheduler_io_process_independent(process));actual_space=1;
    for(unsigned state=THREAD_READY;state<=THREAD_QUARANTINED;state++) {
        threads[4]=(struct thread){.state=state,.user_context=process};
        assert(scheduler_io_process_independent(process)==(state==THREAD_DEAD));
    }
    threads[4]=(struct thread){.state=THREAD_READY,.user_context=other};
    assert(scheduler_io_process_retired((void *)0x30));
    assert(!scheduler_io_process_retired(process) && !scheduler_io_process_retired(other));
    threads[5]=(struct thread){.state=THREAD_QUARANTINED,.user_context=(void *)0x30};
    assert(!scheduler_io_process_retired((void *)0x30));threads[5]=(struct thread){0};
    io_depth=2;io_owner=4;io_private_image=NULL;
    assert(scheduler_io_process_independent(process) && io_depth==2 && io_owner==4);
    io_private_image=process;assert(!scheduler_io_process_independent(process));io_private_image=other;
    assert(!scheduler_io_process_retired(other));
    io_private_image=(void *)0x30;assert(!scheduler_io_process_retired((void *)0x30));io_private_image=other;
    assert(scheduler_io_process_independent(process));
    threads[4].user_context=process;assert(!scheduler_io_process_independent(process));threads[4].user_context=other;
    io_owner=3;assert(!scheduler_io_process_independent(process));io_owner=4;
    uint64_t capacity=thread_capacity;thread_capacity=4097;assert(!scheduler_io_process_independent(process));thread_capacity=capacity;
    io_owner=thread_capacity;assert(!scheduler_io_process_independent(process));io_owner=4;
    current_thread=thread_capacity;assert(!scheduler_io_process_independent(process));current_thread=3;
    switching=true;assert(!scheduler_io_process_independent(process));switching=false;
    io_services_enabled=false;assert(!scheduler_io_process_independent(process));io_services_enabled=true;
    assert(io_depth==2 && io_owner==4 && io_private_image==other);
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=false;current_thread=0;io_depth=0;io_private_image=NULL;
    puts("independent-memory: PASS current main only, siblings/worker/image/owner/CR3/scan bounds, unchanged foreign ownership");
}

static void test_helper_atomic_memory(void) {
    void *p=(void *)0x10,*other=(void *)0x20;
    struct scheduler_wait_queue parked={0},wrong={0};
    initialized=io_services_enabled=true;current_thread=3;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=p,.user_thread_context=(void *)0x40,.io_user_safe=true,.address_space=1};
    threads[4]=(struct thread){.state=THREAD_BLOCKED,.user_context=p,.io_user_safe=true,.address_space=1,.blocked_queue=&parked,.memory_park_queue=&parked};
    threads[5]=(struct thread){.state=THREAD_BLOCKED,.user_context=other};
    io_depth=2;io_owner=5;
    assert(!scheduler_io_process_independent(p)); /* Old path reproduces refusal. */
    assert(scheduler_io_process_helper_memory(p,&parked));
    threads[4].state=THREAD_READY;assert(scheduler_io_process_helper_memory(p,&parked));
    threads[4].memory_park_queue=NULL;assert(!scheduler_io_process_helper_memory(p,&parked));
    threads[4].memory_park_queue=&parked;threads[4].state=THREAD_BLOCKED;
    assert(!scheduler_io_process_helper_memory(p,NULL));
    assert(!scheduler_io_process_helper_memory(p,&wrong));
    threads[4].io_user_safe=false;assert(!scheduler_io_process_helper_memory(p,&parked));threads[4].io_user_safe=true;
    threads[4].state=THREAD_QUARANTINED;assert(!scheduler_io_process_helper_memory(p,&parked));
    threads[4].state=THREAD_RUNNING;assert(!scheduler_io_process_helper_memory(p,&parked));threads[4].state=THREAD_BLOCKED;
    threads[4].address_space=2;assert(!scheduler_io_process_helper_memory(p,&parked));threads[4].address_space=1;
    io_private_image=p;assert(!scheduler_io_process_helper_memory(p,&parked));io_private_image=NULL;
    threads[5].user_context=p;assert(!scheduler_io_process_helper_memory(p,&parked));threads[5].user_context=other;
    io_owner=3;assert(!scheduler_io_process_helper_memory(p,&parked));io_owner=5;
    threads[3].user_thread_context=NULL;assert(!scheduler_io_process_helper_memory(p,&parked));threads[3].user_thread_context=(void *)0x40;
    threads[3].io_user_safe=false;assert(!scheduler_io_process_helper_memory(p,&parked));threads[3].io_user_safe=true;
    actual_space=2;assert(!scheduler_io_process_helper_memory(p,&parked));actual_space=1;
    uint64_t capacity=thread_capacity;thread_capacity=4097;assert(!scheduler_io_process_helper_memory(p,&parked));thread_capacity=capacity;
    io_owner=thread_capacity;assert(!scheduler_io_process_helper_memory(p,&parked));io_owner=5;
    current_thread=thread_capacity;assert(!scheduler_io_process_helper_memory(p,&parked));current_thread=3;
    switching=true;assert(!scheduler_io_process_helper_memory(p,&parked));switching=false;
    io_services_enabled=false;assert(!scheduler_io_process_helper_memory(p,&parked));io_services_enabled=true;
    assert(io_depth==2 && io_owner==5 && !io_private_image);
    io_depth=0;threads[4]=(struct thread){0};threads[5]=(struct thread){0};
    threads[3].memory_park_queue=&parked;scheduler_yield();
    assert(current_thread==3 && !threads[3].memory_park_queue);

    memset(threads,0,sizeof(threads));initialized=io_services_enabled=false;current_thread=0;io_depth=0;
    puts("helper-atomic-memory: PASS safe parked sibling, foreign owner unchanged, unsafe/CR3/owner/image/scan refusal");
}

static void test_user_thread_publication(void) {
    assert(valid_user_worker_stack_extent(0x100000,0xffff800001000000ull));
    assert(!valid_user_worker_stack_extent(0,0xffff800001000000ull));
    assert(!valid_user_worker_stack_extent(0x100001,0xffff800001000000ull));
    assert(!valid_user_worker_stack_extent(0x10000000000000ull,0xffff800001000000ull));
    assert(!valid_user_worker_stack_extent(0x100000,0x100000));
    assert(!valid_user_worker_stack_extent(0x100000,0xffff7fff00000000ull));
    assert(!valid_user_worker_stack_extent(0x100000,0xffff800001000001ull));
    assert(!valid_user_worker_stack_extent(0x100000,0xfffffffffffff000ull));
    unsigned saved_frees=frees;
    ready_edge_setup();threads[3].address_space=1;threads[3].identity=3;threads[3].image_generation=77;growing_allocations=true;
    struct fpu_state fpu={0},parent_fpu={0};void *user_thread=(void *)0x123;
    threads[3].user_fpu=&parent_fpu;threads[3].user_fs_base=24;threads[3].kernel_stack_top=0x8000;
    resolved_parent_binding=(struct process_user_binding){.user_fpu=&parent_fpu,.fs_base=24,.kernel_stack_top=0x8000};
    resolved_binding=(struct process_user_binding){.user_fpu=&fpu,.fs_base=42,.new_worker_allowed=true};
    threads[5]=(struct thread){.state=THREAD_BLOCKED,.user_context=(void *)5};io_depth=2;io_owner=5;
    uint64_t bytes=thread_storage_bytes,id=next_identity;unsigned live=live_allocations;
    assert(!scheduler_spawn_user_thread("invalid",empty_worker,NULL,&fpu,42,77));
    assert(!scheduler_spawn_user_thread("invalid",empty_worker,user_thread,NULL,42,77));
    assert(!scheduler_spawn_user_thread("invalid",empty_worker,user_thread,&fpu,42,78));
    resolved_binding.new_worker_allowed=false;
    assert(!scheduler_spawn_user_thread("dying process",empty_worker,user_thread,&fpu,42,77));
    resolved_binding.new_worker_allowed=true;
    threads[3].user_fs_base=25;
    assert(!scheduler_spawn_user_thread("stale parent",empty_worker,user_thread,&fpu,42,77));
    threads[3].user_fs_base=24;
    /* A live worker parent also supplies its own exact stack and resolver
     * tuple; admission checks it before attempting a new stack allocation. */
    void *parent_stack=aligned_alloc(PMM_FRAME_SIZE,SCHEDULER_STACK_SIZE);assert(parent_stack);
    fixture_parent_worker=(void *)0x124;threads[3].user_thread_context=fixture_parent_worker;
    threads[3].stack=parent_stack;threads[3].stack_physical=(uint64_t)parent_stack;
    threads[3].kernel_stack_top=0;resolved_parent_binding.kernel_stack_top=0;
    unsigned attempts=stack_attempts;
    threads[3].kernel_stack_top=0x8000;
    assert(!scheduler_spawn_user_thread("bad worker parent",empty_worker,user_thread,&fpu,42,77));
    assert(stack_attempts==attempts);threads[3].kernel_stack_top=0;
    fail_stack=true;
    assert(!scheduler_spawn_user_thread("worker parent",empty_worker,user_thread,&fpu,42,77));
    fail_stack=false;assert(stack_attempts==attempts+1);
    free(parent_stack);threads[3].stack=NULL;threads[3].stack_physical=0;threads[3].user_thread_context=NULL;
    fixture_parent_worker=NULL;threads[3].kernel_stack_top=0x8000;resolved_parent_binding.kernel_stack_top=0x8000;
    resolver_ok=false;assert(!scheduler_spawn_user_thread("invalid",empty_worker,user_thread,&fpu,42,77));resolver_ok=true;
    prepare_ok=false;assert(!scheduler_spawn_user_thread("invalid",empty_worker,user_thread,&fpu,42,77));prepare_ok=true;
    actual_space=2;assert(!scheduler_spawn_user_thread("invalid",empty_worker,user_thread,&fpu,42,77));actual_space=1;
    assert(thread_storage_bytes==bytes && next_identity==id && live_allocations==live);
    threads[3].kernel_fpu.active_user_trap=(void *)0x1234;
    invalid_stack_view=true;
    assert(!scheduler_spawn_user_thread("invalid stack",empty_worker,user_thread,&fpu,42,77));
    invalid_stack_view=false;
    assert(thread_storage_bytes==bytes && next_identity==id && live_allocations==live &&
           threads[3].kernel_fpu.active_user_trap==(void *)0x1234);
    fail_stack=true;assert(!scheduler_spawn_user_thread("allocation",empty_worker,user_thread,&fpu,42,77));fail_stack=false;
    assert(thread_storage_bytes==bytes && next_identity==id && live_allocations==live);
    assert(scheduler_spawn_user_thread("prepared",empty_worker,user_thread,&fpu,42,77));
    struct thread *t=&threads[1];
    assert(t->state==THREAD_READY && t->user_context==(void *)3 && t->address_space==1 &&
           t->user_thread_context==user_thread && t->user_fpu==&fpu && t->user_fs_base==42 && t->image_generation==77 && t->io_user_safe);
    assert(io_thread_allowed(1) && !t->io_gate_waiting && io_depth==2 && io_owner==5);
    /* Same-process ownership still excludes even a completely prepared entry. */
    threads[5].user_context=(void *)3;assert(!io_thread_allowed(1));threads[5].user_context=(void *)5;
    t->user_fpu=(void *)1;t->kernel_fpu.active_user_trap=(void *)0xabc;
    scheduler_kill_user_threads((void *)3,NULL);assert(t->state==THREAD_DEAD && !t->kernel_fpu.active_user_trap);
    assert(!t->io_gate_waiting);pmm_free_frame_range(t->stack_physical,SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE);
    thread_storage_bytes=bytes;frees=saved_frees;growing_allocations=false;memset(threads,0,sizeof(threads));current_thread=0;io_depth=io_gate_pending=0;initialized=io_services_enabled=false;
    puts("user-thread-publication: PASS complete READY identity/TLS/FPU, foreign eligibility, same-process exclusion, failure atomicity and pre-entry cancellation");
}

static void test_private_image_scope(void) {
    struct srv_sched_trace trace;
    void *parent=(void *)0x10,*child=(void *)0x20;
    initialized=true;io_services_enabled=true;current_thread=3;actual_space=1;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=parent};
    threads[4]=(struct thread){.state=THREAD_READY,.user_context=parent,.io_user_safe=true};
    threads[5]=(struct thread){.state=THREAD_READY,.user_context=parent};
    threads[6]=(struct thread){.state=THREAD_READY,.user_context=(void *)0x30,.io_user_safe=true};
    assert(!scheduler_io_begin_process_image(NULL));
    assert(!scheduler_io_begin_process_image(parent));
    assert(!scheduler_io_begin_process_image((void *)0x30));
    actual_space=2;assert(!scheduler_io_begin_process_image(child));actual_space=1;
    assert(!io_depth && !io_private_image);
    assert(scheduler_io_begin());
    assert(!io_thread_allowed(4) && !io_thread_allowed(5) && io_thread_allowed(6));
    assert(!scheduler_io_begin_process_image(child) && io_depth==1 && !io_private_image);
    scheduler_io_end();
    /* Reproduce the stalled sibling's READY interval while a private image is
     * built. Existing ordinary ownership must still refuse this admission. */
    current_thread=4;threads[4].state=THREAD_RUNNING;clock_us=1000;
    assert(scheduler_trace(SRV_SCHED_TRACE_BEGIN,&trace));trace_ready(&threads[4],2);
    threads[4].state=THREAD_READY;current_thread=3;
    assert(scheduler_io_begin_process_image(child) && io_depth==1 && io_private_image==child);
    for(unsigned n=0;n<64;n++) {
        clock_us+=4000;
        assert(io_thread_allowed(4)); /* Fails with the old caller-identity exclusion. */
        assert(!io_thread_allowed(5) && io_thread_allowed(6));
    }
    threads[7]=(struct thread){.state=THREAD_READY,.user_context=child,.io_user_safe=true};
    assert(!io_thread_allowed(7)); /* Target identity is still excluded. */
    assert(scheduler_io_begin() && io_depth==2 && io_private_image==child);
    scheduler_io_end();assert(io_depth==1 && io_private_image==child);
    current_thread=6;scheduler_io_end();assert(io_depth==1 && io_private_image==child);
    current_thread=3;scheduler_io_end();assert(!io_depth && !io_private_image);
    assert(scheduler_io_begin() && !io_thread_allowed(4));scheduler_io_end();
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=false;current_thread=0;clock_us=0;io_gate_pending=0;
    puts("private-image-io: PASS caller sibling service, target/unsafe exclusion, inherited/foreign refusal and exact restoration");
}

static unsigned monitors,mwaits;
static bool ready_after_monitor,regress_mwait;
static uint64_t mwait_delay=10000;
void scheduler_test_monitor(void *address) {
    assert(!((uintptr_t)address&63));monitors++;
    if(ready_after_monitor)threads[0].state=THREAD_READY;
}
static uint32_t expected_hint;
static bool bad_residency,zero_residency;
static uint64_t residency_reads;
uint64_t scheduler_test_mwait_read(unsigned reg) {
    assert(expected_hint==0x10);residency_reads++;
    if(reg==0x10)return clock_us*2000;
    assert(reg==0x3fc || reg==0x3f8);
    return zero_residency?0:bad_residency?UINT64_MAX-clock_us:clock_us*(reg==0x3fc?1000:500);
}
void scheduler_test_mwait(uint32_t hint) {
    assert(hint==expected_hint);
    mwaits++;if(regress_mwait)clock_us--;else clock_us+=mwait_delay;
}
static void test_mwait(void) {
    struct scheduler_mwait_test mt;struct scheduler_idle_audit ia;
    assert(!scheduler_mwait_test_start(0));
    initialized=true;idle_thread=current_thread=1;threads[1].state=THREAD_RUNNING;
    assert(scheduler_idle_audit_start());reference=false;assert(!scheduler_mwait_test_start(0));reference=true;
    assert(scheduler_mwait_test_start(0) && !scheduler_mwait_test_start(0));
    ready_after_monitor=true;idle_step();assert(monitors==1 && !mwaits && mwait_test.rechecks==1);
    ready_after_monitor=false;threads[0].state=THREAD_UNUSED;
    for(unsigned i=0;i<32;i++)idle_step();
    assert(mwaits==32 && mwait_test.attempts==32 && !mwait_test.fault && mwait_test.max_us==10000);
    for(unsigned i=0;i<40;i++)idle_step();
    assert(mwaits==32 && mwait_test.hlt==32 && idle_audit.enters==40);
    scheduler_mwait_test_stop(&mt);assert(mt.returns==32 && mt.tick_end>mt.tick_begin);
    scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0));
    mwait_delay=20000;idle_step();assert(!mwait_test.fault);
    mwait_delay=20001;idle_step();assert(mwait_test.fault==2);
    unsigned prior=mwaits;idle_step();assert(mwaits==prior && mwait_test.hlt==1);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0));
    regress_mwait=true;idle_step();assert(mwait_test.fault==1);regress_mwait=false;
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0));
    /* Retirement prevents subsequent MWAIT even if the idle audit restarts. */
    scheduler_idle_audit_stop(&ia);assert(scheduler_idle_audit_start());
    prior=mwaits;idle_step();assert(mwaits==prior);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(!residency_reads); /* Ordinary/shallow trial never reads these MSRs. */
    assert(scheduler_idle_audit_start());assert(!scheduler_mwait_test_start(0x20));
    expected_hint=0x10;mwait_delay=10000;
    assert(scheduler_mwait_test_start(0x10));
    for(unsigned i=0;i<32;i++)idle_step();
    assert(mwait_test.hint==16 && mwait_test.tsc_ticks==640000000 &&
        mwait_test.core_c3_ticks==320000000 && mwait_test.package_c3_ticks==160000000 && !mwait_test.fault);
    prior=mwaits;for(unsigned i=0;i<32;i++)idle_step();assert(mwaits==prior && mwait_test.hlt==32);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0x10));
    bad_residency=true;idle_step();assert(mwait_test.fault==3);bad_residency=false;
    prior=mwaits;idle_step();assert(mwaits==prior && mwait_test.hlt==1);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0x10));
    mwait_delay=10000;zero_residency=true;idle_step();zero_residency=false;
    assert(!mwait_test.fault && mwait_test.tsc_ticks && !mwait_test.core_c3_ticks && !mwait_test.package_c3_ticks);
    mwait_test.tsc_ticks=UINT64_MAX;idle_step();assert(mwait_test.fault==3);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(0x10));
    mwait_delay=20001;idle_step();assert(mwait_test.fault==2);
    prior=mwaits;idle_step();assert(mwaits==prior && mwait_test.hlt==1);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);expected_hint=0;
    mwait_test=(struct scheduler_mwait_test){0};idle_audit=(struct scheduler_idle_audit){0};
    idle_thread=UINT64_MAX;current_thread=0;initialized=false;clock_us=0;
    memset(threads,0,sizeof(threads));
    puts("mwait: PASS readiness recheck, 32 attempt cap, mandatory HLT, latency boundary/fault fallback, clock regression and retired epoch");
}

static void test_coordinated_mwait(void) {
    struct scheduler_mwait_test mt;struct scheduler_idle_audit ia;
    initialized=true;idle_thread=current_thread=1;threads[1].state=THREAD_RUNNING;
    expected_hint=16;mwait_delay=10000;peer_enabled=true;
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(16));
    ready_after_monitor=true;idle_step();assert(!peer_prepares && !peer_finishes);
    ready_after_monitor=false;threads[0].state=THREAD_UNUSED;
    for(unsigned i=0;i<32;i++)idle_step();
    assert(peer_prepares==32 && peer_finishes==32 && !mwait_test.fault);
    for(unsigned i=0;i<32;i++)idle_step();
    assert(mwait_test.hlt==32 && peer_prepares==32);
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(16));
    unsigned prior=mwaits;peer_prepare_fail=true;idle_step();
    assert(mwait_test.fault==4 && mwaits==prior && peer_finishes==32);peer_prepare_fail=false;
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(16));
    peer_finish_fail=true;idle_step();assert(mwait_test.fault==4 && peer_finishes==33);
    prior=mwaits;idle_step();assert(mwaits==prior && mwait_test.hlt==1);peer_finish_fail=false;
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    assert(scheduler_idle_audit_start() && scheduler_mwait_test_start(16));
    bad_residency=true;idle_step();assert(mwait_test.fault==3 && peer_finishes==34);bad_residency=false;
    scheduler_mwait_test_stop(&mt);scheduler_idle_audit_stop(&ia);
    peer_enabled=false;expected_hint=0;
    mwait_test=(struct scheduler_mwait_test){0};idle_audit=(struct scheduler_idle_audit){0};
    idle_thread=UINT64_MAX;current_thread=0;initialized=false;clock_us=0;memset(threads,0,sizeof(threads));
    puts("coordinated-mwait: PASS readiness, peer refusal, counter failure release, cap and HLT fallback");
}

static void test_idle_audit(void) {
    struct scheduler_idle_audit out;
    assert(!scheduler_idle_audit_start());
    initialized=true;idle_thread=1;current_thread=1;
    threads[1].state=THREAD_RUNNING;
    idle_step();scheduler_idle_audit_snapshot(&out);assert(!out.epoch && !out.enters);
    assert(scheduler_idle_audit_start() && !scheduler_idle_audit_start());
    idle_step();scheduler_idle_audit_snapshot(&out);assert(out.epoch==1 && out.enters==1 && out.returns==1 && !out.ready);
    threads[0].state=THREAD_READY;switching=true;idle_step();switching=false;
    scheduler_idle_audit_snapshot(&out);assert(out.ready==1 && out.enters==1 && out.returns==1);
    threads[0].state=THREAD_UNUSED;
    uint64_t pending=idle_audit_enter();scheduler_idle_audit_stop(&out);
    assert(out.enters==2 && out.returns==1);
    idle_audit_return(pending);idle_step();scheduler_idle_audit_snapshot(&out);assert(out.enters==2 && out.returns==1);
    assert(scheduler_idle_audit_start());idle_audit_return(pending);
    scheduler_idle_audit_snapshot(&out);assert(out.epoch==2 && !out.enters && !out.returns);
    idle_step();scheduler_idle_audit_stop(&out);assert(out.enters==1 && out.returns==1);
    idle_audit.epoch=UINT64_MAX;assert(!scheduler_idle_audit_start());
    idle_audit=(struct scheduler_idle_audit){0};idle_thread=UINT64_MAX;current_thread=0;initialized=false;
    memset(threads,0,sizeof(threads));
    puts("idle-audit: PASS opt-in, nested refusal, ready bypass, HLT path, pending return across stop/restart, epoch exhaustion; no policy changes");
}

/* Observation must retain the outer owner identity across nesting, foreign
 * attempts and slot reuse, without changing admission or emitting in a scope. */
static unsigned owner_prints;
void console_printf(const char *format,...) { assert(!io_depth && (!strncmp(format,"io-owner",8) || !strncmp(format,"io-gate",7)));owner_prints++; }
uint64_t process_pid(const struct process *p) { return (uint64_t)(uintptr_t)p; }
static void test_io_observation(void) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;
    switching=false;current_thread=0;io_depth=0;clock_us=1000;reference=true;
    io_observed.enabled=false;
    threads[0]=(struct thread){.identity=99,.name="owner name\n",.state=THREAD_RUNNING,.user_context=(void *)42};
    assert(scheduler_io_begin());clock_us+=200000;scheduler_io_end();
    assert(!io_observed.scopes); /* Ordinary scheduling has no diagnostic history. */
    initialized=false;assert(!scheduler_io_observe_reset());initialized=true;
    reference=false;assert(!scheduler_io_observe_reset());reference=true;
    assert(scheduler_io_observe_reset());
    assert(scheduler_io_begin());assert(!scheduler_io_observe_reset());
    assert(scheduler_io_begin());assert(io_observed.scopes==1);
    threads[1]=(struct thread){.identity=100,.state=THREAD_READY};
    assert(!io_thread_allowed(1));assert(io_observed.active.exclusions==1);
    current_thread=1;assert(!scheduler_io_begin());scheduler_io_end();
    assert(io_depth==2 && io_observed.active.seq==1);current_thread=0;
    clock_us+=200000;scheduler_io_end();assert(!io_observed.long_scopes);
    scheduler_io_end();assert(io_observed.long_scopes==1 && !io_observed.active.seq);
    struct io_owner_sample first=io_observed.rows[0];
    assert(first.identity==99 && first.pid==42 && first.end-first.start==200000 && first.exclusions==1);
    assert(!strcmp(first.name,"owner_name_"));
    threads[0].identity=101;threads[0].name="replacement";
    assert(!strcmp(first.name,"owner_name_") && first.identity==99);
    assert(scheduler_io_begin());clock_us+=99999;scheduler_io_end();
    assert(io_observed.scopes==2 && io_observed.long_scopes==1);
    assert(scheduler_io_begin());clock_us--;scheduler_io_end();
    assert(io_observed.invalid==1 && io_observed.long_scopes==1);
    for(unsigned i=0;i<IO_OBSERVE_ROWS+2;i++) {
        assert(scheduler_io_begin());clock_us+=IO_OBSERVE_MIN_US;scheduler_io_end();
    }
    assert(io_observed.long_scopes==IO_OBSERVE_ROWS+3);
    uint64_t oldest=io_observed.long_scopes-IO_OBSERVE_ROWS;
    for(unsigned i=0;i<IO_OBSERVE_ROWS;i++) {
        const struct io_owner_sample *v=&io_observed.rows[(oldest+i)%IO_OBSERVE_ROWS];
        assert(v->seq==oldest+i+3 && v->identity==101 && v->end-v->start==IO_OBSERVE_MIN_US);
    }
    scheduler_print_io_owners();assert(owner_prints==IO_OBSERVE_ROWS+3);
    assert(scheduler_io_begin());scheduler_print_io_owners();assert(owner_prints==IO_OBSERVE_ROWS+3);scheduler_io_end();
    assert(scheduler_io_observe_reset() && !io_observed.scopes && !io_observed.long_scopes);
    io_observed.enabled=false;io_gate_pending=0;memset(threads,0,sizeof(threads));initialized=io_services_enabled=false;
    puts("io-owner-observe: PASS opt-in, outer nesting, foreign refusal, copied identity, bounded wrap, clock rollback");
}

static void test_gate_retention(void) {
    initialized=io_services_enabled=true;switching=false;current_thread=0;io_depth=0;
    threads[0]=(struct thread){.identity=80,.state=THREAD_RUNNING};
    assert(scheduler_io_observe_reset());scheduler_io_syscall(77);
    assert(threads[0].io_syscall==77);
    struct io_gate_sample v={.pid=12,.identity=80,.syscall=77,.owner_pid=15,.owner_identity=91,.owner_seq=42,.start=1000,.end=101000};
    io_observed.enabled=false;io_gate_record(v);assert(!io_gates.count);
    io_observed.enabled=true;v.end=100999;io_gate_record(v);assert(!io_gates.count);
    v.end=999;io_gate_record(v);assert(!io_gates.count);
    for(unsigned i=0;i<IO_OBSERVE_ROWS+5;i++){v.end=101000+i*1000;io_gate_record(v);}
    assert(io_gates.count==IO_OBSERVE_ROWS && io_gates.total==IO_OBSERVE_ROWS+5);
    uint64_t min=UINT64_MAX;
    for(unsigned i=0;i<io_gates.count;i++) {
        const struct io_gate_sample *r=&io_gates.rows[i];
        assert(r->pid==12 && r->identity==80 && r->syscall==77 && r->owner_pid==15 && r->owner_identity==91 && r->owner_seq==42);
        if(r->end-r->start<min)min=r->end-r->start;
    }
    assert(min==105000 && !io_depth);
    v.end=101000;io_gate_record(v);assert(io_gates.total==IO_OBSERVE_ROWS+6);
    v.syscall=SYS_EXIT;v.end=101000;io_gate_record(v);
    assert(io_gates.exit_worst.syscall==SYS_EXIT && io_gates.exit_worst.end==101000);
    v.syscall=SYS_OPEN;v.end=9000000;io_gate_record(v);
    assert(io_gates.exit_worst.end==101000); /* Longer file waits cannot evict exit. */
    assert(scheduler_io_observe_reset() && !io_gates.count && !io_gates.total && !io_gates.exit_worst.end);
    initialized=io_services_enabled=false;io_observed=(__typeof__(io_observed)){0};memset(threads,0,sizeof(threads));
    puts("io-gate: PASS disabled/short/backward refusal, bounded longest retention, caller/initial owner identity, reset and no owner mutation");
}
static void test_user_timer_return(void) {
    unsigned original_switches=switches;uint64_t original_flags=fixture_irq_flags;
    initialized=true;switching=false;current_thread=thread_capacity;
    uint64_t before=sampled_ticks;scheduler_preempt_user_return_tick();assert(sampled_ticks==before);
    current_thread=4;threads[4].state=THREAD_BLOCKED;
    scheduler_preempt_user_return_tick();assert(sampled_ticks==before);

    for(unsigned mode=0;mode<7;mode++) {
        memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
        current_thread=3;idle_thread=1;io_depth=0;io_owner=0;io_gate_pending=0;actual_space=kernel_space=1;
        fixture_irq_flags=0;
        threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)0x100,.address_space=1};
        assert(scheduler_io_begin());threads[3].state=THREAD_READY;
        threads[4]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)0x200,.address_space=1,.resume_address_space=1};
        current_thread=4;
        if(mode==1)threads[3].user_context=threads[4].user_context;
        if(mode==2)io_private_image=threads[4].user_context;
        if(mode==3)threads[4].user_context=NULL;
        if(mode==4)fixture_irq_flags=0x200;
        if(mode==5)threads[4].address_space=2;
        if(mode==6)threads[4].account_user=true;
        return_timer_publish=mode<3;return_timer_allowed=mode==0;
        /* The account-user negative case remains true, and is not a valid
         * fully-unwound kernel boundary. Check its refusal without switching. */
        if(mode==6) {
            preempt_tick_accumulator=0;scheduler_preempt_user_return_tick();
            assert(!threads[4].io_user_safe && threads[4].account_user);
        } else {
            unsigned before=return_timer_turns;return_timer_test=true;
            preempt_tick_accumulator=SCHEDULER_PREEMPT_QUANTUM_TICKS-1;
            scheduler_preempt_user_return_tick();return_timer_test=false;
            assert(return_timer_turns==before+1 && current_thread==4 && !threads[4].io_user_safe);
            assert((io_depth!=0)==return_timer_allowed);
        }
        io_depth=0;io_private_image=NULL;
    }
    /* A pending cancellation exits while still unsafe, before publishing the
     * return suffix, even when this tick would not otherwise yield. */
    memset(threads,0,sizeof(threads));initialized=true;switching=false;current_thread=4;io_depth=0;fixture_irq_flags=0;
    threads[4]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)0x200,.address_space=1};
    actual_space=1;preempt_cancel=true;preempt_tick_accumulator=0;
    if(!setjmp(preempt_cancel_exit)) {scheduler_preempt_user_return_tick();assert(!"cancellation returned");}
    assert(!threads[4].io_user_safe);preempt_cancel=false;
    switches=original_switches;fixture_irq_flags=original_flags;
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=false;current_thread=0;io_depth=io_owner=io_gate_pending=0;actual_space=kernel_space=1;
    puts("user-timer-return: PASS foreign-owner progress, unchanged same/private-image exclusions and kernel/IRQ/root refusal");
}

static void test_foreign_io_return(void)
{
    memset(threads,0,sizeof(threads)); initialized=io_services_enabled=true; switching=false;
    current_thread=3; idle_thread=1; io_depth=0; io_owner=0; io_gate_pending=0;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)0x100};
    assert(scheduler_io_begin());
    threads[3].state=THREAD_READY;
    threads[4]=(struct thread){.state=THREAD_RUNNING,.user_context=(void *)0x200,.io_user_safe=false};
    threads[5]=(struct thread){.state=THREAD_READY,.user_context=(void *)0x300,.io_user_safe=true};
    current_thread=4;
    assert(!io_thread_allowed(4) && io_thread_allowed(3) && io_thread_allowed(5));
    /* Preempt a timer-entered, unsafe continuation; only the selected owner
     * callback may end the active I/O service before that continuation runs. */
    unsigned before=switches; foreign_timer_test=true;foreign_timer_owner_turns=foreign_timer_service_turns=0;
    preempt_tick_accumulator=SCHEDULER_PREEMPT_QUANTUM_TICKS-1;
    scheduler_preempt_tick();foreign_timer_test=false;
    assert(foreign_timer_service_turns==1 && foreign_timer_owner_turns==1);
    assert(switches==before+2 && !io_depth && current_thread==4 && !threads[4].io_user_safe);
    scheduler_user_enter();
    assert(threads[4].io_user_safe && threads[4].account_user);
    switches=before;
    memset(threads,0,sizeof(threads)); initialized=io_services_enabled=false;
    current_thread=0; io_depth=io_owner=io_gate_pending=0; idle_thread=UINT64_MAX;
    puts("scheduler-foreign-return: PASS (owner progress before user admission)");
}

static void observe_check(bool expected, bool null_output, void *process, void *worker,
                          struct fpu_state *fpu)
{
    struct thread saved_threads[SCHEDULER_INITIAL_THREADS];
    memcpy(saved_threads,threads,sizeof(saved_threads));
    uint64_t saved_current=current_thread, saved_capacity=thread_capacity;
    uint64_t saved_depth=io_depth, saved_owner=io_owner, saved_space=actual_space;
    uint64_t saved_pending=io_gate_pending, saved_cursor=io_gate_cursor, saved_clock=clock_us;
    uint64_t saved_rflags=scheduler_test_rflags;
    void *saved_image=io_private_image;
    bool saved_initialized=initialized, saved_switching=switching, saved_services=io_services_enabled;
    unsigned saved_switches=switches, saved_frees=frees, saved_clock_calls=clock_calls;
    struct scheduler_user_return_observation out, before;
    memset(&out,0xa5,sizeof(out)); memcpy(&before,&out,sizeof(out));
    bool result=scheduler_observe_user_return(null_output ? NULL : &out);
    assert(result==expected);
    if (expected) {
        assert(out.thread_identity==42 && out.process==process && out.user_thread==worker);
        assert(out.address_space==0x2000 && out.image_generation==77 && out.user_fpu==fpu);
        assert(out.kernel_fpu==&threads[3].kernel_fpu && out.user_fs_base==0x4000 && out.kernel_stack_top==0x9000);
    } else assert(!memcmp(&out,&before,sizeof(out)));
    assert(!memcmp(saved_threads,threads,sizeof(saved_threads)));
    assert(current_thread==saved_current && thread_capacity==saved_capacity && io_depth==saved_depth && io_owner==saved_owner);
    assert(actual_space==saved_space && io_gate_pending==saved_pending && io_gate_cursor==saved_cursor && io_private_image==saved_image);
    assert(initialized==saved_initialized && switching==saved_switching && io_services_enabled==saved_services);
    assert(switches==saved_switches && frees==saved_frees && clock_us==saved_clock && clock_calls==saved_clock_calls);
    assert(scheduler_test_rflags==saved_rflags);
}

static void test_user_return_observation(void)
{
    struct fpu_state main_fpu={0}, worker_fpu={0};
    void *process=(void *)0x1234, *worker=(void *)0x5678;
    memset(threads,0,sizeof(threads)); initialized=true; switching=false;
    current_thread=3; io_depth=0; io_owner=0; io_private_image=NULL;
    actual_space=0x2000; scheduler_test_rflags=0;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.identity=42,.user_context=process,
        .address_space=0x2000,.image_generation=77,.user_fpu=&main_fpu,.user_fs_base=0x4000,.kernel_stack_top=0x9000};
#define CHECK(expected) observe_check((expected),false,process,NULL,&main_fpu)
    CHECK(true);
    observe_check(false,true,process,NULL,&main_fpu);
    scheduler_test_rflags=0x200; CHECK(false); scheduler_test_rflags=0;
    initialized=false; CHECK(false); initialized=true;
    switching=true; CHECK(false); switching=false;
    current_thread=thread_capacity; CHECK(false); current_thread=3;
    threads[3].state=THREAD_DEAD; CHECK(false);
    threads[3].state=THREAD_READY; CHECK(false); threads[3].state=THREAD_RUNNING;
    threads[3].user_context=NULL; CHECK(false); threads[3].user_context=process;
    threads[3].address_space=0; CHECK(false); threads[3].address_space=0x2000;
    threads[3].user_fpu=NULL; CHECK(false); threads[3].user_fpu=&main_fpu;
    threads[3].identity=0; CHECK(false); threads[3].identity=42;
    actual_space=1; CHECK(false); actual_space=0x3000; CHECK(false); actual_space=0x2000;
    threads[3].account_user=true; CHECK(false); threads[3].account_user=false;
    threads[3].io_user_safe=true; CHECK(false); threads[3].io_user_safe=false;
    io_depth=1; io_owner=3; CHECK(false); io_owner=4; CHECK(false); io_depth=0; io_owner=0;
    threads[3].user_thread_context=worker; threads[3].user_fpu=&worker_fpu;
    observe_check(true,false,process,worker,&worker_fpu);
#undef CHECK
    memset(threads,0,sizeof(threads)); initialized=false; switching=false; current_thread=0;
    io_depth=0; io_owner=0; io_private_image=NULL; actual_space=1; scheduler_test_rflags=0;
    puts("scheduler-user-return: PASS (observation only)");
}

static void entry_check(bool expected,bool null_output)
{
    struct thread saved[SCHEDULER_INITIAL_THREADS];memcpy(saved,threads,sizeof(saved));
    uint64_t old_current=current_thread,old_space=actual_space,old_depth=io_depth,old_owner=io_owner;
    uint64_t old_flags=scheduler_test_rflags,old_irq=fixture_irq_flags,old_fs=fixture_fs,old_tss=fixture_tss;
    unsigned old_switches=switches,old_frees=frees,old_clock=clock_calls,old_fs_writes=fixture_fs_writes;
    unsigned old_tss_calls=tss_set_calls,old_fpu=fpu_set_calls;
    struct scheduler_user_return_observation out,before;memset(&out,0xa5,sizeof(out));memcpy(&before,&out,sizeof(out));
    assert(scheduler_observe_user_entry(null_output?NULL:&out)==expected);
    if(expected) {
        assert(out.thread_identity==42 && out.process==(void *)0x1234 && out.user_thread==threads[3].user_thread_context);
        assert(out.address_space==0x2000 && out.image_generation==77 && out.user_fpu==threads[3].user_fpu);
        assert(out.kernel_fpu==&threads[3].kernel_fpu && out.user_fs_base==0x4000 && out.kernel_stack_top==0x9000);
    } else assert(!memcmp(&out,&before,sizeof(out)));
    assert(!memcmp(saved,threads,sizeof(saved)) && current_thread==old_current && actual_space==old_space);
    assert(io_depth==old_depth && io_owner==old_owner && scheduler_test_rflags==old_flags && fixture_irq_flags==old_irq);
    assert(fixture_fs==old_fs && fixture_tss==old_tss && fixture_fs_writes==old_fs_writes);
    assert(switches==old_switches && frees==old_frees && clock_calls==old_clock && tss_set_calls==old_tss_calls && fpu_set_calls==old_fpu);
}

static void test_user_entry_observation(void)
{
    struct fpu_state selected={0};
    memset(threads,0,sizeof(threads));initialized=true;switching=false;current_thread=3;
    actual_space=0x2000;kernel_space=0x1000;scheduler_test_rflags=0;io_depth=0;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.identity=42,.user_context=(void *)0x1234,
        .address_space=0x2000,.image_generation=77,.user_fpu=&selected,.user_fs_base=0x4000,.kernel_stack_top=0x9000};
    entry_check(true,false);entry_check(false,true);
    threads[3].account_user=true;threads[3].io_user_safe=true;io_depth=2;io_owner=4;
    entry_check(true,false);threads[3].user_thread_context=(void *)0x5678;entry_check(true,false);
    scheduler_test_rflags=0x200;entry_check(false,false);scheduler_test_rflags=0;
    initialized=false;entry_check(false,false);initialized=true;
    switching=true;entry_check(false,false);switching=false;
    current_thread=thread_capacity;entry_check(false,false);current_thread=3;
    threads[3].state=THREAD_READY;entry_check(false,false);threads[3].state=THREAD_RUNNING;
    threads[3].identity=0;entry_check(false,false);threads[3].identity=42;
    threads[3].user_context=NULL;entry_check(false,false);threads[3].user_context=(void *)0x1234;
    threads[3].image_generation=0;entry_check(false,false);threads[3].image_generation=77;
    threads[3].user_fpu=NULL;entry_check(false,false);threads[3].user_fpu=&selected;
    threads[3].address_space=0;entry_check(false,false);threads[3].address_space=0x2001;entry_check(false,false);
    threads[3].address_space=0x1000;entry_check(false,false);threads[3].address_space=0x3000;entry_check(false,false);
    threads[3].address_space=0x2000;
    memset(threads,0,sizeof(threads));initialized=false;current_thread=0;io_depth=io_owner=0;actual_space=kernel_space=1;
    puts("scheduler-user-entry: PASS (borrowed pre-dispatch observation)");
}

static void worker_stack_refused(struct scheduler_user_worker_stack *out,
                                 const struct scheduler_user_worker_stack *before)
{
    struct thread saved[SCHEDULER_INITIAL_THREADS];memcpy(saved,threads,sizeof(saved));
    uint64_t current=current_thread,space=actual_space,depth=io_depth,owner=io_owner;
    uint64_t irq=fixture_irq_flags,observed_flags=scheduler_test_rflags;
    unsigned clock=clock_calls,changed=switches,free_count=frees;
    assert(!scheduler_observe_user_worker_stack(out));
    assert(!memcmp(out,before,sizeof(*out)) && !memcmp(saved,threads,sizeof(saved)));
    assert(current_thread==current && actual_space==space && io_depth==depth && io_owner==owner);
    assert(fixture_irq_flags==irq && scheduler_test_rflags==observed_flags);
    assert(clock_calls==clock && switches==changed && frees==free_count);
}

static void test_worker_stack_observation(void)
{
    struct fpu_state fpu={0};struct scheduler_user_worker_stack out,before;
    memset(threads,0,sizeof(threads));initialized=true;switching=false;current_thread=3;io_depth=0;
    scheduler_test_rflags=0;actual_space=0x2000;growing_allocations=false;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.identity=7,.user_context=(void *)3,
        .user_thread_context=(void *)4,.user_fpu=&fpu,.address_space=0x2000,.image_generation=77,
        .stack=fake_stack,.stack_physical=4096};
    assert(scheduler_observe_user_worker_stack(&out));
    assert(out.thread_identity==7 && out.base==(uint64_t)fake_stack && out.physical==4096 &&
           out.frames==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE && out.top==(uint64_t)fake_stack+SCHEDULER_STACK_SIZE);
    memset(&out,0xa5,sizeof(out));memcpy(&before,&out,sizeof(out));
    assert(!scheduler_observe_user_worker_stack(NULL));
    threads[3].user_thread_context=NULL;worker_stack_refused(&out,&before);threads[3].user_thread_context=(void *)4;
    threads[3].kernel_stack_top=16;worker_stack_refused(&out,&before);threads[3].kernel_stack_top=0;
    invalid_stack_view=true;worker_stack_refused(&out,&before);invalid_stack_view=false;
    threads[3].stack_physical=0;worker_stack_refused(&out,&before);
    threads[3].stack_physical=4097;worker_stack_refused(&out,&before);
    threads[3].stack_physical=0x10000000000000ull;worker_stack_refused(&out,&before);
    threads[3].stack_physical=4096;threads[3].stack=fake_stack+16;
    worker_stack_refused(&out,&before);threads[3].stack=fake_stack;
    scheduler_test_rflags=0x200;worker_stack_refused(&out,&before);scheduler_test_rflags=0;
    assert(!memcmp(&out,&before,sizeof(out)));
    memset(threads,0,sizeof(threads));initialized=false;current_thread=0;actual_space=1;
    puts("scheduler-worker-stack: PASS (unwired borrowed owned extent)");
}

static void expect_binding_refusal(uint64_t root,uint64_t stack,uint64_t generation)
{
    static struct thread before;
    static unsigned fs_writes,fpu_calls,tss_calls,clock_count,diagnostics;
    static uint64_t fs,tss,space;
    memcpy(&before,&threads[3],sizeof(before));
    fs_writes=fixture_fs_writes;fpu_calls=fpu_set_calls;tss_calls=tss_set_calls;clock_count=clock_calls;
    diagnostics=bind_diagnostics;fs=fixture_fs;tss=fixture_tss;space=actual_space;
    fixture_irq_flags=0x200;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,root,stack,generation);assert(0); }
    assert(bind_diagnostics==diagnostics+1 && !(fixture_irq_flags&0x200));
    assert(!memcmp(&before,&threads[3],sizeof(before)) && fixture_fs_writes==fs_writes && fixture_fs==fs);
    assert(fpu_set_calls==fpu_calls && tss_set_calls==tss_calls && fixture_tss==tss && clock_calls==clock_count && actual_space==space);
}

static void test_binding_contract(void)
{
    static struct fpu_state selected;
    static struct thread before;
    static unsigned initial_diagnostics;
    initialized=false;fixture_irq_flags=0x200;initial_diagnostics=bind_diagnostics;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,0x2000,0x8000,55);assert(0); }
    assert(bind_diagnostics==initial_diagnostics+1 && !(fixture_irq_flags&0x200));
    memset(threads,0,sizeof(threads));initialized=true;switching=false;current_thread=3;io_depth=0;kernel_space=0x1000;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.identity=3,.user_fpu=(void *)1};
    threads[3].kernel_fpu.active_user_trap=(void *)0xabc;
    resolved_parent_binding=(struct process_user_binding){.user_fpu=&selected,.fs_base=0x4000,.kernel_stack_top=0x8000};
    actual_space=1;resolver_ok=prepare_ok=true;
    scheduler_set_user_context((void *)7,0x2000,0x8000,55);
    assert(!threads[3].kernel_fpu.active_user_trap && threads[3].user_fpu==&selected);
    assert(threads[3].user_context==(void *)7 && threads[3].address_space==0x2000 &&
           threads[3].image_generation==55 && threads[3].user_fs_base==0x4000 && actual_space==0x2000);
    /* A stale user-FPU pointer must never be dereferenced on clear. */
    threads[3].user_fpu=(void *)1;threads[3].kernel_fpu.active_user_trap=(void *)0xabc;
    scheduler_clear_user_context();assert(!threads[3].kernel_fpu.active_user_trap && !threads[3].user_context);
    threads[3].kernel_fpu.active_user_trap=(void *)0xabc;
    scheduler_invalidate_current_user_trap();assert(!threads[3].kernel_fpu.active_user_trap);
    expect_binding_refusal(0,0x8000,55);
    expect_binding_refusal(0x2000,0x8000,0);
    expect_binding_refusal(0x2000,0x8010,55);
    switching=true;expect_binding_refusal(0x2000,0x8000,55);switching=false;
    current_thread=thread_capacity;expect_binding_refusal(0x2000,0x8000,55);current_thread=3;
    threads[3].state=THREAD_READY;expect_binding_refusal(0x2000,0x8000,55);threads[3].state=THREAD_RUNNING;
    threads[3].kernel_fpu.active_user_trap=(void *)0xabc;
    threads[3].user_thread_context=(void *)0x123;memcpy(&before,&threads[3],sizeof(before));
    unsigned calls=fpu_set_calls,tss=tss_set_calls,clocks=clock_calls;
    volatile unsigned diags=bind_diagnostics;
    unsigned fs_writes=fixture_fs_writes;uint64_t fs_value=fixture_fs,tss_value=fixture_tss;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,0x2000,0x8000,55);assert(0); }
    assert(bind_diagnostics==diags+1 && !memcmp(&before,&threads[3],sizeof(before)));
    assert(fpu_set_calls==calls && tss_set_calls==tss && clock_calls==clocks && actual_space==0x1000);
    assert(fixture_fs_writes==fs_writes && fixture_fs==fs_value && fixture_tss==tss_value);
    threads[3].user_thread_context=NULL;memcpy(&before,&threads[3],sizeof(before));resolver_ok=false;
    diags=bind_diagnostics;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,0x2000,0x8000,55);assert(0); }
    assert(bind_diagnostics==diags+1 && !memcmp(&before,&threads[3],sizeof(before)) && actual_space==0x1000);
    assert(fixture_fs_writes==fs_writes && fixture_fs==fs_value && fixture_tss==tss_value);
    resolver_ok=true;prepare_ok=false;diags=bind_diagnostics;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,0x2000,0x8000,55);assert(0); }
    assert(bind_diagnostics==diags+1 && !memcmp(&before,&threads[3],sizeof(before)) && actual_space==0x1000);
    assert(fixture_fs_writes==fs_writes && fixture_fs==fs_value && fixture_tss==tss_value);
    prepare_ok=true;
    io_depth=1;io_owner=4;diags=bind_diagnostics;
    if(!setjmp(bind_failure)) { scheduler_set_user_context((void *)7,0x2000,0x8000,55);assert(0); }
    assert(bind_diagnostics==diags+1 && !memcmp(&before,&threads[3],sizeof(before)) && io_depth==1 && io_owner==4);
    assert(fixture_fs_writes==fs_writes && fixture_fs==fs_value && fixture_tss==tss_value);
    io_depth=0;diags=bind_diagnostics;
    if(!setjmp(bind_failure)) { scheduler_set_user_context_fpu((void *)7,0x2000,0x8000,(void *)0x123,55);assert(0); }
    assert(bind_diagnostics==diags+1 && !memcmp(&before,&threads[3],sizeof(before)));
    assert(fixture_fs_writes==fs_writes && fixture_fs==fs_value && fixture_tss==tss_value);
    memset(threads,0,sizeof(threads));initialized=false;current_thread=0;io_depth=0;actual_space=1;kernel_space=1;
    puts("scheduler-binding: PASS (atomic validation, stale-pointer-safe invalidation)");
}

int main(void) {
    test_binding_contract();
    test_user_entry_observation();
    test_worker_stack_observation();
    test_user_return_observation();
    test_user_timer_return();
    test_foreign_io_return();
    test_gate_retention();
    test_idle_audit();
    test_mwait();
    test_coordinated_mwait();
    test_independent_process_memory();
    test_helper_atomic_memory();
    test_user_thread_publication();
    test_private_image_scope();
    test_wait_trace();
    test_cpu_accounting();
    test_resume_address_space();
    _Alignas(16) uint8_t stack[64];
    uint64_t rsp=initial_thread_stack((uint64_t)stack+sizeof(stack));
    assert((rsp&15)==8 && rsp==(uint64_t)stack+56 && *(uint64_t *)rsp==0);
    assert(!scheduler_current_thread_id());
    assert(!scheduler_io_begin());
    initialized = true;
    assert(!scheduler_io_begin()); /* Early boot must not enable IRQs. */
    scheduler_enable_io_services();
    current_thread = 3;
    threads[3].state = THREAD_RUNNING;
    threads[4].state = THREAD_READY;
    threads[4].io_eligible = ready;
    threads[4].arg = (void *)1;
    threads[5].state = THREAD_READY; /* Ordinary application. */
    threads[6].state = THREAD_READY;
    threads[6].io_eligible = ready;
    threads[6].arg = (void *)1;
    threads[6].user_context = (void *)1; /* Never treat userspace as a service. */
    assert(scheduler_io_begin() && scheduler_io_active());
    threads[3].address_space=2;
    assert(!scheduler_io_yield()); /* A temporary CR3 cannot cross a context switch. */
    actual_space=2; assert(scheduler_io_yield());
    clock_us=3999; scheduler_io_checkpoint(); assert(io_service_at==0);
    clock_us=4000; scheduler_io_checkpoint(); assert(io_service_at==4000);
    clock_us=7999; scheduler_io_checkpoint(); assert(io_service_at==4000);
    clock_us=8000; scheduler_io_checkpoint(); assert(io_service_at==8000);
    assert(!scheduler_io_busy());
    assert(io_thread_allowed(3) && io_thread_allowed(4));
    assert(!io_thread_allowed(5) && !io_thread_allowed(6));
    /* Other processes can execute userspace/audited console paths, but an
     * owner's sibling cannot mutate its user buffers while DMA is pending. */
    threads[3].user_context=(void *)2;
    threads[6].io_user_safe=true;
    assert(io_thread_allowed(6));
    threads[6].user_context=(void *)2;
    assert(!io_thread_allowed(6));
    threads[6].user_context=(void *)1;
    threads[6].io_user_safe=false;
    assert(!io_thread_allowed(6));
    assert(has_ready_thread_except(3));
    threads[4].arg = NULL;
    assert(!has_ready_thread_except(3));
    /* Context may be freed as soon as an I/O worker has retired. */
    threads[2].state=THREAD_DEAD;threads[2].io_eligible=retired_context;
    assert(!has_ready_thread_except(3));
    assert(next_ready_thread(3)==3);
    threads[2]=(struct thread){0};
    assert(scheduler_io_begin() && io_depth == 2);
    current_thread = 4;
    assert(scheduler_io_busy());
    assert(!scheduler_io_begin()); scheduler_io_end(); assert(io_depth == 2);
    current_thread = 3;
    threads[7].state=THREAD_BLOCKED; threads[7].blocked_queue=&io_waiters;
    io_waiters.waiters=1;
    scheduler_io_end(); assert(scheduler_io_active());
    assert(threads[7].state==THREAD_BLOCKED);
    scheduler_io_end(); assert(!scheduler_io_active() && io_thread_allowed(5));
    assert(threads[7].state==THREAD_READY && !io_waiters.waiters && !threads[7].blocked_queue);
    scheduler_io_end(); assert(io_depth == 0);
    switching = true; assert(!scheduler_io_begin());
    switching=false;memset(threads,0,sizeof(threads));
    current_thread=3;threads[3].state=THREAD_RUNNING;
    assert(scheduler_io_begin());
    threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;current_thread=4;
    scheduler_io_wait();
    assert(switches==1 && !io_depth && threads[4].blocked_queue==NULL);
    scheduler_io_wait();assert(switches==1); /* Uncontended calls do not sleep. */
    memset(threads,0,sizeof(threads));idle_thread=1;current_thread=3;
    threads[1].state=THREAD_READY;threads[1].io_eligible=idle_eligible;
    threads[3].state=THREAD_RUNNING;threads[4].state=THREAD_READY;
    assert(next_ready_thread(3)==4); /* Idle cannot steal a normal turn. */
    threads[4].state=THREAD_BLOCKED;assert(next_ready_thread(3)==3);
    threads[3].state=THREAD_BLOCKED;assert(next_ready_thread(3)==1);
    assert(has_ready_thread_except(3)); /* Last ordinary worker may sleep. */
    assert(scheduler_io_begin() && io_thread_allowed(1));
    threads[4].state=THREAD_READY;assert(next_ready_thread(3)==1); /* Unsafe during I/O. */
    threads[3].state=THREAD_READY;current_thread=1;threads[1].state=THREAD_RUNNING;
    assert(next_ready_thread(1)==3); /* I/O owner resumes before idle. */
    current_thread=3;scheduler_io_end();
    memset(threads,0,sizeof(threads));current_thread=0;threads[0].state=THREAD_RUNNING;threads[0].identity=1;next_identity=2;
    assert(scheduler_current_thread_id()==1);
    assert(scheduler_spawn("first",empty_worker,NULL) && threads[1].identity==2);
    current_thread=1;assert(scheduler_current_thread_id()==2);current_thread=0;
    threads[1].state=THREAD_DEAD;
    assert(scheduler_spawn("reused",empty_worker,NULL) && threads[1].identity==3 && frees==1);
    current_thread=1;assert(scheduler_current_thread_id()==3);current_thread=0;
    next_identity=UINT64_MAX;assert(!scheduler_spawn("exhausted",empty_worker,NULL));
    memset(threads,0,sizeof(threads));current_thread=3;idle_thread=1;next_identity=20;
    threads[1]=(struct thread){.identity=2,.state=THREAD_READY,.io_eligible=idle_eligible};
    threads[3]=(struct thread){.identity=10,.state=THREAD_RUNNING,.stack=fake_stack,.stack_physical=4096,.io_eligible=ready};
    struct scheduler_wait_queue held_wait={.waiters=1};
    threads[4]=(struct thread){.identity=11,.state=THREAD_BLOCKED,.stack=fake_stack,.stack_physical=4096,.io_eligible=ready,
        .blocked_queue=&held_wait,.blocked_deadline_ticks=1};
    struct thread original[2]={threads[3],threads[4]};
    assert(!scheduler_quarantine_service_pair(9,11));assert(!scheduler_quarantine_service_pair(10,10));
    assert(!scheduler_quarantine_service_pair(10,2));assert(!scheduler_quarantine_service_pair(10,12));
    switching=true;assert(!scheduler_quarantine_service_pair(10,11));switching=false;
    threads[1].state=THREAD_BLOCKED;assert(!scheduler_quarantine_service_pair(10,11));threads[1].state=THREAD_READY;
    threads[3].address_space=1;assert(!scheduler_quarantine_service_pair(10,11));threads[3]=original[0];
    threads[3].user_context=(void *)1;assert(!scheduler_quarantine_service_pair(10,11));threads[3]=original[0];
    threads[4].user_context=(void *)1;assert(!scheduler_quarantine_service_pair(10,11));threads[4]=original[1];
    threads[4].state=THREAD_DEAD;assert(!scheduler_quarantine_service_pair(10,11));threads[4]=original[1];
    io_depth=1;io_owner=4;assert(!scheduler_quarantine_service_pair(10,11));
    io_owner=3;assert(!scheduler_quarantine_service_pair(10,11));
    io_owner=5;threads[5]=(struct thread){.identity=12,.state=THREAD_READY};
    assert(!memcmp(&threads[3],&original[0],sizeof(struct thread)) && !memcmp(&threads[4],&original[1],sizeof(struct thread)));
    assert(scheduler_quarantine_service_pair(10,11));assert(io_depth==1 && io_owner==5);
    assert(scheduler_service_pair_quarantined(10,11));assert(!scheduler_service_pair_quarantined(10,12));
    assert(!held_wait.waiters && !threads[4].blocked_queue && !threads[4].blocked_deadline_ticks);
    held_wait.waiters=1;assert(!scheduler_wake_count(&held_wait,1) && !held_wait.waiters);
    clock_us=20000;wake_expired_waits();assert(scheduler_service_pair_quarantined(10,11));
    assert(next_ready_thread(3)==5);assert(!scheduler_quarantine_service_pair(10,11));
    current_thread=5;scheduler_io_end();unsigned old_frees=frees;
    assert(scheduler_spawn("after quarantine",empty_worker,NULL));assert(frees==old_frees);
    assert(scheduler_service_pair_quarantined(10,11));
    assert(threads[3].stack==original[0].stack && threads[4].stack==original[1].stack);
    for(unsigned i=1;i<SCHEDULER_INITIAL_THREADS;i++)if(threads[i].state==THREAD_UNUSED)threads[i].state=THREAD_READY;
    thread_storage_budget=thread_storage_bytes;
    assert(!scheduler_spawn("must not reuse quarantine",empty_worker,NULL) && frees==old_frees);
    assert(scheduler_service_pair_quarantined(10,11));
    thread_storage_budget=SCHEDULER_STORAGE_BUDGET;

    /* An IRQ entry guard may hold only noncurrent services. All rejection
     * cases must leave the interrupted storage context and both targets intact. */
    memset(threads,0,sizeof(threads));current_thread=5;idle_thread=1;
    threads[1]=(struct thread){.identity=2,.state=THREAD_READY,.io_eligible=idle_eligible};
    threads[3]=original[0];threads[3].state=THREAD_READY;threads[4]=original[1];
    held_wait.waiters=1;
    threads[5]=(struct thread){.identity=12,.state=THREAD_RUNNING,.address_space=2,.user_context=(void *)2};
    struct thread interrupted=threads[5],targets[2]={threads[3],threads[4]};
    io_owner=5;io_depth=2;
    assert(!scheduler_quarantine_noncurrent_services(10,12));
    assert(!scheduler_quarantine_noncurrent_services(12,11));
    assert(!scheduler_quarantine_noncurrent_services(10,10));
    assert(!scheduler_quarantine_noncurrent_services(10,99));
    assert(!scheduler_quarantine_noncurrent_services(2,11));
    switching=true;assert(!scheduler_quarantine_noncurrent_services(10,11));switching=false;
    threads[3].state=THREAD_RUNNING;assert(!scheduler_quarantine_noncurrent_services(10,11));threads[3]=targets[0];
    threads[3].user_thread_context=(void *)1;assert(!scheduler_quarantine_noncurrent_services(10,11));threads[3]=targets[0];
    threads[4].state=THREAD_DEAD;assert(!scheduler_quarantine_noncurrent_services(10,11));threads[4]=targets[1];
    io_owner=3;assert(!scheduler_quarantine_noncurrent_services(10,11));
    io_owner=4;assert(!scheduler_quarantine_noncurrent_services(10,11));io_owner=5;
    assert(!memcmp(&threads[3],&targets[0],sizeof(struct thread)) && !memcmp(&threads[4],&targets[1],sizeof(struct thread)));
    assert(held_wait.waiters==1);
    assert(scheduler_quarantine_noncurrent_services(10,11));
    assert(!memcmp(&threads[5],&interrupted,sizeof(struct thread)) && current_thread==5 && io_owner==5 && io_depth==2);
    assert(scheduler_service_pair_quarantined(10,11) && !held_wait.waiters);
    held_wait.waiters=2;scheduler_wake_all(&held_wait);wake_expired_waits();
    assert(scheduler_service_pair_quarantined(10,11) && next_ready_thread(5)==5);
    assert(!scheduler_quarantine_noncurrent_services(10,11));
    /* Deadline expiry and explicit queue wake are distinct transitions. */
    memset(threads,0,sizeof(threads));
    struct scheduler_wait_queue diagnostic_wait={.waiters=2};
    clock_us=100000;
    threads[1]=(struct thread){.state=THREAD_BLOCKED,.name="test worker",
        .identity=41,.blocked_queue=&diagnostic_wait,.blocked_deadline_ticks=10};
    threads[2]=(struct thread){.state=THREAD_BLOCKED,.name="peer",
        .identity=42,.blocked_queue=&diagnostic_wait,.blocked_deadline_ticks=11};
    wake_expired_waits();wake_expired_waits();
    assert(threads[1].timeout_wakes==1 && !threads[1].signal_wakes);
    assert(threads[2].state==THREAD_BLOCKED && diagnostic_wait.waiters==1);
    assert(scheduler_wake_count(&diagnostic_wait,1)==1);
    assert(!scheduler_wake_count(&diagnostic_wait,1));
    clock_us=200000;wake_expired_waits();
    assert(threads[2].signal_wakes==1 && !threads[2].timeout_wakes);
    struct activity_sample samples[SCHEDULER_INITIAL_THREADS];uint64_t at=0;
    assert(activity_snapshot(samples,&at)==2 && at==clock_us);
    assert(samples[0].identity==41 && samples[0].slot==1 && samples[0].timeouts==1);
    assert(!strcmp(samples[0].name,"test_worker") && samples[1].signals==1);
    /* Slot reuse starts a fresh identity/counter lifetime, with a copied name. */
    threads[1].state=THREAD_DEAD;assert(scheduler_spawn("replacement",empty_worker,NULL));
    assert(!threads[1].timeout_wakes && !threads[1].signal_wakes);
    assert(!strcmp(samples[0].name,"test_worker"));
    assert(activity_snapshot(samples,&at)==2 && samples[0].identity!=41);
    puts("scheduler-activity: PASS expiry/signal transitions, no double counts, stable snapshots, slot reuse");
    puts("scheduler-irq-quarantine: PASS noncurrent-only, atomic refusal, interrupted storage untouched");
    puts("scheduler-quarantine: PASS stale/user/owner refusal, atomicity, wait unlink, no resurrection or slot/stack reuse");
    test_io_observation();
    test_wake_policy();
    test_completion_wait();
    test_ready_gate_admission();
    test_gate_handoff();
    test_gate_handoff_fairness();
    test_ready_edge_admission();
    test_growth();
    puts("io-scheduler-host: PASS (boot gating, owner/nesting, service eligibility, task isolation)");
}

void latency_owner_begin(void) {}
void latency_owner_end(void) {}
