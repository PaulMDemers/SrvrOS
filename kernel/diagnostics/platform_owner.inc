/* Milestone A: software-only owner. Consume retained data, never touch MMIO or
 * invent backing, domain/HDA/IRQ ownership, restore/recovery or firmware proof. */
#include <srvros/display_owner.h>
#include "display_owner_core.c"
static struct display_owner_pool dry_pool;
static struct display_owner dry_owner;
static bool dry_complete;
static uint16_t dry_reasons;
static uint8_t dry_before,dry_after,dry_guard_before,dry_guard_after;
static uint8_t dry_refs[2],dry_inflight,dry_active,dry_assured;
static uint64_t dry_digest;
static struct display_owner_profile dry_profile(unsigned phase) {
    struct display_owner_profile p={.gpu=0x0a268086,.pch=0x9c438086,.hda=0x0a0c8086,
        .subsystem=0x011a106b,.access_valid=!fault && resources[phase].state==1 &&
        dependencies[phase].state==1 && hda_rows[phase].state==1};
    unsigned n=0;
    const struct hda_capture *h=&hda_rows[phase];
    for(unsigned i=0;i<26;i++)p.raw[n++]=resources[phase].raw[i];
    for(unsigned i=0;i<8;i++)p.raw[n++]=dependencies[phase].raw[i];
    for(unsigned i=0;i<8;i++)p.raw[n++]=h->raw[i];
    for(unsigned i=0;i<30;i++)p.raw[n++]=h->sd[i];
    for(unsigned i=0;i<4;i++)p.raw[n++]=h->pci[i];
    for(unsigned i=0;i<4;i++)p.raw[n++]=h->before[i];
    p.raw[n++]=(uint32_t)gpu_base;p.raw[n++]=gpu_base>>32;
    p.raw[n++]=(uint32_t)h->base;p.raw[n++]=h->base>>32;p.raw[n++]=h->n;
    if((h->raw[3]&2) || (h->raw[4]&2) || (h->raw[5]&1))p.active|=1;
    for(unsigned i=0;i<h->n && i<HDA_STREAM_CAP;i++)if(h->sd[i]&2)p.active|=1;
    if(h->raw[2] || dependencies[phase].raw[5] || dependencies[phase].raw[7])p.active|=2;
    const uint32_t *v=resources[phase].raw;
    for(unsigned i=0;i<3;i++)if(v[i]&(1u<<31))p.active|=4;
    if((v[9]|v[10]|v[12]|v[13])&(1u<<31))p.active|=4;
    if((v[15]|v[16])&63)p.active|=4;
    if(((v[7]>>12)&7)==4 || (dependencies[phase].raw[0]&0x444))p.active|=4;
    for(unsigned i=17;i<20;i++)if(v[i]&(1u<<31))p.active|=4;
    for(unsigned i=21;i<23;i++)if(v[i]&(1u<<31))p.active|=4; /* Supported ULT B/C. */
    if(!(v[25]&(1u<<31)))p.active|=4;
    /* No assurance bits: raw quiet bits are not adoption or authority. */
    return p;
}
static void dry_snapshot(unsigned phase) {
    struct display_owner_profile p=dry_profile(phase);
    if(!phase) {
        if(!display_owner_open(&dry_owner,&dry_pool,&p))fault=1;
        dry_digest=14695981039346656037ull;
        for(unsigned i=0;i<DISPLAY_OWNER_WORDS;i++) {
            for(unsigned j=0;j<4;j++)dry_digest=(dry_digest^((p.raw[i]>>(8*j))&255))*1099511628211ull;
        }
        return;
    }
    dry_before=dry_owner.state;dry_guard_before=dry_owner.guard;
    dry_refs[0]=dry_owner.refs[0];dry_refs[1]=dry_owner.refs[1];dry_inflight=dry_owner.inflight;
    dry_assured=p.assured;dry_active=p.active;
    dry_reasons=display_owner_check(&dry_owner,dry_owner.epoch,&p);
    bool closed=display_owner_close(&dry_owner,dry_owner.epoch);
    dry_after=dry_owner.state;dry_guard_after=dry_owner.guard;
    dry_complete=closed && dry_before==DISPLAY_OWNER_OBSERVED && dry_after==DISPLAY_OWNER_RETIRED &&
        dry_reasons==DISPLAY_OWNER_REQUIRED && !dry_refs[0] && !dry_refs[1] && !dry_inflight &&
        dry_guard_before==1 && !dry_guard_after && !dry_active && !dry_assured;
    if(!dry_complete)fault=1;
}
static void dry_report(void) {
    static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    uint8_t bytes[28];char data[41];unsigned n=0,j=0;
    hda_put(bytes,&n,dry_owner.epoch,8);hda_put(bytes,&n,dry_digest,8);hda_put(bytes,&n,dry_reasons,2);
    const uint8_t values[]={dry_before,dry_after,dry_guard_before,dry_guard_after,
        dry_refs[0],dry_refs[1],dry_inflight,dry_assured,dry_active,0}; /* Authority always zero. */
    for(unsigned i=0;i<10;i++)hda_put(bytes,&n,values[i],1);
    for(unsigned i=0;i<n;i+=3) {
        unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
        if(left>1)v|=(uint32_t)bytes[i+1]<<8;
        if(left>2)v|=bytes[i+2];
        data[j++]=alphabet[v>>18];data[j++]=alphabet[(v>>12)&63];
        data[j++]=left>1?alphabet[(v>>6)&63]:'=';data[j++]=left>2?alphabet[v&63]:'=';
    }
    data[j]=0;console_printf("ap-platform-dry: v=1 data=%s\n",data);
}
