/* Reuse the existing independent shared RF/direct-register model. */
#define main channel5_test_main
#include "test_mt76_channel5.c"
#undef main
#include <srvros/mt76_trim5.h>
static struct mt76_trim5 outer,accepted;
static bool combined,check_requests;static int trim_reply_length=16;
static bool trim_auth(void *p){
    struct fake *f=p;assert(mt76_trim5_owned(&outer));
    if(outer.phase==2){
        assert(f->rf[0][22]==0x44);
        if(f->s->phase==2)selected_bank(f,1);
        if(f->s->phase==3){selected_bank(f,2);if(f->s->bbp.operations==441){selected_bank(f,3);f->selected_checks++;}}
    }
    return ++f->auth!=f->deny;
}
static bool trim_read(void *p,uint32_t a,uint32_t *v){
    struct fake *f=p;if(check_requests){assert(mt76_trim5_operations(&outer)==f->calls);assert(mt76_trim5_transfer_valid(&outer,false,a,0) && !mt76_trim5_transfer_valid(&outer,true,a,0) && !mt76_trim5_transfer_valid(&outer,false,a^4,0));}
    if(outer.phase==2)return read_value(p,a,v);
    unsigned n=outer.operations%7,j=n<3?n:n-4;assert(n!=3 && a==(j==0?0x1004u:j==1?0x2000u:0x208u));
    if(!step(f))return false;
    *v=guard(j);if(f->calls==f->bad)*v^=1;return true;
}
static int trim_exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t cap){
    struct fake *f=p;uint8_t want[16];
    if(check_requests){assert(mt76_trim5_operations(&outer)==f->calls);assert(mt76_trim5_packet(&outer,want,16)==16 && !memcmp(want,packet,16));assert(!mt76_trim5_packet(&outer,want,15));}
    if(outer.phase==2)return exchange(p,packet,length,reply,cap);
    assert(length==16 && cap==1024 && mt76_trim5_packet(&outer,want,16)==16 && !memcmp(want,packet,16));
    if(!step(f))return -1;
    uint32_t w[4]={0};for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)w[i]|=(uint32_t)packet[4*i+b]<<(8*b);
    unsigned cmd=(w[0]>>20)&15,seq=(w[0]>>16)&15;unsigned q=outer.operations/7;const unsigned seqs[5]={12,13,14,15,1};assert(cmd==((q==1 || q==3)?12u:10u) && seq==seqs[q] && w[2]==(q==1?0x44u:0u));assert(w[1]==0x80000016u && !w[3] && (cmd==10 || cmd==12));
    if(cmd==12)f->rf[0][22]=(uint8_t)w[2];else assert(!w[2]);
    w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=f->rf[0][22];memset(reply,0,cap);
    for(unsigned i=0;i<4;i++)for(unsigned b=0;b<4;b++)reply[4*i+b]=(uint8_t)(w[i]>>(8*b));
    if(f->calls==f->bad)reply[0]^=1;
    return trim_reply_length;
}
static bool trim_mac(void *p,unsigned op,bool wr,uint32_t a,uint8_t *data,unsigned n){
    struct fake *f=p;assert(outer.phase==2);
    if(check_requests){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)data[b]<<(8*b);assert(mt76_trim5_operations(&outer)==f->calls && mt76_trim5_transfer_valid(&outer,wr,a,v));assert(!mt76_trim5_transfer_valid(&outer,!wr,a,v) && !mt76_trim5_transfer_valid(&outer,wr,a^4,v));if(wr)assert(!mt76_trim5_transfer_valid(&outer,wr,a,v^1));}
    return mac_transfer(p,op,wr,a,data,n);
}
static bool trim_direct(void *p,bool wr,uint32_t a,uint32_t *v){
    struct fake *f=p;assert(outer.phase==2);
    if(check_requests){assert(mt76_trim5_operations(&outer)==f->calls && mt76_trim5_transfer_valid(&outer,wr,a,*v));assert(!mt76_trim5_transfer_valid(&outer,!wr,a,*v) && !mt76_trim5_transfer_valid(&outer,wr,a^4,*v));if(wr)assert(!mt76_trim5_transfer_valid(&outer,wr,a,*v^1));}
    return direct_transfer(p,wr,a,v);
}
static void trim_init(struct fake *f){init(f);memset(&outer,0,sizeof outer);f->s=&outer.channel;requests=trace=save_held=false;}
static enum mt76_mcu_timing_status trim_run(struct fake *f,unsigned trim,unsigned lna){
    const struct mt76_mcu_timing_io rf={f,trim_auth,trim_read,trim_exchange};
    const struct mt76_band_mac_io mac={f,trim_auth,trim_mac};
    const struct mt76_pa5_io pa={f,trim_auth,trim_direct,trim_exchange};
    const struct mt76_bbp5_io bbp={f,trim_auth,trim_direct};
    return combined?mt76_trim5_channel_capture(&outer,0xfd11,0x1004,trim,lna,&rf,&mac,&pa,&bbp):mt76_trim5_capture(&outer,0xfd11,0x1004,trim,&rf);
}
static void trim_preflight(struct fake *f){
    struct mt76_mcu_timing_io rf={f,trim_auth,trim_read,trim_exchange};
    struct mt76_band_mac_io mac={f,trim_auth,trim_mac};
    struct mt76_pa5_io pa={f,trim_auth,trim_direct,trim_exchange};
    struct mt76_bbp5_io bbp={f,trim_auth,trim_direct};
#define NO_IO(call) assert((call)==MT76_TIMING_REFUSED && !outer.attempted && !f->calls && !f->auth)
    for(unsigned b=0;b<16;b++){
        NO_IO(mt76_trim5_capture(&outer,0xfd11^(1u<<b),0x1004,0x44,&rf));
        NO_IO(mt76_trim5_capture(&outer,0xfd11,0x1004^(1u<<b),0x44,&rf));
    }
    NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,256,&rf,&mac,&pa,&bbp));
    for(unsigned i=0;i<3;i++){
        if(i==0)mac.context=NULL;else if(i==1)pa.context=NULL;else bbp.context=NULL;
        NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));mac.context=pa.context=bbp.context=f;
        if(i==0)mac.authorize=NULL;else if(i==1)pa.authorize=NULL;else bbp.authorize=NULL;
        NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));mac.authorize=pa.authorize=bbp.authorize=trim_auth;
    }
    mac.transfer=NULL;NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));mac.transfer=trim_mac;
    pa.transfer=NULL;NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));pa.transfer=trim_direct;
    pa.exchange=NULL;NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));pa.exchange=trim_exchange;
    bbp.transfer=NULL;NO_IO(mt76_trim5_channel_capture(&outer,0xfd11,0x1004,0x44,12,&rf,&mac,&pa,&bbp));bbp.transfer=trim_direct;
    rf.authorize=NULL;NO_IO(mt76_trim5_capture(&outer,0xfd11,0x1004,0x44,&rf));rf.authorize=trim_auth;
    rf.read=NULL;NO_IO(mt76_trim5_capture(&outer,0xfd11,0x1004,0x44,&rf));rf.read=trim_read;
    rf.exchange=NULL;NO_IO(mt76_trim5_capture(&outer,0xfd11,0x1004,0x44,&rf));
#undef NO_IO
}
int main(int argc,char **argv){
    bool quick=argc==2 && !strcmp(argv[1],"--quick");assert(argc==1 || quick);
    struct fake f={0};trim_init(&f);trim_preflight(&f);check_requests=true;assert(trim_run(&f,0x44,12)==MT76_TIMING_OK && mt76_trim5_valid(&outer));accepted=outer;check_requests=false;
    assert(f.calls==35 && f.auth==36 && outer.writes==2 && outer.exchanges==5 && !f.rf[0][22]);
    assert(mt76_trim5_operations(&outer)==35 && mt76_trim5_phase(&outer)==14 && !mt76_trim5_owned(&outer));assert(trim_run(&f,0x44,12)==MT76_TIMING_REFUSED);
    for(unsigned n=1;n<=35;n++){f=(struct fake){.fail=n};trim_init(&f);assert(trim_run(&f,0x44,12)==MT76_TIMING_IO && !outer.complete && f.calls==n);assert(trim_run(&f,0x44,12)==MT76_TIMING_REFUSED);
        f=(struct fake){.bad=n};trim_init(&f);assert(trim_run(&f,0x44,12)!=MT76_TIMING_OK && !outer.complete);}
    for(unsigned n=1;n<=36;n++){f=(struct fake){.deny=n};trim_init(&f);assert(trim_run(&f,0x44,12)==MT76_TIMING_CANCELLED && !outer.complete && f.calls==n-1);}
    for(unsigned q=0;q<5;q++)for(unsigned j=0;j<4;j++)for(unsigned b=0;b<32;b++){outer=accepted;outer.words[q][j]^=1u<<b;assert(!mt76_trim5_valid(&outer));}
    for(unsigned q=0;q<10;q++)for(unsigned j=0;j<3;j++)for(unsigned b=0;b<(j==2?4u:32u);b++){outer=accepted;outer.guards[q][j]^=1u<<b;assert(!mt76_trim5_valid(&outer));}
    f=(struct fake){0};trim_init(&f);f.rf[0][22]=1;assert(trim_run(&f,0x44,12)==MT76_TIMING_REPLY && outer.writes==0 && !outer.complete);
    for(unsigned t=0;t<256;t++)if(t!=0x44){f=(struct fake){0};trim_init(&f);assert(trim_run(&f,t,12)==MT76_TIMING_REFUSED && !f.calls && !outer.attempted);}
    const int lengths[]={-1,0,1,4,8,12,15,17,1024,1025};
    for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++){
        f=(struct fake){0};trim_init(&f);trim_reply_length=lengths[i];assert(trim_run(&f,0x44,12)==(lengths[i]<0?MT76_TIMING_IO:MT76_TIMING_REPLY) && !outer.complete);
    }
    trim_reply_length=16;
    combined=true;f=(struct fake){0};trim_init(&f);check_requests=true;assert(trim_run(&f,0x44,12)==MT76_TIMING_OK && mt76_trim5_valid(&outer) && mt76_channel5_valid(&outer.channel));accepted=outer;check_requests=false;
    assert(f.calls==3767 && f.auth==3782 && f.selected_checks==2 && !f.rf[0][22] && mt76_trim5_operations(&outer)==3767);
    if(!quick){
        for(unsigned n=1;n<=3767;n++){f=(struct fake){.fail=n};trim_init(&f);assert(trim_run(&f,0x44,12)==MT76_TIMING_IO && !outer.complete && f.calls==n);assert(trim_run(&f,0x44,12)==MT76_TIMING_REFUSED);}
        for(unsigned n=1;n<=3782;n++){f=(struct fake){.deny=n};trim_init(&f);assert(trim_run(&f,0x44,12)==MT76_TIMING_CANCELLED && !outer.complete && f.auth==n);}
    }
    for(unsigned lna=0;lna<256;lna++){f=(struct fake){0};trim_init(&f);assert(trim_run(&f,0x44,lna)==MT76_TIMING_OK && mt76_trim5_valid(&outer));}
    outer=accepted;outer.channel.selected=false;assert(!mt76_trim5_valid(&outer));outer=accepted;outer.with_channel=false;assert(!mt76_trim5_valid(&outer));outer=accepted;outer.lna++;assert(!mt76_trim5_valid(&outer));outer=accepted;outer.restored=false;assert(!mt76_trim5_valid(&outer));
    puts(quick?"trim5: PASS quick standalone fault/proof and combined shared-bank/request/gain checks;combined fault sweep not requested":"trim5: PASS35 isolated I/O/36 cancellation,3767 combined I/O/3782 cancellation boundaries,256 gains,reply/MAC/core proof bits and DMA faults,shared trim44 around joint channel state,reverse originals,no replay;host-only/no pulses");
}
