#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/syscall_numbers.h>
#define PROCESS_MAX_FUTEXES 64
struct process { int id; };
struct scheduler_wait_queue { uint64_t waiters; };
typedef bool (*scheduler_wait_condition_fn)(void *);
static struct process a={1},b={2},*current=&a;
static bool masked,killed,signaled;
static uint64_t now;
static unsigned waits,yields,mode,restore_hook,enable_hook;
static uint32_t word,other;
static struct process *process_current(void) { return current; }
static bool process_should_exit_current(void) { return killed; }
static bool process_signal_pending_current(void) { return signaled; }
static uint64_t process_irq_save(void) { uint64_t old=masked;masked=true;return old; }
static void process_irq_restore(uint64_t flags);
static void host_enable_interrupts(void);
static uint64_t timer_ticks(void) { return now; }
static void scheduler_yield(void);
static bool scheduler_wait_timeout(struct scheduler_wait_queue *,scheduler_wait_condition_fn,void *,uint64_t);
static uint64_t scheduler_wake_count(struct scheduler_wait_queue *,uint64_t);
static void scheduler_wake_all(struct scheduler_wait_queue *);
#include "futex-under-test.h"

/* Model an allocator running at the first interruptible lookup/park boundary. */
static struct process_futex *foreign;
static void allocate_other(bool park) {
    uint64_t flags=process_irq_save();
    foreign=process_futex_find(&b,(uintptr_t)&other,true);assert(foreign);
    if(park) foreign->wait_queue.waiters=1;
    process_irq_restore(flags);
}
static void host_enable_interrupts(void) {
    masked=false;
    if(enable_hook) { enable_hook=0;allocate_other(false); }
}
static void process_irq_restore(uint64_t flags) {
    masked=flags;
    if(!masked && restore_hook) {
        unsigned hook=restore_hook;restore_hook=0;
        allocate_other(hook==2);
    }
}
static uint64_t scheduler_wake_count(struct scheduler_wait_queue *q,uint64_t count) {
    uint64_t n=q->waiters<count?q->waiters:count;
    q->waiters-=n;return n;
}
static void scheduler_wake_all(struct scheduler_wait_queue *q) { (void)scheduler_wake_count(q,UINT64_MAX); }
static void scheduler_yield(void) { assert(++yields<10);now++; }
static bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn ready,void *arg,uint64_t deadline) {
    assert(++waits<10);
    uint64_t flags=process_irq_save();
    /* Inject change/signal between the outer checks and atomic enqueue check. */
    if(mode==1) word=1;
    if(mode==2) signaled=true;
    if(ready(arg)) { process_irq_restore(flags);return true; }
    if(mode==3) { process_irq_restore(flags);return false; }
    q->waiters++;process_irq_restore(flags);
    if(mode==4) signaled=true;
    else if(mode==5) killed=true;
    else if(mode==6 || mode==7) {
        /* A wake can make the queue empty while a syscall still owns it. */
        assert(scheduler_wake_count(q,1)==1);
        allocate_other(false);
        struct process_futex *owner=process_futex_find(&a,(uintptr_t)&word,false);
        assert(owner && &owner->wait_queue==q && owner!=foreign);
        if(mode==6) { mode=0;return true; } /* Retry unchanged word. */
        word=1;
        return true;
    } else if(mode==8) {
        struct process_futex *owner=process_futex_find(&a,(uintptr_t)&word,false);
        assert(owner && &owner->wait_queue==q && owner!=foreign);
        word=1;
    } else if(mode==10) {
        /* Two active waits share an entry. The inner return must not unpin it
         * while the outer syscall is runnable but has not resumed. */
        assert(scheduler_wake_count(q,1)==1);
        mode=4;assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR);
        signaled=false;allocate_other(false);
        struct process_futex *owner=process_futex_find(&a,(uintptr_t)&word,false);
        assert(owner && &owner->wait_queue==q && owner!=foreign);
        killed=true;return true;
    } else if(mode==9) {
        /* A spurious wake must not restart the relative deadline. */
        now++;assert(deadline==103);
    } else now=deadline;
    assert(scheduler_wake_count(q,1)==1);
    return true;
}
static void reset(unsigned next_mode) {
    memset(futexes,0,sizeof(futexes));current=&a;masked=false;killed=signaled=false;
    now=100;waits=yields=restore_hook=enable_hook=0;mode=next_mode;word=other=0;foreign=NULL;
}
static void reusable(void) {
    for(unsigned i=0;i<PROCESS_MAX_FUTEXES;i++) assert(!futexes[i].wait_queue.waiters);
    /* Each completed wait must permit reuse, including interruption/error. */
    struct process_futex *entry=process_futex_find(&a,(uintptr_t)&word,false);
    struct process_futex *next=process_futex_find(&b,(uintptr_t)&other,true);
    assert(entry==next);
}
int main(void) {
    reset(0);assert(process_futex_wait(NULL,0,1)==-1);
    assert(process_futex_wait((uint32_t *)((uintptr_t)&word+1),0,1)==-1);
    current=NULL;assert(process_futex_wait(&word,0,1)==-1);current=&a;
    word=1;assert(process_futex_wait(&word,0,1)==-11 && !waits);
    reset(0);signaled=true;assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR && !waits);reusable();
    reset(0);killed=true;assert(process_futex_wait(&word,0,3)==-1 && !waits);reusable();
    reset(1);assert(process_futex_wait(&word,0,3)==0 && waits==1);reusable();
    reset(2);assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR && waits==1);reusable();
    reset(3);assert(process_futex_wait(&word,0,3)==-110 && yields==3);reusable();
    reset(4);assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR && waits==1);reusable();
    reset(5);assert(process_futex_wait(&word,0,3)==-1 && waits==1);reusable();
    reset(9);assert(process_futex_wait(&word,0,3)==-110 && waits==3);reusable();
    puts("futex-contracts: validation, interruption, enqueue recheck and bounded spurious wakes PASS");
    reset(8);masked=true;enable_hook=1;assert(process_futex_wait(&word,0,3)==0 && waits==1);
    reset(6);assert(process_futex_wait(&word,0,3)==-110 && waits==2);
    reset(7);assert(process_futex_wait(&word,0,3)==0 && waits==1);
    reset(10);assert(process_futex_wait(&word,0,3)==-1 && waits==2);
    puts("futex-contracts: queue identity before enqueue, after wake and shared active waits PASS");
    /* Empty A lookup may not wake B if an allocation occurs when IRQs resume. */
    reset(0);assert(process_futex_find(&a,(uintptr_t)&word,true));
    restore_hook=2;assert(process_futex_wake(&word,1)==0);
    assert(foreign && foreign->wait_queue.waiters==1);
    puts("futex-contracts: wake lookup/count ownership PASS");
    reset(0);
    struct process_futex *first=process_futex_find(&a,(uintptr_t)&word,true);
    first->wait_queue.waiters=3;
    assert(process_futex_wake(&word,0)==0 && first->wait_queue.waiters==3);
    assert(process_futex_wake(&word,1)==1 && first->wait_queue.waiters==2);
    current=&b;assert(process_futex_wake(&word,UINT64_MAX)==0);current=&a;
    assert(process_futex_wake(&word,UINT64_MAX)==2 && !first->wait_queue.waiters);
    first->wait_queue.waiters=2;process_futex_wake_process(&a);assert(!first->wait_queue.waiters);
    process_futex_cleanup_process(&a);assert(!first->used && !first->process);
    puts("futex-contracts: wake counts, process isolation and teardown PASS");
    /* Exhaustion refuses without stealing a live queue; released entries recycle. */
    reset(0);uint32_t addresses[PROCESS_MAX_FUTEXES+1]={0};
    for(unsigned i=0;i<PROCESS_MAX_FUTEXES;i++) {
        struct process_futex *entry=process_futex_find(&a,(uintptr_t)&addresses[i],true);
        assert(entry);entry->wait_queue.waiters=1;
    }
    assert(process_futex_wait(&addresses[PROCESS_MAX_FUTEXES],0,1)==-11);
    process_futex_cleanup_process(&a);
    for(unsigned i=0;i<3*PROCESS_MAX_FUTEXES;i++) {
        word=0;mode=4;signaled=false;waits=0;
        assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR);reusable();
    }
    puts("futex-contracts: table exhaustion and 192 interrupted reuse cycles PASS");
    return 0;
}
