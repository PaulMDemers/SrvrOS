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
    puts("package-audit: PASS read-only capability bounds, absence, versions, duplicates, loops, RC endpoint, counter bounds/zero");
}
