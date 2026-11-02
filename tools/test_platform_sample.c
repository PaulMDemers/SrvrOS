#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t test_ticks=100;
static unsigned reads;
static uint32_t rc=50;
static unsigned well_reads,map_calls,map_failure;
static uint64_t occupied;
static uint32_t well_value;
static uint64_t platform_test_tsc(void) {return ++test_ticks;}
static uint32_t platform_test_read(unsigned off) {
    assert(off==0x60 || off==0x108);reads++;
    return off==0x60?3:rc++;
}
static uint32_t platform_test_well_read(unsigned off) {
    assert(off==0x400+4*(well_reads%4));well_reads++;return well_value;
}
#define SRVROS_PLATFORM_HOST_TEST
#include "../kernel/diagnostics/platform_audit.c"
uint64_t timer_monotonic_us(void) {return ++test_ticks;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *pa) {(void)pa;return va==occupied;}
bool vmm_map_page(uint64_t va,uint64_t pa,uint64_t flags) {
    map_calls++;assert(flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));
    assert((va==GFX_VA && pa==gpu_base+0x138000) || (va==WELL_VA && pa==gpu_base+0x45000));
    return map_calls!=map_failure;
}
int main(void) {
    occupied=GFX_VA;assert(!graphics_map_pages() && !map_calls);
    occupied=WELL_VA;assert(!graphics_map_pages() && !map_calls);occupied=0;
    map_failure=1;assert(!graphics_map_pages() && map_calls==1);
    map_calls=0;map_failure=2;assert(!graphics_map_pages() && map_calls==2);
    map_calls=0;map_failure=0;assert(graphics_map_pages() && map_calls==2);
    well_snapshot(0);assert(fault && !well_reads);fault=0;
    mapped=true;well_snapshot(2);assert(fault && !well_reads);fault=0;
    well_snapshot(0);assert(well_reads==4 && well_seen==1 && well_end[0]>well_begin[0]);
    well_snapshot(0);assert(fault && well_reads==4);fault=0;
    well_value=0xffffffffu;well_snapshot(1);assert(fault && well_reads==8 && well_seen==3);
    mapped=false;fault=0;
    platform_audit_sample(1);assert(!reads && !series_count);
    mapped=true;platform_audit_sample(1);assert(!reads && !series_count);
    series_active=true;mapped=false;platform_audit_sample(1);assert(!reads);
    mapped=true;
    for(unsigned i=0;i<24;i++)platform_audit_sample(1);
    assert(series_count==12 && reads==48 && !fault && !series_overflow);
    for(unsigned i=0;i<12;i++) {
        const struct series_row *r=&series[i];
        assert(r->pm0<r->tsc0 && r->tsc0<r->tsc1 && r->tsc1<r->pm1);
        assert(r->r1==r->r0+1 && r->s0==3 && r->s1==3);
    }
    platform_audit_sample(1);assert(series_count==12 && reads==48 && fault && series_overflow);
    series_active=false;platform_audit_sample(1);assert(reads==48);
    series_active=true;series_count=0;series_checkpoints=0;fault=0;rc=0xfffffffeu;
    platform_audit_sample(1);assert(fault && series[0].r1==0xffffffffu);
    assert(well_reads==8); /* Sparse power checkpoints never read display wells. */
    puts("platform-sample: PASS exact read-only mappings/refusals, endpoint well bounds/all-ones, sparse hook isolation, ordered reads and capacity");
}
