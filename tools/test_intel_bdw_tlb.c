/* Synthetic device model. Every setup represents an external reset/new world;
 * no native provider or completion assurance is supplied by this fixture. */
#include <srvros/intel_bdw_tlb.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static const uint32_t expected_regs[5]={0x4260,0x4264,0x4270,0x4268,0x426c};
static struct intel_bdw_tlb result;
static struct intel_bdw_tlb_source source;
static unsigned owner,lifetime;
static struct {
 uint64_t time,fixed;
 unsigned clocks,proofs,reads,writes,issued,pauses,events,at,field,boundary_writes;
 unsigned latency[5],mask;
 enum {NORMAL,CLOCK_ZERO,CLOCK_BACK,CLOCK_EXPIRE,SOURCE_DRIFT,PROOF_BAD,PROOF_FALSE,
 READ_FAIL,READ_FLOAT,READ_RESERVED,INITIAL_BUSY,WRITE_NONE,WRITE_UNKNOWN,WRITE_BAD_ENUM,
 RESULT_DRIFT,REENTER,PROGRESS_DRIFT} fault;
} model;
static unsigned cases;
static void setup(void) {
 memset(&result,0,sizeof(result));memset(&source,0,sizeof(source));memset(&model,0,sizeof(model));
 source.object=&source;
 source.binding=(struct intel_bdw_tlb_binding){.owner=&owner,.provider=&model,.lifetime=&lifetime,
 .epoch=7,.sequence=3,.root=0x1000,.physical=0x1000000,.aperture_bytes=0x10000000,
 .fence_end=0x2200000,.prepare_begin=1000,.gt_begin=900,.hda_begin=850,.outer_begin=800,
 .vendor=0x8086,.device=0x1626,.first=8704,.pages=1};
 model.time=1000;
 for(unsigned i=0;i<5;i++)model.latency[i]=2;
 ++cases;
}
static struct intel_bdw_tlb_io io(void);
static void event(void) {
 ++model.events;
 if(model.fault==SOURCE_DRIFT && model.events==model.at) {
  switch(model.field) {
   case 0:source.binding.epoch++;break;case 1:source.binding.sequence++;break;case 2:source.binding.root+=4096;break;
   case 3:source.binding.physical+=4096;break;case 4:source.binding.aperture_bytes-=4096;break;
   case 5:source.binding.fence_begin+=4096;break;case 6:source.binding.fence_end-=4096;break;
   case 7:source.binding.prepare_begin++;break;case 8:source.binding.gt_begin++;break;case 9:source.binding.outer_begin++;break;
   case 10:source.binding.owner=0;break;case 11:source.binding.provider=&lifetime;break;case 12:source.binding.lifetime=&owner;break;
   case 13:source.binding.vendor^=1;break;case 14:source.binding.device^=1;break;case 15:source.binding.first++;break;
   case 16:source.binding.pages++;break;case 17:source.object=0;break;case 18:source.consumer=0;break;
   case 19:source.binding.hda_begin++;break;
  }
  model.boundary_writes=model.writes;
 }
 if(model.fault==PROGRESS_DRIFT && model.events==model.at) {
  switch(model.field) {
   case 0:result.last=1;break;case 1:result.begin=1;break;case 2:result.reads++;break;
   case 3:result.attempts++;break;case 4:result.writes++;break;case 5:result.completed++;break;
   case 6:result.proofs++;break;case 7:result.requested_mask^=1;break;case 8:result.completed_mask^=1;break;
   case 9:result.input=0;break;
   case 10:result.uncertain=true;break;case 11:result.end=1;break;case 12:result.clock_at=1;break;
   case 13:result.clock_previous=1;break;case 14:result.clock_reason=1;break;case 15:result.retained=true;break;
  }
  model.boundary_writes=model.writes;
 }
 if(model.fault==REENTER) {
  unsigned before=model.events;
  assert(!intel_bdw_tlb_invalidate(&result,&source,io()));
  assert(model.events==before);
 }
}
static uint64_t now(void *ctx) {
 assert(ctx==&model);++model.clocks;event();
 if(model.clocks==model.at) {
  if(model.fault==CLOCK_ZERO)return 0;
  if(model.fault==CLOCK_BACK)return 1;
  if(model.fault==CLOCK_EXPIRE)return source.binding.prepare_begin+100001;
 }
 return model.fixed?model.fixed:++model.time;
}
static bool prerequisite(void *ctx,const struct intel_bdw_tlb_source *s,struct intel_bdw_tlb_proof *p) {
 assert(ctx==&model);++model.proofs;event();
 if(s!=&source || s->consumer!=&result || s->binding.owner!=&owner || s->binding.provider!=&model || s->binding.lifetime!=&lifetime)return false;
 *p=(struct intel_bdw_tlb_proof){.source=s,.owner=&owner,.provider=&model,.lifetime=&lifetime,
 .epoch=7,.sequence=3,.root=0x1000,.checked_at=model.fixed?model.fixed:model.time,
 .engines=31,.pipelines_flushed=31,.memory_retired=31};
 if(model.fault==PROOF_FALSE && model.proofs==model.at)return false;
 if(model.fault==PROOF_BAD && model.proofs==model.at) {
  switch(model.field) {
  case 0:p->source=0;break;case 1:p->owner=0;break;case 2:p->provider=0;break;case 3:p->lifetime=0;break;
  case 4:p->epoch++;break;case 5:p->sequence++;break;case 6:p->root+=4096;break;
  case 7:p->checked_at=1;break;case 8:p->checked_at=UINT64_MAX;break;
  case 9:p->engines=30;break;case 10:p->pipelines_flushed=30;break;case 11:p->memory_retired=30;break;
  case 12:p->engines=63;break;case 13:p->pipelines_flushed=63;break;case 14:p->memory_retired=63;break;
  }
 }
 if(model.fault==RESULT_DRIFT && model.proofs==model.at) {
  switch(model.field) {
  case 0:result.initial[0]=1;break;case 1:result.final[0]=1;break;case 2:result.polls[0]=257;break;
  case 3:result.proofs++;break;case 4:result.completed_mask=0;break;case 5:result.requested_mask=0;break;
  case 6:result.io.ctx=0;break;case 7:source.consumer=0;break;case 8:result.index=5;break;
  }
 }
 return true;
}
static bool read_control(void *ctx,unsigned i,uint32_t reg,uint32_t *out) {
 assert(ctx==&model && i<5 && reg==expected_regs[i]);++model.reads;event();
 if(model.fault==READ_FAIL && model.reads==model.at)return false;
 if(model.fault==READ_FLOAT && model.reads==model.at){*out=UINT32_MAX;return true;}
 if(model.fault==READ_RESERVED && model.reads==model.at){*out=2;return true;}
 if(result.state==BDW_TLB_READING) {
  assert(!model.writes);*out=model.fault==INITIAL_BUSY && model.reads==model.at?1:0;
 } else {
  assert(result.state==BDW_TLB_POLLING && (model.mask&(1u<<i)));
  *out=model.latency[i]?1:0;if(model.latency[i])--model.latency[i];
 }
 return true;
}
static enum intel_gt_write write_control(void *ctx,unsigned i,uint32_t reg,uint32_t value) {
 assert(ctx==&model && i<5 && reg==expected_regs[i] && value==1);
 assert(result.state==BDW_TLB_WRITING && i==model.writes);
 ++model.writes;event();
 if(model.fault==WRITE_NONE && model.writes==model.at)return GT_WRITE_NONE;
 model.mask|=1u<<i;++model.issued;
 if(model.fault==WRITE_UNKNOWN && model.writes==model.at)return GT_WRITE_UNCERTAIN;
 if(model.fault==WRITE_BAD_ENUM && model.writes==model.at)return (enum intel_gt_write)99;
 return GT_WRITE_DONE;
}
static void pause_control(void *ctx){assert(ctx==&model);++model.pauses;event();}
static struct intel_bdw_tlb_io io(void) {
 return (struct intel_bdw_tlb_io){&model,prerequisite,read_control,write_control,now,pause_control};
}
static bool run(void){return intel_bdw_tlb_invalidate(&result,&source,io());}
static void terminal_failure(void) {
 assert(!result.complete && result.state==BDW_TLB_FAULT && result.reason && !result.busy && !result.end);
 if(result.attempts)assert(result.retained);
 unsigned events=model.events,writes=model.writes;
 assert(!run() && model.events==events && model.writes==writes);
 if(source.consumer==&result) {
  struct intel_bdw_tlb other={0};assert(!intel_bdw_tlb_invalidate(&other,&source,io()));
  assert(!other.object && model.events==events);
 } else assert(result.retained); /* Lost registration cannot be repaired/reused. */
}
int main(void) {
 setup();assert(run());
 assert(result.complete && result.state==BDW_TLB_COMPLETE && !result.busy && !result.retained && !result.uncertain);
 assert(result.attempts==5 && result.writes==5 && result.completed==5 && result.requested_mask==31 && result.completed_mask==31);
 assert(result.reads==20 && model.reads==20 && model.writes==5 && model.issued==5 && model.pauses==10);
 assert(result.proofs==26 && source.consumer==&result && result.end==result.last && result.end>result.begin);
 const unsigned clocks=model.clocks,proofs=model.proofs,reads=model.reads,events=model.events;
 assert(!run() && model.events==events);
 for(unsigned n=1;n<=clocks;n++)for(unsigned kind=CLOCK_ZERO;kind<=CLOCK_EXPIRE;kind++) {
  setup();model.fault=kind;model.at=n;assert(!run());terminal_failure();assert(result.clock_reason);
 }
 for(unsigned n=1;n<=proofs;n++) {
  setup();model.fault=PROOF_FALSE;model.at=n;assert(!run());terminal_failure();
  for(unsigned f=0;f<15;f++) {
   setup();model.fault=PROOF_BAD;model.at=n;model.field=f;assert(!run());terminal_failure();
  }
 }
 for(unsigned n=1;n<=reads;n++)for(unsigned kind=READ_FAIL;kind<=READ_RESERVED;kind++) {
  setup();model.fault=kind;model.at=n;assert(!run());terminal_failure();assert(result.reason&BDW_TLB_READ);
 }
 for(unsigned n=1;n<=5;n++) {
  setup();model.fault=INITIAL_BUSY;model.at=n;assert(!run());assert(!model.writes && !model.issued);terminal_failure();
  for(unsigned kind=WRITE_NONE;kind<=WRITE_BAD_ENUM;kind++) {
   setup();model.fault=kind;model.at=n;assert(!run());terminal_failure();assert(model.writes==n);
   assert(result.attempts==n && result.writes==n-1 && result.completed==n-1);
   assert(result.uncertain==(kind!=WRITE_NONE));
  }
  setup();model.latency[n-1]=256;assert(!run());terminal_failure();assert(result.polls[n-1]==256 && model.writes==n);
  setup();model.latency[n-1]=255;assert(run());assert(result.polls[n-1]==256 && result.completed==5);
 }
 for(unsigned f=0;f<20;f++)for(unsigned n=1;n<=events;n++) {setup();model.fault=SOURCE_DRIFT;model.at=n;model.field=f;assert(!run());assert(model.writes==model.boundary_writes);terminal_failure();}
 for(unsigned f=0;f<16;f++)for(unsigned n=1;n<=events;n++) {setup();model.fault=PROGRESS_DRIFT;model.at=n;model.field=f;assert(!run());assert(model.writes==model.boundary_writes);terminal_failure();}
 for(unsigned f=0;f<9;f++) {setup();model.fault=RESULT_DRIFT;model.at=proofs;model.field=f;assert(!run());terminal_failure();}
 setup();model.fault=REENTER;assert(run());
 setup();source.binding.hda_begin=950;assert(run()); /* HDA and GT may start in either order. */
 /* Copied source cannot borrow its original provider, even if rebound. */
 setup();struct intel_bdw_tlb_source copy=source;copy.object=&copy;
 assert(!intel_bdw_tlb_invalidate(&result,&copy,io()) && !model.writes && !model.reads);
 /* Every malformed admission is rejected before consuming either object. */
 for(unsigned f=0;f<19;f++) {
  setup();switch(f) {
   case 0:source.object=0;break;case 1:source.binding.owner=0;break;case 2:source.binding.lifetime=0;break;
   case 3:source.binding.epoch=0;break;case 4:source.binding.sequence=0;break;case 5:source.binding.vendor=1;break;
   case 6:source.binding.device=0x1616;break;case 7:source.binding.root++;break;case 8:source.binding.physical++;break;
   case 9:source.binding.physical=0x100000000ull;break;case 10:source.binding.pages=0;break;
   case 11:source.binding.pages=1537;break;case 12:source.binding.first=8703;break;
   case 13:source.binding.first=0x100000;break;case 14:source.binding.aperture_bytes++;break;
   case 15:source.binding.fence_end++;break;case 16:source.binding.outer_begin=0;break;
   case 17:source.binding.gt_begin=source.binding.prepare_begin+1;break;
   case 18:source.binding.hda_begin=source.binding.prepare_begin+1;break;
  }
  assert(!run() && !result.object && !source.consumer && !model.events);
 }
 for(unsigned f=0;f<6;f++) {
  setup();struct intel_bdw_tlb_io missing=io();
  switch(f) {
   case 0:missing.ctx=0;break;case 1:missing.prerequisite=0;break;case 2:missing.read=0;break;
   case 3:missing.write=0;break;case 4:missing.now=0;break;case 5:missing.pause=0;break;
  }
  assert(!intel_bdw_tlb_invalidate(&result,&source,missing) && !result.object && !source.consumer && !model.events);
 }
 setup();assert(!intel_bdw_tlb_invalidate(0,&source,io()) && !intel_bdw_tlb_invalidate(&result,0,io()) && !model.events);
 setup();source.consumer=&result;assert(!run() && !result.object && !model.events);
 /* Independent literal bounds: touching the limit is allowed but frozen time
  * never completes. One microsecond beyond each ORIGINAL bound refuses. */
 for(unsigned which=0;which<4;which++)for(unsigned extra=0;extra<2;extra++) {
  setup();source.binding.outer_begin=1;source.binding.hda_begin=which==3?150000:1;
  source.binding.gt_begin=which==2?200000:which==3?250000:1;
  source.binding.prepare_begin=which==0?1:which==1?250000:which==2?350000:450000;
  model.fixed=(which==0?100001ull:which==1?300001ull:which==2?400001ull:500001ull)+extra;
  assert(!run());terminal_failure();
  assert(result.clock_reason==(extra?(which==0?BDW_TLB_PREPARE_EXPIRED:which==1?BDW_TLB_GT_EXPIRED:which==2?BDW_TLB_HDA_EXPIRED:BDW_TLB_OUTER_EXPIRED):BDW_TLB_FROZEN));
  assert(model.issued==(extra?0:5));
 }
 printf("intel-bdw-tlb: PASS cases=%u clocks=%u proofs=%u reads=%u events=%u; synthetic only, no native provider or GPU grant\n",cases,clocks,proofs,reads,events);
 return 0;
}
