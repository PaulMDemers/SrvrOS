#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint8_t originals[24]={0,16,16,0,16,16,138,0,0,0,0,0,0,0,0,0,0,0,0,0,18,0,0,0};
static uint8_t banks[24],regs[24],values[24];
struct fake {unsigned calls,auth,fail,deny,bad,corrupt,byte,bit,writes,seed_bad;int size;uint8_t current[24];struct mt76_rf_table *s;};
static struct mt76_rf_table states[840],good;static bool save;
static void init(struct fake *f,struct mt76_rf_table *s){memcpy(f->current,originals,24);if(f->seed_bad)f->current[f->seed_bad-1]^=1;f->s=s;memset(s,0,sizeof(*s));}
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->calls++;if(save)states[op]=*f->s;assert(op%7!=3 && a==mt76_a20_cycle_guard_address(op));unsigned n=op%7,j=n<3?n:n-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->bad==op+1)*v^=1;return f->fail!=op+1;}
static int exchange(void *p,const uint8_t *packet,size_t length,uint8_t *reply,size_t cap){
 struct fake *f=p;unsigned op=f->calls++,q=op/7;if(save)states[op]=*f->s;assert(op%7==3 && length==16 && cap==1024);
 uint32_t w[4]={0};for(unsigned i=0;i<16;i++)w[i/4]|=(uint32_t)packet[i]<<(8*(i%4));unsigned seq=(q+3)%15+1,i=q<72?q%24:q<96?95-q:q-96;
 bool write=(q>=24 && q<48)||(q>=72 && q<96);
 assert(i<24 && w[0]==(0x50000008u|(seq<<16)|((write?12u:10u)<<20)) && w[1]==(0x80000000u|((uint32_t)banks[i]<<16)|regs[i]) && w[3]==0);
 if(write){assert(w[2]==(q<48?values[i]:originals[i]));f->current[i]=(uint8_t)w[2];f->writes++;}else assert(w[2]==0);
 if(f->fail==op+1)return -1;
 w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=f->current[i];memset(reply,0,cap);
 for(unsigned b=0;b<16;b++)reply[b]=(uint8_t)(w[b/4]>>(8*(b%4)));
 if(f->corrupt==op+1)reply[f->byte]^=1u<<f->bit;
 return f->size;
}
static enum mt76_mcu_timing_status run(struct fake *f){const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_a20_cycle_capture(f->s,&io);}
int main(int argc,char **argv){
 assert(argc==2);FILE *b=fopen(argv[1],"rb");assert(b);for(unsigned i=0;i<24;i++){banks[i]=(uint8_t)fgetc(b);regs[i]=(uint8_t)fgetc(b);values[i]=(uint8_t)fgetc(b);assert(mt76_a20_cycle_initial(i)==originals[i]);}assert(fgetc(b)==EOF);fclose(b);
 struct fake f={.size=16};struct mt76_rf_table s;init(&f,&s);save=true;assert(run(&f)==MT76_TIMING_OK && mt76_a20_cycle_valid(&s));save=false;good=s;
 assert(f.calls==840 && f.auth==841 && f.writes==48 && s.reads==720 && s.exchanges==120 && s.acks==48 && s.observed==72 && !memcmp(f.current,originals,24));assert(run(&f)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=840;n++){
  f=(struct fake){.size=16,.fail=n};init(&f,&s);assert(run(&f)==MT76_TIMING_IO && !s.complete && f.calls==n);assert(run(&f)==MT76_TIMING_REFUSED);
  s=states[n-1];assert(mt76_a20_cycle_admit(&s,n-1));s.reads++;assert(!mt76_a20_cycle_admit(&s,n-1));
  if((n-1)%7!=3){f=(struct fake){.size=16,.bad=n};init(&f,&s);assert(run(&f)==MT76_TIMING_STATE && !s.complete);}
 }
 for(unsigned n=1;n<=841;n++){f=(struct fake){.size=16,.deny=n};init(&f,&s);assert(run(&f)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=1;i<=24;i++){f=(struct fake){.size=16,.seed_bad=i};init(&f,&s);assert(run(&f)==MT76_TIMING_REPLY && f.writes==0 && !s.complete);}
 const unsigned commands[]={0,23,24,47,48,71,72,95,96,119};
 for(unsigned c=0;c<sizeof commands/sizeof commands[0];c++)for(unsigned byte=0;byte<16;byte++)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.size=16,.corrupt=commands[c]*7+4,.byte=byte,.bit=bit};init(&f,&s);assert(run(&f)==MT76_TIMING_REPLY && !s.complete);}
 for(unsigned q=0;q<120;q++)for(unsigned j=0;j<4;j++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][j]^=1u<<bit;assert(!mt76_a20_cycle_valid(&s));}
 for(unsigned q=0;q<240;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_a20_cycle_valid(&s));}
 const int lengths[]={0,1,4,8,12,15,17,1024,1025};for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++){f=(struct fake){.size=lengths[i]};init(&f,&s);assert(run(&f)==MT76_TIMING_REPLY && !s.complete);}
 puts("A20 cycle: PASS v111 exact24-original gate,immutable fixture,840 I/O/841 cancel boundaries,independent selected/original reads,reverse restoration,reply/proof mutations,no replay");
}
