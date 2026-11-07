#include <srvros/intel_dma_observe.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct intel_dma_topology topology={INTEL_DMA_DMAR_HASH,0xfed90000,0xfed91000,0x8b800000,0x90000000};
static unsigned reads,maps,unmaps,guards,clocks,fail_read,fail_map,fail_unmap,fail_guard,mode;
static bool mapped;
static uint64_t ticks;
static bool guard(void *p){(void)p;assert(!mapped);return ++guards!=fail_guard;}
static bool map(void *p,uint64_t address){(void)p;assert(!mapped && address==0xfed90000);if(++maps==fail_map)return false;mapped=true;return true;}
static bool unmap(void *p){(void)p;assert(mapped);if(++unmaps==fail_unmap)return false;mapped=false;return true;}
static bool read_reg(void *p,uint32_t offset,uint32_t *out) {
    (void)p;assert(mapped);if(++reads==fail_read)return false;
    switch(offset){case 0:*out=0x10;break;case 8:*out=0x1234;break;case 12:*out=1;break;case 16:*out=0x5678;break;case 20:*out=2;break;case 28:*out=0xc0000000;break;case 32:*out=0x800000;break;case 36:*out=0;break;default:assert(0);}
    if(mode==1 && reads==5)*out=3;
    if(mode==2 && reads>12 && offset==8)*out=0x1235;
    if(mode==3 && offset==28)*out=UINT32_MAX;
    if(mode==7 && offset==28)*out=0;
    return true;
}
static uint64_t now(void *p){(void)p;clocks++;if(mode==4)return 100;if(mode==5 && clocks>2)return ticks;if(mode==6 && clocks==3)return 0;ticks+=100;return ticks;}
static struct intel_dma_io io={0,guard,map,unmap,read_reg,now};
static void reset(void){reads=maps=unmaps=guards=clocks=fail_read=fail_map=fail_unmap=fail_guard=mode=0;mapped=false;ticks=0;}
static void put(uint8_t *p,uint64_t x,unsigned n){for(unsigned i=0;i<n;i++)p[i]=(uint8_t)(x>>(8*i));}
static void checksum(uint8_t *p){unsigned sum=0;p[9]=0;for(unsigned i=0;i<136;i++)sum+=p[i];p[9]=(uint8_t)(0-sum);}
static void table_test(void) {
    uint8_t bytes[136]={0};memcpy(bytes,"DMAR",4);put(bytes+4,136,4);bytes[8]=1;bytes[36]=38;bytes[37]=3;
    memcpy(bytes+10,"TEST  ",6); /* Synthetic header, not firmware copied from a machine. */
    put(bytes+50,24,2);put(bytes+56,0xfed90000,8);memcpy(bytes+64,(uint8_t[]){1,8,0,0,0,0,2,0},8);
    put(bytes+74,32,2);bytes[76]=1;put(bytes+80,0xfed91000,8);memcpy(bytes+88,(uint8_t[]){3,8,0,0,2,240,31,0,4,8,0,0,0,240,15,0},16);
    put(bytes+104,1,2);put(bytes+106,32,2);put(bytes+112,0x8b800000,8);put(bytes+120,0x8fffffff,8);memcpy(bytes+128,bytes+64,8);checksum(bytes);
    struct intel_dma_topology t;assert(intel_dma_table(bytes,136,&t) && t.hash!=INTEL_DMA_DMAR_HASH && t.unit==topology.unit);
    reset();struct intel_dma_result r;assert(!intel_dma_observe(&r,&t,&io) && maps==0); /* Shape is not the native pin. */
    for(unsigned n=0;n<136;n++)assert(!intel_dma_table(bytes,n,&t));
    for(unsigned i=36;i<136;i++) {
        uint8_t copy[136];memcpy(copy,bytes,136);copy[i]^=1;checksum(copy);
        assert(!intel_dma_table(copy,136,&t));
    }
    bytes[9]^=1;assert(!intel_dma_table(bytes,136,&t));
}
int main(void) {
    table_test();struct intel_dma_result r;
    reset();assert(intel_dma_observe(&r,&topology,&io));assert(r.status==DMA_OBSERVED && maps==2 && unmaps==2 && !mapped && reads==24 && guards==4);
    assert(r.sample[0].reads==12 && r.sample[1].begin_us-r.sample[0].end_us>=20000);
    reset();mode=7;assert(intel_dma_observe(&r,&topology,&io) && r.sample[0].after==0); /* Disabled != DMA authority. */
    for(unsigned i=1;i<=24;i++){reset();fail_read=i;assert(!intel_dma_observe(&r,&topology,&io) && r.status==DMA_READ && !mapped && maps==unmaps);}
    for(unsigned i=1;i<=2;i++){
        reset();fail_map=i;assert(!intel_dma_observe(&r,&topology,&io) && r.status==DMA_MAP && !mapped);
        reset();fail_unmap=i;assert(!intel_dma_observe(&r,&topology,&io) && r.status==DMA_RETAINED && r.retained && mapped && maps==i);
    }
    for(unsigned i=1;i<=4;i++){reset();fail_guard=i;assert(!intel_dma_observe(&r,&topology,&io) && !mapped && maps==unmaps);}
    for(unsigned i=1;i<=6;i++) {reset();mode=i;assert(!intel_dma_observe(&r,&topology,&io) && !mapped && maps==unmaps && clocks<=1000002);}
    reset();struct intel_dma_topology bad=topology;bad.unit+=4096;assert(!intel_dma_observe(&r,&bad,&io) && !maps);
    puts("intel-dma: PASS exact topology/checksum/hash separation, read-only offsets, both TES states, torn/changed registers, all read/map/guard/unmap faults, clock rollback/freeze and bounded retirement");
}
