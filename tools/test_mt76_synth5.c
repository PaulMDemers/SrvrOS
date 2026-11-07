#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_synth5 *s;unsigned calls,auth,fail,deny,bad,corrupt_q,corrupt_byte,corrupt_bit,writes,seed_bad;int size;uint8_t regs[256];};
static struct mt76_synth5 states[896],good;static bool save;
static const uint8_t regs[13]={37,36,35,34,33,32,31,30,29,28,26,27,24};
static const uint8_t final[13]={2,63,104,221,210,80,73,2,48,48,0,0,51};
static void init(struct fake *f,struct mt76_synth5 *s){f->s=s;memset(s,0,sizeof(*s));memset(f->regs,0,sizeof(f->regs));f->regs[24]=0x33;if(f->seed_bad)f->regs[regs[f->seed_bad-1]]^=1;}
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static void snap(struct fake *f){if(save)states[f->calls]=*f->s;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;snap(f);unsigned op=f->calls++,n=op%7;assert(n!=3 && a==mt76_synth5_guard_address(op));unsigned j=n<3?n:n-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->bad==op+1)*v^=1;return f->fail!=op+1;}
static int exchange(void *p,const uint8_t *packet,size_t len,uint8_t *reply,size_t cap){
 struct fake *f=p;snap(f);unsigned op=f->calls++,q=op/7;assert(op%7==3 && len==16 && cap==1024);uint32_t h=0,a=0,v=0;
 for(unsigned j=0;j<4;j++){h|=(uint32_t)packet[j]<<(8*j);a|=(uint32_t)packet[4+j]<<(8*j);v|=(uint32_t)packet[8+j]<<(8*j);assert(packet[12+j]==0);}
 bool write=(q>=13 && q<76 && (q-13)%3==1)||(q>=89 && q<115 && (q-89)%2==0);
 unsigned target=q<13?q:q<76?mt76_synth5_step((q-13)/3):q<89?q-76:q<115?12-(q-89)/2:q-115;
 assert(target<13 && a==(0x80000000u|regs[target]));unsigned seq=(q+9)%15+1;
 assert(h==(0x50000008u|(seq<<16)|((write?12u:10u)<<20)));
 if(f->fail==op+1)return -1;
 if(write){if(q<76){unsigned k=(q-13)/3;assert(v==((f->regs[regs[target]]&~mt76_synth5_mask(k))|mt76_synth5_bits(k)));}else assert(v==(target==12?0x33u:0u));f->regs[regs[target]]=(uint8_t)v;f->writes++;}
 else {assert(v==0);v=f->regs[regs[target]];}
 if(q==88)for(unsigned i=0;i<13;i++)assert(f->regs[regs[i]]==final[i]);
 uint32_t words[]={0x4a000008u|(seq<<16),a,v,0x4a000008u|(seq<<16)};memset(reply,0,cap);
 for(unsigned j=0;j<4;j++)for(unsigned b=0;b<4;b++)reply[4*j+b]=(uint8_t)(words[j]>>(8*b));
 if(f->corrupt_q==q+1)reply[f->corrupt_byte]^=1u<<f->corrupt_bit;
 return f->size;
}
static enum mt76_mcu_timing_status run(struct fake *f){const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_synth5_capture(f->s,&io);}
int main(int argc,char **argv){
 assert(argc==2);FILE *fixture=fopen(argv[1],"rb");assert(fixture);
 for(unsigned k=0;k<21;k++){assert(regs[mt76_synth5_step(k)]==fgetc(fixture));assert(mt76_synth5_mask(k)==(unsigned)fgetc(fixture));assert(mt76_synth5_bits(k)==(unsigned)fgetc(fixture));}
 assert(fgetc(fixture)==EOF);fclose(fixture);
 struct mt76_synth5 s={0};struct fake f={.size=16};init(&f,&s);save=true;assert(run(&f)==MT76_TIMING_OK && mt76_synth5_valid(&s));save=false;good=s;
 assert(f.calls==896 && f.auth==897 && f.writes==34 && s.exchanges==128 && s.writes==34 && s.observed==94 && s.reads==768);
 for(unsigned i=0;i<13;i++){assert(f.regs[regs[i]]==(i==12?0x33:0));assert(mt76_synth5_selected(i)==final[i]);}
 assert(run(&f)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=896;n++){
  f=(struct fake){.size=16,.fail=n};init(&f,&s);assert(run(&f)==MT76_TIMING_IO && !s.complete && f.calls==n);assert(run(&f)==MT76_TIMING_REFUSED);
  if((n-1)%7!=3){f=(struct fake){.size=16,.bad=n};init(&f,&s);assert(run(&f)==MT76_TIMING_STATE && !s.complete && f.calls==n);}
  s=states[n-1];assert(mt76_synth5_admit(&s,n-1));s.operations++;assert(!mt76_synth5_admit(&s,n-1));
 }
 for(unsigned n=1;n<=897;n++){f=(struct fake){.size=16,.deny=n};init(&f,&s);assert(run(&f)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=1;i<=13;i++){f=(struct fake){.size=16,.seed_bad=i};init(&f,&s);assert(run(&f)==MT76_TIMING_REPLY && f.writes==0 && !s.complete);}
 const unsigned commands[]={0,12,13,14,15,75,76,88,89,90,114,115,127};
 for(unsigned c=0;c<sizeof commands/sizeof commands[0];c++)for(unsigned b=0;b<16;b++)for(unsigned bit=0;bit<8;bit++){
  f=(struct fake){.size=16,.corrupt_q=commands[c]+1,.corrupt_byte=b,.corrupt_bit=bit};init(&f,&s);assert(run(&f)==MT76_TIMING_REPLY && !s.complete);
 }
 const int lengths[]={-1,0,1,4,8,12,15,17,1024,1025};
 for(unsigned i=0;i<sizeof lengths/sizeof lengths[0];i++){f=(struct fake){.size=lengths[i]};init(&f,&s);assert(run(&f)==(lengths[i]<0?MT76_TIMING_IO:MT76_TIMING_REPLY) && !s.complete);}
 for(unsigned q=0;q<128;q++)for(unsigned j=0;j<4;j++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][j]^=1u<<bit;assert(!mt76_synth5_valid(&s));}
 for(unsigned q=0;q<256;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_synth5_valid(&s));}
 s=good;s.attempted=false;assert(!mt76_synth5_valid(&s));s=good;s.complete=false;assert(!mt76_synth5_valid(&s));s=good;s.writes++;assert(!mt76_synth5_valid(&s));s=good;s.observed++;assert(!mt76_synth5_valid(&s));
 assert(!mt76_synth5_valid(NULL) && mt76_synth5_target(128)==UINT32_MAX && mt76_synth5_address(13)==UINT32_MAX && mt76_synth5_step(21)==UINT32_MAX && mt76_synth5_guard_address(896)==UINT32_MAX && mt76_synth5_sequence(128)==0);
 puts("synth5: PASS channel161 fixture,896 I/O failures,897 cancellations,13-word prewrite gate/independent selection/reverse restoration,reply mutations and no replay;no VCO/RX");
}
