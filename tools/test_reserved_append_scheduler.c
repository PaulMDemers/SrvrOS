/* Reuse the complete real-scheduler fixture and its backend seams. The new
 * entry predicate is exercised directly against actual scheduler state. */
#include <srvros/signal_syscall_numbers.h>
/* The reused legacy C main relies on the language's implicit return0. It is
 * not executed here; renaming that fixture entry removes that special rule. */
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type"
#define main existing_io_scheduler_main
#include "test_io_scheduler.c"
#undef main
#pragma GCC diagnostic pop

static void entry_setup(void *process,void *other) {
    memset(threads,0,sizeof(threads));initialized=io_services_enabled=true;switching=false;
    current_thread=3;actual_space=2;io_depth=2;io_owner=4;io_private_image=NULL;
    scheduler_test_rflags=0;fixture_irq_flags=0;
    threads[3]=(struct thread){.state=THREAD_RUNNING,.user_context=process,.address_space=2,.io_syscall=SYS_WRITE};
    threads[4]=(struct thread){.state=THREAD_READY,.user_context=other,.address_space=3};
}
static bool queued_ready;
static bool queued_condition(void *arg) {assert(arg==&queued_ready);return queued_ready;}
static void queued_switch(struct scheduler_context *old,struct scheduler_context *next) {
    assert(old==&threads[3].context && next==&threads[4].context && current_thread==4);
    assert(threads[3].state==THREAD_BLOCKED && threads[3].blocked_queue==&io_waiters && io_waiters.waiters==1);
    assert(io_depth==2 && io_owner==4 && threads[3].io_gate_waiting);
    queued_ready=true;scheduler_io_private_append_progress();
    assert(threads[3].state==THREAD_READY && !io_waiters.waiters && io_thread_allowed(3));
    assert(!io_thread_allowed(5)); /* The ordinary queued continuation remains excluded. */
    threads[4].state=THREAD_READY;threads[3].state=THREAD_RUNNING;current_thread=3;actual_space=2;
}
static void queued_wait(void *process,void *other) {
    entry_setup(process,other);threads[3].io_user_safe=true;threads[3].identity=30;threads[4].identity=40;
    threads[5]=(struct thread){.state=THREAD_READY,.user_context=(void *)0x30,.address_space=1};
    idle_thread=UINT64_MAX;io_waiters=(struct scheduler_wait_queue){0};io_gate_pending=0;queued_ready=false;
    extra_context_switch=queued_switch;
    assert(scheduler_io_private_append_wait(process,queued_condition,&queued_ready));
    extra_context_switch=NULL;
    assert(queued_ready && io_owner==4 && io_depth==2 && !io_waiters.waiters && actual_space==2 && !threads[3].io_gate_waiting);
    assert(scheduler_io_process_independent(process));
    threads[3].io_syscall=SYS_READ;assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));
    threads[3].io_syscall=SYS_WRITE;threads[3].io_user_safe=false;
    assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));
    threads[3].io_user_safe=true;io_private_image=process;
    assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));io_private_image=NULL;
    threads[4].user_context=process;
    assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));threads[4].user_context=other;
    threads[6]=(struct thread){.state=THREAD_BLOCKED,.user_context=process};
    assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));threads[6]=(struct thread){0};
    actual_space=1;assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));actual_space=2;
    assert(!scheduler_io_private_append_wait(process,NULL,NULL));
    io_depth=0;assert(!scheduler_io_private_append_wait(process,queued_condition,&queued_ready));
}
int main(void) {
    void *process=(void *)0x10,*other=(void *)0x20;
    entry_setup(process,other);
    struct thread before=threads[3];uint64_t owner=io_owner,depth=io_depth,space=actual_space;
    assert(scheduler_io_entry_process_independent(process));
    assert(!scheduler_io_process_independent(process)); /* Old mutation proof stays stronger. */
    assert(!memcmp(&before,&threads[3],sizeof(before)) && io_owner==owner && io_depth==depth && actual_space==space);
    threads[3].io_user_safe=true;
    assert(scheduler_io_entry_process_independent(process) && scheduler_io_process_independent(process));
    threads[3].io_user_safe=false;
    scheduler_test_rflags=0x200;assert(!scheduler_io_entry_process_independent(process));scheduler_test_rflags=0;
    threads[3].account_user=true;assert(!scheduler_io_entry_process_independent(process));threads[3].account_user=false;
    threads[3].io_syscall=SYS_READ;assert(!scheduler_io_entry_process_independent(process));
    threads[3].io_syscall=SYS_SIGNAL_IO;assert(!scheduler_io_entry_process_independent(process));threads[3].io_syscall=SYS_WRITE;
    threads[3].user_thread_context=process;assert(!scheduler_io_entry_process_independent(process));threads[3].user_thread_context=NULL;
    threads[3].user_context=other;assert(!scheduler_io_entry_process_independent(process));threads[3].user_context=process;
    threads[3].state=THREAD_READY;assert(!scheduler_io_entry_process_independent(process));threads[3].state=THREAD_RUNNING;
    actual_space=3;assert(!scheduler_io_entry_process_independent(process));actual_space=2;
    for(unsigned state=THREAD_READY;state<=THREAD_QUARANTINED;state++) {
        threads[5]=(struct thread){.state=state,.user_context=process};
        assert(scheduler_io_entry_process_independent(process)==(state==THREAD_DEAD));
    }
    threads[5]=(struct thread){0};
    io_private_image=process;assert(!scheduler_io_entry_process_independent(process));io_private_image=NULL;
    io_owner=3;assert(!scheduler_io_entry_process_independent(process));io_owner=4;
    threads[4].user_context=process;assert(!scheduler_io_entry_process_independent(process));threads[4].user_context=other;
    uint64_t capacity=thread_capacity;thread_capacity=4097;assert(!scheduler_io_entry_process_independent(process));thread_capacity=capacity;
    io_owner=thread_capacity;assert(!scheduler_io_entry_process_independent(process));io_owner=4;
    current_thread=thread_capacity;assert(!scheduler_io_entry_process_independent(process));current_thread=3;
    switching=true;assert(!scheduler_io_entry_process_independent(process));switching=false;
    io_services_enabled=false;assert(!scheduler_io_entry_process_independent(process));io_services_enabled=true;
    initialized=false;assert(!scheduler_io_entry_process_independent(process));initialized=true;
    assert(!scheduler_io_entry_process_independent(NULL));
    assert(io_depth==2 && io_owner==4 && io_private_image==NULL && actual_space==2 && !threads[3].io_user_safe);
    io_depth=0;assert(scheduler_io_entry_process_independent(process));
    queued_wait(process,other);
    puts("reserved-append-entry-scheduler: PASS read-only IF0/raw-write/sole-main/CR3 proof, original mutation strength and foreign-owner exclusions");
}
