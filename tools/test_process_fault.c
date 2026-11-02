#include <assert.h>
#include <stdint.h>
#define SRVROS_PROCESS_HOST_TEST
#include "../kernel/src/process.c"
static struct process target;
static struct process_user_thread worker;
static unsigned wakes, thread_wakes;
static unsigned gate_order;
void scheduler_io_set_user_safe(bool safe) { assert(!safe && gate_order==0); gate_order=1; }
void scheduler_io_wait(void) { assert(gate_order==1); gate_order=2; }
void *scheduler_current_user_context(void) { return &target; }
void *scheduler_current_user_thread_context(void) { return &worker; }
void net_process_wake(struct process *p) { assert(p==&target); }
void keyboard_wake_waiters(void) {}
void pty_wake(void) {}
void scheduler_wake_all(struct scheduler_wait_queue *q) {
    wakes++; if(q==&target.thread_wait_queue) thread_wakes++;
}
int main(void) {
    process_exit_gate(); assert(gate_order==2);
    target.active=true; target.allocated=true; worker.owner=&target; worker.active=true; worker.used=true;
    assert(process_record_user_fault(SRV_SIGNAL_ILL));
    assert(target.killed && target.kill_signal==SRV_SIGNAL_ILL && process_should_exit_current());
    assert(process_current_kill_status()==128+SRV_SIGNAL_ILL && thread_wakes==1 && wakes>=5);
    assert(process_record_user_fault(SRV_SIGNAL_SEGV));
    assert(target.kill_signal==SRV_SIGNAL_ILL); /* First fatal cause wins. */
    target.active=false; assert(!process_record_user_fault(SRV_SIGNAL_FPE));
    return 0;
}
