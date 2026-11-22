#include <srvros/mt76_mcu.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {struct mt76_bbp5 *s;unsigned calls,auth,fail,deny,bad;uint32_t regs[21];};
static struct mt76_bbp5 states[735],good;static bool save;
static bool auth(void *p){struct fake *f=p;return ++f->auth!=f->deny;}
static bool transfer(void *p,bool wr,uint32_t a,uint32_t *v){
 struct fake *f=p;unsigned op=f->calls++,n=op%7,q=op/7,i=mt76_bbp5_index(q);if(save)states[op]=*f->s;
 assert(mt76_bbp5_transfer_valid(f->s,op,wr,a,*v));assert(!mt76_bbp5_transfer_valid(f->s,op,!wr,a,*v));assert(!mt76_bbp5_transfer_valid(f->s,op,wr,a^4,*v));if(wr)assert(!mt76_bbp5_transfer_valid(f->s,op,wr,a,*v^1));
 if(f->fail==op+1)return false;
 if(n==3){if(wr)f->regs[i]=*v;else *v=f->regs[i];}else{unsigned j=n<3?n:n-4;*v=j==0?0:j==1?0xf000f200:0x40000050;}
 if(f->bad==op+1)*v^=1;
 return true;
}
static enum mt76_mcu_timing_status run(struct fake *f,struct mt76_bbp5 *s,unsigned lna){f->s=s;for(unsigned i=0;i<21;i++)f->regs[i]=mt76_bbp5_initial(i);const struct mt76_bbp5_io io={f,auth,transfer};return mt76_bbp5_capture(s,lna,&io);}
int main(int argc,char **argv){
 assert(argc==2);FILE *b=fopen(argv[1],"rb");assert(b);for(unsigned i=0;i<21;i++)for(unsigned j=0;j<3;j++){uint32_t v=0;for(unsigned n=0;n<4;n++){int c=fgetc(b);assert(c>=0);v|=(uint32_t)c<<(8*n);}assert(v==(j==0?mt76_bbp5_target(i):j==1?mt76_bbp5_initial(i):mt76_bbp5_raw(i)));}assert(fgetc(b)==EOF);fclose(b);
 struct mt76_bbp5 s={0};struct fake f={0};save=true;assert(run(&f,&s,12)==MT76_TIMING_OK && mt76_bbp5_valid(&s));save=false;good=s;
 assert(f.calls==735 && f.auth==736 && s.writes==42 && s.reads==693);for(unsigned i=0;i<21;i++)assert(f.regs[i]==mt76_bbp5_initial(i));assert(run(&f,&s,12)==MT76_TIMING_REFUSED);
 for(unsigned n=1;n<=735;n++){
  f=(struct fake){.fail=n};s=(struct mt76_bbp5){0};assert(run(&f,&s,12)==MT76_TIMING_IO && !s.complete && f.calls==n);assert(run(&f,&s,12)==MT76_TIMING_REFUSED);
  f=(struct fake){.bad=n};s=(struct mt76_bbp5){0};assert(run(&f,&s,12)==MT76_TIMING_STATE && !s.complete);
  s=states[n-1];assert(mt76_bbp5_admit(&s,n-1));s.reads++;assert(!mt76_bbp5_admit(&s,n-1));
 }
 for(unsigned n=1;n<=736;n++){f=(struct fake){.deny=n};s=(struct mt76_bbp5){0};assert(run(&f,&s,12)==MT76_TIMING_CANCELLED && !s.complete && f.calls==n-1);}
 for(unsigned i=0;i<105;i++)for(unsigned bit=0;bit<32;bit++){s=good;s.values[i]^=1u<<bit;assert(!mt76_bbp5_valid(&s));}
 for(unsigned i=0;i<210;i++)for(unsigned j=0;j<3;j++){s=good;s.guards[i][j]^=1;assert(!mt76_bbp5_valid(&s));}
 for(unsigned lna=0;lna<256;lna++){s=(struct mt76_bbp5){0};f=(struct fake){0};assert(run(&f,&s,lna)==MT76_TIMING_OK && mt76_bbp5_valid(&s));assert(mt76_bbp5_expected(23,lna)==((0x122c54f2u&~0x7f00u)|(((84-lna*2)&127)<<8)));}
 s=(struct mt76_bbp5){0};f=(struct fake){0};assert(run(&f,&s,256)==MT76_TIMING_REFUSED && !s.attempted && !f.calls);
 struct mt76_5ghz_prereq parent={.attempted=true,.complete=true,.operations=308,.reads=294,.exchanges=14,.observed=44,.received=16};
 for(unsigned i=0;i<88;i++){parent.guards[i][1]=0xf000f200;parent.guards[i][2]=0x40000050;}
 for(unsigned i=0;i<21;i++)parent.values[i+4]=mt76_bbp5_initial(i);
 for(unsigned i=30;i<44;i++){uint32_t h=0x4a000008u|((i-29)<<16);parent.words[i-30][0]=parent.words[i-30][3]=h;parent.words[i-30][1]=mt76_frontend_target(i);}
 assert(mt76_bbp5_parent_valid(&parent));for(unsigned i=0;i<21;i++){parent.values[i+4]^=1;assert(mt76_frontend_valid(&parent) && !mt76_bbp5_parent_valid(&parent));parent.values[i+4]^=1;}
 assert(mt76_bbp5_target(21)==UINT32_MAX && mt76_bbp5_address(735)==UINT32_MAX && !mt76_bbp5_valid(NULL));
 puts("BBP5 cycle: PASS21 pinned originals/raw table,735 I/O/736 cancellation boundaries,all proof bits,256 gain scalars,three exact all-ones rows,reverse restoration,no replay/no RF route");
}
