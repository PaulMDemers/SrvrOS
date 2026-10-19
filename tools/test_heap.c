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
    puts("heap: PASS 6000 live allocations, fragmentation, 16-byte alignment, overflow/OOM and real cycle rejection");
}
