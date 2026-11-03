/* Isolated exact-Air6,1 read-only HDA controls. HDA 1.0a sections 3.1.2,
 * 3.3.2/7/14/22/29/32/35. No adjacent status, ring pointers or codec windows. */
#define HDA_VA 0xffffc001f3230000ull
#define HDA_STREAM_CAP 30u
struct hda_capture {
    uint64_t begin,end,base;
    unsigned state,reads,n;
    uint32_t before[4],after[4],pci[4],raw[8],sd[HDA_STREAM_CAP];
};
static struct hda_capture hda_rows[2];
static const struct pci_device *hda_device;
static uint64_t hda_base;
static bool hda_mapped;
static uint32_t hda_read(unsigned off,unsigned width) {
#ifdef SRVROS_PLATFORM_HOST_TEST
    return platform_test_hda_read(off,width);
#else
    if(width==1)return *(volatile const uint8_t *)(HDA_VA+off);
    if(width==2)return *(volatile const uint16_t *)(HDA_VA+off);
    return *(volatile const uint32_t *)(HDA_VA+off);
#endif
}
/* Returns 1 accessible, 2 off (no MMIO), 3 invalid. No config writes. */
static unsigned hda_access(uint32_t *command,uint32_t *pm,uint64_t *base) {
    if(!hda_device || hda_device->bus || hda_device->device!=3 || hda_device->function ||
       hda_device->class_code!=4 || hda_device->subclass!=3)return 3;
    if(pci_read_config32(hda_device,0)!=0x0a0c8086 ||
       (pci_read_config32(hda_device,8)>>8)!=0x040300 ||
       pci_read_config32(hda_device,0x2c)!=0x011a106b)return 3;
    struct package_pci_snapshot p;package_pci_read(&p,conventional_read,(void *)hda_device);
    if(p.state!=1 || !(p.flags&1))return 3;
    uint32_t lo=pci_read_config32(hda_device,0x10),hi=pci_read_config32(hda_device,0x14);
    *base=((uint64_t)hi<<32)|(lo&~15u);*command=p.command;*pm=p.pm;
    /* Captured BAR0: non-prefetchable 64-bit 16KiB. Never size or relocate it.
     * Only first page is mapped; all 30 descriptor controls fit within it. */
    if((lo&15)!=4 || !*base || (*base&0x3fff) || *base>0x000fffffffffc000ull ||
       (*base>=gpu_base && *base<gpu_base+0x400000))return 3;
    return (p.pm&3) || !(p.command&2)?2:1;
}
static void hda_snapshot(unsigned phase) {
    if(phase>1 || !mapped || (hda_seen&(1u<<phase))){fault=1;return;}
    struct hda_capture *r=&hda_rows[phase];*r=(struct hda_capture){0};
    hda_seen|=1u<<phase;r->begin=timer_monotonic_us();r->state=3;
    if(!hda_device)for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(!p->bus && p->device==3 && !p->function)hda_device=p;
    }
    if(!graphics_access_valid() || !dependency_pch_valid(&(uint32_t){0}))goto done;
    unsigned access=hda_access(&r->pci[0],&r->pci[1],&r->base);
    if(access==3)goto done;
    if(access==2){r->state=2;goto done;}
    if(hda_mapped && hda_base!=r->base)goto done;
    for(unsigned i=0;i<4;i++)r->before[i]=well_read(0x400+4*i);
    for(unsigned i=0;i<4;i++)if(r->before[i]==0xffffffffu)goto done;
    if(!display_resource_well_on(r->before)){r->state=2;goto done;}
    if(!hda_mapped) {
        uint64_t existing;
        if(vmm_virt_to_phys(HDA_VA,&existing))goto done;
        hda_base=r->base;
        if(!vmm_map_page(HDA_VA,hda_base,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE))goto done;
        hda_mapped=true;
    }
    r->raw[0]=hda_read(0,2);r->reads++;
    r->raw[1]=hda_read(8,4);r->reads++;
    if(r->raw[0]==0xffff || r->raw[1]==0xffffffffu)goto done;
    unsigned n=((r->raw[0]>>12)&15)+((r->raw[0]>>8)&15)+((r->raw[0]>>3)&31);
    if(!n || n>HDA_STREAM_CAP || ((r->raw[0]>>1)&3)==3)goto done;
    if(r->raw[1]&1) {
        r->n=n;
        static const unsigned offsets[]={0x20,0x4c,0x5c,0x70};
        static const unsigned widths[]={4,1,1,4};
        for(unsigned i=0;i<4;i++) {
            r->raw[i+2]=hda_read(offsets[i],widths[i]);r->reads++;
            if(r->raw[i+2]==(widths[i]==1?0xffu:0xffffffffu))goto done;
        }
        for(unsigned i=0;i<n;i++) {
            /* 16+8 bits excludes SDnSTS at +3. */
            unsigned off=0x80+0x20*i;
            uint32_t low=hda_read(off,2),high=hda_read(off+2,1);
            r->sd[i]=low|(high<<16);r->reads+=2;
            if(r->sd[i]==0xffffffu)goto done;
        }
    }
    r->raw[6]=hda_read(0,2);r->raw[7]=hda_read(8,4);r->reads+=2;
    if(r->raw[6]!=r->raw[0] || r->raw[7]!=r->raw[1])goto done;
    for(unsigned i=0;i<4;i++)r->after[i]=well_read(0x400+4*i);
    uint64_t base;
    if(hda_access(&r->pci[2],&r->pci[3],&base)!=1 || base!=r->base ||
       r->pci[0]!=r->pci[2] || r->pci[1]!=r->pci[3] || !graphics_access_valid() ||
       !dependency_pch_valid(&(uint32_t){0}) || !display_resource_well_on(r->after))goto done;
    for(unsigned i=0;i<4;i++)if(r->before[i]!=r->after[i])goto done;
    r->state=(r->raw[1]&1)?1:4; /* Reset asserted: other controls unknown. */
done:
    r->end=timer_monotonic_us();if(r->state!=1)fault=1;
}
/* Fixed-width little-endian wire schema: 179 bytes, canonical base64 (240).
 * This preserves every field, including partial/failure rows and unused slots.
 * No struct padding, byte count guess, numeric truncation or semantic omission. */
static void hda_put(uint8_t *bytes,unsigned *n,uint64_t value,unsigned width) {
    for(unsigned i=0;i<width;i++)bytes[(*n)++]=(uint8_t)(value>>(8*i));
}
static void hda_report(void) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for(unsigned phase=0;phase<2;phase++) {
        const struct hda_capture *r=&hda_rows[phase];
        uint8_t bytes[179];char encoded[241];unsigned n=0,j=0;
        hda_put(bytes,&n,r->state,1);hda_put(bytes,&n,r->reads,1);hda_put(bytes,&n,r->n,1);
        hda_put(bytes,&n,r->begin,8);hda_put(bytes,&n,r->end,8);hda_put(bytes,&n,r->base,8);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->pci[i],2);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->before[i],4);
        for(unsigned i=0;i<4;i++)hda_put(bytes,&n,r->after[i],4);
        static const unsigned widths[]={2,4,4,1,1,4,2,4};
        for(unsigned i=0;i<8;i++)hda_put(bytes,&n,r->raw[i],widths[i]);
        for(unsigned i=0;i<HDA_STREAM_CAP;i++)hda_put(bytes,&n,r->sd[i],3);
        for(unsigned i=0;i<n;i+=3) {
            unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
            if(left>1)v|=(uint32_t)bytes[i+1]<<8;
            if(left>2)v|=bytes[i+2];
            encoded[j++]=alphabet[v>>18];encoded[j++]=alphabet[(v>>12)&63];
            encoded[j++]=left>1?alphabet[(v>>6)&63]:'=';
            encoded[j++]=left>2?alphabet[v&63]:'=';
        }
        encoded[j]=0;
        console_printf("ap-platform-hda: v=1 phase=%u data=%s\n",(uint64_t)phase,encoded);
    }
}
