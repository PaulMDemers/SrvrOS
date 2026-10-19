#include <srvros/semaphore.h>
#include <srvros/timer.h>
#include <stddef.h>
#ifdef SRVROS_SEMAPHORE_HOST_TEST
static uint64_t test_irq_flags=1ull<<9;
#endif
static uint64_t lock(void) {
#ifdef SRVROS_SEMAPHORE_HOST_TEST
    uint64_t f=test_irq_flags;test_irq_flags&=~(1ull<<9);return f;
#else
    uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_SEMAPHORE_HOST_TEST
    test_irq_flags=f;
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
void semaphore_init(struct kernel_semaphore *s,uint64_t units) {
    *s=(struct kernel_semaphore){.units=units};
}
static bool ready(void *arg) { struct kernel_semaphore *s=arg;return s->closed || s->units; }
static uint64_t add_saturated(uint64_t a,uint64_t b) { return b>UINT64_MAX-a?UINT64_MAX:a+b; }
enum semaphore_result semaphore_wait(struct kernel_semaphore *s,uint64_t ms) {
    if(!s)return SEMAPHORE_UNAVAILABLE;
    uint64_t flags=lock();
    if(s->closed) { unlock(flags);return SEMAPHORE_CLOSED; }
    if(s->units) { s->units--;unlock(flags);return SEMAPHORE_OK; }
    if(!ms) { unlock(flags);return SEMAPHORE_TIMEOUT; }
    if(!(flags&(1ull<<9)) || (ms!=SEMAPHORE_WAIT_FOREVER && !timer_reference_ready())) {
        unlock(flags);return SEMAPHORE_UNAVAILABLE;
    }
    uint64_t now=timer_monotonic_us();
    if(ms!=SEMAPHORE_WAIT_FOREVER && (ms>UINT64_MAX/1000 || now>UINT64_MAX-ms*1000)) {
        unlock(flags);return SEMAPHORE_UNAVAILABLE;
    }
    uint64_t end=ms==SEMAPHORE_WAIT_FOREVER?UINT64_MAX:now+ms*1000;
    s->waiters++;unlock(flags);
    enum semaphore_result result;
    for(;;) {
        flags=lock();
        if(s->closed)result=SEMAPHORE_CLOSED;
        else if(s->units) { s->units--;result=SEMAPHORE_OK; }
        else {
            now=timer_monotonic_us();
            if(ms!=SEMAPHORE_WAIT_FOREVER && now>=end)result=SEMAPHORE_TIMEOUT;
            else {
                /* The current kernel tick is 10 ms. Round upward and include
                 * one phase tick; the monotonic deadline is authoritative. */
                uint64_t left=end-now;
                uint64_t ticks=left/10000+(left%10000!=0)+1;
                uint64_t due=ms==SEMAPHORE_WAIT_FOREVER?0:add_saturated(timer_ticks(),ticks);
                unlock(flags);
                if(scheduler_wait_timeout(&s->queue,ready,s,due))continue;
                flags=lock();result=SEMAPHORE_UNAVAILABLE;
            }
        }
        s->waiters--;unlock(flags);return result;
    }
}
bool semaphore_signal(struct kernel_semaphore *s) {
    if(!s)return false;
    uint64_t f=lock();
    bool ok=!s->closed && s->units!=UINT64_MAX;
    if(ok) { s->units++;scheduler_wake_count(&s->queue,1); }
    unlock(f);return ok;
}
void semaphore_close(struct kernel_semaphore *s) {
    if(!s)return;
    uint64_t f=lock();s->closed=true;scheduler_wake_all(&s->queue);unlock(f);
}
bool semaphore_drained(struct kernel_semaphore *s) {
    if(!s)return false;
    uint64_t f=lock();bool done=s->closed && !s->waiters;unlock(f);return done;
}
