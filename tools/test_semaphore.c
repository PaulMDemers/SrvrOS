#define SRVROS_SEMAPHORE_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include "../kernel/src/semaphore.c"
static uint64_t now,ticks;
static bool clock_ready=true,wait_available=true;
static unsigned calls,wakes;
static enum { SIGNAL, TIMEOUT, SPURIOUS, CLOSE, RACE } action;
static struct kernel_semaphore sem;
uint64_t timer_monotonic_us(void) {return now;}
uint64_t timer_ticks(void) {return ticks;}
bool timer_reference_ready(void) {return clock_ready;}
uint64_t scheduler_wake_count(struct scheduler_wait_queue *q,uint64_t n) {
    assert(q==&sem.queue && n==1 && !(test_irq_flags&(1ull<<9)));wakes++;return 1;
}
void scheduler_wake_all(struct scheduler_wait_queue *q) { assert(q==&sem.queue && !(test_irq_flags&(1ull<<9)));wakes++; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn condition,void *arg,uint64_t deadline) {
    assert(q==&sem.queue && arg==&sem && (test_irq_flags&(1ull<<9)));calls++;
    assert(!semaphore_drained(&sem));
    if(!wait_available)return false;
    if(action==RACE) { assert(semaphore_signal(&sem));assert(condition(arg));return true; }
    assert(!condition(arg));
    if(action==CLOSE)semaphore_close(&sem);
    else if(action==SIGNAL) { uint64_t old=test_irq_flags;test_irq_flags=0;assert(semaphore_signal(&sem));assert(test_irq_flags==0);test_irq_flags=old; }
    else if(action==TIMEOUT) { assert(deadline==ticks+2);now+=1000; }
    else { assert(action==SPURIOUS && deadline>ticks);if(calls==3)assert(semaphore_signal(&sem)); }
    return true;
}
static void reset(void) { semaphore_init(&sem,0);now=100000;ticks=10;calls=wakes=0;clock_ready=wait_available=true;test_irq_flags=1ull<<9; }
int main(void) {
    reset();assert(semaphore_wait(&sem,0)==SEMAPHORE_TIMEOUT && !calls);
    assert(semaphore_signal(&sem));assert(semaphore_wait(&sem,0)==SEMAPHORE_OK && !sem.units);
    reset();action=RACE;assert(semaphore_wait(&sem,1)==SEMAPHORE_OK && calls==1 && !sem.waiters);
    reset();action=SIGNAL;assert(semaphore_wait(&sem,SEMAPHORE_WAIT_FOREVER)==SEMAPHORE_OK && !sem.waiters);
    reset();action=TIMEOUT;assert(semaphore_wait(&sem,1)==SEMAPHORE_TIMEOUT && now==101000 && calls==1);
    reset();action=SPURIOUS;assert(semaphore_wait(&sem,20)==SEMAPHORE_OK && calls==3);
    reset();action=CLOSE;assert(semaphore_wait(&sem,SEMAPHORE_WAIT_FOREVER)==SEMAPHORE_CLOSED && semaphore_drained(&sem));
    assert(!semaphore_signal(&sem) && semaphore_wait(&sem,0)==SEMAPHORE_CLOSED);
    reset();wait_available=false;assert(semaphore_wait(&sem,100)==SEMAPHORE_UNAVAILABLE && !sem.waiters);
    reset();clock_ready=false;assert(semaphore_wait(&sem,1)==SEMAPHORE_UNAVAILABLE && !calls);
    reset();test_irq_flags=0;assert(semaphore_wait(&sem,1)==SEMAPHORE_UNAVAILABLE && !calls && !test_irq_flags);
    assert(semaphore_signal(&sem));assert(semaphore_wait(&sem,0)==SEMAPHORE_OK && !test_irq_flags);
    reset();sem.units=UINT64_MAX;assert(!semaphore_signal(&sem) && sem.units==UINT64_MAX);
    assert(semaphore_wait(&sem,0)==SEMAPHORE_OK && semaphore_signal(&sem) && sem.units==UINT64_MAX);
    reset();assert(semaphore_wait(&sem,UINT64_MAX-1)==SEMAPHORE_UNAVAILABLE && !calls);
    now=UINT64_MAX-10;assert(semaphore_wait(&sem,1)==SEMAPHORE_UNAVAILABLE && !calls);
    assert(add_saturated(UINT64_MAX-1,2)==UINT64_MAX);
    puts("semaphore: PASS unit accounting, IRQ signal, lost-wakeup boundary, finite/infinite waits, spurious wakes, close/drain, overflow and refusal paths");
}
