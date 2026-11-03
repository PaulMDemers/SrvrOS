/* Air6,1 milestone B first readout. No GGTT/PTE, backing data, ownership or
 * control writes. All new registers live in the already read-only mapped
 * primary-A / pipe-A pages. Linux v7.0 i9xx_plane_regs.h / i9xx_plane.c.
 * HSW uses DSPOFFSET (not the older linear-offset interpretation). */
#include <limine.h>
extern volatile struct limine_framebuffer_request framebuffer_request;
extern volatile struct limine_memmap_request memmap_request;
#define BACKING_PAGE_CAP 4096u
#define BACKING_MEMMAP_CAP 4096u
static const unsigned backing_offsets[9]={0x70180,0x70188,0x70184,0x7019c,0x701a4,0x6001c,0x701ac,0x70180,0x7019c};
struct backing_capture {
    uint64_t begin,end,fb[7],entry[3]; /* count, VA, PA, width/height/pitch/bytes */
    uint32_t pci[4],before[4],after[4],raw[9];
    uint16_t command[2],bpp,entries,pages;
    uint8_t state,reads,color[7],classification,contiguous;
};
static struct backing_capture backing_rows[2];
static unsigned backing_seen;
static bool backing_descriptor(struct backing_capture *r) {
    const struct limine_framebuffer_response *response=framebuffer_request.response;
    const struct limine_memmap_response *map=memmap_request.response;
    if(!response || response->framebuffer_count!=1 || !response->framebuffers || !response->framebuffers[0])return false;
    const struct limine_framebuffer *f=response->framebuffers[0];
    r->fb[0]=response->framebuffer_count;r->fb[1]=(uint64_t)f->address;
    r->fb[3]=f->width;r->fb[4]=f->height;r->fb[5]=f->pitch;r->bpp=f->bpp;
    r->color[0]=f->memory_model;r->color[1]=f->red_mask_size;r->color[2]=f->red_mask_shift;
    r->color[3]=f->green_mask_size;r->color[4]=f->green_mask_shift;
    r->color[5]=f->blue_mask_size;r->color[6]=f->blue_mask_shift;
    /* Only the known bounded RGB32 descriptor may drive a page-table walk.
     * No pixel or aperture data are read. Endpoint preservation is not a pin. */
    if(r->fb[1]<0xffff800000000000ull || !f->width || f->width>4096 || !f->height || f->height>4096 ||
       f->bpp!=32 || f->memory_model!=LIMINE_FRAMEBUFFER_RGB ||
       f->red_mask_size!=8 || f->red_mask_shift!=16 || f->green_mask_size!=8 ||
       f->green_mask_shift!=8 || f->blue_mask_size!=8 || f->blue_mask_shift!=0 ||
       f->pitch<f->width*4 || f->pitch>65536)return false;
    r->fb[6]=f->pitch*f->height;
    if(r->fb[6]>BACKING_PAGE_CAP*4096ull-(r->fb[1]&4095) || r->fb[1]>UINT64_MAX-r->fb[6])return false;
    uint64_t space=vmm_kernel_address_space(),physical;
    if(!space || !vmm_virt_to_phys_in_space(space,r->fb[1],&physical) || !physical || physical>UINT64_MAX-r->fb[6])return false;
    r->fb[2]=physical;
    uint64_t first=r->fb[1]&~4095ull,last=(r->fb[1]+r->fb[6]-1)&~4095ull;
    if((physical&4095)!=(r->fb[1]&4095))return false;
    unsigned page_count=(unsigned)((last-first)/4096)+1;
    for(unsigned i=0;i<page_count;i++) {
        uint64_t va=first+4096ull*i;
        uint64_t pa;
        if(!vmm_virt_to_phys_in_space(space,va,&pa) || pa!=(physical&~4095ull)+(va-first))return false;
        r->pages++;
    }
    r->contiguous=1;
    if(!map || !map->entries || !map->entry_count || map->entry_count>BACKING_MEMMAP_CAP)return false;
    r->entries=map->entry_count;
    /* Preserve one enclosing firmware classification only when no other entry
     * overlaps any byte. Do not infer PMM reservation, GGTT backing or ownership
     * from this firmware map. Unknown/mixed maps remain explicitly unknown. */
    unsigned overlaps=0;
    for(unsigned i=0;i<r->entries;i++) {
        const struct limine_memmap_entry *e=map->entries[i];
        if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
        uint64_t end=e->base+e->length;
        if(e->base<physical+r->fb[6] && physical<end) {
            overlaps++;
            if(e->base<=physical && physical+r->fb[6]<=end) {
                r->entry[0]=e->base;r->entry[1]=e->length;r->entry[2]=e->type;
                r->classification=e->type==LIMINE_MEMMAP_FRAMEBUFFER?1:e->type==LIMINE_MEMMAP_RESERVED?2:3;
            }
        }
    }
    if(overlaps!=1 || !r->classification) {
        r->classification=0;for(unsigned i=0;i<3;i++)r->entry[i]=0;
    }
    return true;
}
static void backing_snapshot(unsigned phase) {
    if(phase>1 || (backing_seen&(1u<<phase))) {fault=1;return;}
    backing_seen|=1u<<phase;
    struct backing_capture *r=&backing_rows[phase];r->state=3;r->begin=timer_monotonic_us();
    if(!mapped || !resource_mapped || resources[phase].state!=1 || !graphics_access_valid())goto done;
    r->pci[0]=pci_read_config32(gpu,0x18);r->pci[1]=pci_read_config32(gpu,0x1c);r->command[0]=pci_read_config16(gpu,4);
    /* Captured Haswell aperture BAR2: prefetchable 64-bit, 256 MiB aligned.
     * No destructive BAR sizing; size/translation authority remain unknown. */
    if((r->pci[0]&0x0fffffff)!=0xcu || r->pci[1] || !(r->pci[0]&0xf0000000u) || !(r->command[0]&2))goto done;
    for(unsigned i=0;i<4;i++)r->before[i]=well_read(0x400+4*i);
    if(!display_resource_well_on(r->before)){r->state=2;goto done;}
    if(!backing_descriptor(r))goto done;
    for(unsigned i=0;i<9;i++) {
        r->raw[i]=resource_read(backing_offsets[i]);r->reads++;
        if(r->raw[i]==0xffffffffu)goto done;
    }
    for(unsigned i=0;i<4;i++)r->after[i]=well_read(0x400+4*i);
    r->pci[2]=pci_read_config32(gpu,0x18);r->pci[3]=pci_read_config32(gpu,0x1c);r->command[1]=pci_read_config16(gpu,4);
    if(!graphics_access_valid() || !display_resource_well_on(r->after) || r->pci[0]!=r->pci[2] || r->pci[1]!=r->pci[3] ||
       r->command[0]!=r->command[1] || r->raw[0]!=r->raw[7] || r->raw[3]!=r->raw[8] || r->raw[0]!=resources[phase].raw[8])goto done;
    for(unsigned i=0;i<4;i++)if(r->before[i]!=r->after[i])goto done;
    r->state=1;
done:
    r->end=timer_monotonic_us();if(r->state!=1)fault=1;
}
static void backing_report(void) {
    static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for(unsigned phase=0;phase<2;phase++) {
        const struct backing_capture *r=&backing_rows[phase];
        uint8_t bytes[256];char data[345];unsigned n=0,j=0;
        hda_put(bytes,&n,r->state,1);hda_put(bytes,&n,r->reads,1);
        hda_put(bytes,&n,r->begin,8);hda_put(bytes,&n,r->end,8);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->pci[i],4);
        for(unsigned i=0;i<2;i++)hda_put(bytes,&n,r->command[i],2);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->before[i],4);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->after[i],4);
        for(unsigned i=0;i<9;i++)hda_put(bytes,&n,r->raw[i],4);
        for(unsigned i=0;i<7;i++)hda_put(bytes,&n,r->fb[i],8);
        hda_put(bytes,&n,r->bpp,2);for(unsigned i=0;i<7;i++)hda_put(bytes,&n,r->color[i],1);
        hda_put(bytes,&n,r->entries,2);hda_put(bytes,&n,r->pages,2);hda_put(bytes,&n,r->classification,1);
        for(unsigned i=0;i<3;i++)hda_put(bytes,&n,r->entry[i],8);
        hda_put(bytes,&n,r->contiguous,1);hda_put(bytes,&n,0,1);hda_put(bytes,&n,0,1); /* GGTT proof/authority unknown. */
        for(unsigned i=0;i<n;i+=3) {
            unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
            if(left>1)v|=(uint32_t)bytes[i+1]<<8;
            if(left>2)v|=bytes[i+2];
            data[j++]=abc[v>>18];data[j++]=abc[(v>>12)&63];
            data[j++]=left>1?abc[(v>>6)&63]:'=';data[j++]=left>2?abc[v&63]:'=';
        }
        data[j]=0;console_printf("ap-platform-backing: v=1 phase=%u data=%s\n",(uint64_t)phase,data);
    }
}
/* Version-9 lossless resource/dependency packing preserves all existing fields
 * within the original retention budget. This changes no read path or capacity. */
static void endpoint_report(unsigned phase,bool dependency) {
    const struct display_resource_snapshot *r=dependency?&dependencies[phase]:&resources[phase];
    static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t bytes[154];char data[209];unsigned n=0,j=0;
    hda_put(bytes,&n,r->state,1);hda_put(bytes,&n,r->read_count,1);
    hda_put(bytes,&n,r->begin,8);hda_put(bytes,&n,r->end,8);
    if(dependency)for(unsigned i=0;i<2;i++)hda_put(bytes,&n,dep_pch[phase][i],4);
    for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->before[i],4);
    for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->after[i],4);
    for(unsigned i=0;i<(dependency?8u:26u);i++)hda_put(bytes,&n,r->raw[i],4);
    for(unsigned i=0;i<n;i+=3) {
        unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
        if(left>1)v|=(uint32_t)bytes[i+1]<<8;
        if(left>2)v|=bytes[i+2];
        data[j++]=abc[v>>18];data[j++]=abc[(v>>12)&63];
        data[j++]=left>1?abc[(v>>6)&63]:'=';data[j++]=left>2?abc[v&63]:'=';
    }
    data[j]=0;
    if(dependency)console_printf("ap-platform-deps: v=2 phase=%u data=%s\n",(uint64_t)phase,data);
    else console_printf("ap-platform-resources: v=2 phase=%u data=%s\n",(uint64_t)phase,data);
}
