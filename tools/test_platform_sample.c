#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
static uint64_t test_ticks=100;
static unsigned reads;
static uint32_t rc=50;
static unsigned well_reads,map_calls,map_failure;
static uint64_t occupied;
static uint32_t well_value;
static unsigned resource_reads,resource_bad,well_drop_at,dep_reads,dep_bad;
static unsigned dep_change_pch;
static void dependency_after_read(void);
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
static uint32_t platform_test_dependency_read(unsigned off) {
    static const unsigned expected[]={0x650c0,0x65000,0x65100,0x65200,0x44004,0x4400c,0xc4004,0xc400c};
    assert(off==expected[dep_reads%8]);dep_reads++;dependency_after_read();
    return dep_bad==dep_reads?0xffffffffu:off==0x44004 || off==0xc4004?0xffffffffu:off;
}
#define SRVROS_PLATFORM_HOST_TEST
#include "../kernel/diagnostics/platform_audit.c"
#include "../kernel/diagnostics/package_audit_core.c"
static uint32_t pci_config[64],pch_config[64];
static void dependency_after_read(void) {if(dep_change_pch && dep_reads==8)pch_config[0]=0xffffffffu;}
uint32_t pci_read_config32(const struct pci_device *p,uint8_t offset) {
    assert((p==gpu || p==pch_device) && !(offset&3));return p==gpu?pci_config[offset/4]:pch_config[offset/4];
}
uint64_t timer_monotonic_us(void) {return ++test_ticks;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *pa) {(void)pa;return va==occupied;}
bool vmm_map_page(uint64_t va,uint64_t pa,uint64_t flags) {
    map_calls++;assert(flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));
    bool match=(va==GFX_VA && pa==gpu_base+0x138000) || (va==WELL_VA && pa==gpu_base+0x45000);
    for(unsigned i=0;i<DISPLAY_RESOURCE_PAGES;i++)
        if(va==RESOURCE_VA+4096ull*i && pa==gpu_base+display_resource_pages[i])match=true;
    for(unsigned i=0;i<DEP_PAGES;i++)if(va==DEP_VA+4096ull*i && pa==gpu_base+dep_pages[i])match=true;
    assert(match);
    return map_calls!=map_failure;
}
uint16_t pci_read_config16(const struct pci_device *p,uint8_t off) {
    return (uint16_t)(pci_read_config32(p,off&~3u)>>((off&2)*8));
}
static unsigned dependency_reports;
void console_printf(const char *fmt,...) {
    if(strncmp(fmt,"ap-platform-deps:",17))return;
    va_list ap;va_start(ap,fmt);uint64_t phase=va_arg(ap,uint64_t);assert(phase<2);
    const struct display_resource_snapshot *r=&dependencies[phase];
    uint64_t expected[]={r->state,r->begin,r->end,r->read_count,dep_pch[phase][0],dep_pch[phase][1],
        r->before[0],r->before[1],r->before[2],r->before[3],r->after[0],r->after[1],r->after[2],r->after[3],
        r->raw[0],r->raw[1],r->raw[2],r->raw[3],r->raw[4],r->raw[5],r->raw[6],r->raw[7]};
    unsigned fields=0;for(const char *p=fmt;*p;p++)if(p[0]=='%' && p[1]=='u')fields++;
    assert(fields==23);
    for(unsigned i=0;i<22;i++)assert(va_arg(ap,uint64_t)==expected[i]);
    va_end(ap);dependency_reports++;
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
    /* Separate exact PCH and private-page gates for the eight dependency reads. */
    struct pci_device lpc={.device=31,.class_code=6,.subclass=1};pch_device=&lpc;
    pch_config[0]=0x9c438086;pch_config[2]=0x06010000;well_value=0x40000000;
    for(unsigned i=0;i<DEP_PAGES;i++) {
        occupied=DEP_VA+4096ull*i;map_calls=0;assert(!dependency_map_pages() && !map_calls);
    }
    occupied=0;
    for(unsigned i=1;i<=DEP_PAGES;i++){map_calls=0;map_failure=i;assert(!dependency_map_pages() && map_calls==i);}
    map_calls=0;map_failure=0;assert(dependency_map_pages() && map_calls==DEP_PAGES);
    dep_mapped=true;dep_seen=0;fault=0;dep_reads=0;dependency_snapshot(0);
    assert(dependencies[0].state==1 && dep_reads==8 && !fault);
    assert(dependencies[0].raw[4]==0xffffffffu && dependencies[0].raw[6]==0xffffffffu);
    dependency_snapshot(0);assert(fault && dep_reads==8);
    for(unsigned j=0;j<9;j++) {
        dep_seen=0;fault=0;dep_reads=0;pch_config[0]=0x9c438086;pch_config[2]=0x06010000;
        lpc.device=31;well_value=0x40000000;dep_mapped=true;dep_bad=0;dep_change_pch=0;
        if(j==0)pch_config[0]=0xffffffffu;
        if(j==1)pch_config[2]=0x06000000;
        if(j==2)lpc.device=30;
        if(j==3)well_value=0;
        if(j==4)dep_bad=3;
        if(j==5)dep_change_pch=1;
        if(j==6)well_drop_at=well_reads+5;
        if(j==7)dep_mapped=false;
        if(j==8)pci_config[1]&=~2u;
        dependency_snapshot(0);assert(fault);
        if(j<4 || j>=7)assert(!dep_reads);
        if(j==4)assert(dep_reads==3 && dependencies[0].raw[2]==0xffffffffu);
        if(j==5 || j==6)assert(dep_reads==8 && dependencies[0].state==3);
        well_drop_at=0;pci_config[1]|=2;
    }
    dep_seen=0;dep_reads=0;dep_bad=0;dep_change_pch=0;dep_mapped=true;fault=0;
    pch_config[0]=0x9c438086;pch_config[2]=0x06010000;lpc.device=31;well_value=0x40000000;
    dependency_snapshot(0);dependency_snapshot(1);
    assert(dependencies[0].state==1 && dependencies[1].state==1 && dep_reads==16);
    (void)platform_audit_report();assert(dependency_reports==2);
    puts("platform-sample: PASS exact read-only mappings/refusals, endpoint well bounds/all-ones, sparse hook isolation, ordered reads and capacity");
}
