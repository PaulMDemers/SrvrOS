#include <srvros/mt76_handoff5.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {
 struct mt76_handoff5 *result;struct mt76_handoff5_plan *plan;
 uint32_t words[8],original[8];unsigned calls,fail,commands,auth,deny,inspect_mode;
 bool apply_uncertain,lost;unsigned corrupt_read,corrupt_word;uint32_t corrupt_bits;
};
static struct mt76_handoff5_plan plan(void){
 struct mt76_handoff5_plan p={.generation=1,.slot=7,.old_ring=0x10000,.old_dequeue=0x10081,.new_ring=0x20000,.old_posts=8,.new_slots=63,.protected_count=3,.protected_pages={0x30000,0x40000,0x50000}};return p;
}
static void init(struct fake *f,struct mt76_handoff5 *s,struct mt76_handoff5_plan *p){
 memset(s,0,sizeof *s);f->result=s;f->plan=p;f->calls=f->commands=f->auth=0;f->lost=false;
 f->words[0]=3;f->words[1]=(3u<<1)|(6u<<3)|(512u<<16);f->words[2]=(uint32_t)p->old_dequeue;f->words[3]=p->old_dequeue>>32;f->words[4]=4096;f->words[5]=f->words[6]=f->words[7]=0;memcpy(f->original,f->words,sizeof f->words);
}
static bool owned(void *c,bool cleanup){
 struct fake *f=c;(void)cleanup;f->auth++;if(f->auth==f->deny)f->lost=true;return !f->lost;
}
static bool read_context(void *c,uint32_t *w){
 struct fake *f=c;assert(!f->lost);f->calls++;if(f->calls==f->fail)return false;
 memcpy(w,f->words,sizeof f->words);
 if(f->calls==f->corrupt_read)w[f->corrupt_word]^=f->corrupt_bits;
 return true;
}
static bool set(void *c,uint64_t d){
 struct fake *f=c;assert(!f->lost);f->calls++;f->commands++;
 assert(d==(f->commands==1?(f->plan->new_ring|1):f->plan->old_dequeue));assert(f->commands<=2);
 if(f->calls!=f->fail || f->apply_uncertain){f->words[2]=d;f->words[3]=d>>32;}
 return f->calls!=f->fail;
}
static bool inspect(void *c){
 struct fake *f=c;assert(!f->lost);f->calls++;
 struct mt76_handoff5_io io={f,owned,read_context,set,inspect};unsigned before=f->calls;
 assert(!mt76_handoff5_run(f->result,f->plan,&io) && f->calls==before);
 if(f->inspect_mode==1)f->words[0]=1;
 if(f->inspect_mode>=2 && f->inspect_mode<=66)f->words[2]=(uint32_t)(f->plan->new_ring+16*(f->inspect_mode-2))|1;
 if(f->inspect_mode==67)f->words[2]=(uint32_t)(f->plan->new_ring+16*65)|1;
 if(f->inspect_mode==68)f->words[2]&=~1u;
 return f->calls!=f->fail;
}
static bool run(struct fake *f,struct mt76_handoff5 *s,struct mt76_handoff5_plan *p){struct mt76_handoff5_io io={f,owned,read_context,set,inspect};return mt76_handoff5_run(s,p,&io);}
int main(void){
 struct mt76_handoff5_plan p=plan();struct mt76_handoff5 s={0};struct fake f={0};init(&f,&s,&p);
 assert(mt76_handoff5_plan_valid(&p) && run(&f,&s,&p));assert(s.complete && s.restored && s.inspected && !s.unsafe && !s.failed && s.reads==4 && s.commands==2 && f.calls==7 && f.auth==16 && !memcmp(f.words,f.original,sizeof f.words));
 assert(!run(&f,&s,&p) && f.calls==7);
 for(unsigned n=1;n<=7;n++)for(unsigned after=0;after<2;after++){
  f=(struct fake){.fail=n,.apply_uncertain=after};init(&f,&s,&p);assert(!run(&f,&s,&p) && s.failed && !s.complete);
  if(n==4){assert(s.restored && !s.unsafe && f.commands==2 && f.calls==7 && !memcmp(f.words,f.original,sizeof f.words));}
  else {assert(f.calls==n && !s.restored);assert(s.unsafe==(n>=2));assert(f.commands==(n==1?0:n<6?1:2));}
 }
 for(unsigned n=1;n<=16;n++){
  f=(struct fake){.deny=n};init(&f,&s,&p);assert(!run(&f,&s,&p) && f.auth==n && s.failed && !s.complete);
 }
 for(unsigned mode=1;mode<=68;mode++){
  f=(struct fake){.inspect_mode=mode};init(&f,&s,&p);bool ok=run(&f,&s,&p);
  if(mode>=2 && mode<=65)assert(ok && s.restored && !memcmp(f.words,f.original,sizeof f.words));
  else assert(!ok && s.unsafe && !s.restore_attempted && f.commands==1 && f.calls==5);
 }
 const unsigned reads[]={1,3,5,7};
 for(unsigned r=0;r<4;r++)for(unsigned w=0;w<8;w++)for(unsigned bit=0;bit<32;bit++){
  f=(struct fake){.corrupt_read=reads[r],.corrupt_word=w,.corrupt_bits=1u<<bit};init(&f,&s,&p);bool ok=run(&f,&s,&p);
  /* A post-inspection dequeue may advance within the separately retained
   * fresh ring;all other snapshot changes must refuse. */
  if(r==2 && (w==2 || w==3)){
   if(ok)assert(s.restored && !s.unsafe);else assert(s.unsafe && !s.restore_attempted);
  }else assert(!ok && (r==0 || s.unsafe));
  assert(!ok || !memcmp(f.words,f.original,sizeof f.words));
 }
 for(unsigned fault=0;fault<18;fault++){
  p=plan();
  if(fault==0)p.generation=0;
  if(fault==1)p.generation=UINT64_MAX;
  if(fault==2)p.slot=0;
  if(fault==3)p.old_ring=0;
  if(fault==4)p.old_ring|=1;
  if(fault==5)p.new_ring=p.old_ring;
  if(fault==6)p.new_ring=UINT64_MAX-4095;
  if(fault==7)p.old_posts=0;
  if(fault==8)p.old_posts=9;
  if(fault==9)p.new_slots=62;
  if(fault==10)p.old_dequeue^=1;
  if(fault==11)p.old_dequeue=p.old_ring+16*9+1;
  if(fault==12)p.protected_count=33;
  if(fault==13)p.protected_pages[0]=p.old_ring;
  if(fault==14)p.protected_pages[0]=p.new_ring;
  if(fault==15)p.protected_pages[1]=p.protected_pages[0];
  if(fault==16)p.protected_pages[0]|=1;
  if(fault==17)p.protected_pages[31]=0x90000;
  f=(struct fake){0};init(&f,&s,&p);assert(!mt76_handoff5_plan_valid(&p) && !run(&f,&s,&p) && !s.attempted && !f.calls && !f.auth);
 }
 /* Explicit immutable cleanup frontier retains old_posts=8;the zero field
  * still rejects legacy posts9 above. Mechanical plans grant no native DMA. */
 for(unsigned cleanup=1;cleanup<=8;cleanup++)for(unsigned end=0;end<2;end++){
  p=plan();p.cleanup_posts=cleanup;p.old_dequeue=p.old_ring+16*(8+cleanup-1+end)+1;
  f=(struct fake){0};init(&f,&s,&p);assert(mt76_handoff5_plan_valid(&p) && run(&f,&s,&p) && s.restored && !memcmp(f.words,f.original,sizeof f.words));
 }
 for(unsigned n=0;n<4;n++){
  p=plan();p.cleanup_posts=n==0?9:2;p.old_dequeue=p.old_ring+16*(8+p.cleanup_posts)+1;
  if(n==1)p.old_posts=7;
  if(n==2)p.old_dequeue=p.old_ring+129;
  if(n==3)p.old_dequeue+=16;
  assert(!mt76_handoff5_plan_valid(&p));
 }
 p=plan();f=(struct fake){0};init(&f,&s,&p);struct mt76_handoff5_io io={&f,owned,read_context,set,inspect};
 assert(!mt76_handoff5_run(NULL,&p,&io));assert(!mt76_handoff5_run(&s,NULL,&io));assert(!mt76_handoff5_run(&s,&p,NULL));
 struct mt76_handoff5_plan saved=p;assert(!mt76_handoff5_run((void *)&p,&p,&io) && !memcmp(&p,&saved,sizeof p));
 struct mt76_handoff5_io savedio=io;assert(!mt76_handoff5_run((void *)&io,&p,&io) && !memcmp(&io,&savedio,sizeof io));
 io.context=&s;assert(!mt76_handoff5_run(&s,&p,&io) && !s.attempted);io.context=&f;
 for(unsigned n=0;n<4;n++){struct mt76_handoff5_io bad=io;if(n==0)bad.owned=NULL;if(n==1)bad.read_context=NULL;if(n==2)bad.set_dequeue=NULL;if(n==3)bad.inspect=NULL;assert(!mt76_handoff5_run(&s,&p,&bad) && !s.attempted);}
 puts("handoff5: PASS exact stopped-context retarget/inspection/original restoration;all7 callback failures before/after uncertain writes,16 ownership losses,all context-bit mutations,64 fresh dequeue positions,alias/plan/replay and running/DCS/range refusals;no native authority or hardware I/O");return 0;
}
