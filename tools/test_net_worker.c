#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../kernel/src/net.c"
static jmp_buf done;
static unsigned polls,waits;
static bool fallback,wake_ready,missed;
static uint64_t clock_ticks=100;
bool net_device_rx_wake_ready(void) { return wake_ready; }
uint64_t timer_ticks(void) { return clock_ticks; }
void scheduler_yield(void) { assert(fallback);longjmp(done,1); }
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==&rx_wait_queue); }
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    assert(budget==32 && !verbose);polls++;
    if(missed && polls==4) { assert(clock_ticks==150);longjmp(done,2); }
    if(polls==1) return 32; /* A nonempty budget drains without sleeping. */
    if(polls==2) { net_notify_rx();return 0; } /* IRQ races with empty poll. */
    return 0;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&rx_wait_queue && deadline==clock_ticks+(wake_ready?50:1));waits++;
    if(waits==1) { assert(polls==2 && ready(arg));return true; }
    assert(polls==3 && !ready(arg));
    if(fallback) return false;
    if(missed) { /* Timer expiry permits another poll even with no notification. */
        clock_ticks=deadline;assert(!ready(arg));return true;
    }
    net_notify_rx();assert(ready(arg));longjmp(done,1);
}
int main(void) {
    if(!setjmp(done)) net_worker(NULL);
    assert(polls==3 && waits==2 && rx_worker_packets==32 && rx_worker_empty_polls==2);
    polls=waits=0;fallback=true;
    if(!setjmp(done)) net_worker(NULL);
    assert(polls==3 && waits==2);
    fallback=false;wake_ready=true;polls=waits=0;
    if(!setjmp(done)) net_worker(NULL);
    assert(polls==3 && waits==2);
    missed=true;polls=waits=0;
    int result=setjmp(done);if(!result)net_worker(NULL);
    assert(result==2 && clock_ticks==150 && polls==4 && waits==2);
    wake_ready=false;missed=false;polls=waits=0;
    if(!setjmp(done))net_worker(NULL); /* Readiness loss restores one-tick fallback. */
    assert(polls==3 && waits==2);
    puts("net-worker: PASS provider-dependent deadlines, receive wake, empty-poll race, missed-event recovery and readiness/scheduler fallback");
}
