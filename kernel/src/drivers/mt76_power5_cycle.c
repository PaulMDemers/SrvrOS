#include <srvros/mt76_power5_cycle.h>
static bool overlap(const void *a,size_t an,const void *b,size_t bn){uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;return x<=y?y-x<an:x-y<bn;}
static bool read_word(struct mt76_power5_cycle *s,const struct mt76_power5_cycle_io *io,uint32_t a,uint32_t *v,bool cleanup){
 *v=0;if(!io->owned(io->context,cleanup)){s->failed=true;return false;}
 s->reads++;bool ok=io->read(io->context,a,v,cleanup) && io->owned(io->context,cleanup);
 if(!ok)s->failed=true;
 return ok;
}
static bool write_word(struct mt76_power5_cycle *s,const struct mt76_power5_cycle_io *io,uint32_t a,uint32_t v,bool cleanup){
 if(!io->owned(io->context,cleanup)){s->failed=true;return false;}
 s->writes++;bool ok=io->write(io->context,a,v,cleanup) && io->owned(io->context,cleanup);
 if(!ok)s->failed=true;
 return ok;
}
static bool guards(struct mt76_power5_cycle *s,const struct mt76_power5_cycle_io *io,bool cleanup){
 const uint32_t addresses[]={0x1004,0x2000,0x208},values[]={0,0xf000f200,0x40000050};
 for(unsigned i=0;i<3;i++){uint32_t v;if(!read_word(s,io,addresses[i],&v,cleanup) || v!=values[i]){s->failed=true;return false;}}
 return true;
}
bool mt76_power5_cycle_run(struct mt76_power5_cycle *s,const struct mt76_power5_cycle_io *io,const struct mt76_power5_capture *f,unsigned limit){
 if(!s || !io || !f || overlap(s,sizeof *s,io,sizeof *io) || overlap(s,sizeof *s,f,sizeof *f) ||
    (io->context && overlap(s,sizeof *s,io->context,1)) || s->attempted || !io->owned || !io->read || !io->write || !io->inspect || !mt76_power5_capture_valid(f))return false;
 const uint8_t us[2]={'U','S'};struct mt76_power5_plan plan={0};
 if(mt76_power5_plan(&f->fields,us,161,20,limit,&plan)!=MT76_POWER5_OK)return false;
 *s=(struct mt76_power5_cycle){.attempted=true,.generation=plan.generation};
 /* These originals,plan and masks remain local authorities if a callback
  * damages the public receipt. The native transport separately gates values. */
 uint32_t originals[9]={0};unsigned modified=0;bool ok=true;
 for(unsigned i=0;i<9;i++){
  if(!guards(s,io,false) || !read_word(s,io,plan.write[i].address,&originals[i],false) || originals[i]==UINT32_MAX){ok=false;break;}
  s->saved[i]=originals[i];s->saved_mask|=1u<<i;
  s->chosen[i]=(originals[i]&~plan.write[i].mask)|plan.write[i].value;
 }
 s->snapshot_complete=ok && s->saved_mask==511;
 for(unsigned i=0;ok && i<9;i++){
  uint32_t want=(originals[i]&~plan.write[i].mask)|plan.write[i].value,v;
  if(!guards(s,io,false)){ok=false;break;}
  modified|=1u<<i;s->modified_mask=modified;
  bool written=write_word(s,io,plan.write[i].address,want,false);
  bool seen=read_word(s,io,plan.write[i].address,&v,false) && v==want;
  if(seen)s->verified_mask|=1u<<i;
  ok=written && seen;
 }
 s->selected=ok && s->snapshot_complete && s->verified_mask==511 && guards(s,io,false);
 if(s->selected){s->inspected=io->owned(io->context,false) && io->inspect(io->context);ok=s->inspected;}
 else ok=false;
 s->selected=false;
 /* A guard failure prevents writing even an original power value while MAC
  * may still transmit. Independent safe originals are attempted in reverse. */
 for(unsigned n=9;n>0;n--){unsigned i=n-1;if(!(modified&(1u<<i)))continue;
  if(!guards(s,io,true)){ok=false;continue;}
  bool written=write_word(s,io,plan.write[i].address,originals[i],true);uint32_t v;
  bool seen=read_word(s,io,plan.write[i].address,&v,true) && v==originals[i];
  s->restored[i]=v;if(written && seen)s->restored_mask|=1u<<i;
  if(!written || !seen){ok=false;s->failed=true;}
 }
 s->cleanup_complete=s->restored_mask==modified && guards(s,io,true);
 s->complete=ok && !s->failed && s->cleanup_complete && io->owned(io->context,false);
 if(!s->complete)s->failed=true;
 return s->complete;
}
