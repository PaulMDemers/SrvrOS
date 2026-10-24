#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SCHEDULER_HOST_TEST
#include "../kernel/src/scheduler.c"
static uint64_t actual_space=1,clock_us;
static _Alignas(16) uint8_t fake_stack[SCHEDULER_STACK_SIZE];
static unsigned frees;
uint64_t pmm_alloc_frames_above_tagged(uint64_t count,uint64_t minimum,enum pmm_frame_tag tag) {
    assert(count==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE && minimum==SCHEDULER_STACK_MIN_PHYSICAL && tag==PMM_FRAME_KERNEL_STACK);return 4096;
}
void pmm_free_frame_range(uint64_t physical,uint64_t count) { assert(physical==4096 && count==SCHEDULER_STACK_SIZE/PMM_FRAME_SIZE);frees++; }
void *pmm_phys_to_virt(uint64_t physical) { assert(physical==4096);return fake_stack; }
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
static bool completion_wait_test,completion_signal,completion_ready,reference=true;
bool timer_reference_ready(void) { return reference; }
static struct scheduler_wait_queue completion_queue;
static bool completion_condition(void *arg) { assert(arg==&completion_ready);return completion_ready; }
void scheduler_context_switch(struct scheduler_context *old,struct scheduler_context *next) {
    if(completion_wait_test) {
        assert(old==&threads[3].context && next==&threads[1].context);
        assert(current_thread==1 && threads[3].state==THREAD_BLOCKED);
        assert(io_depth==2 && io_owner==3 && !io_thread_allowed(5));
        assert(completion_queue.waiters==(1ull<<3));
        if(completion_signal) { completion_ready=true;scheduler_wake_all(&completion_queue); }
        else { clock_us+=10000;wake_expired_waits(); }
        assert(threads[3].state==THREAD_READY && !completion_queue.waiters);
        threads[1].state=THREAD_READY;threads[3].state=THREAD_RUNNING;
        current_thread=3;actual_space=2;switches++;return;
    }
    assert(old==&threads[4].context && next==&threads[3].context);
    assert(current_thread==3 && threads[4].state==THREAD_BLOCKED);
    assert(io_waiters.waiters==(1ull<<4));
    scheduler_io_end(); /* Simulate owner completing while caller sleeps. */
    assert(threads[4].state==THREAD_READY && !io_waiters.waiters);
    threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;
    current_thread=4;switches++;
}
static bool retired_context(void *arg) { (void)arg;assert(!"retired eligibility callback");return false; }
static bool ready(void *arg) { return arg != NULL; }
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
int main(void) {
    test_cpu_accounting();
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
    io_waiters.waiters=1ull<<7;
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
    struct scheduler_wait_queue held_wait={.waiters=1ull<<4};
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
    held_wait.waiters=1ull<<4;assert(!scheduler_wake_count(&held_wait,1) && !held_wait.waiters);
    clock_us=20000;wake_expired_waits();assert(scheduler_service_pair_quarantined(10,11));
    assert(next_ready_thread(3)==5);assert(!scheduler_quarantine_service_pair(10,11));
    current_thread=5;scheduler_io_end();unsigned old_frees=frees;
    assert(scheduler_spawn("after quarantine",empty_worker,NULL));assert(frees==old_frees);
    assert(scheduler_service_pair_quarantined(10,11));
    assert(threads[3].stack==original[0].stack && threads[4].stack==original[1].stack);
    for(unsigned i=1;i<SCHEDULER_MAX_THREADS;i++)if(threads[i].state==THREAD_UNUSED)threads[i].state=THREAD_READY;
    assert(!scheduler_spawn("must not reuse quarantine",empty_worker,NULL) && frees==old_frees);
    assert(scheduler_service_pair_quarantined(10,11));

    /* An IRQ entry guard may hold only noncurrent services. All rejection
     * cases must leave the interrupted storage context and both targets intact. */
    memset(threads,0,sizeof(threads));current_thread=5;idle_thread=1;
    threads[1]=(struct thread){.identity=2,.state=THREAD_READY,.io_eligible=idle_eligible};
    threads[3]=original[0];threads[3].state=THREAD_READY;threads[4]=original[1];
    held_wait.waiters=1ull<<4;
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
    assert(held_wait.waiters==(1ull<<4));
    assert(scheduler_quarantine_noncurrent_services(10,11));
    assert(!memcmp(&threads[5],&interrupted,sizeof(struct thread)) && current_thread==5 && io_owner==5 && io_depth==2);
    assert(scheduler_service_pair_quarantined(10,11) && !held_wait.waiters);
    held_wait.waiters=(1ull<<3)|(1ull<<4);scheduler_wake_all(&held_wait);wake_expired_waits();
    assert(scheduler_service_pair_quarantined(10,11) && next_ready_thread(5)==5);
    assert(!scheduler_quarantine_noncurrent_services(10,11));
    /* Deadline expiry and explicit queue wake are distinct transitions. */
    memset(threads,0,sizeof(threads));
    struct scheduler_wait_queue diagnostic_wait={.waiters=6};
    clock_us=100000;
    threads[1]=(struct thread){.state=THREAD_BLOCKED,.name="test worker",
        .identity=41,.blocked_queue=&diagnostic_wait,.blocked_deadline_ticks=10};
    threads[2]=(struct thread){.state=THREAD_BLOCKED,.name="peer",
        .identity=42,.blocked_queue=&diagnostic_wait,.blocked_deadline_ticks=11};
    wake_expired_waits();wake_expired_waits();
    assert(threads[1].timeout_wakes==1 && !threads[1].signal_wakes);
    assert(threads[2].state==THREAD_BLOCKED && diagnostic_wait.waiters==4);
    assert(scheduler_wake_count(&diagnostic_wait,1)==1);
    assert(!scheduler_wake_count(&diagnostic_wait,1));
    clock_us=200000;wake_expired_waits();
    assert(threads[2].signal_wakes==1 && !threads[2].timeout_wakes);
    struct activity_sample samples[SCHEDULER_MAX_THREADS];uint64_t at=0;
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
    test_completion_wait();
    puts("io-scheduler-host: PASS (boot gating, owner/nesting, service eligibility, task isolation)");
}

void latency_owner_begin(void) {}
void latency_owner_end(void) {}
