#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_dac5.h>
static struct mt76_dac5 state,accepted;
static uint32_t fixture[13][3];
static bool trace;
struct fake {unsigned calls,auth,fail,deny,bad,mutate,dac_edges;int length;uint32_t trim,dac;};
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool authorize(void *p){
    struct fake *f=p;assert(mt76_dac5_owned(&state));
    unsigned q=state.operations/7;if(q>=3 && q<=10)assert(f->trim==0x44);
    if(++f->auth==f->mutate)state.trim^=1;
    return f->auth!=f->deny;
}
static bool step(struct fake *f){return ++f->calls!=f->fail;}
static bool read_guard(void *p,uint32_t a,uint32_t *v){
    struct fake *f=p;unsigned n=state.operations%7,j=n<3?n:n-4;
    assert(state.operations==f->calls && mt76_dac5_transfer_valid(&state,false,a) && !mt76_dac5_transfer_valid(&state,true,a) && !mt76_dac5_transfer_valid(&state,false,a^4));
    assert(n!=3 && a==(j==0?0x1004u:j==1?0x2000u:0x208u));
    if(!step(f))return false;
    *v=j==0?0:j==1?0xf000f200:0x40000050;if(f->calls==f->bad)*v^=1;
    if(trace)printf("wifi-dac5-usb: op=%u address=0x%016llx value=0x%016llx ok=1\n",f->calls-1,(unsigned long long)a,(unsigned long long)*v);
    return true;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t cap){
    struct fake *f=p;unsigned q=state.operations/7;uint8_t expected[16];
    assert(state.operations==f->calls && state.operations%7==3 && q<13);
    assert(mt76_dac5_packet(&state,expected,16)==16 && !memcmp(expected,packet,16) && !mt76_dac5_packet(&state,expected,15));
    assert(length==16 && cap==1024);
    uint32_t w[4];for(unsigned i=0;i<4;i++)w[i]=get32(packet+4*i);
    unsigned cmd=(w[0]>>20)&15,seq=(w[0]>>16)&15;bool write=fixture[q][2]!=0;
    assert(cmd==(write?12u:10u) && seq==q+2 && (w[0]&0xffff)==8 && w[1]==fixture[q][0] && w[2]==(write?fixture[q][1]:0) && !w[3]);
    assert(w[1]!=0x80000004);if(!step(f))return -1;
    uint32_t *reg=w[1]==0x80000016?&f->trim:&f->dac;
    if(write){if(w[1]==0x80000049 && ((*reg^w[2])&0x80))f->dac_edges++;*reg=w[2];}
    w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=*reg;
    for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)reply[4*i+b]=(uint8_t)(w[i]>>(8*b));
    if(f->calls==f->bad)reply[0]^=1;
    if(trace)printf("wifi-dac5-command: op=%u write=%u sent=16 received=16 cc=13 residue=1008 h=0x%016llx address=0x%016llx value=0x%016llx t=0x%016llx\n",f->calls-1,(unsigned)write,(unsigned long long)w[0],(unsigned long long)w[1],(unsigned long long)w[2],(unsigned long long)w[3]);
    return f->length;
}
static enum mt76_mcu_timing_status run(struct fake *f){struct mt76_mcu_timing_io io={f,authorize,read_guard,exchange};return mt76_dac5_capture(&state,0xfd11,0x1004,0x44,&io);}
static void init(struct fake *f){memset(&state,0,sizeof state);*f=(struct fake){.length=16,.dac=0x93};}
int main(int argc,char **argv){
    assert(argc==2 || argc==3);trace=argc==3 && !strcmp(argv[2],"--trace");assert(argc==2 || trace);
    FILE *file=fopen(argv[1],"rb");assert(file);uint8_t data[156];assert(fread(data,1,sizeof data,file)==sizeof data && fgetc(file)==EOF);fclose(file);
    for(unsigned q=0;q<13;q++)for(unsigned i=0;i<3;i++)fixture[q][i]=get32(data+q*12+i*4);
    struct fake f;init(&f);assert(run(&f)==MT76_TIMING_OK && mt76_dac5_valid(&state));accepted=state;
    assert(f.calls==91 && f.auth==92 && f.dac_edges==2 && f.trim==0 && f.dac==0x93 && state.writes==5 && state.exchanges==13 && state.reads==78);
    if(trace)return 0;
    assert(!mt76_dac5_owned(&state) && !mt76_dac5_packet(&state,data,sizeof data) && run(&f)==MT76_TIMING_REFUSED);
    for(unsigned n=1;n<=91;n++){
        init(&f);f.fail=n;assert(run(&f)==MT76_TIMING_IO && f.calls==n && !state.complete);assert(run(&f)==MT76_TIMING_REFUSED && f.calls==n);
        init(&f);f.bad=n;assert(run(&f)!=MT76_TIMING_OK && f.calls==n && !state.complete);
    }
    for(unsigned n=1;n<=92;n++){
        init(&f);f.deny=n;assert(run(&f)==MT76_TIMING_CANCELLED && f.auth==n && f.calls==n-1 && !state.complete);
        init(&f);f.mutate=n;assert(run(&f)==MT76_TIMING_STATE && f.calls==n-1 && !state.complete);
    }
    for(unsigned q=0;q<13;q++)for(unsigned i=0;i<4;i++)for(unsigned b=0;b<32;b++){state=accepted;state.words[q][i]^=1u<<b;assert(!mt76_dac5_valid(&state));}
    for(unsigned q=0;q<26;q++)for(unsigned i=0;i<3;i++)for(unsigned b=0;b<(i==2?4u:32u);b++){state=accepted;state.guards[q][i]^=1u<<b;assert(!mt76_dac5_valid(&state));}
    for(unsigned i=0;i<26;i++){state=accepted;state.guards[i][2]=UINT32_MAX;assert(!mt76_dac5_valid(&state));}
    for(unsigned field=0;field<9;field++){
        state=accepted;
        switch(field){case 0:state.reads++;break;case 1:state.exchanges++;break;case 2:state.writes++;break;case 3:state.operations--;break;case 4:state.trim++;break;case 5:state.complete=false;break;case 6:state.words_restored=false;break;case 7:state.dac_reset=false;break;default:state.received=15;}
        assert(!mt76_dac5_valid(&state));
    }
    for(unsigned n=0;n<2;n++){init(&f);if(n)f.dac=0x13;else f.trim=1;assert(run(&f)==MT76_TIMING_REPLY && !state.complete && !f.dac_edges);}
    const int lengths[]={-1,0,1,4,8,15,17,1024};
    for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++){init(&f);f.length=lengths[i];assert(run(&f)==(f.length<0?MT76_TIMING_IO:MT76_TIMING_REPLY) && !state.complete);}
    for(unsigned b=0;b<16;b++)for(unsigned which=0;which<2;which++){
        init(&f);struct mt76_mcu_timing_io io={&f,authorize,read_guard,exchange};
        assert(mt76_dac5_capture(&state,0xfd11^(which?0:1u<<b),0x1004^(which?1u<<b:0),0x44,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);
    }
    for(unsigned t=0;t<256;t++)if(t!=0x44){init(&f);struct mt76_mcu_timing_io io={&f,authorize,read_guard,exchange};assert(mt76_dac5_capture(&state,0xfd11,0x1004,t,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);}
    for(unsigned i=0;i<3;i++){init(&f);struct mt76_mcu_timing_io io={&f,i==0?NULL:authorize,i==1?NULL:read_guard,i==2?NULL:exchange};assert(mt76_dac5_capture(&state,0xfd11,0x1004,0x44,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);}
    puts("dac5: PASS91 transfer failure/corruption,92 cancellation/state-mutation boundaries,exact pinned requests,reply/guard bits,DMA faults,board/trim/refusal,no replay;trim44 during DAC edges,word originals restored;no VCO/RX");return 0;
}
