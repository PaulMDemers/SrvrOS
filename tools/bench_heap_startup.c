/* Namespace-sized deterministic replay, not an AML trace or native timing. */
#define _POSIX_C_SOURCE 200809L
#define SRVROS_HEAP_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#ifndef HEAP_IMPLEMENTATION
#define HEAP_IMPLEMENTATION "../kernel/src/memory/heap.c"
#endif
#include HEAP_IMPLEMENTATION
static _Alignas(4096) unsigned char arena[8*1024*1024];
static uint64_t cursor=PMM_FRAME_SIZE;
void latency_enter(enum latency_site s) {(void)s;}
void latency_leave(enum latency_site s) {(void)s;}
void console_write(const char *s) {(void)s;}
void console_printf(const char *s,...) {(void)s;}
void halt_forever(void) { abort(); }
uint64_t pmm_alloc_frames_tagged(uint64_t n,enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_HEAP);
    if(n>(sizeof(arena)-cursor)/PMM_FRAME_SIZE)return 0;
    uint64_t p=cursor;cursor+=n*PMM_FRAME_SIZE;return p;
}
void *pmm_phys_to_virt(uint64_t p) { assert(p<sizeof(arena));return arena+p; }
static uint64_t ns(void) {
    struct timespec t;assert(!clock_gettime(CLOCK_MONOTONIC,&t));return (uint64_t)t.tv_sec*1000000000+t.tv_nsec;
}
int main(void) {
    void *slots[11436];uint64_t checksum=0;
    heap_init();validation_visits=search_visits=0;
    for(unsigned epoch=0;epoch<2;epoch++) {
        uint64_t begin=ns(),v=validation_visits,s=search_visits;
        for(unsigned i=0;i<11436;i++) {
            slots[i]=kmalloc(32+(i%8)*16);assert(slots[i]);
            *(uint64_t *)slots[i]=i+epoch;
        }
        for(unsigned i=0;i<7411;i++) {
            unsigned j=(i*7)%11436;checksum+=*(uint64_t *)slots[j];kfree(slots[j]);
            slots[j]=kmalloc(32+(j%8)*16);assert(slots[j]);*(uint64_t *)slots[j]=j+epoch;
        }
        uint64_t setup=ns()-begin,free_start=ns();
        for(unsigned i=11436;i>0;i--){checksum+=*(uint64_t *)slots[i-1];kfree(slots[i-1]);}
        uint64_t free_ns=ns()-free_start;
        assert(!heap_used_bytes());heap_validate_locked("replay-end");
        printf("epoch=%u setup_ns=%llu free_ns=%llu validation_visits=%llu search_visits=%llu checksum=%llu\n",epoch+1,
            (unsigned long long)setup,(unsigned long long)free_ns,(unsigned long long)(validation_visits-v),
            (unsigned long long)(search_visits-s),(unsigned long long)checksum);
    }
}
