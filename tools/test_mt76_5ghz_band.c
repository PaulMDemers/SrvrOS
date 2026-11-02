#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_5ghz_band *s;unsigned calls,auth,fail,deny,writes,seed_bad;uint8_t rf[8][256];uint32_t mac[5];};
static struct mt76_5ghz_band state,good;static bool channel;
static const uint32_t targets[7]={0x13c8,0x13a0,0x518,0x13a8,0x13b4,0x80050000,0x80060000};
static void init(struct fake *f){
 unsigned fail=f->fail,deny=f->deny,bad=f->seed_bad;memset(f,0,sizeof(*f));f->fail=fail;f->deny=deny;f->seed_bad=bad;
 f->rf[0][24]=0x33;f->mac[2]=1;f->mac[3]=0x6c6c6c6c;f->mac[4]=0xc9540000;
 for(unsigned i=0;i<38;i++)f->rf[6][mt76_rf_bank6_address(i)&255]=(uint8_t)mt76_rf_bank6_value(i);
 if(bad && bad<=5)f->mac[bad-1]^=1;
 if(bad>5)f->rf[bad-1][0]=1;
 memset(&state,0,sizeof(state));f->s=&state;
}
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny && mt76_5ghz_band_owned(f->s);}
static uint32_t guard(unsigned n){return n==0?0:n==1?0xf000f200:0x40000050;}
static bool read_value(void *p,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=mt76_5ghz_band_operations(f->s),n=op%7;assert(mt76_5ghz_band_read_valid(f->s,op,a));
 if(n==3){unsigned i=op/7;assert(i<5 && a==targets[i]);*v=f->mac[i];}
 else {unsigned j=n<3?n:n-4;assert(a==(j==0?0x1004u:j==1?0x2000u:0x208u));*v=guard(j);}
 return ++f->calls!=f->fail;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;unsigned op=mt76_5ghz_band_operations(f->s);uint8_t expected[16];assert(length==16 && capacity==1024 && mt76_5ghz_band_packet(f->s,op,expected,16)==16 && !memcmp(expected,packet,16));
 uint32_t w[4]={0};for(unsigned n=0;n<4;n++)for(unsigned b=0;b<4;b++)w[n]|=(uint32_t)packet[4*n+b]<<(8*b);
 unsigned bank=(w[1]>>16)&7,reg=w[1]&255,command=(w[0]>>20)&15,seq=(w[0]>>16)&15;
 assert((w[1]&0xfff80000u)==0x80000000u && w[3]==0 && (command==10 || command==12));
 if(command==12){assert(f->s->phase==2 || f->s->phase==6 || f->s->phase==8);f->rf[bank][reg]=(uint8_t)w[2];f->writes++;}
 else assert(w[2]==0);
 if(++f->calls==f->fail)return -1;
 w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=f->rf[bank][reg];memset(reply,0,capacity);
 for(unsigned n=0;n<4;n++)for(unsigned b=0;b<4;b++)reply[4*n+b]=(uint8_t)(w[n]>>(8*b));
 return 16;
}
static bool transfer(void *p,unsigned op,bool write,uint32_t a,uint8_t *data,unsigned length){
 struct fake *f=p;assert(mt76_5ghz_band_transfer_valid(f->s,op,write,a,data,length));unsigned n=op%9;
 if(n<3 || n>=6){unsigned j=n<3?n:n-6;uint32_t v=guard(j);for(unsigned b=0;b<4;b++)data[b]=(uint8_t)(v>>(8*b));}
 else {
  unsigned i=a==0x13c8?0:a==0x13a0?1:9;assert(i<2);
  assert(a==(f->s->phase==3?(op/9?0x13a0u:0x13c8u):(op/9?0x13c8u:0x13a0u)));
  if(write){uint32_t v=0;for(unsigned b=0;b<4;b++)v|=(uint32_t)data[b]<<(8*b);assert(v==(f->s->phase==3?(i?0x01010102u:5u):0));f->mac[i]=v;f->writes++;}
  else for(unsigned b=0;b<4;b++)data[b]=(uint8_t)(f->mac[i]>>(8*b));
 }
 return ++f->calls!=f->fail;
}
static enum mt76_mcu_timing_status run(struct fake *f){const struct mt76_mcu_timing_io rf={f,auth,read_value,exchange};const struct mt76_band_mac_io mac={f,auth,transfer};return channel?mt76_5ghz_channel_capture(f->s,&rf,&mac):mt76_5ghz_band_capture(f->s,&rf,&mac);}
int main(void){
 assert(mt76_rf_restore_address(0)==0x80060000u && mt76_rf_restore_address(1)==0x80050000u && mt76_rf_restore_address(2)==UINT32_MAX);
 assert(mt76_rf_restore_sequence(0)==6 && mt76_rf_restore_sequence(3)==9 && mt76_rf_restore_sequence(4)==0 && mt76_rf_restore_value(0)==0);
 struct fake f={0};init(&f);assert(run(&f)==MT76_TIMING_OK && mt76_5ghz_band_valid(&state));good=state;
 assert(f.calls==771 && f.auth==779 && f.writes==46 && f.mac[0]==0 && f.mac[1]==0 && f.rf[5][0]==0 && f.rf[6][0]==0);
 assert(run(&f)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=771;n++){f=(struct fake){.fail=n};init(&f);assert(run(&f)==MT76_TIMING_IO && !state.complete && f.calls==n);assert(run(&f)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=779;n++){f=(struct fake){.deny=n};init(&f);assert(run(&f)==MT76_TIMING_CANCELLED && !state.complete && f.auth==n);}
 for(unsigned n=1;n<=7;n++){f=(struct fake){.seed_bad=n};init(&f);assert(run(&f)==MT76_TIMING_STATE && !state.complete && f.writes==0 && f.calls==49);}
 for(unsigned i=0;i<7;i++){
  state=good;state.before.values[i]^=1;assert(!mt76_5ghz_band_valid(&state));
  state=good;state.selected.values[i]^=1;assert(!mt76_5ghz_band_valid(&state));
  state=good;state.original.values[i]^=1;assert(!mt76_5ghz_band_valid(&state));
 }
 state=good;state.rf_restore.values[0]=1;assert(!mt76_5ghz_band_valid(&state));state=good;state.mac_restore.before[0]^=1;assert(!mt76_5ghz_band_valid(&state));state=good;state.restored=false;assert(!mt76_5ghz_band_valid(&state));
 state=good;state.attempted=false;assert(!mt76_5ghz_band_valid(&state));
 assert(!mt76_5ghz_band_valid(NULL));
 channel=true;f=(struct fake){0};init(&f);assert(run(&f)==MT76_TIMING_OK && state.channel161 && mt76_5ghz_band_valid(&state) && mt76_synth5_valid(&state.synth));good=state;
 assert(f.calls==1667 && f.auth==1676 && f.writes==80 && f.mac[0]==0 && f.mac[1]==0 && f.rf[5][0]==0 && f.rf[6][0]==0);
 for(unsigned i=0;i<13;i++)assert(f.rf[0][mt76_synth5_address(i)&255]==mt76_synth5_initial(i));
 for(unsigned n=1;n<=1667;n++){f=(struct fake){.fail=n};init(&f);assert(run(&f)==MT76_TIMING_IO && !state.complete && f.calls==n);assert(run(&f)==MT76_TIMING_REFUSED);}
 for(unsigned n=1;n<=1676;n++){f=(struct fake){.deny=n};init(&f);assert(run(&f)==MT76_TIMING_CANCELLED && !state.complete && f.auth==n);}
 state=good;state.synth.complete=false;assert(!mt76_5ghz_band_valid(&state));state=good;state.synth.words[127][2]^=1;assert(!mt76_5ghz_band_valid(&state));
 puts("5GHz integrated channel161: PASS1667 I/O and1676 authorization failures,band selection around synth,all17 touched words restored,no replay");
 puts("5GHz select/restore: PASS captured gate,771 I/O failures,779 cancellation boundaries, exact forward/readback/reverse order, final four-word restoration, proof mutations and no replay");
}
