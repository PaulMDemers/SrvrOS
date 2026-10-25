#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {unsigned auth,ops,fail,deny,bad,mutate,byte,bit;int received;bool all_different;struct mt76_rf_table *s;};
static struct mt76_rf_table states[MT76_RF_TABLE_OPS];
static bool save_states;
static bool authorize(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void snapshot(struct fake *f){assert(f->ops<MT76_RF_TABLE_OPS);if(save_states)states[f->ops]=*f->s;}
static bool read_reg(void *p,uint32_t address,uint32_t *value){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,step=op%7;assert(step!=3 && address==mt76_rf_table_guard_address(op));
 unsigned n=step<3?step:step-4;*value=n==1?0xf000f200:n==2?0x40000050:0;
 if(f->ops==f->bad)*value^=1;
 return f->ops!=f->fail;
}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t capacity){
 struct fake *f=p;snapshot(f);unsigned op=f->ops++,q=op/7,i=q%44,seq=(8+q)%15+1;assert(op%7==3 && length==16 && capacity==1024);
 uint32_t header=0,address=0,value=0;for(unsigned b=0;b<4;b++){header|=(uint32_t)packet[b]<<(8*b);address|=(uint32_t)packet[4+b]<<(8*b);value|=(uint32_t)packet[8+b]<<(8*b);assert(packet[12+b]==0);}
 assert(header==(8u|(seq<<16)|((q<44?12u:10u)<<20)|0x50000000));assert(address==mt76_rf_central_address(i) && value==(q<44?mt76_rf_central_value(i):0));
 if(f->ops==f->fail)return -1;
 value=mt76_rf_central_value(i);if(q>=44 && (i==5 || f->all_different))value^=1;
 uint32_t words[]={0x4a000008u|(seq<<16),address,value,0x4a000008u|(seq<<16)};memset(reply,0,capacity);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));
 if(f->ops==f->mutate)reply[f->byte]^=1u<<f->bit;
 return f->received;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_rf_table *s){f->s=s;const struct mt76_mcu_timing_io io={f,authorize,read_reg,exchange};return mt76_rf_table_capture(s,&io);}
int main(void){
 struct mt76_rf_table s={0};struct fake f={.received=16};save_states=true;
 assert(run(&f,&s)==MT76_TIMING_OK && s.complete && s.operations==616 && s.reads==528 && s.exchanges==88 && s.acks==44 && s.observed==44 && s.equal==43 && s.mismatch_mask==(1ull<<5));save_states=false;
 assert(f.ops==616 && f.auth==617 && run(&f,&s)==MT76_TIMING_REFUSED);
 assert(mt76_rf_table_sequence(88)==0 && mt76_rf_table_guard_address(616)==UINT32_MAX && !mt76_rf_table_packet(616,NULL,16));
 for(unsigned op=0;op<616;op++){
  bool command=op%7==3;uint8_t packet[16];assert(mt76_rf_table_packet(op,packet,16)==(command?16:0));assert(!mt76_rf_table_packet(op,packet,15));
  f=(struct fake){.received=16,.fail=op+1};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.ops==op+1);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  if(!command){f=(struct fake){.received=16,.bad=op+1};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete && f.ops==op+1);}
  else for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){
   f=(struct fake){.received=16,.mutate=op+1,.byte=byte,.bit=bit};s=(struct mt76_rf_table){0};enum mt76_mcu_timing_status result=run(&f,&s);
   if(op/7>=44 && byte==8)assert(result==MT76_TIMING_OK && s.complete);
   else assert(result==MT76_TIMING_REPLY && !s.complete && f.ops==op+1);
  }
 }
 for(unsigned n=1;n<=617;n++){f=(struct fake){.received=16,.deny=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.ops==n-1);}
 f=(struct fake){.received=16,.all_different=true};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_OK && !s.equal && s.mismatch_mask==((1ull<<44)-1));
 for(unsigned op=0;op<616;op++){
  s=states[op];assert(mt76_rf_table_admit(&s,op));s.equal++;assert(!mt76_rf_table_admit(&s,op));s=states[op];s.mismatch_mask^=1ull<<43;assert(!mt76_rf_table_admit(&s,op));
  s=states[op];s.exchanges++;assert(!mt76_rf_table_admit(&s,op));s=states[op];s.reads++;assert(!mt76_rf_table_admit(&s,op));
  if(s.exchanges){s=states[op];s.words[0][2]^=1;assert(!mt76_rf_table_admit(&s,op));}
  if(op>1){s=states[op];s.guards[0][0]=1;assert(!mt76_rf_table_admit(&s,op));}
 }
 for(int n=0;n<=1025;n++)if(n!=16){f=(struct fake){.received=n};s=(struct mt76_rf_table){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete && f.ops==4);}
 s=states[615];s.guards[175][2]=0x40000050;s.reads++;s.operations++;s.complete=true;
 assert(!mt76_rf_table_zero_profile_valid(&s));s.values[5]=0;s.words[49][2]=0;assert(mt76_rf_table_zero_profile_valid(&s));
 struct mt76_rf_table complete=s;
 for(unsigned i=0;i<176;i++)for(unsigned n=0;n<3;n++){s=complete;s.guards[i][n]^=1;assert(!mt76_rf_table_zero_profile_valid(&s));}
 for(unsigned q=0;q<88;q++)for(unsigned n=0;n<4;n++){s=complete;s.words[q][n]^=1;assert(!mt76_rf_table_zero_profile_valid(&s));}
 s=complete;s.mismatch_hi=1;assert(!mt76_rf_table_zero_profile_valid(&s));s=complete;s.equal--;assert(!mt76_rf_table_zero_profile_valid(&s));
 puts("mt76 RF table: PASS fixed two-pass order, all I/O/cancel/guard boundaries, reply-bit checks, complete mismatch accounting, proof mutation and no replay");
}
