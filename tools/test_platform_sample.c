#include <assert.h>
#include <stdint.h>
#include <stdio.h>
static uint64_t test_ticks=100;
static unsigned reads;
static uint32_t rc=50;
static unsigned well_reads,map_calls,map_failure;
static uint64_t occupied;
static uint32_t well_value;
static unsigned resource_reads,resource_bad,well_drop_at;
static uint64_t platform_test_tsc(void) {return ++test_ticks;}
static uint32_t platform_test_read(unsigned off) {
    assert(off==0x60 || off==0x108);reads++;
    return off==0x60?3:rc++;
}
static uint32_t platform_test_well_read(unsigned off) {
    assert(off==0x400+4*(well_reads%4));well_reads++;return well_drop_at && well_reads>=well_drop_at?0:well_value;
}
static uint32_t platform_test_resource_read(unsigned off) {
    resource_reads++;assert(off>=0x41000 && off<=0x7f008);
    return resource_bad==resource_reads?0xffffffffu:off;
}
#define SRVROS_PLATFORM_HOST_TEST
#include "../kernel/diagnostics/platform_audit.c"
#include "../kernel/diagnostics/package_audit_core.c"
static uint32_t pci_config[64];
uint32_t pci_read_config32(const struct pci_device *p,uint8_t offset) {
    assert(p==gpu && !(offset&3));return pci_config[offset/4];
}
uint64_t timer_monotonic_us(void) {return ++test_ticks;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *pa) {(void)pa;return va==occupied;}
bool vmm_map_page(uint64_t va,uint64_t pa,uint64_t flags) {
    map_calls++;assert(flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));
    bool match=(va==GFX_VA && pa==gpu_base+0x138000) || (va==WELL_VA && pa==gpu_base+0x45000);
    for(unsigned i=0;i<DISPLAY_RESOURCE_PAGES;i++)
        if(va==RESOURCE_VA+4096ull*i && pa==gpu_base+display_resource_pages[i])match=true;
    assert(match);
    return map_calls!=map_failure;
}
uint16_t pci_read_config16(const struct pci_device *p,uint8_t off) {
    return (uint16_t)(pci_read_config32(p,off&~3u)>>((off&2)*8));
}
int main(void) {
    struct pci_device device={0};gpu=&device;
    pci_config[0]=0x0a268086;pci_config[1]=0x00100002;pci_config[0x34/4]=0x40;pci_config[0x40/4]=0x00010001;
    assert(graphics_d0());pci_config[0x44/4]=3;assert(!graphics_d0());pci_config[0x44/4]=0;
    pci_config[1]=2;assert(!graphics_d0());pci_config[1]=0x00100002;
    pci_config[0x40/4]=1;assert(!graphics_d0());pci_config[0x40/4]=0x00010001;
    pci_config[0]=0xffffffffu;assert(!graphics_d0());pci_config[0]=0x0a268086;
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
    /* Real producer: exact map failures and access refusal before any dependent read. */
    for(unsigned i=0;i<DISPLAY_RESOURCE_PAGES;i++) {
        map_calls=0;occupied=RESOURCE_VA+4096ull*i;
        assert(!resource_map_pages() && !map_calls);
    }
    occupied=0;
    for(unsigned i=1;i<=DISPLAY_RESOURCE_PAGES;i++) {
        map_calls=0;map_failure=i;assert(!resource_map_pages() && map_calls==i);
    }
    map_calls=0;map_failure=0;assert(resource_map_pages() && map_calls==DISPLAY_RESOURCE_PAGES);
    pci_config[4]=0xc0000004;pci_config[5]=0;gpu_base=0xc0000000;
    resource_mapped=true;well_value=0x40000000;resource_seen=0;fault=0;
    resource_snapshot(0);assert(resources[0].state==1 && resources[0].read_count==26 && resource_reads==26 && !fault);
    for(unsigned i=0;i<26;i++)assert(resources[0].raw[i]==display_resource_offsets[i]);
    resource_snapshot(0);assert(fault && resource_reads==26);fault=0;
    resource_seen=0;well_value=0;resource_snapshot(0);
    assert(resources[0].state==2 && resources[0].read_count==0 && resource_reads==26 && fault);
    /* Zero output storage between simulated independent boots. */
    for(unsigned j=0;j<8;j++) {
        resources[0]=(struct display_resource_snapshot){0};resource_seen=0;fault=0;well_value=0x40000000;resource_reads=0;
        uint32_t saved=0;
        if(j==0){saved=pci_config[0];pci_config[0]=0xffffffffu;}
        if(j==1){saved=pci_config[1];pci_config[1]&=~2u;}
        if(j==2){saved=pci_config[0x44/4];pci_config[0x44/4]=3;}
        if(j==3){saved=pci_config[4];pci_config[4]=0xc0400004;}
        if(j==4)well_value=0xffffffffu;
        if(j==5)resource_bad=7;
        if(j==6)well_drop_at=well_reads+5;
        if(j==7)resource_mapped=false;
        resource_snapshot(0);assert(fault);
        if(j<5 || j==7)assert(!resource_reads);
        if(j==5)assert(resource_reads==7 && resources[0].raw[6]==0xffffffffu);
        if(j==6)assert(resource_reads==26 && resources[0].state==3);
        if(j==0)pci_config[0]=saved;
        if(j==1)pci_config[1]=saved;
        if(j==2)pci_config[0x44/4]=saved;
        if(j==3)pci_config[4]=saved;
        resource_bad=well_drop_at=0;resource_mapped=true;
    }
    puts("platform-sample: PASS exact read-only mappings/refusals, endpoint well bounds/all-ones, sparse hook isolation, ordered reads and capacity");
}
