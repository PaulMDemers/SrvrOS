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
uint64_t vmm_current_address_space(void) { return actual_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t space) { actual_space=space; }
uint64_t gdt_default_kernel_stack_top(void) { return 0; }
void gdt_set_kernel_stack(uint64_t stack) { (void)stack; }
void fpu_switch_kernel_state(struct fpu_state *old,struct fpu_state *next,struct fpu_state *user) {
    (void)old;(void)next;(void)user;
}
static unsigned switches;
void scheduler_context_switch(struct scheduler_context *old,struct scheduler_context *next) {
    assert(old==&threads[4].context && next==&threads[3].context);
    assert(current_thread==3 && threads[4].state==THREAD_BLOCKED);
    assert(io_waiters.waiters==(1ull<<4));
    scheduler_io_end(); /* Simulate owner completing while caller sleeps. */
    assert(threads[4].state==THREAD_READY && !io_waiters.waiters);
    threads[3].state=THREAD_READY;threads[4].state=THREAD_RUNNING;
    current_thread=4;switches++;
}
static bool ready(void *arg) { return arg != NULL; }
int main(void) {
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
    puts("io-scheduler-host: PASS (boot gating, owner/nesting, service eligibility, task isolation)");
}

void latency_owner_begin(void) {}
void latency_owner_end(void) {}
