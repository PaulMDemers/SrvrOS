#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#define SRVROS_SCHEDULER_HOST_TEST
#include "../kernel/src/scheduler.c"
static uint64_t actual_space=1,clock_us;
static _Alignas(16) uint8_t fake_stack[SCHEDULER_STACK_SIZE];
static unsigned frees;
static bool growing_allocations,fail_stack,fail_records;
static unsigned live_allocations;

uint64_t pmm_alloc_frames_above_tagged(uint64_t count,uint64_t minimum,enum pmm_frame_tag tag) {
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
void fpu_init_state(struct fpu_state *state) {(void)state;}
static void empty_worker(void *arg) {(void)arg;}

uint64_t timer_monotonic_us(void) { return clock_us; }
uint64_t timer_ticks(void) { return clock_us/10000; }
uint64_t vmm_current_address_space(void) { return actual_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t space) { actual_space=space; }
uint64_t gdt_default_kernel_stack_top(void) { return 0; }
void gdt_set_kernel_stack(uint64_t stack) { (void)stack; }
void fpu_switch_kernel_state(struct fpu_state *old,struct fpu_state *next,struct fpu_state *user) {
    (void)old;(void)next;(void)user;
}
static unsigned switches;
static bool address_space_test;
static uint64_t expected_space,return_space;
static bool completion_wait_test,completion_signal,completion_ready,reference=true;
bool timer_reference_ready(void) { return reference; }
static struct scheduler_wait_queue completion_queue;
static bool high_wait_test;
static bool handoff_test;
static bool retry_turn_test;
static unsigned trace_test;
void fpu_set_current_user_state(struct fpu_state *state) { (void)state; }
static unsigned handoff_admissions;
static struct scheduler_wait_queue high_wait;
static bool completion_condition(void *arg) { assert(arg==&completion_ready);return completion_ready; }
void scheduler_context_switch(struct scheduler_context *old,struct scheduler_context *next) {
    if(trace_test) {
        if(current_thread==1) {
            assert(old==&threads[0].context && threads[0].state==THREAD_BLOCKED);
            assert(threads[0].trace.out.blocks==1);
            switching=false;clock_us+=10000;wake_expired_waits();
            assert(threads[0].state==THREAD_READY && threads[0].trace.out.timeouts==1);
            io_depth=1;io_owner=1;
            assert(!io_thread_allowed(0)); /* Recording must not admit the unsafe thread. */
            clock_us+=520000;assert(!io_thread_allowed(0));
            io_depth=0;scheduler_yield();
            assert(current_thread==0);return;
        }
        assert(current_thread==0 && old==&threads[1].context && next==&threads[0].context);
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
    threads[3]=(struct thread){.state=THREAD_RUNNING,.io_user_safe=true};
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
    current_thread=6;threads[6].state=THREAD_RUNNING;threads[3].state=THREAD_READY;
    scheduler_clear_user_context();
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
    threads[3]=(struct thread){.state=THREAD_RUNNING,.io_user_safe=true};
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
    address_space_test=true;expected_space=2;return_space=1;
    scheduler_yield();assert(threads[0].resume_address_space==1 && actual_space==1);
    /* Resume the sibling's temporary kernel continuation, even though its
     * process identity still names address space two. */
    actual_space=2;threads[1].resume_address_space=1;expected_space=1;return_space=2;
    scheduler_yield();assert(threads[0].resume_address_space==2 && actual_space==2);
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
    clock_us--;trace_ready(&threads[0],0);assert(scheduler_trace(SRV_SCHED_TRACE_END,&out));assert(out.invalid);
    memset(threads,0,sizeof(threads));initialized=false;switching=false;io_depth=io_gate_pending=0;clock_us=0;current_thread=0;
    puts("scheduler-wait-trace: PASS blocked versus ready, unchanged exclusion, timeout/signal, reset/refusal and invalid clock");
}

int main(void) {
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
    test_wake_policy();
    test_completion_wait();
    test_ready_gate_admission();
    test_gate_handoff();
    test_growth();
    puts("io-scheduler-host: PASS (boot gating, owner/nesting, service eligibility, task isolation)");
}

void latency_owner_begin(void) {}
void latency_owner_end(void) {}
