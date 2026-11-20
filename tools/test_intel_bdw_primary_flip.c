#include <srvros/intel_bdw_primary_flip.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static struct intel_bdw_primary_flip result,copy;
static struct bdw_flip_source source,other_source;
static unsigned owner,lifetime,from_mapping,to_mapping,other,cases;
enum fault {NONE,READ_FALSE,READ_WRONG,WRITE_NONE,WRITE_UNKNOWN,WRITE_INVALID,
 PROOF_FALSE,PROOF_FIELD,PROOF_BIT,CLOCK_ZERO,CLOCK_BACK,CLOCK_OVER,CLOCK_EXACT,FROZEN,
 SOURCE_FIELD,BOTH_BINDING,PROGRESS,OBJECT,SOURCE_OBJECT,REGISTRATION,SOURCE_POINTER,
 CALLBACK,BOTH_CALLBACKS,TERMINAL,REENTER};
static struct {
 uint64_t time;
 uint32_t frame,surface,live;
 unsigned events,reads,proofs,clocks,calls,stores,kind,at,field,hit_events;
 bool hit,owned,visible,transition;
} m;
static struct bdw_flip_io io(void);
struct field {size_t offset,bytes;};
#define FIELD(T,n) {offsetof(struct T,n),sizeof(((struct T *)0)->n)}
static const struct field binding_fields[]={
 FIELD(bdw_flip_binding,owner),FIELD(bdw_flip_binding,provider),FIELD(bdw_flip_binding,lifetime),
 FIELD(bdw_flip_binding,from_mapping),FIELD(bdw_flip_binding,to_mapping),FIELD(bdw_flip_binding,root),
 FIELD(bdw_flip_binding,epoch),FIELD(bdw_flip_binding,sequence),FIELD(bdw_flip_binding,admitted_at),
 FIELD(bdw_flip_binding,aperture_bytes),FIELD(bdw_flip_binding,from_gpu),FIELD(bdw_flip_binding,to_gpu),
 FIELD(bdw_flip_binding,mapped_bytes),FIELD(bdw_flip_binding,vendor),FIELD(bdw_flip_binding,device),
 FIELD(bdw_flip_binding,source_size),FIELD(bdw_flip_binding,control),FIELD(bdw_flip_binding,stride)};
static const struct field progress_fields[]={
 FIELD(bdw_flip_progress,begin),FIELD(bdw_flip_progress,last),FIELD(bdw_flip_progress,state),
 FIELD(bdw_flip_progress,proofs),FIELD(bdw_flip_progress,reads),FIELD(bdw_flip_progress,attempts),
 FIELD(bdw_flip_progress,writes),FIELD(bdw_flip_progress,polls),FIELD(bdw_flip_progress,frame),
 FIELD(bdw_flip_progress,before_frame),FIELD(bdw_flip_progress,after_frame),FIELD(bdw_flip_progress,programmed),
 FIELD(bdw_flip_progress,live[0]),FIELD(bdw_flip_progress,live[1]),FIELD(bdw_flip_progress,uncertain)};
static const struct field proof_fields[]={
 FIELD(bdw_flip_proof,source),FIELD(bdw_flip_proof,owner),FIELD(bdw_flip_proof,provider),FIELD(bdw_flip_proof,lifetime),
 FIELD(bdw_flip_proof,from_mapping),FIELD(bdw_flip_proof,to_mapping),FIELD(bdw_flip_proof,root),
 FIELD(bdw_flip_proof,epoch),FIELD(bdw_flip_proof,sequence),FIELD(bdw_flip_proof,checked_at),FIELD(bdw_flip_proof,state)};
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static void mutate(void *p,struct field f) {
 uint64_t v=0;assert(f.bytes<=8);memcpy(&v,(unsigned char *)p+f.offset,f.bytes);v^=1;
 memcpy((unsigned char *)p+f.offset,&v,f.bytes);
}
static void hit(void){m.hit=true;m.hit_events=m.events;}
static void hook(void) {
 ++m.events;
 if(m.kind==REENTER) {
  assert(!intel_bdw_primary_flip_begin(&result,&source,io()));
  assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_INVALID);return;
 }
 if(m.events!=m.at)return;
 switch(m.kind) {
 case SOURCE_FIELD:mutate(&source.binding,binding_fields[m.field]);break;
 case BOTH_BINDING:mutate(&source.binding,binding_fields[m.field]);mutate(&result.binding,binding_fields[m.field]);break;
 case PROGRESS:mutate(&result.p,progress_fields[m.field]);break;
 case OBJECT:result.object=&copy;break;
 case SOURCE_OBJECT:source.object=&other_source;break;
 case REGISTRATION:source.consumer=&copy;break;
 case SOURCE_POINTER:result.source=&other_source;break;
 case CALLBACK:result.io.read=NULL;break;
 case BOTH_CALLBACKS:result.io.read=result.original_io.read=NULL;break;
 case TERMINAL:result.retained=true;result.end=1;break;
 default:return;
 }
 hit();
}
static uint64_t now(void *ctx) {
 assert(ctx==&m);++m.clocks;hook();
 if(m.clocks==m.at) {
  if(m.kind==CLOCK_ZERO){hit();return 0;}
  if(m.kind==CLOCK_BACK){hit();return result.p.last-1;}
  if(m.kind==CLOCK_OVER || m.kind==CLOCK_EXACT){hit();m.time=result.p.begin+100000+(m.kind==CLOCK_OVER);return m.time;}
 }
 return m.kind==FROZEN || (m.kind==CLOCK_EXACT && m.hit)?m.time:++m.time;
}
static bool ownership(void *ctx,const struct bdw_flip_source *s,struct bdw_flip_proof *p) {
 assert(ctx==&m && s==&source);++m.proofs;hook();
 *p=(struct bdw_flip_proof){&source,&owner,&m,&lifetime,&from_mapping,&to_mapping,
  0x1000,9,1,m.time,63,result.p.state};
 if(m.proofs==m.at) {
  if(m.kind==PROOF_FALSE){hit();return false;}
  if(m.kind==PROOF_FIELD){mutate(p,proof_fields[m.field]);if(m.field==9)p->checked_at=0;hit();}
  if(m.kind==PROOF_BIT){p->requirements^=1u<<m.field;hit();}
 }
 return m.owned && m.visible;
}
static bool read_word(void *ctx,uint32_t off,uint32_t *out) {
 assert(ctx==&m);++m.reads;hook();
 /* Independent device state: writing SURF does NOT modify SURFLIVE. */
 switch(off) {
 case 0x70180:*out=0x98000400u;break;
 case 0x70188:*out=6144;break;
 case 0x6001c:*out=(1439u<<16)|899;break;
 case 0x701a4:*out=0;break;
 case 0x70040:*out=m.frame;break;
 case 0x7019c:*out=m.surface;break;
 case 0x701ac:
  *out=m.live;
  if(m.transition){m.transition=false;m.live=0x2400000;++m.frame;}
  break;
 default:assert(false);
 }
 if(m.reads==m.at) {
  if(m.kind==READ_FALSE){hit();return false;}
  if(m.kind==READ_WRONG){hit();*out^=0x1000;}
 }
 return true;
}
static enum intel_gt_write write_word(void *ctx,uint32_t off,uint32_t value) {
 assert(ctx==&m && off==0x7019c && value==0x2400000 && m.owned && m.visible);
 assert(!m.calls);++m.calls;hook();
 if(m.kind==WRITE_NONE){hit();return GT_WRITE_NONE;}
 if(m.kind==WRITE_UNKNOWN){hit();m.surface=value;return GT_WRITE_UNCERTAIN;}
 if(m.kind==WRITE_INVALID){hit();m.surface=value;return (enum intel_gt_write)3;}
 m.surface=value;++m.stores;return GT_WRITE_DONE;
}
static struct bdw_flip_io io(void){return (struct bdw_flip_io){&m,ownership,read_word,write_word,now};}
static void setup(unsigned kind,unsigned at,unsigned field) {
 memset(&result,0,sizeof(result));memset(&copy,0,sizeof(copy));memset(&source,0,sizeof(source));memset(&other_source,0,sizeof(other_source));
 memset(&m,0,sizeof(m));m.kind=kind;m.at=at;m.field=field;m.time=1000000;
 m.frame=100;m.surface=8;m.owned=m.visible=true;
 source.object=&source;source.binding=(struct bdw_flip_binding){&owner,&m,&lifetime,&from_mapping,&to_mapping,
  0x1000,9,1,m.time,0x10000000,0,0x2400000,0x600000,0x8086,0x1626,(1439u<<16)|899,0x98000400u,6144};
}
static void terminal_failure(void) {
 assert(result.retained && result.reason && result.p.state==BDW_FLIP_FAULT && !result.end && !result.busy);
 assert(result.p.attempts==m.calls && result.p.writes==m.stores);
 assert(result.source==&source && result.object==&result && source.object==&source && source.consumer==&result);
 unsigned events=m.events;
 assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_FAILED);
 assert(!intel_bdw_primary_flip_begin(&result,&source,io()));
 assert(!intel_bdw_primary_flip_begin(&copy,&source,io()));assert(events==m.events);
}
static bool exercise(void) {
 if(!intel_bdw_primary_flip_begin(&result,&source,io()))return false;
 assert(m.calls==1 && m.stores==1 && m.live==0 && !result.end);
 /* Mere elapsed frames and the posting read cannot complete the flip. */
 ++m.frame;
 if(intel_bdw_primary_flip_poll(&result)!=BDW_FLIP_WAIT)return false;
 m.live=0x2400000;++m.frame;
 return intel_bdw_primary_flip_poll(&result)==BDW_FLIP_DONE;
}
static void positives(void) {
 for(unsigned kind=NONE;kind<=REENTER;kind++) {
  if(kind!=NONE && kind!=REENTER)continue;
  setup(kind,0,0);assert(exercise());++cases;
  assert(result.p.state==BDW_FLIP_LATCHED && !result.retained && !result.reason && result.end>result.p.begin);
  assert(result.p.polls==2 && result.p.reads==28 && result.p.attempts==1 && result.p.writes==1);
  unsigned events=m.events;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_INVALID && events==m.events);
  copy=result;copy.object=&copy;assert(intel_bdw_primary_flip_poll(&copy)==BDW_FLIP_INVALID && events==m.events);
 }
 setup(NONE,0,0);m.frame=UINT32_MAX;assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 ++m.frame;m.live=0x2400000;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_DONE);++cases;
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 m.transition=true;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_WAIT);
 assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_DONE);++cases;
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 m.live=0x2400000;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_WAIT);
 ++m.frame;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_DONE);++cases;
 /* Copied pending results and source reuse have no callbacks or store. */
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 copy=result;unsigned e=m.events;
 assert(intel_bdw_primary_flip_poll(&copy)==BDW_FLIP_INVALID && m.events==e);
 copy.object=&copy;assert(intel_bdw_primary_flip_poll(&copy)==BDW_FLIP_INVALID && m.events==e);
 assert(source.consumer==&result && !result.retained);
 m.live=0x2400000;++m.frame;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_DONE);
 ++cases;
}
static void fault_matrix(void) {
 setup(NONE,0,0);assert(exercise());
 unsigned events=m.events,reads=m.reads,proofs=m.proofs,clocks=m.clocks;
 for(unsigned kind=SOURCE_FIELD;kind<=TERMINAL;kind++) {
  unsigned fields=kind==SOURCE_FIELD || kind==BOTH_BINDING?COUNT(binding_fields):kind==PROGRESS?COUNT(progress_fields):1;
  for(unsigned field=0;field<fields;field++)for(unsigned at=1;at<=events;at++) {
   setup(kind,at,field);assert(!exercise() && m.hit);terminal_failure();++cases;
   assert(m.events==m.hit_events);
  }
 }
 for(unsigned kind=PROOF_FALSE;kind<=PROOF_BIT;kind++) {
  unsigned fields=kind==PROOF_FIELD?COUNT(proof_fields):kind==PROOF_BIT?6:1;
  for(unsigned field=0;field<fields;field++)for(unsigned at=1;at<=proofs;at++) {
   setup(kind,at,field);assert(!exercise() && m.hit);terminal_failure();++cases;
   assert(m.events<=m.hit_events+1);
  }
 }
 for(unsigned kind=CLOCK_ZERO;kind<=CLOCK_OVER;kind++)for(unsigned at=1;at<=clocks;at++) {
  setup(kind,at,0);assert(!exercise() && m.hit);terminal_failure();++cases;
  assert(m.events==m.hit_events);
 }
 for(unsigned at=1;at<=reads;at++) {
  setup(READ_FALSE,at,0);assert(!exercise() && m.hit);terminal_failure();++cases;
  assert(m.events==m.hit_events);
 }
 for(unsigned kind=WRITE_NONE;kind<=WRITE_INVALID;kind++) {
  setup(kind,0,0);assert(!exercise() && m.hit);terminal_failure();++cases;
  assert(result.p.uncertain==(kind!=WRITE_NONE));
 }
 for(unsigned at=1;at<=clocks;at++) {
  setup(CLOCK_EXACT,at,0);bool ok=exercise();assert(m.hit);++cases;
  if(ok)assert(result.end==result.p.begin+100000 && !result.retained);
  else terminal_failure();
  if(at==clocks)assert(ok);
 }
 /* A completely frozen clock cannot certify even successful arming. */
 setup(FROZEN,0,0);assert(!exercise());terminal_failure();++cases;
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 m.kind=FROZEN;m.live=0x2400000;++m.frame;
 assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_FAILED);terminal_failure();++cases;
}
static void changed_hardware(void) {
 for(unsigned variant=0;variant<11;variant++) {
  setup(NONE,0,0);
  if(variant==0)m.surface=0x40000;else if(variant==1)m.live=0x40000;
  else if(variant==2)m.owned=false;else if(variant==3)m.visible=false;
  if(variant<4){assert(!intel_bdw_primary_flip_begin(&result,&source,io()));assert(!m.calls);}
  else {
   assert(intel_bdw_primary_flip_begin(&result,&source,io()));
   if(variant==4)m.surface=0x2800000;
   if(variant==5)m.live=0x2800000;
   if(variant==6)m.owned=false;
   if(variant==7)m.visible=false;
   if(variant==8)m.frame+=9;
   if(variant==9)m.time=result.p.begin+100001;
   if(variant==10){m.live=0x2400000;--m.frame;}
   assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_FAILED);
  }
  terminal_failure();++cases;
 }
 /* Regressions between separate batches must not hide behind an otherwise
  * valid delta from the original frame or the same source's old address. */
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 ++m.frame;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_WAIT);
 --m.frame;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_FAILED);terminal_failure();++cases;
 setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
 m.transition=true;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_WAIT);
 m.live=0;++m.frame;assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_FAILED);terminal_failure();++cases;
 /* The 256th bounded observation may succeed; there is never a 257th read. */
 for(unsigned complete=0;complete<2;complete++) {
  setup(NONE,0,0);assert(intel_bdw_primary_flip_begin(&result,&source,io()));
  for(unsigned i=0;i<255;i++)assert(intel_bdw_primary_flip_poll(&result)==BDW_FLIP_WAIT);
  if(complete){m.live=0x2400000;++m.frame;}
  assert(intel_bdw_primary_flip_poll(&result)==(complete?BDW_FLIP_DONE:BDW_FLIP_FAILED));
  if(!complete)terminal_failure();
  ++cases;
 }
 /* Wrong geometry, floating surface, reserved/async/control drift, nonzero
  * offset and changed source size all fail the actual hardware comparisons. */
 setup(NONE,0,0);assert(exercise());unsigned reads=m.reads;
 for(unsigned i=1;i<=reads;i++) {
  setup(READ_WRONG,i,0);assert(!exercise() && m.hit);
  /* This injected bit changes frame counters by 4096, outside the bounded
   * one-frame read batch; every non-frame read is independently exact. */
  terminal_failure();
  ++cases;
 }
}
static void invalid_inputs(void) {
 for(unsigned field=0;field<COUNT(binding_fields);field++) {
  setup(NONE,0,0);struct field f=binding_fields[field];memset((unsigned char *)&source.binding+f.offset,0,f.bytes);
  /* Zero source size and from_gpu are legitimate 1x1/base0 fields. */
  if(field==10 || field==15)continue;
  assert(!intel_bdw_primary_flip_begin(&result,&source,io()) && !m.events && !result.object);++cases;
 }
 for(unsigned variant=0;variant<17;variant++) {
  setup(NONE,0,0);
  switch(variant) {
  case 0:source.binding.device=0x1616;break;
  case 1:source.binding.control|=512;break;
  case 2:source.binding.control|=1u<<15;break;
  case 3:source.binding.control|=1u<<14;break;
  case 4:source.binding.control|=64;break;
  case 5:source.binding.stride=6143;break;
  case 6:source.binding.stride=512;break;
  case 7:source.binding.stride=65536;break;
  case 8:source.binding.mapped_bytes=0x54c000;break;
  case 9:source.binding.to_gpu=0x1000;break;
  case 10:source.binding.to_gpu=0x40000;break;
  case 11:source.binding.from_gpu=0x100000000ull;break;
  case 12:source.binding.to_gpu=0xfffc0000;break;
  case 13:source.binding.aperture_bytes=0x100000001ull;break;
  case 14:source.binding.mapped_bytes=UINT64_MAX;break;
  case 15:source.consumer=&copy;break;
  case 16:source.object=&other_source;break;
  }
  assert(!intel_bdw_primary_flip_begin(&result,&source,io()) && !m.events && !result.object);++cases;
 }
 for(unsigned variant=0;variant<5;variant++) {
  setup(NONE,0,0);struct bdw_flip_io ops=io();
  if(!variant)ops.ctx=&other;else if(variant==1)ops.ownership=NULL;else if(variant==2)ops.read=NULL;
  else if(variant==3)ops.write=NULL;else ops.now=NULL;
  assert(!intel_bdw_primary_flip_begin(&result,&source,ops) && !m.events);++cases;
 }
}
void test_flip_rollback(void);

int main(void) {
 test_flip_rollback();
 positives();invalid_inputs();fault_matrix();changed_hardware();
 printf("primary-flip PASS cases=%u native=0 mappings=0 hardware_flips=0 reader_retirement=0\n",cases);
 return 0;
}
