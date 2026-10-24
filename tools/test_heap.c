/* The real allocator over a bounded fake physical arena. */
#define SRVROS_HEAP_HOST_TEST
#include <assert.h>
#include <setjmp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../kernel/src/memory/heap.c"
static _Alignas(4096) unsigned char arena[2*1024*1024];
static uint64_t cursor=PMM_FRAME_SIZE;
static jmp_buf trapped;
static bool expect_halt;
void latency_enter(enum latency_site s) {(void)s;}
void latency_leave(enum latency_site s) {(void)s;}
void console_write(const char *s) {(void)s;}
void console_printf(const char *s,...) {(void)s;}
void halt_forever(void) { assert(expect_halt);longjmp(trapped,1); }
uint64_t pmm_alloc_frames_tagged(uint64_t n,enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_HEAP);
    if(n>(sizeof(arena)-cursor)/PMM_FRAME_SIZE)return 0;
    uint64_t p=cursor;cursor+=n*PMM_FRAME_SIZE;return p;
}
void *pmm_phys_to_virt(uint64_t p) { assert(p<sizeof(arena));return arena+p; }
static uint64_t block_count(void) {
    uint64_t n=0;for(struct heap_block *b=head;b;b=b->next)n++;return n;
}
static void allocation_must_halt(void) {
    expect_halt=true;
    if(!setjmp(trapped)){(void)kmalloc(16);assert(!"corruption accepted");}
    expect_halt=false;
}
static void free_must_halt(void *p) {
    expect_halt=true;
    if(!setjmp(trapped)){kfree(p);assert(!"invalid free accepted");}
    expect_halt=false;
}
int main(void) {
    heap_init();
    assert(!kmalloc(0) && !kmalloc(SIZE_MAX) && !kmalloc(SIZE_MAX-1024));
    assert(!kcalloc(SIZE_MAX,2));
    void *slots[6000];
    for(unsigned i=0;i<6000;i++) {
        size_t n=1+i%65;slots[i]=kmalloc(n);
        assert(slots[i] && !((uintptr_t)slots[i]&15));memset(slots[i],i&255,n);
    }
    for(unsigned i=0;i<6000;i++)for(unsigned j=0;j<1+i%65;j++)assert(((unsigned char *)slots[i])[j]==(i&255));
    for(unsigned i=0;i<6000;i+=2)kfree(slots[i]);
    for(unsigned i=0;i<6000;i+=2) { slots[i]=kcalloc(1,1+i%65);assert(slots[i] && !((uintptr_t)slots[i]&15));for(unsigned j=0;j<1+i%65;j++)assert(!((unsigned char *)slots[i])[j]); }
    for(unsigned i=6000;i>0;i--)kfree(slots[i-1]);
    assert(!heap_used_bytes());
    void *large=kmalloc(512*1024);assert(large && !((uintptr_t)large&15));kfree(large);
    assert(!kmalloc(4*1024*1024));
    /* A real cycle is still rejected; no allocation-count ceiling is needed. */
    struct heap_block *next=head->next;head->next=head;expect_halt=true;
    if(!setjmp(trapped)) { heap_validate_locked("cycle-test");assert(!"cycle accepted"); }
    head->next=next;expect_halt=false;heap_validate_locked("restored");
    /* A fit at the head must not hide corruption later in the chain. */
    void *a=kmalloc(64),*b=kmalloc(64),*c=kmalloc(64);assert(a&&b&&c);
    kfree(a);
    struct heap_block *hb=payload_block(b),*hc=payload_block(c);
    struct heap_block saved_b=*hb,saved_c=*hc,saved_head=*head;
    hc->magic=0;allocation_must_halt();*hc=saved_c;
    hc->prev=head;allocation_must_halt();*hc=saved_c;
    hc->next=head;allocation_must_halt();*hc=saved_c;
    hb->next=(struct heap_block *)1;allocation_must_halt();*hb=saved_b;
    hb->size=(uintptr_t)hc-(uintptr_t)hb;allocation_must_halt();*hb=saved_b;
    assert(!memcmp(head,&saved_head,sizeof(saved_head)));
    /* Both full passes still visit every block; selection uses first fit. */
    uint64_t before_count=block_count(),before_visits=validation_visits;
    assert(kmalloc(64)==a);
    assert(validation_visits-before_visits==before_count+block_count());
    assert(!search_visits);
    before_count=block_count();before_visits=validation_visits;kfree(b);
    assert(validation_visits-before_visits==before_count+block_count());
    free_must_halt(b); /* already-free block, with the rest of the heap valid */
    free_must_halt((unsigned char *)c+16);
    kfree(a);kfree(c);assert(!heap_used_bytes());
    puts("heap: PASS 6000 live allocations, fragmentation, 16-byte alignment, overflow/OOM, full entry/exit validation, distal corruption and invalid-free rejection");
}
