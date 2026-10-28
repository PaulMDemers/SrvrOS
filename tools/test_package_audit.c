#include <srvros/package_audit.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static uint32_t cfg[64];static unsigned reads;
static uint32_t read_cfg(void *ctx,unsigned offset) {
    assert(ctx==cfg && !(offset&3) && offset<=252 && ++reads<128);return cfg[offset/4];
}
static struct package_pci_snapshot snap(void) {struct package_pci_snapshot s;reads=0;package_pci_read(&s,read_cfg,cfg);return s;}
static void init(void) {memset(cfg,0,sizeof(cfg));cfg[0]=0x9c318086;cfg[1]=(1u<<20)|6;cfg[2]=0x0c033001;cfg[0x34/4]=0x40;}
static uint32_t ext[1024];static unsigned ext_reads;static bool ext_ok;
static bool read_ext(void *ctx,unsigned offset,uint32_t *v) {
    assert(ctx==ext && offset>=256 && offset<=4092 && !(offset&3) && ++ext_reads<=961);
    if(!ext_ok)return false;
    *v=ext[offset/4];return true;
}
static struct package_ltr ltr(void) {struct package_ltr s;ext_reads=0;package_ltr_read(&s,read_ext,ext);return s;}
int main(void) {
    uint64_t dt=99,dc=99;
    assert(package_counter_delta(10,20,20,30,&dt,&dc) && dt==20 && dc==0);
    assert(package_counter_delta(10,20,40,30,&dt,&dc) && dc==20);
    assert(!package_counter_delta(10,20,41,30,&dt,&dc));assert(!package_counter_delta(10,20,19,30,&dt,&dc));
    assert(!package_counter_delta(10,20,20,10,&dt,&dc));
    init();cfg[0x40/4]=0x00035001;cfg[0x44/4]=0x8003;cfg[0x50/4]=0x00420010;
    cfg[0x5c/4]=0xc00;cfg[0x60/4]=0x20130003;cfg[0x74/4]=1u<<11;cfg[0x78/4]=1u<<10;
    struct package_pci_snapshot s=snap();assert(s.state==1 && s.flags==7 && s.pm==0x8003 && s.command==6 && s.class_code==0x0c0330 && s.link==0x20130003 && s.ltr==1024);
    cfg[0x50/4]=0x00920010;s=snap();assert(s.state==1 && s.flags==5 && !s.link && !s.link_cap); /* RC-integrated endpoint has no link */
    init();cfg[1]=6;s=snap();assert(s.state==1 && !s.flags && reads==4); /* absence isn't zero-valued PM */
    init();cfg[0]=~0u;assert(snap().state==3 && reads==1);
    init();cfg[3]=2u<<16;assert(snap().state==4);
    init();cfg[0x40/4]=0x4005;assert(snap().state==2); /* loop */
    init();cfg[0x40/4]=0x4105;assert(snap().state==2); /* unaligned */
    init();cfg[0x34/4]=0xfc;cfg[0xfc/4]=0x30001;assert(snap().state==2); /* PMCSR past end */
    init();cfg[0x40/4]=0x00035001;cfg[0x50/4]=0x00030001;assert(snap().state==2); /* duplicate PM */
    init();cfg[0x40/4]=0x00040001;assert(snap().state==4); /* unknown PM version */
    init();cfg[0x40/4]=0x00430010;assert(snap().state==4); /* unknown PCIe version */
    init();cfg[0x34/4]=0xd8;cfg[0xd8/4]=0x00420010;assert(snap().state==2); /* v2 fields out of config */
    init();for(unsigned p=0x40;p<0xfc;p+=4)cfg[p/4]=((p+4)<<8)|5;cfg[0xfc/4]=5;
    s=snap();assert(s.state==1 && reads==53); /* bounded maximal unknown-cap chain */
    init();cfg[0x40/4]=0x00034401;cfg[0x44/4]=5;assert(snap().state==2); /* next inside PMCSR */
    init();cfg[0x40/4]=0x00410010;cfg[0x4c/4]=0xc00;cfg[0x50/4]=3;
    s=snap();assert(s.state==1 && s.flags==2 && !s.ltr && !s.ltr_cap); /* v1 has no Device Control 2 */
    uint64_t base;assert(package_graphics_bar(0xc0000004,0,2,&base) && base==0xc0000000);
    assert(!package_graphics_bar(0xc0001004,0,2,&base));assert(!package_graphics_bar(0xc0000000,0,2,&base));assert(!package_graphics_bar(0xc0000004,0,0,&base));assert(!package_graphics_bar(4,0,2,&base));
    assert(ltr().state==3);ext_ok=true;assert(ltr().state==2);
    ext[64]=0x00010018;ext[65]=0x08010802;struct package_ltr l=ltr();assert(l.state==1 && l.offset==256 && l.raw==0x08010802 && l.headers==1);
    ext[64]=0x10010018;assert(ltr().state==4); /* self loop */
    ext[64]=0x10410018;assert(ltr().state==4); /* next overlaps LTR data */
    ext[64]=0xffc10001;ext[1023]=0x10018;assert(ltr().state==4); /* data beyond 4096 */
    ext[64]=0x10810018;ext[66]=0x10018;assert(ltr().state==4); /* duplicate LTR */
    ext[64]=0x00020018;assert(ltr().state==4); /* unknown version */
    ext[64]=0x10110001;assert(ltr().state==4); /* misaligned next */
    ext[64]=0x0fc10001;assert(ltr().state==4); /* conventional alias */
    memset(ext,0,sizeof(ext));for(unsigned i=64;i<1023;i++)ext[i]=((i+1)*4u<<20)|0x10001;ext[1023]=0x10001;
    l=ltr();assert(l.state==2 && l.headers==960 && ext_reads==960);
    puts("package-audit: PASS read-only capability bounds, absence, versions, duplicates, loops, RC endpoint, counter bounds/zero");
}
