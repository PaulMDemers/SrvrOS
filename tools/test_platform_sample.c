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
static unsigned backing_mode,backing_reads,backing_bad,backing_change,backing_emit;
static uint32_t backing_values[9]={0x98000000,5632,0,0x100000,0,(1365u<<16)|767,0x100000,0x98000000,0x100000};
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
    if(backing_mode) {
        static const unsigned expected[]={0x70180,0x70188,0x70184,0x7019c,0x701a4,0x6001c,0x701ac,0x70180,0x7019c};
        assert(backing_reads<9 && off==expected[backing_reads]);unsigned i=backing_reads++;
        if(backing_change==2 && i==8)backing_values[8]^=4096;
        return backing_bad==i+1?0xffffffffu:backing_values[i];
    }
    resource_reads++;assert(off>=0x41000 && off<=0x7f008);
    return resource_bad==resource_reads?0xffffffffu:off;
}
static uint32_t platform_test_dependency_read(unsigned off) {
    static const unsigned expected[]={0x650c0,0x65000,0x65100,0x65200,0x44004,0x4400c,0xc4004,0xc400c};
    assert(off==expected[dep_reads%8]);dep_reads++;dependency_after_read();
    return dep_bad==dep_reads?0xffffffffu:off==0x44004 || off==0xc4004?0xffffffffu:off;
}
static unsigned hda_reads,hda_bad,hda_change;
static void hda_after_read(void);
static uint32_t hda_gcap=0x1001,hda_gctl=1;
static uint32_t platform_test_hda_read(unsigned off,unsigned width) {
    hda_reads++;hda_after_read();
    assert((off==0 && width==2) || (off==8 && width==4) ||
           (off==0x20 && width==4) || (off==0x4c && width==1) ||
           (off==0x5c && width==1) || (off==0x70 && width==4) ||
           (off>=0x80 && off<=0x422 && ((off%32==0 && width==2) || (off%32==2 && width==1))));
    if(hda_bad==hda_reads)return width==1?255:width==2?65535:0xffffffffu;
    if(off==0)return hda_gcap;
    if(off==8)return hda_change==1 && hda_reads>2?hda_gctl^1:hda_gctl;
    if(off>=0x80)return width==2?2:0x10; /* Active stream remains an observation. */
    return 0;
}
#define SRVROS_PLATFORM_HOST_TEST
#include "../kernel/diagnostics/platform_audit.c"
#include "../kernel/diagnostics/package_audit_core.c"
volatile struct limine_framebuffer_request framebuffer_request;
volatile struct limine_memmap_request memmap_request;
static unsigned mapping_hole,mapping_disjoint,map_walks;
uint64_t vmm_kernel_address_space(void) {return 0x1000;}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t va,uint64_t *pa) {
    assert(space==0x1000);map_walks++;
    if(mapping_hole && map_walks==mapping_hole)return false;
    *pa=va-0xffff800000000000ull+(mapping_disjoint && map_walks==mapping_disjoint?4096:0);return true;
}
static uint32_t pci_config[64],pch_config[64],hda_config[64];
static void hda_after_read(void) {
    if(hda_reads==10 && hda_change==2)hda_config[0]^=1;
    if(hda_reads==10 && hda_change==3)hda_config[4]+=0x4000;
    if(hda_reads==10 && hda_change==4)hda_config[1]&=~2u;
}
static void dependency_after_read(void) {if(dep_change_pch && dep_reads==8)pch_config[0]=0xffffffffu;}
uint32_t pci_read_config32(const struct pci_device *p,uint8_t offset) {
    assert((p==gpu || p==pch_device || p==hda_device) && !(offset&3));return p==gpu?pci_config[offset/4]:p==pch_device?pch_config[offset/4]:hda_config[offset/4];
}
uint64_t timer_monotonic_us(void) {return ++test_ticks;}
bool vmm_virt_to_phys(uint64_t va,uint64_t *pa) {(void)pa;return va==occupied;}
bool vmm_map_page(uint64_t va,uint64_t pa,uint64_t flags) {
    map_calls++;assert(flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));
    bool match=(va==GFX_VA && pa==gpu_base+0x138000) || (va==WELL_VA && pa==gpu_base+0x45000);
    for(unsigned i=0;i<DISPLAY_RESOURCE_PAGES;i++)
        if(va==RESOURCE_VA+4096ull*i && pa==gpu_base+display_resource_pages[i])match=true;
    for(unsigned i=0;i<DEP_PAGES;i++)if(va==DEP_VA+4096ull*i && pa==gpu_base+dep_pages[i])match=true;
    if(va==HDA_VA && pa==hda_base)match=true;
    assert(match);
    return map_calls!=map_failure;
}
uint16_t pci_read_config16(const struct pci_device *p,uint8_t off) {
    return (uint16_t)(pci_read_config32(p,off&~3u)>>((off&2)*8));
}
uint64_t pci_device_count(void) {return 0;}
const struct pci_device *pci_device_at(uint64_t i) {(void)i;return NULL;}
static unsigned dependency_reports,hda_reports,dry_reports;
void console_printf(const char *fmt,...) {
    if(!strncmp(fmt,"ap-platform-backing:",20)) {
        assert(!strcmp(fmt,"ap-platform-backing: v=1 phase=%u data=%s\n"));
        va_list ap;va_start(ap,fmt);uint64_t phase=va_arg(ap,uint64_t);const char *data=va_arg(ap,const char *);
        assert(phase<2 && strlen(data)==272 && data[271]=='=');
        if(backing_emit)printf("ap-platform-backing: v=1 phase=%llu data=%s\n",(unsigned long long)phase,data);
        va_end(ap);return;
    }
    if(!strncmp(fmt,"ap-platform-dry:",16)) {
        assert(!strcmp(fmt,"ap-platform-dry: v=1 data=%s\n"));
        va_list ap;va_start(ap,fmt);const char *data=va_arg(ap,const char *);assert(strlen(data)==40 && data[38]=='=' && data[39]=='=');
        const char *abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        uint8_t bytes[30];unsigned out=0;
        for(unsigned i=0;i<40;i+=4) {
            uint32_t v=0;for(unsigned k=0;k<4;k++){const char *p=strchr(abc,data[i+k]);assert(p || data[i+k]=='=');v=(v<<6)|(data[i+k]=='='?0:(unsigned)(p-abc));}
            bytes[out++]=v>>16;bytes[out++]=v>>8;bytes[out++]=v;
        }
        assert(out==30 && !bytes[28] && !bytes[29]);
        uint64_t epoch=0,digest=0;for(unsigned i=0;i<8;i++){epoch|=(uint64_t)bytes[i]<<(8*i);digest|=(uint64_t)bytes[8+i]<<(8*i);}
        assert(epoch==dry_owner.epoch && digest==dry_digest && (bytes[16]|(bytes[17]<<8))==dry_reasons);
        const uint8_t expected[]={dry_before,dry_after,dry_guard_before,dry_guard_after,dry_refs[0],dry_refs[1],dry_inflight,dry_assured,dry_active,0};
        assert(!memcmp(bytes+18,expected,sizeof(expected)));va_end(ap);dry_reports++;return;
    }
    if(!strncmp(fmt,"ap-platform-hda:",16)) {
        assert(!strcmp(fmt,"ap-platform-hda: v=1 phase=%u data=%s\n"));
        va_list ap;va_start(ap,fmt);uint64_t phase=va_arg(ap,uint64_t);assert(phase<2);
        const char *data=va_arg(ap,const char *);assert(strlen(data)==240 && data[239]=='=');
        const char *abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
        uint8_t bytes[180];unsigned out=0;
        for(unsigned i=0;i<240;i+=4) {
            uint32_t v=0;for(unsigned k=0;k<4;k++){const char *p=strchr(abc,data[i+k]);assert(p || data[i+k]=='=');v=(v<<6)|(data[i+k]=='='?0:(unsigned)(p-abc));}
            bytes[out++]=v>>16;bytes[out++]=v>>8;bytes[out++]=v;
        }
        assert(out==180 && !bytes[179]);
        const struct hda_capture *r=&hda_rows[phase];
        uint64_t values[]={r->state,r->reads,r->n,r->begin,r->end,r->base,
            r->pci[0],r->pci[1],r->pci[2],r->pci[3],
            r->before[0],r->before[1],r->before[2],r->before[3],
            r->after[0],r->after[1],r->after[2],r->after[3],
            r->raw[0],r->raw[1],r->raw[2],r->raw[3],r->raw[4],r->raw[5],r->raw[6],r->raw[7]};
        const unsigned widths[]={1,1,1,8,8,8,2,2,2,2,4,4,4,4,4,4,4,4,2,4,4,1,1,4,2,4};
        unsigned at=0;
        for(unsigned i=0;i<56;i++) {
            unsigned width=i<26?widths[i]:3;uint64_t value=0;
            for(unsigned k=0;k<width;k++)value|=(uint64_t)bytes[at++]<<(8*k);
            assert(value==(i<26?values[i]:r->sd[i-26]));
        }
        assert(at==179);va_end(ap);hda_reports++;return;
    }
    bool dep=!strncmp(fmt,"ap-platform-deps:",17),res=!strncmp(fmt,"ap-platform-resources:",22);
    if(!dep && !res)return;
    assert(!strcmp(fmt,dep?"ap-platform-deps: v=2 phase=%u data=%s\n":"ap-platform-resources: v=2 phase=%u data=%s\n"));
    va_list ap;va_start(ap,fmt);uint64_t phase=va_arg(ap,uint64_t);assert(phase<2);
    const char *data=va_arg(ap,const char *);unsigned len=dep?120:208;assert(strlen(data)==len);
    const char *abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";uint8_t bytes[156];unsigned out=0;
    for(unsigned i=0;i<len;i+=4) {
        uint32_t v=0;for(unsigned k=0;k<4;k++){const char *p=strchr(abc,data[i+k]);assert(p || data[i+k]=='=');v=(v<<6)|(data[i+k]=='='?0:(unsigned)(p-abc));}
        bytes[out++]=v>>16;bytes[out++]=v>>8;bytes[out++]=v;
    }
    const struct display_resource_snapshot *r=dep?&dependencies[phase]:&resources[phase];
    uint64_t values[40]={r->state,r->read_count,r->begin,r->end};unsigned widths[40]={1,1,8,8},count=4;
    if(dep)for(unsigned i=0;i<2;i++){values[count]=dep_pch[phase][i];widths[count++]=4;}
    for(unsigned i=0;i<4;i++){values[count]=r->before[i];widths[count++]=4;}
    for(unsigned i=0;i<4;i++){values[count]=r->after[i];widths[count++]=4;}
    for(unsigned i=0;i<(dep?8u:26u);i++){values[count]=r->raw[i];widths[count++]=4;}
    unsigned at=0;for(unsigned i=0;i<count;i++) {
        uint64_t value=0;for(unsigned k=0;k<widths[i];k++)value|=(uint64_t)bytes[at++]<<(8*k);
        assert(value==values[i]);
    }
    assert(at==(dep?90u:154u));if(res)assert(!bytes[154] && !bytes[155]);
    if(backing_emit)printf(dep?"ap-platform-deps: v=2 phase=%llu data=%s\n":"ap-platform-resources: v=2 phase=%llu data=%s\n",(unsigned long long)phase,data);
    va_end(ap);if(dep)dependency_reports++;

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
    struct pci_device hd={.device=3,.class_code=4,.subclass=3};hda_device=&hd;
    hda_config[0]=0x0a0c8086;hda_config[1]=0x00100006;hda_config[2]=0x04030009;
    hda_config[4]=0xb0a10004;hda_config[0x2c/4]=0x011a106b;
    hda_config[0x34/4]=0x40;hda_config[0x40/4]=0x00010001;
    for(unsigned j=0;j<20;j++) {
        hda_seen=0;fault=0;hda_reads=0;hda_mapped=false;map_calls=0;
        uint32_t saved[64];memcpy(saved,hda_config,sizeof(saved));
        hda_gcap=0x1001;hda_gctl=1;hd.device=3;
        if(j==1)hda_config[0]^=1;
        if(j==2)hda_config[0x2c/4]^=1;
        if(j==3)hda_config[2]^=0x100;
        if(j==4)hd.device=4;
        if(j==5)hda_config[0x44/4]=3;
        if(j==6)hda_config[1]&=~2u;
        if(j==7)hda_config[4]|=8;
        if(j==8)occupied=HDA_VA;
        if(j==9)map_failure=1;
        if(j==10)hda_bad=1;
        if(j==11)hda_gcap=0xf0f9; /* >30 streams */
        if(j==12)hda_gctl=0;
        if(j==13)hda_change=1;
        if(j==14)well_drop_at=well_reads+5;
        if(j==15)hda_gcap=0xf079; /* 30 stream descriptors */
        if(j==16)well_value=0;
        if(j>=17)hda_change=j-15;
        hda_snapshot(0);
        if(j==0 || j==15)assert(hda_rows[0].state==1 && !fault && hda_rows[0].reads==8+2*hda_rows[0].n);
        else assert(fault);
        if((j>=1 && j<=9) || j==16)assert(!hda_reads);
        if(j==5 || j==6 || j==16)assert(hda_rows[0].state==2);
        if(j==12)assert(hda_rows[0].state==4 && hda_reads==4 && !hda_rows[0].n);
        memcpy(hda_config,saved,sizeof(saved));occupied=0;map_failure=0;hda_bad=0;hda_change=0;well_drop_at=0;well_value=0x40000000;
    }
    hda_seen=0;fault=0;hd.device=3;hda_gcap=0x1001;hda_gctl=1;hda_reads=0;
    hda_snapshot(0);hda_snapshot(1);assert(hda_rows[1].state==1);
    hda_snapshot(1);assert(fault);fault=0;
    (void)platform_audit_report();assert(dependency_reports==2 && hda_reports==2);
    for(unsigned phase=0;phase<2;phase++) {
        resources[phase].state=1;
        for(unsigned i=0;i<26;i++)resources[phase].raw[i]=0;
        resources[phase].raw[7]=0x82200000;resources[phase].raw[8]=1u<<31;
        resources[phase].raw[20]=1u<<31;resources[phase].raw[25]=1u<<31;
        for(unsigned i=0;i<8;i++)dependencies[phase].raw[i]=0;
        for(unsigned i=0;i<30;i++)hda_rows[phase].sd[i]=0;
    }
    unsigned saved_hda_reads=hda_reads,saved_resource_reads=resource_reads,saved_dep_reads=dep_reads,saved_well_reads=well_reads;
    dry_snapshot(0);dry_snapshot(1);assert(dry_complete && !fault && dry_reasons==127 && dry_owner.guard==0);
    assert(hda_reads==saved_hda_reads && resource_reads==saved_resource_reads && dep_reads==saved_dep_reads && well_reads==saved_well_reads);
    dry_report();assert(dry_reports==2); /* First zero/invalid row also tested above. */
    /* Actual backing adapter: no new maps, only the exact nine register reads.
     * Positive wire output is also independently decoded by the Python suite. */
    struct limine_framebuffer fb={.address=(void *)0xffff8000a0100000ull,.width=1366,.height=768,.pitch=5632,.bpp=32,
        .memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
    struct limine_framebuffer *fbs[]={&fb};struct limine_framebuffer_response response={.framebuffer_count=1,.framebuffers=fbs};
    struct limine_memmap_entry entry={.base=0xa0000000,.length=0x10000000,.type=LIMINE_MEMMAP_FRAMEBUFFER};
    struct limine_memmap_entry *entries[]={&entry,&entry};struct limine_memmap_response memory={.entry_count=1,.entries=entries};
    framebuffer_request.response=&response;memmap_request.response=&memory;
    for(unsigned c=0;c<26;c++) {
        backing_rows[0]=(struct backing_capture){0};backing_seen=0;fault=0;backing_reads=0;map_walks=0;
        backing_mode=1;backing_bad=backing_change=mapping_hole=mapping_disjoint=0;
        pci_config[6]=0xa000000c;pci_config[7]=0;pci_config[1]=0x00100002;
        resource_mapped=true;mapped=true;well_value=0x40000000;well_drop_at=0;resources[0].state=1;resources[0].raw[8]=backing_values[0];
        struct limine_framebuffer saved=fb;
        if(c==1)pci_config[6]|=0x1000;
        if(c==2)pci_config[7]=1;
        if(c==3)pci_config[1]&=~2u;
        if(c==4)response.framebuffer_count=2;
        if(c==5)fb.pitch=1;
        if(c==6)fb.height=4097;
        if(c==7)fb.bpp=24;
        if(c==8)fb.red_mask_shift=0;
        if(c==9)fb.address=(void *)(UINT64_MAX-1024);
        if(c==10)fb.pitch=65536;
        if(c==11)mapping_hole=3;
        if(c==12)mapping_disjoint=3;
        if(c==13)memory.entry_count=4097;
        if(c==14)entry.length=UINT64_MAX;
        if(c==15)well_value=0;
        if(c==16)backing_bad=4;
        if(c==17)backing_change=2;
        if(c==18)well_drop_at=well_reads+5;
        if(c==19)resource_mapped=false;
        if(c==20)memory.entry_count=2; /* Ambiguous classification remains unknown, never ownership. */
        if(c==21)entry.type=LIMINE_MEMMAP_USABLE;
        if(c==22)entry.type=LIMINE_MEMMAP_RESERVED;
        if(c==23)entry.base=0xb0000000;
        if(c==24){fb.address=(void *)0xfffffffffffff000ull;fb.width=1;fb.height=1;fb.pitch=4;} /* Last canonical page: increment must not wrap. */
        if(c==25){fb.width=2048;fb.height=2048;fb.pitch=8192;} /* Exact 4096-page cap. */
        unsigned maps=map_calls;backing_snapshot(0);assert(map_calls==maps && map_walks<=BACKING_PAGE_CAP+1);
        if(c==0 || c>=20) {
            assert(!fault && backing_rows[0].state==1 && backing_reads==9 && backing_rows[0].pages==(c==24?1u:c==25?4096u:1056u));
            assert(backing_rows[0].classification==(c==0 || c==25?1:c==21?3:c==22?2:0));
        } else assert(fault && backing_rows[0].state!=1);
        if(c==1 || c==2 || c==3 || c==15 || c==19)assert(!backing_reads && !map_walks);
        fb=saved;response.framebuffer_count=1;memory.entry_count=1;entry.base=0xa0000000;entry.length=0x10000000;entry.type=LIMINE_MEMMAP_FRAMEBUFFER;
        backing_values[8]=backing_values[3];
    }
    backing_seen=0;backing_rows[0]=(struct backing_capture){0};backing_rows[1]=(struct backing_capture){0};
    fault=0;backing_reads=0;map_walks=0;backing_bad=backing_change=mapping_hole=mapping_disjoint=0;resource_mapped=true;well_drop_at=0;
    resources[1]=resources[0];backing_snapshot(0);backing_reads=0;backing_snapshot(1);
    assert(!fault && backing_seen==3);backing_emit=1;backing_report();
    for(unsigned i=0;i<2;i++){endpoint_report(i,false);endpoint_report(i,true);}
    backing_snapshot(1);assert(fault);backing_emit=0;backing_mode=0;
    puts("platform-sample: PASS exact read-only mappings/refusals, endpoint well bounds/all-ones, sparse hook isolation, ordered reads and capacity");
}
