#include <assert.h>
#include <stdio.h>
#define SRVROS_SYSCALL_SLEEP_HOST_TEST
#include "../kernel/src/arch/x86_64/syscall.c"
static uint64_t now;
static unsigned waits,yields,spurious;
static bool killed,signaled,no_scheduler;
static int interrupt;
static struct scheduler_wait_queue queue;
uint64_t timer_ticks(void) { return now; }
bool process_should_exit_current(void) { return killed; }
bool process_signal_pending_current(void) { return signaled; }
struct scheduler_wait_queue *process_file_poll_wait_queue(void) { return &queue; }
void scheduler_yield(void) { yields++;now++; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&queue && !ready(arg) && deadline>now);waits++;
    if(no_scheduler) return false;
    if(spurious) { spurious--;now++;assert(!ready(arg));return true; }
    if(interrupt==1) killed=true;
    else if(interrupt==2) signaled=true;
    else now=deadline;
    assert(ready(arg));return true;
}
int main(void) {
    now=40;assert(syscall_sleep_ticks(0)==0 && !waits && !yields);
    assert(syscall_sleep_ticks(7)==0 && now==47 && waits==1 && !yields);
    spurious=3;assert(syscall_sleep_ticks(20)==0 && now==67 && waits==5 && !yields);
    interrupt=2;assert(syscall_sleep_ticks(500)==SRV_ERR_INTR && now==67 && waits==6);
    signaled=false;interrupt=1;assert(syscall_sleep_ticks(500)==-1 && waits==7);
    killed=false;interrupt=0;no_scheduler=true;assert(syscall_sleep_ticks(3)==0 && now==70 && yields==3);
    now=UINT64_MAX-10;assert(syscall_sleep_ticks(11)==-1 && waits==10);
    puts("syscall-sleep: PASS deadline blocking, spurious wakes, signals/cancel, zero/overflow and early-boot fallback");
}
