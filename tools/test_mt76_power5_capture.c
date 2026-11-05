#include <srvros/mt76_power5_capture.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct fake {
 struct mt76_power5_gate gate;
 unsigned calls,auths,fail,cancel,mutate,mode,busy;
 uint32_t control;
 uint8_t logical[512];
};
static const unsigned offsets[]={0,0x30,0x90,0xd0,0x120};
static bool auth(void *c){struct fake *f=c;return ++f->auths!=f->cancel;}
static bool read_reg(void *c,uint32_t a,uint32_t *v){
 struct fake *f=c;unsigned p=f->gate.phase,b=f->gate.block;
 assert(mt76_power5_begin(&f->gate,false,a,0));
 if(++f->calls==f->fail)return false;
 *v=0;
 if(a==0x2000)*v=0xf000f200;
 else if(a==0x208)*v=0x40000050;
 else if(a==0x24){
  *v=p==3?0x80000000:f->control&~(1u<<30);
  if(p==5 && f->busy){*v|=1u<<30;if(f->busy!=~0u)f->busy--;}
  if(p==8 && f->mode==1)*v|=63;
  if(p==8 && f->mode==2)*v|=1u<<6;
  if(p==8 && f->mode==3)*v^=16u<<16;
 }
 else if(a>=0x28 && a<=0x34){unsigned n=offsets[b]+a-0x28;*v=f->logical[n]|(uint32_t)f->logical[n+1]<<8|(uint32_t)f->logical[n+2]<<16|(uint32_t)f->logical[n+3]<<24;}
 if(f->calls==f->mutate)*v^=1;
 /* Faults in the modeled device are rejected by the production gate. The
  * fake gate still records that a USB operation completed,not its validity. */
 (void)mt76_power5_finish(&f->gate,true,*v);return true;
}
static bool write_reg(void *c,uint32_t a,uint32_t v){
 struct fake *f=c;assert(a==0x24 && f->gate.block<5);
 assert(v==((0x80000000u)|(offsets[f->gate.block]<<16)|(1u<<30)));
 assert((v&(3u<<6))==0);assert(mt76_power5_begin(&f->gate,true,a,v));
 if(++f->calls==f->fail)return false;
 f->control=v;assert(mt76_power5_finish(&f->gate,true,0));return true;
}
static bool delay_reg(void *c,uint32_t us){struct fake *f=c;assert(mt76_power5_delay(&f->gate,us));return ++f->calls!=f->fail;}
static struct mt76_efuse_map_io io_for(struct fake *f){return (struct mt76_efuse_map_io){{f,read_reg,write_reg,delay_reg},auth};}
static void setup(struct fake *f){
 *f=(struct fake){0};memset(f->logical,0xa5,sizeof f->logical);
 f->logical[0]=0x10;f->logical[1]=0x76;f->logical[0x34]=0x11;f->logical[0x35]=0xfd;
 f->logical[0x36]=4;f->logical[0x37]=0x10;f->logical[0x99]=19;
 f->logical[0xde]=f->logical[0xdf]=0;
 f->logical[0x120]=5;f->logical[0x121]=4;f->logical[0x122]=4;f->logical[0x123]=0;
 f->logical[0x124]=f->logical[0x125]=f->logical[0x126]=f->logical[0x127]=0;
 f->logical[0x12c]=f->logical[0x12d]=0;
}
static void empty_fields(const struct mt76_power5_capture *s){struct mt76_power5_fields zero={0};assert(!s->complete && !memcmp(&s->fields,&zero,sizeof zero));assert(!mt76_power5_capture_valid(s));}
static const struct mt76_identity identity={.asic=0x76100002,.mac=0x76502000};
static enum mt76_capture_status run(struct fake *f,struct mt76_power5_capture *s){struct mt76_efuse_map_io io=io_for(f);return mt76_power5_capture_run(s,&io,&identity,17);}
int main(void){
 struct fake f;struct mt76_power5_capture s={0};setup(&f);assert(run(&f,&s)==MT76_CAPTURE_OK);
 assert(mt76_power5_capture_valid(&s));assert(s.reads==60 && s.writes==5 && s.blocks==5 && s.polls==5);
 assert(s.fields.generation==17 && s.fields.chip==0x7610 && s.fields.channel_pair==0x13a5);
 assert(s.fields.ofdm_lo==0x0405 && s.fields.ofdm_hi==4 && s.fields.cck==0);
 assert(f.gate.phase==15 && f.gate.block==5 && f.gate.submitted==65);
 unsigned calls=f.calls,auths=f.auths;assert(calls==70);
 struct mt76_power5_capture original=s;unsigned before=f.calls;assert(run(&f,&s)==MT76_CAPTURE_REFUSED && f.calls==before && !memcmp(&s,&original,sizeof s));
 for(unsigned n=1;n<=calls;n++){setup(&f);f.fail=n;s=(struct mt76_power5_capture){0};assert(run(&f,&s)!=MT76_CAPTURE_OK);empty_fields(&s);assert(f.calls==n);}
 for(unsigned n=1;n<=auths;n++){setup(&f);f.cancel=n;s=(struct mt76_power5_capture){0};assert(run(&f,&s)==MT76_CAPTURE_CANCELLED);empty_fields(&s);assert(f.auths==n);}
 for(unsigned mode=1;mode<=3;mode++){setup(&f);f.mode=mode;s=(struct mt76_power5_capture){0};assert(run(&f,&s)!=MT76_CAPTURE_OK);empty_fields(&s);}
 setup(&f);f.busy=~0u;s=(struct mt76_power5_capture){0};assert(run(&f,&s)==MT76_CAPTURE_TIMEOUT);empty_fields(&s);assert(s.polls==1024);
 setup(&f);f.busy=3;s=(struct mt76_power5_capture){0};assert(run(&f,&s)==MT76_CAPTURE_OK && s.polls==8 && s.reads==63 && f.gate.polls==8);
 for(unsigned bad=0;bad<3;bad++){setup(&f);f.logical[bad==0?0:bad==1?0x34:0x36]^=1;s=(struct mt76_power5_capture){0};assert(run(&f,&s)!=MT76_CAPTURE_OK);empty_fields(&s);}
 /* Every guard occurrence is independently mutated (both sides of5 blocks). */
 for(unsigned b=0;b<5;b++)for(unsigned k=0;k<5;k++){const unsigned guard[]={1,2,3,13,14};setup(&f);f.mutate=14*b+guard[k];s=(struct mt76_power5_capture){0};assert(run(&f,&s)!=MT76_CAPTURE_OK);empty_fields(&s);}
 uint32_t command=0;for(unsigned b=0;b<5;b++){assert(mt76_power5_command(0x800000c0,b,&command));assert(command==(0xc0000000u|offsets[b]<<16));}
 assert(!mt76_power5_command(0,5,&command));assert(!mt76_power5_command(~0u,0,&command));assert(!mt76_power5_command(1u<<30,0,&command));
 for(unsigned p=0;p<16;p++)for(unsigned op=0;op<4;op++){
  struct mt76_power5_gate g={.phase=p};
  if(op==0){assert(!mt76_power5_begin(&g,false,0xdead,0));assert(g.failed);}
  if(op==1){assert(!mt76_power5_finish(&g,true,0));assert(g.failed);}
  if(op==2){assert(!mt76_power5_delay(&g,7));assert(g.failed);}
  if(op==3){g.pending=true;assert(!mt76_power5_begin(&g,false,0x1004,0));assert(g.failed);}
 }
 /* Every changed command bit is refused before publication,including mode,
  * address,read kick and preserved control bits. Failed gates stay terminal. */
 for(unsigned b=0;b<5;b++)for(unsigned bit=0;bit<32;bit++){
  struct mt76_power5_gate g={.phase=4,.block=b,.control=0x80000000};uint32_t cmd=0;
  assert(mt76_power5_command(g.control,b,&cmd));
  assert(!mt76_power5_begin(&g,true,0x24,cmd^(1u<<bit)) && g.failed && !g.pending);
  assert(!mt76_power5_begin(&g,true,0x24,cmd));assert(!mt76_power5_finish(&g,true,0));assert(!mt76_power5_delay(&g,2));
 }
 for(unsigned p=0;p<15;p++){
  if(p==6 || p==7)continue;
  struct mt76_power5_gate g={.phase=p,.pending=true};assert(!mt76_power5_finish(&g,false,0) && g.failed);
 }
 struct mt76_power5_gate exhausted={.phase=5,.polls=1024};assert(!mt76_power5_begin(&exhausted,false,0x24,0) && exhausted.failed);
 setup(&f);struct mt76_efuse_map_io io=io_for(&f);s=(struct mt76_power5_capture){0};
 assert(mt76_power5_capture_run(&s,&io,&identity,0)==MT76_CAPTURE_REFUSED && !f.calls);
 assert(mt76_power5_capture_run(&s,&io,&identity,UINT64_MAX)==MT76_CAPTURE_REFUSED && !f.calls);
 union aliases {struct mt76_power5_capture state;struct mt76_efuse_map_io io;struct mt76_identity id;} alias;
 memset(&alias,0,sizeof alias);alias.io=io;
 assert(mt76_power5_capture_run(&alias.state,&alias.io,&identity,1)==MT76_CAPTURE_REFUSED);
 memset(&alias,0,sizeof alias);alias.id=identity;assert(mt76_power5_capture_run(&alias.state,&io,&alias.id,1)==MT76_CAPTURE_REFUSED);
 io.registers.context=&s;assert(mt76_power5_capture_run(&s,&io,&identity,1)==MT76_CAPTURE_REFUSED);io=io_for(&f);
 io.authorize=NULL;assert(mt76_power5_capture_run(&s,&io,&identity,1)==MT76_CAPTURE_REFUSED && !s.attempted);
 printf("power5-capture: PASS;5 mapped blocks;70 operation faults;%u authorization cancellations;guards/mode/alias/timeout/reentry refusals;no hardware\n",auths);
 return 0;
}
