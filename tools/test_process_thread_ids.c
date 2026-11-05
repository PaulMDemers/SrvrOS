/* Actual process.c creation paths with the existing modeled scheduler/PMM/VMM.
 * Run separately as a negative control before fixing ID issuance. */
#include <assert.h>
#include <stdint.h>
static uint64_t test_irq_flags=0x202;
static unsigned irq_depth;
static uint64_t process_test_irq_save(void) {
    uint64_t old=test_irq_flags;test_irq_flags&=~(1ull<<9);irq_depth++;return old;
}
static void process_test_irq_restore(uint64_t old) {
    assert(irq_depth && !(test_irq_flags&(1ull<<9)));irq_depth--;test_irq_flags=old;
}
static uint64_t process_test_rflags(void) {return test_irq_flags;}
#define SRVROS_PROCESS_IRQ_HOST_TEST
#define main process_service_fixture_unused_main
#include "test_process_service.c"
#undef main
#include <limits.h>

static struct process snapshot;

static struct process *new_subject(void) {
    struct process *p=startup_process();
    assert(map_process_page(p,0x400000,VMM_PAGE_USER));
    assert(map_process_page(p,0x500000,VMM_PAGE_USER|VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE));
    return p;
}

static int64_t create_worker(bool private_route) {
    if (!private_route) return process_thread_create(0x400000,123,0x500800,SRV_THREAD_DETACHED);
    int64_t result=731;
    assert(process_try_thread_create_private(0x400000,123,0x500800,SRV_THREAD_DETACHED,&result));
    return result;
}

static void assert_exhausted(struct process *p,bool private_route,uint64_t counter) {
    p->next_thread_id=counter;
    memcpy(&snapshot,p,sizeof(snapshot));
    uint64_t irq_before=test_irq_flags;assert(!irq_depth);
    unsigned allocations=alloc_calls, mappings=map_calls, deallocations=freed;
    unsigned published=spawn_calls, services=begins, callbacks=checkpoints;
    assert(create_worker(private_route)==SRV_ERR_AGAIN);
    assert(!memcmp(p,&snapshot,sizeof(snapshot)) && !irq_depth && test_irq_flags==irq_before);
    assert(alloc_calls==allocations && map_calls==mappings && freed==deallocations &&
        spawn_calls==published && begins==services && checkpoints==callbacks);
}

static void verify_route(bool private_route) {
    static const uint64_t invalid[]={0,1,(uint64_t)INT64_MAX+1,UINT64_MAX};
    for (unsigned i=0;i<sizeof(invalid)/sizeof(invalid[0]);i++) {
        struct process *p=new_subject();
        assert_exhausted(p,private_route,invalid[i]);
        cleanup_process_address_space(p);free(p);current=observed=NULL;
    }
    struct process *p=new_subject();
    p->next_thread_id=(uint64_t)INT64_MAX;
    unsigned published=spawn_calls, allocations=alloc_calls;
    int64_t tid=create_worker(private_route);
    assert(tid==INT64_MAX && spawn_calls==published+1 && alloc_calls>allocations);
    struct process_user_thread *thread=&p->user_threads[0];
    assert(thread->used && thread->active && thread->tid==(uint64_t)INT64_MAX && thread->tls_base);
    uint64_t physical=0;
    assert(vmm_virt_to_phys_in_space(p->address_space,thread->tls_base,&physical));
    assert(!memcmp(backing+physical,"TLS!",4));
    /* Model completed worker retirement using the actual TLS cleanup, then
     * check exhaustion with a free slot so capacity cannot mask wraparound. */
    process_release_thread_tls(thread);
    *thread=(struct process_user_thread){0};
    uint64_t exhausted=p->next_thread_id;
    assert(exhausted>(uint64_t)INT64_MAX);
    assert_exhausted(p,private_route,exhausted);
    cleanup_process_address_space(p);free(p);current=observed=NULL;
}

int main(int argc,char **argv) {
    if(argc>1) {
        bool private_route=!strcmp(argv[1],"private");
        struct process *p=new_subject();p->next_thread_id=(uint64_t)INT64_MAX+1;
        unsigned published=spawn_calls;int64_t result=create_worker(private_route);
        printf("thread-id-overflow: route=%s result=%lld published=%u active=%d\n",
            private_route?"private":"ordinary",(long long)result,spawn_calls-published,p->user_threads[0].active);
        return result==SRV_ERR_AGAIN && spawn_calls==published && !p->user_threads[0].used ? 0 : 1;
    }
    for(unsigned enabled=0;enabled<2;enabled++) {
        test_irq_flags=enabled?0x202:2;
        verify_route(false);verify_route(true);
        assert(!irq_depth && test_irq_flags==(enabled?0x202u:2u));
    }
    puts("process-thread-ids: bounded IDs, no wrap or negative publication");
    return 0;
}
