/* Independent synthetic register/order model, not native hardware acceptance. */
#include <srvros/intel_ggtt_invalidate.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
static struct intel_ggtt_invalidate result;
static struct intel_ggtt_invalidate_source source;
static unsigned owner,lifetime,cases;
static struct {
 uint64_t time,fixed;
 unsigned events,clocks,guards,reads,writes,issued,at,field;
 unsigned boundary_reads,boundary_writes;
 bool hit;
 enum {NORMAL,SOURCE_DRIFT,RESULT_DRIFT,CLOCK_ZERO,CLOCK_BACK,CLOCK_EXPIRE,
 GUARD_FALSE,READ_FALSE,READ_NONZERO,READ_FLOAT,WRITE_NONE,WRITE_UNKNOWN,WRITE_ENUM,REENTER} fault;
} model;
static struct intel_ggtt_invalidate_io io(void);
static bool run(void){return intel_ggtt_invalidate_run(&result,&source,io());}
static void setup(void) {
 memset(&result,0,sizeof(result));memset(&source,0,sizeof(source));memset(&model,0,sizeof(model));
 source.object=&source;
 source.binding=(struct intel_ggtt_invalidate_binding){.owner=&owner,.provider=&model,.lifetime=&lifetime,
 .root=0x1000,.epoch=7,.sequence=3,.mmio=0xc1600000,.mmio_bytes=0x400000,
 .prepare_begin=1000,.gt_begin=900,.hda_begin=850,.outer_begin=800,.vendor=0x8086,.device=0x1626};
 model.time=1000;++cases;
}
static void source_drift(unsigned n) {
 switch(n) {
 case 0:source.binding.owner=0;break;case 1:source.binding.provider=0;break;case 2:source.binding.lifetime=0;break;
 case 3:source.binding.root+=4096;break;case 4:source.binding.epoch++;break;case 5:source.binding.sequence++;break;
 case 6:source.binding.mmio+=65536;break;case 7:source.binding.mmio_bytes+=4096;break;
 case 8:source.binding.prepare_begin++;break;case 9:source.binding.gt_begin++;break;
 case 10:source.binding.hda_begin++;break;case 11:source.binding.outer_begin++;break;
 case 12:source.binding.vendor^=1;break;case 13:source.binding.device^=1;break;
 case 14:source.object=0;break;case 15:source.consumer=0;break;
 default:assert(false);
 }
}
static void result_drift(unsigned n) {
 switch(n) {
 case 0:result.p.begin++;break;case 1:result.p.last++;break;case 2:result.p.state++;break;
 case 3:result.p.reads++;break;case 4:result.p.guards++;break;case 5:result.p.attempts++;break;
 case 6:result.p.writes++;break;case 7:result.p.initial++;break;case 8:result.p.posted++;break;case 9:result.p.uncertain++;break;
 case 10:result.object=0;break;case 11:result.source=0;break;case 12:result.end++;break;
 case 13:result.clock_at++;break;case 14:result.clock_previous++;break;case 15:result.clock_reason++;break;
 case 16:result.reason++;break;case 17:result.busy=false;break;case 18:result.complete=true;break;case 19:result.retained=true;break;
 case 20:result.io.ctx=0;break;case 21:result.io.guard=0;break;case 22:result.io.read=0;break;case 23:result.io.write=0;break;case 24:result.io.now=0;break;
 case 25:result.original_io.ctx=0;break;case 26:result.original_io.guard=0;break;case 27:result.original_io.read=0;break;case 28:result.original_io.write=0;break;case 29:result.original_io.now=0;break;
 case 30:result.binding.owner=0;break;case 31:result.binding.provider=0;break;case 32:result.binding.lifetime=0;break;
 case 33:result.binding.root+=4096;break;case 34:result.binding.epoch++;break;case 35:result.binding.sequence++;break;
 case 36:result.binding.mmio+=65536;break;case 37:result.binding.mmio_bytes+=4096;break;
 case 38:result.binding.prepare_begin++;break;case 39:result.binding.gt_begin++;break;
 case 40:result.binding.hda_begin++;break;case 41:result.binding.outer_begin++;break;
 case 42:result.binding.vendor^=1;break;case 43:result.binding.device^=1;break;
 case 44:source.binding.epoch++;result.binding.epoch++;break;
 case 45:source.binding.mmio+=65536;result.binding.mmio+=65536;break;
 default:assert(false);
 }
}
static void event(void) {
 ++model.events;
 if(model.events!=model.at)return;
 if(model.fault==SOURCE_DRIFT || model.fault==RESULT_DRIFT) {
  if(model.fault==SOURCE_DRIFT)source_drift(model.field);else result_drift(model.field);
  model.hit=true;model.boundary_reads=model.reads;model.boundary_writes=model.writes;
 }
 if(model.fault==REENTER) {
  unsigned char original[sizeof(result)];memcpy(original,&result,sizeof(result));
  assert(!run());assert(!memcmp(original,&result,sizeof(result)));model.hit=true;
 }
}
static uint64_t now(void *ctx) {
 assert(ctx==&model);++model.clocks;event();
 if(model.clocks==model.at) {
  if(model.fault==CLOCK_ZERO)return 0;
  if(model.fault==CLOCK_BACK)return result.p.last-1;
  if(model.fault==CLOCK_EXPIRE)return model.fixed;
 }
 return model.fixed && model.fault!=CLOCK_EXPIRE?model.fixed:++model.time;
}
static bool guard(void *ctx,const struct intel_ggtt_invalidate_source *s) {
 assert(ctx==&model && s==&source);++model.guards;event();
 return !(model.fault==GUARD_FALSE && model.guards==model.at);
}
static bool read_word(void *ctx,uint32_t offset,uint32_t *value) {
 assert(ctx==&model && offset==0x101008);
 assert(model.reads<2 && model.writes==model.reads);
 assert(result.p.state==(model.reads?GGTT_INVALIDATE_POSTING:GGTT_INVALIDATE_OPENING));
 ++model.reads;event();
 if(model.reads==model.at) {
  if(model.fault==READ_FALSE)return false;
  if(model.fault==READ_NONZERO){*value=1;return true;}
  if(model.fault==READ_FLOAT){*value=UINT32_MAX;return true;}
 }
 *value=0;return true;
}
static enum intel_gt_write write_word(void *ctx,uint32_t offset,uint32_t value) {
 assert(ctx==&model && offset==0x101008 && value==1);
 assert(!model.writes && model.reads==1 && result.p.state==GGTT_INVALIDATE_WRITING);
 ++model.writes;event();
 if(model.fault==WRITE_NONE)return GT_WRITE_NONE;
 ++model.issued;
 if(model.fault==WRITE_UNKNOWN)return GT_WRITE_UNCERTAIN;
 if(model.fault==WRITE_ENUM)return (enum intel_gt_write)42;
 return GT_WRITE_DONE;
}
static struct intel_ggtt_invalidate_io io(void) {
 return (struct intel_ggtt_invalidate_io){&model,guard,read_word,write_word,now};
}
static void terminal(bool success) {
 assert(!result.busy);
 if(success)assert(result.object==&result && result.source==&source);
 assert(result.complete==success && result.retained==!success);
 if(success) {
  assert(result.p.state==GGTT_INVALIDATE_COMPLETE && !result.reason && !result.clock_at);
  assert(result.p.reads==2 && result.p.writes==1 && result.p.guards==4 && result.p.attempts==1);
  assert(result.end==result.p.last && result.end>result.p.begin && !result.p.initial && !result.p.posted && !result.p.uncertain);
 } else assert(result.p.state==GGTT_INVALIDATE_FAULT && result.reason && !result.end);
 if(model.hit && model.fault!=REENTER)assert(model.reads==model.boundary_reads && model.writes==model.boundary_writes);
 unsigned char saved[sizeof(result)];memcpy(saved,&result,sizeof(result));
 unsigned events=model.events;assert(!run());assert(events==model.events && !memcmp(saved,&result,sizeof(result)));
}
int main(void) {
 setup();assert(run());terminal(true);unsigned events=model.events,clocks=model.clocks;
 assert(events==18 && clocks==11);
 for(unsigned event_at=1;event_at<=events;event_at++) {
  for(unsigned field=0;field<16;field++) {
   setup();model.fault=SOURCE_DRIFT;model.at=event_at;model.field=field;assert(!run());assert(model.hit);terminal(false);
  }
  for(unsigned field=0;field<46;field++) {
   setup();model.fault=RESULT_DRIFT;model.at=event_at;model.field=field;assert(!run());assert(model.hit);terminal(false);
  }
  setup();model.fault=REENTER;model.at=event_at;assert(run());assert(model.hit);terminal(true);
 }
 for(unsigned c=1;c<=clocks;c++) {
  for(unsigned f=CLOCK_ZERO;f<=CLOCK_BACK;f++) {
   setup();model.fault=f;model.at=c;assert(!run());assert(result.reason&GGTT_INVALIDATE_CLOCK);terminal(false);
  }
  const uint64_t limits[4]={INTEL_GGTT_INVALIDATE_PREPARE_US,300000,400000,500000};
  for(unsigned budget=0;budget<4;budget++) {
   setup();model.fault=CLOCK_EXPIRE;model.at=c;
   source.binding.outer_begin=1;source.binding.hda_begin=600000;source.binding.gt_begin=900000;source.binding.prepare_begin=1100000;
   uint64_t *origin=budget==0?&source.binding.prepare_begin:budget==1?&source.binding.gt_begin:budget==2?&source.binding.hda_begin:&source.binding.outer_begin;
   /* Isolate each parent expiry while the later budgets remain valid. */
   uint64_t endpoint=1500000;
   source.binding.prepare_begin=endpoint-10;source.binding.gt_begin=endpoint-20;
   source.binding.hda_begin=endpoint-30;source.binding.outer_begin=endpoint-40;
   *origin=endpoint-limits[budget];model.time=endpoint-10;model.fixed=endpoint+1;
   /* Existing source order must still be legal for this budget. */
   if(budget==0){source.binding.gt_begin=*origin-10;source.binding.hda_begin=*origin-20;source.binding.outer_begin=*origin-30;}
   if(budget==1){source.binding.hda_begin=*origin-10;source.binding.outer_begin=*origin-20;}
   if(budget==2)source.binding.outer_begin=*origin-10;
   assert(!run());assert(model.clocks==c);assert(result.clock_reason==GGTT_INVALIDATE_PREPARE_EXPIRED+budget);terminal(false);
  }
 }
 /* Exact endpoint and one-us excess at the FINAL clock for all parents. */
 for(unsigned budget=0;budget<4;budget++)for(unsigned excess=0;excess<2;excess++) {
  setup();const uint64_t endpoint=1500000;
  const uint64_t limits[4]={INTEL_GGTT_INVALIDATE_PREPARE_US,300000,400000,500000};
  source.binding.prepare_begin=endpoint-10;source.binding.gt_begin=endpoint-20;
  source.binding.hda_begin=endpoint-30;source.binding.outer_begin=endpoint-40;
  uint64_t *origin=budget==0?&source.binding.prepare_begin:budget==1?&source.binding.gt_begin:budget==2?&source.binding.hda_begin:&source.binding.outer_begin;
  *origin=endpoint-limits[budget];
  if(budget==0){source.binding.gt_begin=*origin-10;source.binding.hda_begin=*origin-20;source.binding.outer_begin=*origin-30;}
  if(budget==1){source.binding.hda_begin=*origin-10;source.binding.outer_begin=*origin-20;}
  if(budget==2)source.binding.outer_begin=*origin-10;
  model.time=endpoint-clocks;model.fault=CLOCK_EXPIRE;model.at=clocks;model.fixed=endpoint+excess;
  assert(run()==!excess);assert(model.clocks==clocks);terminal(!excess);
 }
 for(unsigned g=1;g<=4;g++){setup();model.fault=GUARD_FALSE;model.at=g;assert(!run());assert(result.reason&GGTT_INVALIDATE_GUARD);assert(model.writes==(g>2));assert(model.reads==(g==1?0:g==4?2:1));terminal(false);}
 for(unsigned rd=1;rd<=2;rd++)for(unsigned f=READ_FALSE;f<=READ_FLOAT;f++) {
  setup();model.fault=f;model.at=rd;assert(!run());assert(model.writes==(rd==2));terminal(false);
 }
 for(unsigned f=WRITE_NONE;f<=WRITE_ENUM;f++) {
  setup();model.fault=f;assert(!run());assert(result.reason&GGTT_INVALIDATE_WRITE);
  assert(result.p.attempts==1 && result.p.writes==0 && model.reads==1);
  assert(result.p.uncertain==(f!=WRITE_NONE));terminal(false);
 }
 setup();model.fixed=1001;assert(!run());assert(result.clock_reason==GGTT_INVALIDATE_FROZEN);terminal(false);
 /* Invalid input must be refused before consuming/registering either object. */
 for(unsigned f=0;f<14;f++) {
  setup();source_drift(f);
  /* Some changed nonzero descriptions are valid, but do not match an external
   * provider. Initial syntax tests below set them unambiguously invalid. */
  if(f==3)source.binding.root=1;
  if(f==4)source.binding.epoch=0;
  if(f==5)source.binding.sequence=0;
  if(f==6)source.binding.mmio=1;
  if(f==7)source.binding.mmio_bytes=0x10100b;
  if(f>=8 && f<=11)source.binding.outer_begin=0;
  assert(!run());assert(!source.consumer && !result.object && !model.events);
 }
 setup();source.binding.mmio=UINT64_MAX&~65535ull;assert(!run());assert(!model.events);
 for(unsigned f=0;f<5;f++) {
  setup();struct intel_ggtt_invalidate_io bad=io();
  if(f==0)bad.ctx=0;
  if(f==1)bad.guard=0;
  if(f==2)bad.read=0;
  if(f==3)bad.write=0;
  if(f==4)bad.now=0;
  assert(!intel_ggtt_invalidate_run(&result,&source,bad));assert(!source.consumer && !result.object);
 }
 setup();assert(!intel_ggtt_invalidate_run(NULL,&source,io()));assert(!intel_ggtt_invalidate_run(&result,NULL,io()));
 setup();source.consumer=&result;assert(!run());assert(!model.events);
 setup();struct intel_ggtt_invalidate_source copied=source;assert(!intel_ggtt_invalidate_run(&result,&copied,io()));assert(!model.events);
 setup();
 union {uint64_t alignment;unsigned char bytes[sizeof(result)+sizeof(source)];} storage={0};
 struct intel_ggtt_invalidate *aliased_result=(void *)storage.bytes;
 struct intel_ggtt_invalidate_source *aliased_source=(void *)(storage.bytes+16);
 *aliased_source=source;aliased_source->object=aliased_source;
 unsigned char original[sizeof(storage)];memcpy(original,&storage,sizeof(storage));
 assert(!intel_ggtt_invalidate_run(aliased_result,aliased_source,io()));
 assert(!model.events && !memcmp(original,&storage,sizeof(storage)));
 setup();assert(run());
 struct intel_ggtt_invalidate copied_result;memcpy(&copied_result,&result,sizeof(result));
 unsigned prior_events=model.events;assert(!intel_ggtt_invalidate_run(&copied_result,&source,io()));assert(prior_events==model.events);
 /* Rebinding the copy's self pointer still cannot steal original registration. */
 copied_result.object=&copied_result;
 assert(!intel_ggtt_invalidate_run(&copied_result,&source,io()));assert(prior_events==model.events);
 printf("intel-ggtt-invalidate: PASS cases=%u events=%u clocks=%u prepare_us=%llu; synthetic only, no native provider or GPU grant\n",cases,events,clocks,(unsigned long long)INTEL_GGTT_INVALIDATE_PREPARE_US);
 return 0;
}
