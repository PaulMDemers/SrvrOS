#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <srvros/mt76_channel_vco5.h>
static struct mt76_cvco5 state,accepted;
static uint32_t fixture[11][3];
static bool trace;
struct fake {unsigned calls,auth,fail,deny,bad,mutate,vco_writes,waits,delay_fail,clear_at;bool pending,reassert;int length;uint32_t trim,vco;};
static uint32_t get32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static bool authorize(void *p){
    struct fake *f=p;assert(mt76_cvco5_owned(&state));
    unsigned q=state.operations/7;if(q>=3 && q<=10)assert(f->trim==0x44);
    if(++f->auth==f->mutate)state.trim^=1;
    return f->auth!=f->deny;
}
static bool step(struct fake *f){return ++f->calls!=f->fail;}
static bool read_guard(void *p,uint32_t a,uint32_t *v){
    struct fake *f=p;unsigned n=state.operations%7,j=n<3?n:n-4;
    assert(state.operations==f->calls && mt76_cvco5_transfer_valid(&state,false,a) && !mt76_cvco5_transfer_valid(&state,true,a) && !mt76_cvco5_transfer_valid(&state,false,a^4));
    assert(n!=3 && a==(j==0?0x1004u:j==1?0x2000u:0x208u));
    if(!step(f))return false;
    *v=j==0?0:j==1?0xf000f200:0x40000050;if(f->calls==f->bad)*v^=1;
    if(trace)printf("wifi-vco5-usb: op=%u address=0x%016llx value=0x%016llx ok=1\n",f->calls-1,(unsigned long long)a,(unsigned long long)*v);
    return true;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t cap){
    struct fake *f=p;unsigned q=state.operations/7;uint8_t expected[16];
    assert(state.operations==f->calls && state.operations%7==3 && q<11);
    assert(mt76_cvco5_packet(&state,expected,16)==16 && !memcmp(expected,packet,16) && !mt76_cvco5_packet(&state,expected,15));
    assert(length==16 && cap==1024);
    uint32_t w[4];for(unsigned i=0;i<4;i++)w[i]=get32(packet+4*i);
    unsigned cmd=(w[0]>>20)&15,seq=(w[0]>>16)&15;bool write=fixture[q][2]!=0;
    assert(cmd==(write?12u:10u) && seq==(q+1)%15+1 && (w[0]&0xffff)==8 && w[1]==fixture[q][0] && w[2]==(write?fixture[q][1]:0) && !w[3]);
    if(!step(f))return -1;
    uint32_t *reg=w[1]==0x80000016?&f->trim:&f->vco;
    if(write){if(w[1]==0x80000004){assert(w[2]==0xb0);f->vco_writes++;}*reg=w[2];}
    if(!write && q>=3 && q<=9){if(!f->pending && q>=f->clear_at)*reg=0x30;if(f->reassert && q==9)*reg=0xb0;}
    w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=*reg;
    for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)reply[4*i+b]=(uint8_t)(w[i]>>(8*b));
    if(f->calls==f->bad)reply[0]^=1;
    if(trace)printf("wifi-vco5-command: op=%u write=%u sent=16 received=16 cc=13 residue=1008 h=0x%016llx address=0x%016llx value=0x%016llx t=0x%016llx\n",f->calls-1,(unsigned)write,(unsigned long long)w[0],(unsigned long long)w[1],(unsigned long long)w[2],(unsigned long long)w[3]);
    return f->length;
}
static bool delay(void *p,uint32_t us){
    struct fake *f=p;assert(mt76_cvco5_delay_admit(&state));
    const unsigned expected[6]={10000,40000,50000,100000,300000,500000};assert(f->waits<6 && us==expected[f->waits]);
    ++f->waits;if(trace)printf("wifi-vco5-delay: sample=%u us=%u ok=1\n",f->waits-1,us);
    return f->waits!=f->delay_fail;
}
static enum mt76_mcu_timing_status run(struct fake *f){struct mt76_rf_wait_io io={{f,authorize,read_guard,exchange},delay};return mt76_cvco5_capture(&state,0xfd11,0x1004,0x44,&io);}
static void init(struct fake *f){memset(&state,0,sizeof state);*f=(struct fake){.length=16,.vco=0x30,.trim=0x44,.clear_at=4};}
int main(int argc,char **argv){
    assert(argc==2 || argc==3);trace=argc==3 && !strcmp(argv[2],"--trace");assert(argc==2 || trace);
    FILE *file=fopen(argv[1],"rb");assert(file);uint8_t data[132];assert(fread(data,1,sizeof data,file)==sizeof data && fgetc(file)==EOF);fclose(file);
    for(unsigned q=0;q<11;q++)for(unsigned i=0;i<3;i++)fixture[q][i]=get32(data+q*12+i*4);
    struct fake f;init(&f);assert(run(&f)==MT76_TIMING_OK && mt76_cvco5_valid(&state));accepted=state;
    assert(f.calls==77 && f.auth==84 && f.vco_writes==1 && f.waits==6 && f.trim==0x44 && f.vco==0x30 && state.writes==1 && state.exchanges==11 && state.reads==66 && state.delay_us==1000000);
    if(trace)return 0;
    assert(!mt76_cvco5_owned(&state) && !mt76_cvco5_packet(&state,data,sizeof data) && run(&f)==MT76_TIMING_REFUSED);
    for(unsigned n=1;n<=77;n++){
        init(&f);f.fail=n;assert(run(&f)==MT76_TIMING_IO && f.calls==n && !state.complete);assert(run(&f)==MT76_TIMING_REFUSED && f.calls==n);
        init(&f);f.bad=n;assert(run(&f)!=MT76_TIMING_OK && f.calls==n && !state.complete);
    }
    for(unsigned n=1;n<=84;n++){
        init(&f);f.deny=n;assert(run(&f)==MT76_TIMING_CANCELLED && f.auth==n && f.calls+f.waits==n-1 && !state.complete);
        init(&f);f.mutate=n;assert(run(&f)==MT76_TIMING_STATE && f.calls+f.waits==n-1 && !state.complete);
    }
    for(unsigned q=0;q<11;q++)for(unsigned i=0;i<4;i++)for(unsigned b=0;b<32;b++){state=accepted;state.words[q][i]^=1u<<b;assert(!mt76_cvco5_valid(&state));}
    for(unsigned q=0;q<22;q++)for(unsigned i=0;i<3;i++)for(unsigned b=0;b<(i==2?4u:32u);b++){state=accepted;state.guards[q][i]^=1u<<b;assert(!mt76_cvco5_valid(&state));}
    for(unsigned i=0;i<22;i++){state=accepted;state.guards[i][2]=UINT32_MAX;assert(!mt76_cvco5_valid(&state));}
    for(unsigned field=0;field<9;field++){
        state=accepted;
        switch(field){case 0:state.reads++;break;case 1:state.exchanges++;break;case 2:state.writes++;break;case 3:state.operations--;break;case 4:state.trim++;break;case 5:state.complete=false;break;case 6:state.trim_selected=false;break;case 7:state.triggered=false;break;default:state.received=15;}
        assert(!mt76_cvco5_valid(&state));
    }
    for(unsigned n=0;n<2;n++){init(&f);if(n)f.vco=0xb0;else f.trim=1;assert(run(&f)==MT76_TIMING_REPLY && !state.complete && !f.vco_writes);}
    const int lengths[]={-1,0,1,4,8,15,17,1024};
    for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++){init(&f);f.length=lengths[i];assert(run(&f)==(f.length<0?MT76_TIMING_IO:MT76_TIMING_REPLY) && !state.complete);}
    for(unsigned b=0;b<16;b++)for(unsigned which=0;which<2;which++){
        init(&f);struct mt76_rf_wait_io io={{&f,authorize,read_guard,exchange},delay};
        assert(mt76_cvco5_capture(&state,0xfd11^(which?0:1u<<b),0x1004^(which?1u<<b:0),0x44,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);
    }
    for(unsigned t=0;t<256;t++)if(t!=0x44){init(&f);struct mt76_rf_wait_io io={{&f,authorize,read_guard,exchange},delay};assert(mt76_cvco5_capture(&state,0xfd11,0x1004,t,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);}
    for(unsigned i=0;i<4;i++){init(&f);struct mt76_rf_wait_io io={{&f,i==0?NULL:authorize,i==1?NULL:read_guard,i==2?NULL:exchange},i==3?NULL:delay};assert(mt76_cvco5_capture(&state,0xfd11,0x1004,0x44,&io)==MT76_TIMING_REFUSED && !f.calls && !state.attempted);}
    for(unsigned n=1;n<=6;n++){init(&f);f.delay_fail=n;assert(run(&f)==MT76_TIMING_IO && f.waits==n && f.calls==28+7*(n-1) && !state.complete);}
    for(unsigned q=3;q<=9;q++){init(&f);f.clear_at=q;assert(run(&f)==MT76_TIMING_OK && mt76_cvco5_valid(&state) && state.first_clear==q-2);}
    init(&f);f.pending=true;assert(run(&f)==MT76_TIMING_STATE && f.calls==70 && f.trim==0x44 && f.vco_writes==1 && !state.complete);assert(run(&f)==MT76_TIMING_REFUSED && f.calls==70);
    init(&f);f.reassert=true;assert(run(&f)==MT76_TIMING_STATE && f.calls==67 && f.trim==0x44 && f.vco_writes==1 && !state.complete);
    state=accepted;state.waits++;assert(!mt76_cvco5_valid(&state));state=accepted;state.delay_us++;assert(!mt76_cvco5_valid(&state));state=accepted;state.first_clear++;assert(!mt76_cvco5_valid(&state));
    puts("cvco5: PASS77 transfer failure/corruption,84 cancellation/state-mutation boundaries,exact pinned requests,reply/guard bits,DMA faults,board/trim/refusal,no replay;trim44 during one VCO pulse and bounded observations,trim44 retained,RF0:4 self-cleared;no RF-lock/RX");return 0;
}
