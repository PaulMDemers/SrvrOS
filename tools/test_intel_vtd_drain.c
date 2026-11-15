#include <srvros/intel_vtd_drain.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

static struct intel_vtd_drain result;
static struct intel_vtd_drain_source source;
static int owner,lifetime;
static unsigned cases;
enum fault {NONE,GUARD_FAIL,READ_FAIL,FLOAT_READ,BAD_READ,CLOCK_ZERO,CLOCK_BACK,CLOCK_EXPIRE,
 SOURCE_DRIFT,PROGRESS_DRIFT,IO_DRIFT,WRITE_NONE,WRITE_UNCERTAIN,WRITE_ENUM,REENTER};
static struct {
 enum fault fault;
 unsigned at,field,events,clocks,guards,reads,writes,issued,pauses,polls,latency,boundary_writes;
 uint64_t time,fixed;
 uint32_t bad,offset;
 bool match_offset,previous,stuck;
} model;
static struct intel_vtd_drain_io io(void);
static void drift_source(unsigned n) {
 switch(n) {
 case 0:source.binding.owner=0;break;case 1:source.binding.provider=&lifetime;break;
 case 2:source.binding.lifetime=0;break;case 3:source.binding.root+=4096;break;
 case 4:source.binding.epoch++;break;case 5:source.binding.sequence++;break;
 case 6:source.binding.hash^=1;break;case 7:source.binding.unit+=4096;break;
 case 8:source.binding.prepare_begin++;break;case 9:source.binding.gt_begin++;break;
 case 10:source.binding.hda_begin++;break;case 11:source.binding.outer_begin++;break;
 case 12:source.binding.vendor++;break;case 13:source.binding.device++;break;
 case 14:source.object=0;break;case 15:source.consumer=0;break;
 default:assert(false);
 }
}
static void drift_progress(unsigned n) {
 switch(n) {
 case 0:result.p.begin++;break;case 1:result.p.last++;break;case 2:result.p.state++;break;
 case 3:result.p.reads++;break;case 4:result.p.guards++;break;case 5:result.p.attempts++;break;
 case 6:result.p.writes++;break;case 7:result.p.polls++;break;case 8:result.p.uncertain=1;break;
 case 9:result.p.initial++;break;case 10:result.p.final++;break;case 11:result.p.context[0][0]++;break;
 case 12:result.p.context[1][7]++;break;case 13:result.p.observed[0]++;break;
 case 14:result.p.observed[255]++;break;case 15:result.object=0;break;case 16:result.source=0;break;
 case 17:result.reason=VTD_DRAIN_RESULT;break;case 18:result.end=1;break;case 19:result.clock_at=1;break;
 case 20:result.clock_previous=1;break;case 21:result.clock_reason=1;break;case 22:result.busy=false;break;
 case 23:result.complete=true;break;case 24:result.retained=true;break;
 case 25:result.binding.root+=4096;source.binding.root+=4096;break;
 case 26:result.binding.epoch++;source.binding.epoch++;break;
 case 27:result.binding.prepare_begin++;source.binding.prepare_begin++;break;
 default:assert(false);
 }
}
static void event(void) {
 ++model.events;
 if(model.events==model.at) {
  model.boundary_writes=model.writes;
  if(model.fault==SOURCE_DRIFT)drift_source(model.field);
  if(model.fault==PROGRESS_DRIFT)drift_progress(model.field);
  if(model.fault==IO_DRIFT) {
   switch(model.field) {
   case 0:result.io.ctx=0;break;case 1:result.io.guard=0;break;case 2:result.io.read=0;break;
   case 3:result.io.write=0;break;case 4:result.io.now=0;break;case 5:result.io.pause=0;break;
   case 6:result.io.ctx=result.original_io.ctx=0;break;
   case 7:result.io.guard=result.original_io.guard=0;break;
   case 8:result.io.read=result.original_io.read=0;break;
   case 9:result.io.write=result.original_io.write=0;break;
   case 10:result.io.now=result.original_io.now=0;break;
   case 11:result.io.pause=result.original_io.pause=0;break;
   default:assert(false);
   }
  }
 }
 if(model.fault==REENTER) {
  unsigned count=model.events;
  assert(!intel_vtd_drain_run(&result,&source,io()));assert(model.events==count);
 }
}
static bool guard(void *ctx,const struct intel_vtd_drain_source *s) {
 assert(ctx==&model);++model.guards;event();
 return !(model.fault==GUARD_FAIL && model.guards==model.at) && s==&source && s->consumer==&result &&
  s->binding.owner==&owner && s->binding.provider==&model && s->binding.lifetime==&lifetime;
}
static bool read_word(void *ctx,uint32_t off,uint32_t *out) {
 assert(ctx==&model && out);++model.reads;event();
 if(model.fault==READ_FAIL && model.reads==model.at)return false;
 if(model.fault==FLOAT_READ && model.reads==model.at){*out=UINT32_MAX;return true;}
 bool hit=model.match_offset?off==model.offset:model.reads==model.at;
 if(model.fault==BAD_READ && hit){*out=model.bad;return true;}
 switch(off) {
 case 0:*out=0x10;break;
 case 8:*out=(uint32_t)0x01c0000c40660462ull;break;
 case 12:*out=0x01c0000c;break;
 case 16:*out=(uint32_t)0x7e1ff0505eull;break;
 case 20:*out=0x7e;break;
 case 28:case 32:case 36:case 40:case 52:case 0x508:*out=0;break;
 case 44:*out=0x08000000;break;
 case 0x50c:
  if(!model.issued)*out=model.previous?0x12030000:0x02000000;
  else {*out=model.stuck || model.polls<model.latency?0x90030000:0x12030000;++model.polls;}
  break;
 default:assert(false);return false;
 }
 return true;
}
static enum intel_gt_write write_word(void *ctx,uint32_t off,uint32_t v) {
 assert(ctx==&model && off==0x50c && v==0x90030000);
 assert(result.p.state==VTD_DRAIN_REQUESTING && result.p.attempts==1 && !model.writes && !model.issued);
 ++model.writes;event();
 if(model.fault==WRITE_NONE)return GT_WRITE_NONE;
 ++model.issued;
 if(model.fault==WRITE_UNCERTAIN)return GT_WRITE_UNCERTAIN;
 if(model.fault==WRITE_ENUM)return (enum intel_gt_write)99;
 return GT_WRITE_DONE;
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
static void pause_control(void *ctx){assert(ctx==&model);++model.pauses;event();}
static struct intel_vtd_drain_io io(void) {
 return (struct intel_vtd_drain_io){&model,guard,read_word,write_word,now,pause_control};
}
static void setup(void) {
 memset(&result,0,sizeof(result));memset(&source,0,sizeof(source));memset(&model,0,sizeof(model));
 source.object=&source;
 source.binding=(struct intel_vtd_drain_binding){.owner=&owner,.provider=&model,.lifetime=&lifetime,
 .root=0x1000,.epoch=7,.sequence=3,.hash=0x722775ad66626863ull,.unit=0xfed90000,
 .prepare_begin=1000,.gt_begin=900,.hda_begin=800,.outer_begin=700,.vendor=0x8086,.device=0x1626};
 model.time=1000;++cases;
}
static bool run(void){return intel_vtd_drain_run(&result,&source,io());}
static void refused(void) {
 assert(!run());assert(!result.complete && !result.end);
 assert(result.reason && result.retained && !result.busy && result.p.state==VTD_DRAIN_FAULT);
 unsigned before=model.events;assert(!run());assert(model.events==before);
 if(source.consumer) {struct intel_vtd_drain copy={0};assert(!intel_vtd_drain_run(&copy,&source,io()));assert(model.events==before);}
 assert(model.writes<=1);
}
static void success(void) {
 assert(run());assert(result.complete && !result.reason && !result.retained && !result.busy);
 assert(result.p.state==VTD_DRAIN_COMPLETE && result.end==result.p.last && result.end>result.p.begin);
 assert(result.p.attempts==1 && result.p.writes==1 && !result.p.uncertain && model.writes==1 && model.issued==1);
 assert(result.p.polls==model.latency+1 && result.p.final==0x1203000000000000ull);
 assert(result.p.reads==43+7*result.p.polls && result.p.guards==45+8*result.p.polls);
 assert(model.reads==result.p.reads && model.guards==result.p.guards && model.pauses==model.latency);
 assert(result.p.context[0][2]==0x01c0000c40660462ull && result.p.context[0][3]==0x7e1ff0505eull);
 for(unsigned i=0;i<8;i++)assert(result.p.context[0][i]==result.p.context[1][i]);
 for(unsigned i=0;i<result.p.polls;i++)assert(result.p.observed[i]==(i<model.latency?0x90030000u:0x12030000u));
 unsigned before=model.events;assert(!run());assert(model.events==before);
}
int main(void) {
 setup();success();unsigned clocks=model.clocks,guards=model.guards,reads=model.reads;
 setup();model.latency=2;success();unsigned events=model.events,fault_clocks=model.clocks,fault_guards=model.guards,fault_reads=model.reads;
 setup();model.previous=true;success();
 setup();model.latency=255;success();
 setup();model.stuck=true;refused();assert(result.reason&VTD_DRAIN_PENDING);assert(result.p.polls==256 && model.issued==1);
 setup();model.fault=REENTER;model.latency=2;success();
 for(unsigned n=1;n<=fault_clocks;n++)for(unsigned f=CLOCK_ZERO;f<=CLOCK_EXPIRE;f++) {
  setup();model.latency=2;model.at=n;model.fault=(enum fault)f;refused();assert(result.reason&VTD_DRAIN_CLOCK);
 }
 for(unsigned n=1;n<=fault_guards;n++){setup();model.latency=2;model.at=n;model.fault=GUARD_FAIL;refused();assert(result.reason&VTD_DRAIN_GUARD);}
 for(unsigned n=1;n<=fault_reads;n++)for(unsigned f=READ_FAIL;f<=FLOAT_READ;f++) {
  setup();model.latency=2;model.at=n;model.fault=(enum fault)f;refused();assert(result.reason&VTD_DRAIN_READ);
 }
 for(unsigned n=1;n<=events;n++)for(unsigned f=SOURCE_DRIFT;f<=IO_DRIFT;f++) {
  unsigned fields=f==SOURCE_DRIFT?16:f==PROGRESS_DRIFT?28:12;
  for(unsigned field=0;field<fields;field++) {
   setup();model.latency=2;model.at=n;model.fault=(enum fault)f;model.field=field;refused();
   assert(model.writes==model.boundary_writes);assert(result.reason&VTD_DRAIN_SOURCE);
  }
 }
 for(unsigned f=WRITE_NONE;f<=WRITE_ENUM;f++) {
  setup();model.fault=(enum fault)f;refused();assert(result.reason&VTD_DRAIN_WRITE);
  assert(result.p.attempts==1 && !result.p.writes && !result.p.polls && model.writes==1);
  assert(result.p.uncertain==(f!=WRITE_NONE));assert(model.issued==(f!=WRITE_NONE));
 }
 /* Fault every allowed register at each read position, including torn pairs,
  * floating/reserved response, wrong granularity and a pending final recheck. */
 const uint32_t offsets[]={0,8,12,16,20,28,32,36,40,44,52,0x508,0x50c};
 const uint32_t bad[]={0,1,0x80000000,0xffffffff,0x04000000,0x14030000,0x10030000,0x16030000,0x12020000,0x12010000,0x12030001,0x92030000};
 for(unsigned i=0;i<sizeof(offsets)/sizeof(*offsets);i++)for(unsigned j=0;j<sizeof(bad)/sizeof(*bad);j++) {
  /* Only test values known wrong for the selected register. */
  if((offsets[i]==28 || offsets[i]==32 || offsets[i]==36 || offsets[i]==40 || offsets[i]==52 || offsets[i]==0x508) && !bad[j])continue;
  setup();model.fault=BAD_READ;model.match_offset=true;model.offset=offsets[i];model.bad=bad[j];refused();
  if(offsets[i]==28)assert(model.reads==1 && !model.writes);
  if(offsets[i]==52)assert(model.reads==15 && !model.writes);
 }
 for(unsigned j=0;j<sizeof(bad)/sizeof(*bad);j++) {
  /* First fresh IVT response is read30. Semantic refusal follows its two
   * policy reads, and must stop before any final-context/read callback. */
  if(bad[j]==0x92030000u)continue; /* legal still-pending response */
  setup();model.fault=BAD_READ;model.at=30;model.bad=bad[j];refused();
  assert(model.reads<=32 && model.writes==1);
 }
 for(unsigned n=1;n<=reads;n++) {
  setup();model.at=n;model.fault=BAD_READ;model.bad=0xdeadbeef;refused();if(n<=25)assert(!model.writes);assert(result.reason&(VTD_DRAIN_PROFILE|VTD_DRAIN_BUSY|VTD_DRAIN_RESULT));
 }
 /* All clocks at their exact original deadline are accepted, +1 is refused.
  * Isolate each parent's budget without another smaller one expiring first. */
 for(unsigned n=0;n<4;n++)for(unsigned over=0;over<2;over++) {
  setup();model.fixed=501000;struct intel_vtd_drain_binding *b=&source.binding;
  b->outer_begin=1;b->hda_begin=150000;b->gt_begin=250000;b->prepare_begin=450000;
  if(n==0)b->prepare_begin=model.fixed-100000-over;
  if(n==1)b->gt_begin=model.fixed-300000-over;
  if(n==2)b->hda_begin=model.fixed-400000-over;
  if(n==3)b->outer_begin=model.fixed-500000-over;
  /* The other start times need headroom; fixed clock is allowed until the
   * separate end-advancement gate, but must not bypass the chosen deadline. */
  if(n!=3)b->outer_begin=10000;
  if(!over){refused();assert(result.clock_reason==VTD_DRAIN_FROZEN && model.issued==1);}
  else {refused();assert(result.clock_reason==VTD_DRAIN_PREPARE_EXPIRED+n && !model.writes);}
 }
 for(unsigned i=0;i<16;i++) {
  setup();drift_source(i);unsigned before=model.events;
  if(i==8 || i==9 || i==10 || i==11 || i==4 || i==5 || i==3 || i==15)continue; /* structurally valid alternate descriptor */
  assert(!run() && model.events==before && !model.writes);
 }
 setup();struct intel_vtd_drain_io broken=io();broken.write=0;assert(!intel_vtd_drain_run(&result,&source,broken) && !model.events);
 setup();assert(!intel_vtd_drain_run(&result,(struct intel_vtd_drain_source *)&result,io()) && !model.events);
 printf("intel-vtd-drain: PASS cases=%u baseline clocks=%u guards=%u reads=%u mutation_boundaries=%u; synthetic only, no native writer or ownership grant\n",cases,clocks,guards,reads,events);
}
