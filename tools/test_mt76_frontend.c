#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_5ghz_prereq *s;unsigned calls,auth,fail,deny,bad,max_slot,corrupt,byte,bit;int size;};
static struct mt76_5ghz_prereq states[308],good;static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static uint32_t value(unsigned i){return i>=30?(i*7)&255:0x10000000u+i*0x1001;}
static bool raw(unsigned i){return i==20 || i==21 || i==22 || i==28;}
static bool read_reg(void *p,uint32_t a,uint32_t *v){struct fake *f=p;unsigned op=f->calls++,n=op%7,i=op/7;if(save)states[op]=*f->s;assert(!mt76_frontend_rf(op) && a==mt76_frontend_address(op));if(n==3){assert(i<30);*v=f->max_slot==i+1?UINT32_MAX:value(i);}else{unsigned j=n<3?n:n-4;*v=j==1?0xf000f200:j==2?0x40000050:0;if(f->bad==op+1)*v^=1;}return f->fail!=op+1;}
static int exchange(void *p,const uint8_t *packet,size_t len,uint8_t *reply,size_t cap){
 struct fake *f=p;unsigned op=f->calls++,i=op/7;if(save)states[op]=*f->s;assert(mt76_frontend_rf(op) && len==16 && cap==1024);
 uint32_t w[4]={0};for(unsigned b=0;b<16;b++)w[b/4]|=(uint32_t)packet[b]<<(8*(b%4));unsigned seq=i-29;
 assert(w[0]==(0x50a00008u|(seq<<16)) && w[1]==mt76_frontend_target(i) && w[2]==0 && w[3]==0);
 if(f->fail==op+1)return -1;
 w[0]=w[3]=0x4a000008u|(seq<<16);w[2]=value(i);memset(reply,0,cap);for(unsigned b=0;b<16;b++)reply[b]=(uint8_t)(w[b/4]>>(8*(b%4)));
 if(f->corrupt==op+1)reply[f->byte]^=1u<<f->bit;
 return f->size;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_5ghz_prereq *s){f->s=s;const struct mt76_mcu_timing_io io={f,auth,read_reg,exchange};return mt76_frontend_capture(s,&io);}
int main(int argc,char **argv){
 assert(argc==2);FILE *b=fopen(argv[1],"rb");assert(b);for(unsigned i=0;i<44;i++){uint32_t a=0;for(unsigned j=0;j<4;j++){int c=fgetc(b);assert(c>=0);a|=(uint32_t)c<<(8*j);}assert(a==mt76_frontend_target(i));}assert(fgetc(b)==EOF);fclose(b);
 struct mt76_5ghz_prereq s={0};struct fake f={.size=16};save=true;assert(run(&f,&s)==MT76_TIMING_OK && mt76_frontend_valid(&s));save=false;good=s;
 assert(f.calls==308 && f.auth==309 && s.reads==294 && s.exchanges==14 && s.observed==44 && !mt76_5ghz_prereq_valid(&s) && run(&f,&s)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=308;n++){
  f=(struct fake){.size=16,.fail=n};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_IO && !s.complete && f.calls==n);assert(run(&f,&s)==MT76_TIMING_REFUSED);
  s=states[n-1];assert(mt76_frontend_admit(&s,n-1));s.reads++;assert(!mt76_frontend_admit(&s,n-1));
  if((n-1)%7!=3){f=(struct fake){.size=16,.bad=n};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_STATE && !s.complete);}
  if(mt76_frontend_rf(n-1))for(unsigned byte=0;byte<16;byte++)if(byte!=8)for(unsigned bit=0;bit<8;bit++){f=(struct fake){.size=16,.corrupt=n,.byte=byte,.bit=bit};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 }
 for(unsigned n=1;n<=309;n++){f=(struct fake){.size=16,.deny=n};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<30;i++){f=(struct fake){.size=16,.max_slot=i+1};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==(raw(i)?MT76_TIMING_OK:MT76_TIMING_STATE) && s.complete==raw(i));}
 for(unsigned q=0;q<14;q++)for(unsigned j=0;j<4;j++)for(unsigned bit=0;bit<32;bit++){s=good;s.words[q][j]^=1u<<bit;assert(!mt76_frontend_valid(&s));}
 for(unsigned q=0;q<88;q++)for(unsigned j=0;j<3;j++){s=good;s.guards[q][j]^=1;assert(!mt76_frontend_valid(&s));}
 const int sizes[]={0,1,4,8,12,15,17,1024,1025};for(unsigned i=0;i<sizeof sizes/sizeof sizes[0];i++){f=(struct fake){.size=sizes[i]};s=(struct mt76_5ghz_prereq){0};assert(run(&f,&s)==MT76_TIMING_REPLY && !s.complete);}
 assert(mt76_frontend_target(44)==UINT32_MAX && mt76_frontend_address(308)==UINT32_MAX && !mt76_frontend_valid(NULL));
 puts("frontend inventory: PASS44 pinned targets,308 I/O/309 cancellation boundaries,30 direct+14 RF reads,explicit4 raw sentinels,reply/proof/mode mutations,no writes/no replay");
}
