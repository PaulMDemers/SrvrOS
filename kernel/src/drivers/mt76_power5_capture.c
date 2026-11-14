#include <srvros/mt76_power5_capture.h>
#define KICK (1u<<30)
#define ADDRESS (0x3ffu<<16)
#define MODE (3u<<6)
static const uint32_t blocks[]={0,0x30,0x90,0xd0,0x120};
static const unsigned wanted[]={0,0x34,0x36,0x98,0xde,0x120,0x122,0x124,0x126,0x12c};
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static void scrub(void *p,size_t n){volatile uint8_t *v=p;while(n--)*v++=0;}
bool mt76_power5_command(uint32_t old,unsigned b,uint32_t *out){
 if(!out || b>=5 || old==UINT32_MAX || (old&KICK))return false;
 *out=(old&~(ADDRESS|MODE))|(blocks[b]<<16)|KICK;return true;
}
bool mt76_power5_begin(struct mt76_power5_gate *g,bool write,uint32_t a,uint32_t v){
 if(!g || g->failed)return false;
 unsigned p=g->phase;uint32_t expected=p==0 || p==13?0x1004:p==1 || p==14?0x2000:p==2?0x208:p>=9 && p<=12?0x28+4*(p-9):0x24;
 if(g->pending || g->block>=5 || p>14 || p==6 || p==7 || write!=(p==4) || a!=expected ||
    (p==5 && g->polls>=1024) || (write && (!mt76_power5_command(g->control,g->block,&g->command) || v!=g->command))){g->failed=true;return false;}
 g->pending=true;g->submitted++;if(p==5)g->polls++;return true;
}
bool mt76_power5_finish(struct mt76_power5_gate *g,bool ok,uint32_t v){
 if(!g || g->failed)return false;
 if(!g->pending || !ok || g->block>=5){g->failed=true;return false;}
 g->pending=false;unsigned p=g->phase;
 if((p==0 || p==13) && v)goto fail;
 if((p==1 || p==14) && v!=0xf000f200)goto fail;
 if(p==2 && v!=0x40000050)goto fail;
 if(p==3){g->control=v;if(!mt76_power5_command(v,g->block,&g->command))goto fail;}
 if(p==5){if(v==UINT32_MAX)goto fail;g->phase=v&KICK?6:7;return true;}
 if(p==8 && (v==UINT32_MAX || (v&KICK) || (v&MODE) || (v&ADDRESS)!=(g->command&ADDRESS) || (v&63)==63))goto fail;
 if(p==14){g->block++;g->phase=g->block==5?15:0;}else g->phase++;
 return true;
fail:g->failed=true;return false;
}
bool mt76_power5_delay(struct mt76_power5_gate *g,unsigned us){
 if(!g || g->failed)return false;
 if(g->pending || !((g->phase==6 && us==1000) || (g->phase==7 && us==2))){g->failed=true;return false;}
 g->phase=g->phase==6?5:8;return true;
}
bool mt76_power5_capture_valid(const struct mt76_power5_capture *s){
 if(!s || !s->attempted || !s->complete || s->blocks!=5 || s->writes!=5 || s->polls<5 || s->polls>1024 || s->reads!=55+s->polls)return false;
 struct mt76_power5_plan p={0};const uint8_t us[2]={'U','S'};
 return mt76_power5_plan(&s->fields,us,161,20,30,&p)==MT76_POWER5_OK;
}
enum mt76_capture_status mt76_power5_capture_run(struct mt76_power5_capture *s,
 const struct mt76_efuse_map_io *io,const struct mt76_identity *id,uint64_t generation){
 if(!s || !io || !id || overlap(s,sizeof *s,io,sizeof *io) || overlap(s,sizeof *s,id,sizeof *id) ||
    (io->registers.context && overlap(s,sizeof *s,io->registers.context,1)) || s->attempted ||
    !generation || generation==UINT64_MAX || id->asic!=0x76100002 || id->mac!=0x76502000 ||
    !io->authorize || !io->registers.read || !io->registers.write || !io->registers.delay)return MT76_CAPTURE_REFUSED;
 *s=(struct mt76_power5_capture){.attempted=true};struct mt76_power5_gate g={0};
 /* Only wanted field bytes enter this sparse logical parser buffer. Factory
  * MAC/other block bytes are discarded,not copied. Scratch is always scrubbed. */
 uint8_t logical[512]={0};const struct mt76_register_io *r=&io->registers;uint32_t v=0,command=0;
 enum mt76_capture_status result=MT76_CAPTURE_IO;
#define AUTH() do{if(!io->authorize(r->context)){result=MT76_CAPTURE_CANCELLED;goto done;}}while(0)
#define READ(a) do{AUTH();if(!mt76_power5_begin(&g,false,(a),0) || !r->read(r->context,(a),&v) || !mt76_power5_finish(&g,true,v))goto done;s->reads++;AUTH();}while(0)
#define DELAY(us) do{AUTH();if(!mt76_power5_delay(&g,(us)) || !r->delay(r->context,(us))){result=MT76_CAPTURE_CANCELLED;goto done;}AUTH();}while(0)
#define GUARDS() do{READ(0x1004);READ(0x2000);}while(0)
 for(unsigned b=0;b<5;b++){
  GUARDS();READ(0x208);READ(0x24);
  if(!mt76_power5_command(v,b,&command))goto done;
  AUTH();if(!mt76_power5_begin(&g,true,0x24,command) || !r->write(r->context,0x24,command) || !mt76_power5_finish(&g,true,0))goto done;s->writes++;AUTH();
  for(;;){if(s->polls>=1024){result=MT76_CAPTURE_TIMEOUT;goto done;}s->polls++;READ(0x24);if(!(v&KICK))break;DELAY(1000);}
  DELAY(2);READ(0x24);
  for(unsigned w=0;w<4;w++){
   READ(0x28+4*w);
   for(unsigned i=0;i<4;i++){unsigned offset=blocks[b]+w*4+i;
    for(unsigned k=0;k<sizeof wanted/sizeof wanted[0];k++)if(offset==wanted[k] || offset==wanted[k]+1)logical[offset]=(uint8_t)(v>>(8*i));
   }
  }
  GUARDS();s->blocks++;
 }
 AUTH();if(g.phase!=15 || g.block!=5 || g.pending || g.failed || g.submitted!=s->reads+s->writes || g.polls!=s->polls)goto done;
 if(mt76_power5_fields(logical,sizeof logical,generation,&s->fields)!=MT76_POWER5_OK){result=MT76_CAPTURE_REFUSED;goto done;}
 AUTH();s->complete=true;if(!mt76_power5_capture_valid(s)){result=MT76_CAPTURE_REFUSED;goto done;}
 result=MT76_CAPTURE_OK;
done:
 if(result!=MT76_CAPTURE_OK){scrub(&s->fields,sizeof s->fields);s->complete=false;}
 scrub(logical,sizeof logical);scrub(&v,sizeof v);return result;
#undef AUTH
#undef READ
#undef DELAY
#undef GUARDS
}
