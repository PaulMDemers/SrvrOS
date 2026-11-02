#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <srvros/syscall_numbers.h>
#define PROCESS_MAX_FUTEXES 64
#define PAGE_SIZE 4096
#define VMM_PAGE_USER 4
struct mapping { uint64_t virtual_address,physical_address,flags; };
struct process { int id; uint64_t address_space,mapping_count;struct mapping mappings[2]; };
struct scheduler_wait_queue { uint64_t waiters; };
typedef bool (*scheduler_wait_condition_fn)(void *);
static struct process a={.id=1},b={.id=2},*current=&a;
static bool masked,killed,signaled,mapped=true;
/* The real futex bodies must not acquire/release a foreign I/O owner. Every
 * modeled boundary checks this sentinel, including parked/woken continuations. */
static unsigned foreign_io_owner=77,foreign_io_depth=1;
static void assert_foreign_owner(void) {assert(foreign_io_owner==77 && foreign_io_depth==1);}
static uint64_t now;
static unsigned waits,yields,mode,restore_hook,enable_hook;
static uint32_t word,other;
static struct process *process_current(void) { return current; }
static bool process_should_exit_current(void) { return killed; }
static bool process_signal_pending_current(void) { return signaled; }
static uint64_t process_irq_save(void) { assert_foreign_owner();uint64_t old=masked;masked=true;return old; }
static void process_irq_restore(uint64_t flags);
static void host_enable_interrupts(void);
static uint64_t timer_ticks(void) { return now; }
static void scheduler_yield(void);
static bool scheduler_wait_timeout(struct scheduler_wait_queue *,scheduler_wait_condition_fn,void *,uint64_t);
static uint64_t scheduler_wake_count(struct scheduler_wait_queue *,uint64_t);
static void scheduler_wake_all(struct scheduler_wait_queue *);
static bool protect_fails;
bool vmm_user_range_mapped(uint64_t address,uint64_t length,bool writable) {
    assert(masked && address && length==4 && !writable);return mapped;
}
static int64_t process_mapping_index(struct process *p,uint64_t page) {
    return p->mapping_count && p->mappings[0].virtual_address==page ? 0 : -1;
}
static bool vmm_unmap_page_in_space(uint64_t space,uint64_t page,uint64_t *physical) {
    assert(masked && space==1 && page==a.mappings[0].virtual_address);
    mapped=false;*physical=123;return true;
}
static bool vmm_protect_page_in_space(uint64_t space,uint64_t page,uint64_t flags) {
    assert(masked && space==1 && page==a.mappings[0].virtual_address);
    if(protect_fails) return false;
    mapped=(flags&VMM_PAGE_USER)!=0;return true;
}
static void pmm_free_frame(uint64_t physical) { assert(masked && physical==123); }
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
    assert_foreign_owner();masked=false;
    if(enable_hook) { enable_hook=0;allocate_other(false); }
}
static void process_irq_restore(uint64_t flags) {
    assert_foreign_owner();masked=flags;
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
    if(mode==12) { process_unmap_mapping_at(&a,0);assert(ready(arg)); }
    if(mode==1) word=1;
    if(mode==2) signaled=true;
    if(ready(arg)) { process_irq_restore(flags);return true; }
    if(mode==3) { process_irq_restore(flags);return false; }
    q->waiters++;process_irq_restore(flags);
    if(mode>=13 && mode<=17) {
        uint64_t page=a.mappings[0].virtual_address;
        if(mode==13 || mode==15) process_unmap_mapping_at(&a,0);
        else { protect_fails=mode==17;assert(mmap_apply_protection(&a,page,mode==16?VMM_PAGE_USER:0)==!protect_fails); }
        if(mode<=15) {
            assert(!q->waiters); /* Removal/revocation itself woke the waiter. */
            if(mode==15) {
                mapped=true; /* Immediate remap: old wait must still fault. */
                struct process_futex *replacement=process_futex_find(&a,(uintptr_t)&word,true);
                assert(replacement && &replacement->wait_queue!=q);
            }
            mode=0;return true;
        }
        now=deadline;
    } else if(mode==11) { mapped=false;now=deadline; }
    else if(mode==4) signaled=true;
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
    mapped=true;protect_fails=false;
    a.address_space=1;a.mapping_count=1;
    a.mappings[0]=(struct mapping){.virtual_address=(uintptr_t)&word & ~(uint64_t)(PAGE_SIZE-1),.physical_address=123,.flags=VMM_PAGE_USER};
    now=100;waits=yields=restore_hook=enable_hook=0;mode=next_mode;word=other=0;foreign=NULL;
}
static void reusable(void) {
    for(unsigned i=0;i<PROCESS_MAX_FUTEXES;i++) assert(!futexes[i].wait_queue.waiters);
    /* Each completed wait must permit reuse, including interruption/error. */
    struct process_futex *entry=process_futex_find(&a,(uintptr_t)&word,false);
    struct process_futex *next=process_futex_find(&b,(uintptr_t)&other,true);
    assert(entry==next);
}
int main(int argc,char **argv) {
    if(argc>1 && !strcmp(argv[1],"deadline")) {
        reset(4);now=100;
        assert(process_futex_wait(&word,0,UINT64_MAX)==SRV_ERR_INVAL && !waits);
        return 0;
    }
    if(argc>1 && !strcmp(argv[1],"mapping")) {
        reset(11);
        assert(process_futex_wait(&word,0,3)==SRV_ERR_FAULT);
        return 0;
    }
    reset(0);mapped=false;assert(process_futex_wait(&word,0,3)==SRV_ERR_FAULT && !waits);
    for(unsigned test=11;test<=15;test++) {
        reset(test);assert(process_futex_wait(&word,0,3)==SRV_ERR_FAULT);
    }
    for(unsigned test=16;test<=17;test++) {
        reset(test);assert(process_futex_wait(&word,0,3)==-110);reusable();
    }
    const uint64_t starts[]={0,100,UINT64_MAX-3,UINT64_MAX};
    for(unsigned i=0;i<4;i++) {
        reset(4);now=starts[i];assert(process_futex_wait(&word,0,0)==SRV_ERR_INTR);reusable();
        uint64_t limit=UINT64_MAX-starts[i];
        if(limit) { reset(0);now=starts[i];assert(process_futex_wait(&word,0,limit)==-110 && waits==1);reusable(); }
        if(starts[i]) {
            reset(0);now=starts[i];assert(process_futex_wait(&word,0,limit+1)==SRV_ERR_INVAL && !waits);
        }
    }
    puts("futex-contracts: timeout limits, unmap, revocation, replacement and preserved mappings PASS");
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
    /* Existing keys remain usable at capacity, even across processes. Only a
     * fully retired reservation may be reused; invalidated calls still own it. */
    struct process_futex *kept[PROCESS_MAX_FUTEXES];
    for(unsigned i=0;i<PROCESS_MAX_FUTEXES;i++) {
        kept[i]=process_futex_find(&a,(uintptr_t)&addresses[i],false);
        assert(kept[i]);kept[i]->active_waits=1;
        if(i&1)kept[i]->process=&b;
    }
    current=&b;assert(process_futex_wait(&addresses[PROCESS_MAX_FUTEXES],0,1)==SRV_ERR_AGAIN);
    assert(process_futex_find(&b,(uintptr_t)&addresses[1],true)==kept[1]);
    assert(process_futex_wait(&addresses[1],0,1)==-110);
    assert(kept[1]->active_waits==1 && kept[1]->wait_queue.waiters==1);
    assert(process_futex_wake(&addresses[1],1)==1);
    assert(process_futex_wait(&addresses[PROCESS_MAX_FUTEXES],0,1)==SRV_ERR_AGAIN);
    kept[1]->invalidated=true;
    assert(!process_futex_find(&b,(uintptr_t)&addresses[1],true));
    kept[1]->active_waits=0;
    struct process_futex *reused=process_futex_find(&b,(uintptr_t)&addresses[PROCESS_MAX_FUTEXES],true);
    assert(reused==kept[1] && !reused->invalidated);
    for(unsigned i=0;i<PROCESS_MAX_FUTEXES;i++)if(i!=1) {
        assert(kept[i]->address==(uintptr_t)&addresses[i] && kept[i]->active_waits==1 && kept[i]->wait_queue.waiters==1);
    }
    process_futex_cleanup_process(&b);current=&a;
    process_futex_cleanup_process(&a);
    puts("futex-contracts: cross-process saturation, existing-key admission and retired-only reuse PASS");
    for(unsigned i=0;i<3*PROCESS_MAX_FUTEXES;i++) {
        word=0;mode=4;signaled=false;waits=0;
        assert(process_futex_wait(&word,0,3)==SRV_ERR_INTR);reusable();
    }
    puts("futex-contracts: table exhaustion and 192 interrupted reuse cycles PASS");
    return 0;
}
