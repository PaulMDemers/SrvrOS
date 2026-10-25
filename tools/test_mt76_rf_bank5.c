#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned auth,ops,fail,deny,bad,mutate,byte,bit;int received;bool all_different,exact;struct mt76_rf_table *s;};
static struct mt76_rf_table states[MT76_RF_BANK5_OPS];
static bool save_states;
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void snapshot(struct fake *f){assert(f->ops<MT76_RF_BANK5_OPS);if(save_states)states[f->ops]=*f->s;}
static bool read_reg(void *p,uint32_t address,uint32_t *value){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,step=op%7;assert(step!=3 && address==mt76_rf_bank5_guard_address(op));
 unsigned n=step<3?step:step-4;*value=n==1?0xf000f200:n==2?0x40000050:0;
 if(f->ops==f->bad)*value^=1;
 return f->ops!=f->fail;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,q=op/7,i=q%68,seq=(96+q)%15+1;assert(op%7==3 && length==16 && capacity==1024);
 uint32_t header=0,address=0,value=0;for(unsigned b=0;b<4;b++){header|=(uint32_t)packet[b]<<(8*b);address|=(uint32_t)packet[4+b]<<(8*b);value|=(uint32_t)packet[8+b]<<(8*b);assert(packet[12+b]==0);}
 assert(header==(8u|(seq<<16)|((q<68?12u:10u)<<20)|0x50000000));assert(address==mt76_rf_bank5_address(i) && value==(q<68?mt76_rf_bank5_value(i):0));
 if(f->ops==f->fail)return -1;
 value=mt76_rf_bank5_value(i);if(q>=68 && (!f->exact && (i==5 || f->all_different)))value^=1;
 uint32_t words[]={0x4a000008u|(seq<<16),address,value,0x4a000008u|(seq<<16)};memset(reply,0,capacity);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));
 if(f->ops==f->mutate)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_table *s){f->s=s;const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};return mt76_rf_bank5_capture(s,&io);}
int main(void){
 struct mt76_rf_table s={0};struct fake f={.received=16};save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==952 && s.reads==816 && s.exchanges==136 && s.acks==68 && s.observed==68 && s.equal==67 && s.mismatch_mask==(1ull<<5));save_states=false;
 assert(f.ops==952 && f.auth==953 && run(&f,&s)==MT76_TIMING_REFUSED);
 assert(mt76_rf_bank5_sequence(136)==0 && mt76_rf_bank5_guard_address(952)==UINT32_MAX && !mt76_rf_bank5_packet(952,NULL,16));
 for(unsigned op=0;op<952;op++){
  bool command=op%7==3;uint8_t packet[16];assert(mt76_rf_bank5_packet(op,packet,16)==(command?16:0));assert(!mt76_rf_bank5_packet(op,packet,15));
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(!command){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_table){0};enum mt76_mcu_timing_status result=run(&f,&s);
   if(op/7>=68 && byte==8)assert(result==MT76_TIMING_OK && s.complete);
   else assert(result==MT76_TIMING_REPLY && !s.complete && f.ops==op+1);
  }
 }
 for(unsigned n=1;n<=953;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==n-1);}
 f=(struct fake){.received=16,.all_different=true};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_OK && !s.equal && s.mismatch_mask==UINT64_MAX && s.mismatch_hi==15);
 for(unsigned op=0;op<952;op++){
  s=states[op];assert(mt76_rf_bank5_admit(&s,op));s.equal++;assert(!mt76_rf_bank5_admit(&s,op));s=states[op];s.mismatch_hi^=8;assert(!mt76_rf_bank5_admit(&s,op));
  s=states[op];s.exchanges++;assert(!mt76_rf_bank5_admit(&s,op));s=states[op];s.reads++;assert(!mt76_rf_bank5_admit(&s,op));
  if(s.exchanges){s=states[op];s.words[0][2]^=1;assert(!mt76_rf_bank5_admit(&s,op));}
  if(op>1){s=states[op];s.guards[0][0]=1;assert(!mt76_rf_bank5_admit(&s,op));}
 }
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==4);}
 f=(struct fake){.received=16,.exact=true};s=(struct mt76_rf_table){0};
 assert(run(&f,&s)==MT76_TIMING_OK && mt76_rf_bank5_exact_profile_valid(&s));
 struct mt76_rf_table good=s;
 for(unsigned g=0;g<272;g++)for(unsigned n=0;n<3;n++){
  s=good;s.guards[g][n]^=1;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 }
 for(unsigned q=0;q<136;q++)for(unsigned n=0;n<4;n++){
  s=good;s.words[q][n]^=1;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 }
 for(unsigned i=0;i<68;i++){s=good;s.values[i]^=1;assert(!mt76_rf_bank5_exact_profile_valid(&s));}
 s=good;s.complete=false;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 s=good;s.operations--;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 s=good;s.mismatch_hi=1;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 s=good;s.equal--;assert(!mt76_rf_bank5_exact_profile_valid(&s));
 assert(!mt76_rf_bank5_exact_profile_valid(NULL));
 puts("mt76 RF bank5: PASS fixed two-pass order, all I/O/cancel/guard boundaries, reply-bit checks, complete mismatch accounting, proof mutation and no replay");
}
