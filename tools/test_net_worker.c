#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include "../kernel/src/net.c"
static jmp_buf done;
static unsigned polls,waits;
static bool fallback;
uint64_t timer_ticks(void) { return 100; }
void scheduler_yield(void) { assert(fallback);longjmp(done,1); }
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==&rx_wait_queue); }
uint64_t net_device_poll(uint64_t budget,bool verbose) {
    assert(budget==32 && !verbose);polls++;
    if(polls==1) return 32; /* A nonempty budget drains without sleeping. */
    if(polls==2) { net_notify_rx();return 0; } /* IRQ races with empty poll. */
    return 0;
}
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(q==&rx_wait_queue && deadline==101);waits++;
    if(waits==1) { assert(polls==2 && ready(arg));return true; }
    assert(polls==3 && !ready(arg));
    if(fallback) return false;
    net_notify_rx();assert(ready(arg));longjmp(done,1);
}
int main(void) {
    if(!setjmp(done)) net_worker(NULL);
    assert(polls==3 && waits==2 && rx_worker_packets==32 && rx_worker_empty_polls==2);
    polls=waits=0;fallback=true;
    if(!setjmp(done)) net_worker(NULL);
    assert(polls==3 && waits==2);
    puts("net-worker: PASS bounded polling, receive wake, empty-poll race and unavailable-scheduler fallback");
}
