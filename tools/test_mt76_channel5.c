#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
/* One shared register bank: nested tests cannot pass by returning canned words
 * from separate devices, or by restoring a parent before selecting its child. */
struct fake {
    struct mt76_channel5 *s;
    unsigned calls,auth,fail,deny,bad,selected_checks;
    uint8_t rf[8][256];
    uint32_t direct[0x3000/4];
};
static struct mt76_channel5 state,good,held;static bool save_held,trace,requests=true;
static uint32_t guard(unsigned n){return n==0?0:n==1?0xf000f200:0x40000050;}
static uint32_t get(struct fake *f,uint32_t a){assert(!(a&3) && a<0x3000);return f->direct[a/4];}
static void put(struct fake *f,uint32_t a,uint32_t v){assert(!(a&3) && a<0x3000);f->direct[a/4]=v;}
static void selected_bank(struct fake *f,unsigned depth){
    assert(get(f,0x13c8)==5 && get(f,0x13a0)==0x01010102 && f->rf[5][0]==0x44 && f->rf[6][0]==0x45);
    for(unsigned i=0;i<13;i++)assert(f->rf[0][mt76_synth5_address(i)&255]==mt76_synth5_selected(i));
    for(unsigned i=0;i<24;i++){uint32_t a=mt76_rf_a20_address(i);assert(f->rf[(a>>16)&7][a&255]==mt76_rf_a20_value(i));}
    if(depth>=2)for(unsigned i=0;i<14;i++){uint32_t a=mt76_pa5_target(i);assert((a&0x80000000u?f->rf[(a>>16)&7][a&255]:get(f,a))==mt76_pa5_selected(i));}
    if(depth>=3)for(unsigned i=0;i<21;i++)assert(get(f,mt76_bbp5_target(i))==mt76_bbp5_expected(21+i,f->s->lna));
}
static bool auth(void *p){
    struct fake *f=p;
    assert(mt76_channel5_owned(f->s));
    if(f->s->phase==2)selected_bank(f,1);
    if(f->s->phase==3){selected_bank(f,2);if(f->s->bbp.operations==441){selected_bank(f,3);f->selected_checks++;if(save_held)held=*f->s;}}
    return ++f->auth!=f->deny;
}
static bool step(struct fake *f){return ++f->calls!=f->fail;}
static bool read_value(void *p,uint32_t a,uint32_t *v){
    struct fake *f=p;unsigned op=mt76_5ghz_band_operations(&f->s->band),n=op%7;
    assert(f->s->phase==1 && mt76_5ghz_band_read_valid(&f->s->band,op,a));
    if(requests){assert(mt76_channel5_operations(f->s)==f->calls && mt76_channel5_transfer_valid(f->s,false,a,0));
    assert(!mt76_channel5_transfer_valid(f->s,true,a,0) && !mt76_channel5_transfer_valid(f->s,false,a^4,0));}
    if(!step(f))return false;
    *v=n==3?get(f,a):guard(n<3?n:n-4);
    if(f->calls==f->bad)*v^=1;
    if(trace)printf("wifi-held-usb: op=%u phase=%u local=%u write=0 address=0x%016llx value=0x%016llx ok=1\n",f->calls-1,mt76_channel5_phase(f->s),mt76_channel5_local(f->s),(unsigned long long)a,(unsigned long long)*v);
    return true;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
    struct fake *f=p;uint8_t expected[16];size_t n;
    if(f->s->phase==1)n=mt76_5ghz_band_packet(&f->s->band,mt76_5ghz_band_operations(&f->s->band),expected,16);
    else{assert(f->s->phase==2);n=mt76_pa5_packet(&f->s->pa,f->s->pa.operations,expected,16);}
    uint8_t native[16];if(requests){assert(mt76_channel5_operations(f->s)==f->calls && mt76_channel5_packet(f->s,native,16)==16 && !memcmp(native,expected,16));
    assert(!mt76_channel5_packet(f->s,native,15));}
    assert(n==16 && length==16 && capacity==1024 && !memcmp(expected,packet,16));
    if(!step(f))return -1;
    uint32_t w[4]={0};for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)w[j]|=(uint32_t)packet[4*j+b]<<(8*b);
    unsigned command=(w[0]>>20)&15,seq=(w[0]>>16)&15,bank=(w[1]>>16)&7,reg=w[1]&255;
    assert((w[1]&0xfff80000u)==0x80000000u && w[3]==0 && (command==10 || command==12));
    if(command==12)f->rf[bank][reg]=(uint8_t)w[2];else assert(!w[2]);
    w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=f->rf[bank][reg];memset(reply,0,capacity);
    for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(w[j]>>(8*b));
    if(f->calls==f->bad)reply[0]^=1;
    if(trace)printf("wifi-held-command: op=%u phase=%u local=%u write=%u sent=16 received=16 cc=13 residue=1008 h=0x%016llx address=0x%016llx value=0x%016llx t=0x%016llx\n",f->calls-1,mt76_channel5_phase(f->s),mt76_channel5_local(f->s),(unsigned)(command==12),(unsigned long long)w[0],(unsigned long long)w[1],(unsigned long long)w[2],(unsigned long long)w[3]);
    return 16;
}
static bool mac_transfer(void *p,unsigned op,bool wr,uint32_t a,uint8_t *data,unsigned length){
    struct fake *f=p;assert(f->s->phase==1 && mt76_5ghz_band_transfer_valid(&f->s->band,op,wr,a,data,length));
    uint32_t proposed=0;for(unsigned b=0;b<4;b++)proposed|=(uint32_t)data[b]<<(8*b);
    if(requests){assert(mt76_channel5_operations(f->s)==f->calls && mt76_channel5_transfer_valid(f->s,wr,a,proposed));
    assert(!mt76_channel5_transfer_valid(f->s,!wr,a,proposed) && !mt76_channel5_transfer_valid(f->s,wr,a^4,proposed));
    if(wr)assert(!mt76_channel5_transfer_valid(f->s,wr,a,proposed^1));}
    if(!step(f))return false;
    unsigned n=op%9;uint32_t v=0;
    if(n<3 || n>=6)v=guard(n<3?n:n-6);
    else if(wr){for(unsigned b=0;b<4;b++)v|=(uint32_t)data[b]<<(8*b);put(f,a,v);}else v=get(f,a);
    for(unsigned b=0;b<4;b++)data[b]=(uint8_t)(v>>(8*b));
    if(f->calls==f->bad)data[0]^=1;
    if(trace)printf("wifi-held-usb: op=%u phase=%u local=%u write=%u address=0x%016llx value=0x%016llx ok=1\n",f->calls-1,mt76_channel5_phase(f->s),op,(unsigned)wr,(unsigned long long)a,(unsigned long long)v);
    return true;
}
static bool direct_transfer(void *p,bool wr,uint32_t a,uint32_t *v){
    struct fake *f=p;unsigned op;
    if(f->s->phase==2){op=f->s->pa.operations;assert(mt76_pa5_transfer_valid(&f->s->pa,op,wr,a,*v));}
    else{assert(f->s->phase==3);op=f->s->bbp.operations;assert(mt76_bbp5_transfer_valid(&f->s->bbp,op,wr,a,*v));}
    if(requests){assert(mt76_channel5_operations(f->s)==f->calls && mt76_channel5_transfer_valid(f->s,wr,a,*v));
    assert(!mt76_channel5_transfer_valid(f->s,!wr,a,*v) && !mt76_channel5_transfer_valid(f->s,wr,a^4,*v));
    if(wr)assert(!mt76_channel5_transfer_valid(f->s,wr,a,*v^1));}
    if(!step(f))return false;
    unsigned n=op%7;
    if(n!=3)*v=guard(n<3?n:n-4);else if(wr)put(f,a,*v);else *v=get(f,a);
    if(f->calls==f->bad)*v^=1;
    if(trace)printf("wifi-held-usb: op=%u phase=%u local=%u write=%u address=0x%016llx value=0x%016llx ok=1\n",f->calls-1,mt76_channel5_phase(f->s),op,(unsigned)wr,(unsigned long long)a,(unsigned long long)*v);
    return true;
}
static void init(struct fake *f){
    unsigned fail=f->fail,deny=f->deny,bad=f->bad;memset(f,0,sizeof(*f));f->fail=fail;f->deny=deny;f->bad=bad;f->s=&state;memset(&state,0,sizeof state);
    for(unsigned i=0;i<24;i++){uint32_t a=mt76_rf_a20_address(i);f->rf[(a>>16)&7][a&255]=(uint8_t)mt76_a20_cycle_initial(i);}
    for(unsigned i=0;i<13;i++)f->rf[0][mt76_synth5_address(i)&255]=(uint8_t)mt76_synth5_initial(i);
    for(unsigned i=0;i<38;i++)f->rf[6][mt76_rf_bank6_address(i)&255]=(uint8_t)mt76_rf_bank6_value(i);
    for(unsigned i=0;i<14;i++){uint32_t a=mt76_pa5_target(i);if(a&0x80000000u)f->rf[(a>>16)&7][a&255]=(uint8_t)mt76_pa5_initial(i);else put(f,a,mt76_pa5_initial(i));}
    for(unsigned i=0;i<21;i++)put(f,mt76_bbp5_target(i),mt76_bbp5_initial(i));
}
static enum mt76_mcu_timing_status run(struct fake *f,unsigned lna){
    const struct mt76_mcu_timing_io rf={f,auth,read_value,exchange};
    const struct mt76_band_mac_io mac={f,auth,mac_transfer};
    const struct mt76_pa5_io pa={f,auth,direct_transfer,exchange};
    const struct mt76_bbp5_io bbp={f,auth,direct_transfer};
    return mt76_channel5_capture(f->s,0xfd11,0x1004,lna,&rf,&mac,&pa,&bbp);
}
static bool different_auth(void *p){(void)p;return true;}
static void refusals(struct fake *f){
    struct mt76_mcu_timing_io rf={f,auth,read_value,exchange};
    struct mt76_band_mac_io mac={f,auth,mac_transfer};
    struct mt76_pa5_io pa={f,auth,direct_transfer,exchange};
    struct mt76_bbp5_io bbp={f,auth,direct_transfer};
#define REFUSED(n0,n1,l) assert(mt76_channel5_capture(f->s,n0,n1,l,&rf,&mac,&pa,&bbp)==MT76_TIMING_REFUSED && !state.attempted && !f->calls && !f->auth)
    REFUSED(0xfd10,0x1004,12);REFUSED(0xfd11,0x1005,12);REFUSED(0xfd11,0x1004,256);
    for(unsigned i=0;i<3;i++){
        if(i==0)mac.context=NULL;else if(i==1)pa.context=NULL;else bbp.context=NULL;
        REFUSED(0xfd11,0x1004,12);mac.context=pa.context=bbp.context=f;
        if(i==0)mac.authorize=different_auth;else if(i==1)pa.authorize=different_auth;else bbp.authorize=different_auth;
        REFUSED(0xfd11,0x1004,12);mac.authorize=pa.authorize=bbp.authorize=auth;
    }
    rf.read=NULL;REFUSED(0xfd11,0x1004,12);rf.read=read_value;
    rf.exchange=NULL;REFUSED(0xfd11,0x1004,12);rf.exchange=exchange;
    rf.authorize=NULL;REFUSED(0xfd11,0x1004,12);rf.authorize=auth;
    mac.transfer=NULL;REFUSED(0xfd11,0x1004,12);mac.transfer=mac_transfer;
    pa.transfer=NULL;REFUSED(0xfd11,0x1004,12);pa.transfer=direct_transfer;
    pa.exchange=NULL;REFUSED(0xfd11,0x1004,12);pa.exchange=exchange;
    bbp.transfer=NULL;REFUSED(0xfd11,0x1004,12);bbp.transfer=direct_transfer;
#undef REFUSED
}
int main(int argc,char **argv){
    trace=argc==2 && !strcmp(argv[1],"--trace");bool quick=trace || (argc==2 && !strcmp(argv[1],"--quick"));assert(argc==1 || quick);
    if(trace)puts("wifi-held-progress: select band/frequency/A20/PA/BBP together;independent reads;reverse restoration;no VCO/RX/network OUT");
    struct fake f={0};init(&f);refusals(&f);save_held=true;assert(run(&f,12)==MT76_TIMING_OK && mt76_channel5_valid(&state));good=state;save_held=false;
    if(trace)puts("wifi-held: status=0 complete=1 valid=1 selected=1 restored=1 operations=3732 submitted=3732 lna=12 parent=1 network_out=0");
    if(trace)return 0;
    requests=false;
    unsigned calls=f.calls,auths=f.auth;assert(mt76_channel5_operations(&state)==3732 && mt76_channel5_phase(&state)==12 && !mt76_channel5_transfer_valid(&state,false,0x1004,0));assert(calls==3732 && auths==3746 && f.selected_checks==2);
    assert(!mt76_channel5_owned(&state) && run(&f,12)==MT76_TIMING_REFUSED);
    for(unsigned i=0;i<14;i++){uint32_t a=mt76_pa5_target(i);assert((a&0x80000000u?f.rf[(a>>16)&7][a&255]:get(&f,a))==mt76_pa5_initial(i));}
    for(unsigned i=0;i<21;i++)assert(get(&f,mt76_bbp5_target(i))==mt76_bbp5_initial(i));
    assert(!get(&f,0x13c8) && !get(&f,0x13a0) && !f.rf[5][0] && !f.rf[6][0]);
    for(unsigned i=0;i<13;i++)assert(f.rf[0][mt76_synth5_address(i)&255]==mt76_synth5_initial(i));
    for(unsigned i=0;i<24;i++){uint32_t a=mt76_rf_a20_address(i);assert(f.rf[(a>>16)&7][a&255]==mt76_a20_cycle_initial(i));}
    assert(mt76_channel5_owned(&held) && held.selected);
    for(unsigned i=0;i<9;i++){
        state=held;
        switch(i){case 0:state.band.synth.operations--;break;case 1:state.band.a20.operations--;break;case 2:state.pa.operations--;break;case 3:state.bbp.operations--;break;case 4:state.band.synth.paused=false;break;case 5:state.band.selected.values[0]^=1;break;case 6:state.pa.words[41][2]^=1;break;case 7:state.lna++;break;case 8:state.band.channel161=false;break;}
        assert(!mt76_channel5_owned(&state));
    }
    if(!quick)for(unsigned n=1;n<=calls;n++){f=(struct fake){.fail=n};init(&f);assert(run(&f,12)==MT76_TIMING_IO && !state.complete && !mt76_channel5_valid(&state) && f.calls==n);assert(run(&f,12)==MT76_TIMING_REFUSED);}
    if(!quick)for(unsigned n=1;n<=auths;n++){f=(struct fake){.deny=n};init(&f);assert(run(&f,12)==MT76_TIMING_CANCELLED && !state.complete && f.auth==n);}
    for(unsigned lna=0;lna<256;lna++){f=(struct fake){0};init(&f);assert(run(&f,lna)==MT76_TIMING_OK && mt76_channel5_valid(&state));}
    f=(struct fake){0};init(&f);assert(run(&f,256)==MT76_TIMING_REFUSED && !state.attempted && !f.calls);
    state=good;state.selected=false;assert(!mt76_channel5_valid(&state));state=good;state.band.a20.complete=false;assert(!mt76_channel5_valid(&state));state=good;state.pa.restored=false;assert(!mt76_channel5_valid(&state));state=good;state.bbp.restored=false;assert(!mt76_channel5_valid(&state));state=good;state.lna++;assert(!mt76_channel5_valid(&state));
    puts(quick?"channel161 held engine: PASS quick shared-bank/proof/preflight/256-gain checks;fault sweep not requested":"channel161 held engine: PASS3732 I/O/3746 cancellation boundaries,256 gains,shared bank selected PA/BBP under band/frequency/A20,reverse originals,no replay;native owner separately gated;no VCO/RX");
    return 0;
}
