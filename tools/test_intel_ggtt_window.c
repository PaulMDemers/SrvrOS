#include <srvros/intel_ggtt_window.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct intel_window_endpoint endpoint;
static struct intel_window_result result;
static struct {bool mapped,frozen;unsigned op,fail,read,tamper,unmaps,guards,clock,jump,rollback;uint64_t us;} m;
static uint64_t entry(unsigned i){return i?0x180000001ull+i*4096:0;}
static bool act(void){return ++m.op!=m.fail;}
static bool guard(void *ctx){assert(ctx==&m && !m.mapped);m.guards++;return act();}
static bool ep(void *ctx,struct intel_window_endpoint *out){assert(ctx==&m && !m.mapped);*out=endpoint;return act();}
static bool map(void *ctx,uint32_t page){assert(ctx==&m && !m.mapped && page==INTEL_WINDOW_PAGE);if(!act())return false;m.mapped=true;return true;}
static bool read(void *ctx,uint32_t off,uint32_t *out){
    assert(ctx==&m && m.mapped && off>=INTEL_WINDOW_PAGE && off<INTEL_WINDOW_PAGE+128 && !(off&3));
    if(!act())return false;
    unsigned i=(off-INTEL_WINDOW_PAGE)/8;*out=(uint32_t)(entry(i)>>((off&4)?32:0));
    if(++m.read==m.tamper)*out^=1;
    return true;
}
static bool unmap(void *ctx){assert(ctx==&m && m.mapped);m.unmaps++;if(!act())return false;m.mapped=false;return true;}
static uint64_t now(void *ctx){
    assert(ctx==&m);m.clock++;
    if(m.clock==m.jump)m.us+=100001;
    if(m.clock==m.rollback)return m.us-1;
    if(!m.frozen)m.us++;
    return m.us;
}
static struct intel_window_io io={&m,guard,ep,map,read,unmap,now};
static void setup(void){
    endpoint=(struct intel_window_endpoint){0xc0000000,0xb000000c,0x800000,0,0x2200000,1};
    memset(&m,0,sizeof(m));m.us=100;
}
int main(void){
    const unsigned codes[]={0,1,3,7,15,31};
    for(unsigned i=0;i<6;i++) {
        uint64_t b=1,n=2;assert(intel_ggtt_aperture((uint8_t)(codes[i]|0xe0),0x10000000cull,&b,&n));
        assert(b==0x100000000ull && n==(uint64_t)(codes[i]+1)*0x8000000);
    }
    for(unsigned i=0;i<32;i++)if(i!=0 && i!=1 && i!=3 && i!=7 && i!=15 && i!=31){uint64_t b=1,n=2;assert(!intel_ggtt_aperture(i,0x10000000cull,&b,&n) && b==1 && n==2);}
    for(unsigned i=0;i<4;i++){uint64_t b=1,n=2;const uint64_t raw[]={12,0xb000100c,0xb0000008,1ull<<39};assert(!intel_ggtt_aperture(1,raw[i],&b,&n) && b==1 && n==2);}
    setup();assert(intel_ggtt_window(&result,&io));unsigned ops=m.op,clocks=m.clock,reads=m.read;
    assert(!m.mapped && result.status==WINDOW_OBSERVED && result.maps==2 && !result.retained && m.unmaps==2 && m.guards==5 && reads==96);
    assert(result.aperture_bytes==0x10000000 && result.aperture_base==0xb0000000);
    for(unsigned n=0;n<2;n++){assert(result.sample[n].reads==48 && result.sample[n].zero==1 && result.sample[n].bit0==15);for(unsigned i=0;i<16;i++)assert(result.sample[n].entry[i]==entry(i));}
    assert(result.sample[0].hash==result.sample[1].hash);
    for(unsigned i=1;i<=ops;i++) {
        setup();m.fail=i;assert(!intel_ggtt_window(&result,&io));
        assert(result.status!=WINDOW_OBSERVED && result.retained==(unsigned)m.mapped);
        if(m.mapped)assert(result.status==WINDOW_RETAINED);
    }
    for(unsigned i=1;i<=reads;i++) {
        setup();m.tamper=i;assert(!intel_ggtt_window(&result,&io));assert(result.status==WINDOW_CHANGED && !m.mapped);
    }
    for(unsigned i=2;i<=clocks;i++) {
        setup();m.jump=i;assert(!intel_ggtt_window(&result,&io));assert(result.status==WINDOW_CLOCK && !m.mapped);
        setup();m.rollback=i;assert(!intel_ggtt_window(&result,&io));assert(result.status==WINDOW_CLOCK && !m.mapped);
    }
    setup();m.frozen=true;assert(!intel_ggtt_window(&result,&io) && result.status==WINDOW_CLOCK && !m.mapped);
    for(unsigned i=0;i<6;i++) {
        setup();if(i==0)endpoint.msac=2;
        if(i==1)endpoint.gmadr+=0x1000;
        if(i==2)endpoint.ggtt_bytes=0x200000;
        if(i==3)endpoint.fence_end=0x54c000;
        if(i==4)endpoint.fence_begin=4096;
        if(i==5)endpoint.mmio+=0x1000000;
        assert(!intel_ggtt_window(&result,&io) && !m.mapped && !result.maps);
    }
    printf("intel-window: PASS legal geometry, full-fence refusal, raw vectors, %u callback failures, %u torn/changed reads, %u clock sites and retained-map failure\n",ops,reads,clocks);
}
