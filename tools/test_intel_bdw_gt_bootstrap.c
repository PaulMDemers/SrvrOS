/* Independent register device model. Synthetic admission is host evidence only. */
#include <srvros/intel_bdw_gt_bootstrap.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct intel_bdw_gt_bootstrap result,copy;
static struct bdw_boot_source source,source_copy;
static unsigned owner,lifetime,other;
static const uint32_t device_controls[5]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0};
enum mode {GOOD,KEEP_FENCES,MANY_FENCES,MIXED_FENCES,REENTER,OBJECT,REGISTRATION,BUSY,COMPLETE,
 SOURCE_POINTER,SOURCE_SEAL,SOURCE_CHILD,BIND_ROOT,BIND_EPOCH,IO_TABLE,COUNTER,TIME,VECTOR,
 ZERO,BACK,OVER,EXACT,FROZEN,PROOF_FALSE,PROOF_BITS,PROOF_ROOT,PROOF_EPOCH,PROOF_OWNER,
 PROOF_PROVIDER,PROOF_LIFETIME,PROOF_SEQUENCE,PROOF_SOURCE,PROOF_STALE,PROOF_FUTURE,
 READ_FALSE,READ_FLOAT,WRITE_NONE,WRITE_UNKNOWN,WRITE_CALLBACK,EPOCH_FALSE,PRESERVE_FALSE,
 FOREIGN_INITIAL,FOREIGN_POST,FENCE_DRIFT,TORN_FENCE,RESET_PENDING,RESET_NEVER,CANCEL_PENDING,CANCEL_NEVER,
 LATE_FENCE,LATE_DOMAIN,EARLY_FENCE_ACCESS};
struct event {unsigned tag,stage,index,clocks;uint32_t writes;uint64_t phase;};
static struct event events[65536],baseline[65536];
static unsigned baseline_events,baseline_clocks,baseline_proofs,baseline_reads,baseline_writes;
static struct {
 enum mode mode;unsigned at,events,clocks,proofs,reads,attempts,writes,advances,preserves;
 unsigned hit_event,hit_tag,writes_at_hit,domain,bit,reset_left,cancel_left[5],reset_wait,cancel_wait,pattern;
 uint64_t clock,limit,epoch,original[32],fences[32];uint32_t ctl[5],gdrst,fence_phase[32];
 uint32_t expected_offset[103],expected_value[103];unsigned expected_count;
 bool hit,record,clients_stopped,live;
} m;
static struct bdw_boot_io io(void);
static unsigned cases;
static void mark(unsigned tag) {
 if(!m.hit){m.hit=true;m.hit_event=m.events;m.hit_tag=tag;m.writes_at_hit=m.writes;}
}
static void hook(unsigned tag) {
 ++m.events;assert(m.events<65536);
 if(m.record)events[m.events]=(struct event){tag,result.stage,result.index,m.clocks,result.writes,result.phase_begin};
 if(m.events!=m.at)return;
 switch(m.mode){
 case REENTER:{unsigned before=m.events;assert(!intel_bdw_gt_bootstrap_run(&result,&source,io()));assert(before==m.events);mark(tag);break;}
 case OBJECT:result.object=NULL;mark(tag);break;
 case REGISTRATION:result.registered=false;mark(tag);break;
 case BUSY:result.busy=false;mark(tag);break;
 case COMPLETE:result.complete=true;mark(tag);break;
 case SOURCE_POINTER:source_copy=source;source_copy.object=&source_copy;result.source=&source_copy;mark(tag);break;
 case SOURCE_SEAL:source.object=&source_copy;mark(tag);break;
 case SOURCE_CHILD:source.consumer=&copy;mark(tag);break;
 case BIND_ROOT:source.binding.root^=4096;mark(tag);break;
 case BIND_EPOCH:source.binding.epoch++;mark(tag);break;
 case IO_TABLE:result.io.read=NULL;mark(tag);break;
 case COUNTER:result.reads++;mark(tag);break;
 case TIME:result.last++;mark(tag);break;
 case VECTOR:result.fence_before[31]^=4096;mark(tag);break;
 default:break;
 }
}
static bool fault(enum mode mode,unsigned index,unsigned tag) {
 if(m.mode==mode && m.at==index){mark(tag);return true;}return false;
}
static uint64_t now(void *ctx) {
 assert(ctx==&m);++m.clocks;hook('n');
 if(fault(ZERO,m.clocks,'n'))return 0;
 if(fault(BACK,m.clocks,'n'))return result.last?result.last-1:0;
 if(fault(OVER,m.clocks,'n') || fault(EXACT,m.clocks,'n')){m.clock=m.limit+(m.mode==OVER);return m.clock;}
 if(m.mode==FROZEN)return m.clock;
 return ++m.clock;
}
static bool proof(void *ctx,const struct bdw_boot_source *s,struct bdw_boot_proof *p) {
 assert(ctx==&m);++m.proofs;hook('p');
 /* Independent model authority, not copied from the supplied descriptor. */
 *p=(struct bdw_boot_proof){&source,&owner,&m,&lifetime,0x4000,m.epoch,1,m.clock,BDW_BOOT_REQUIRED};
 if(fault(PROOF_FALSE,m.proofs,'p'))return false;
 if(fault(PROOF_BITS,m.proofs,'p'))p->prerequisites&=~(1u<<m.bit);
 if(fault(PROOF_ROOT,m.proofs,'p'))p->root^=4096;
 if(fault(PROOF_EPOCH,m.proofs,'p'))p->epoch++;
 if(fault(PROOF_OWNER,m.proofs,'p'))p->owner=&other;
 if(fault(PROOF_PROVIDER,m.proofs,'p'))p->provider=&other;
 if(fault(PROOF_LIFETIME,m.proofs,'p'))p->lifetime=&other;
 if(fault(PROOF_SEQUENCE,m.proofs,'p'))p->sequence++;
 if(fault(PROOF_SOURCE,m.proofs,'p'))p->source=&source_copy;
 if(fault(PROOF_STALE,m.proofs,'p'))p->checked_at=result.last-1;
 if(fault(PROOF_FUTURE,m.proofs,'p'))p->checked_at=UINT64_MAX;
 return s==&source && m.live && m.clients_stopped;
}
static bool read_reg(void *ctx,uint32_t offset,uint32_t *value) {
 assert(ctx==&m);++m.reads;hook('r');
 if(fault(READ_FALSE,m.reads,'r'))return false;
 if(offset==0x941c){
  if(m.mode==FOREIGN_INITIAL && !m.attempts)*value=8;
  else if(m.mode==FOREIGN_POST && result.stage==BDW_BOOT_POST)*value=8;
  else if(m.mode==LATE_DOMAIN && result.stage==BDW_BOOT_VERIFY)*value=8;
  else if(m.gdrst && m.mode==RESET_NEVER)*value=1;
  else if(m.gdrst && m.reset_left){--m.reset_left;*value=1;}
  else {*value=0;m.gdrst=0;}
 }else if(offset>=0x100000 && offset<0x100100 && !(offset&3)){
  unsigned i=(offset-0x100000)/8;bool high=(offset&4)!=0;
  uint64_t v=m.fences[i];
  if(m.mode==FENCE_DRIFT && result.stage==BDW_BOOT_POST && !i)v=0x4001;
  if(m.mode==LATE_FENCE && result.stage==BDW_BOOT_VERIFY && !i)v^=4096;
  *value=high?(uint32_t)(v>>32):(uint32_t)v;
  if(m.mode==TORN_FENCE && m.reads==3)*value^=4096;
  if(!high && m.fence_phase[i]==1)m.fence_phase[i]=2;
  else if(!high && m.fence_phase[i]==4)m.fence_phase[i]=5;
 }else{
  unsigned e;for(e=0;e<5;e++)if(offset==device_controls[e])break;
  assert(e<5);
  if(result.stage==BDW_BOOT_CANCEL && m.mode==CANCEL_NEVER)*value=3;
  else if(result.stage==BDW_BOOT_CANCEL && m.cancel_left[e]){--m.cancel_left[e];*value=3;}
  else *value=m.ctl[e];
 }
 if(fault(READ_FLOAT,m.reads,'r'))*value=UINT32_MAX;
 return true;
}
static void expected(uint32_t offset,uint32_t value) {
 assert(m.expected_count<103);m.expected_offset[m.expected_count]=offset;m.expected_value[m.expected_count++]=value;
}
static void issue(uint32_t offset,uint32_t value) {
 assert(m.live && m.clients_stopped);
 for(unsigned i=0;i<32;i++)assert(m.fence_phase[i]!=4);
 assert(m.writes<m.expected_count && offset==m.expected_offset[m.writes] && value==m.expected_value[m.writes]);
 if(offset==0x941c){
  assert(m.advances==1 && m.epoch==8 && value==1);
  m.gdrst=1;m.reset_left=m.reset_wait;
  for(unsigned i=0;i<5;i++)m.ctl[i]=0;
  if(m.mode!=KEEP_FENCES)for(unsigned i=0;i<32;i++)if(m.mode!=MIXED_FENCES || !(i&1))m.fences[i]=0;
 }else if(offset>=0x100000 && offset<0x100100){
  unsigned i=(offset-0x100000)/8;
  if(offset&4){assert(m.fence_phase[i]==2 && !(m.fences[i]&1));m.fence_phase[i]=3;m.fences[i]=(m.fences[i]&UINT32_MAX)|(uint64_t)value<<32;}
  else {
   if(!m.fence_phase[i]){assert(value==0);m.fence_phase[i]=1;}
   else {assert(m.fence_phase[i]==3);m.fence_phase[i]=4;}
   m.fences[i]=(m.fences[i]&0xffffffff00000000ull)|value;
  }
 }else{
  unsigned e;for(e=0;e<5;e++)if(offset==device_controls[e])break;
  assert(e<5 && value==0x10000 && m.gdrst==0);m.ctl[e]=0;m.cancel_left[e]=m.cancel_wait;
 }
 ++m.writes;
}
static enum intel_gt_write write_reg(void *ctx,uint32_t offset,uint32_t value) {
 assert(ctx==&m);++m.attempts;hook('w');
 if(fault(WRITE_NONE,m.attempts,'w'))return GT_WRITE_NONE;
 issue(offset,value);
 if(fault(WRITE_UNKNOWN,m.attempts,'w'))return GT_WRITE_UNCERTAIN;
 if(fault(WRITE_CALLBACK,m.attempts,'w')){result.registered=false;return GT_WRITE_DONE;}
 return GT_WRITE_DONE;
}
static bool epoch(void *ctx,const struct bdw_boot_source *s,uint64_t before,uint64_t after) {
 assert(ctx==&m);hook('e');assert(s==&source && before==7 && after==8 && m.epoch==7 && !m.advances && !m.attempts);
 if(m.mode==EPOCH_FALSE){mark('e');return false;}
 ++m.advances;m.epoch=8;return true;
}
static bool preserved(void *ctx,const struct intel_bdw_gt_bootstrap *r) {
 assert(ctx==&m);++m.preserves;hook('v');assert(r==&result);
 if(m.mode==PRESERVE_FALSE){mark('v');return false;}
 if(m.mode==EARLY_FENCE_ACCESS){m.clients_stopped=false;mark('v');return false;}
 return m.writes==m.expected_count && !m.gdrst && !memcmp(m.fences,m.original,sizeof(m.fences));
}
static void pause_tick(void *ctx){assert(ctx==&m);hook('s');}
static struct bdw_boot_io io(void){return (struct bdw_boot_io){&m,proof,read_reg,write_reg,epoch,preserved,now,pause_tick};}
static void setup(enum mode mode,unsigned at,unsigned bit,unsigned domain) {
 memset(&result,0,sizeof(result));memset(&copy,0,sizeof(copy));memset(&source,0,sizeof(source));memset(&m,0,sizeof(m));
 m.mode=mode;m.at=at;m.bit=bit;m.domain=domain;m.epoch=7;m.clock=1000000;m.live=true;m.clients_stopped=true;m.record=mode==GOOD;
 m.reset_wait=mode==RESET_PENDING?255:0;m.cancel_wait=mode==CANCEL_PENDING?255:0;
 source.binding=(struct bdw_boot_binding){.owner=&owner,.provider=&m,.lifetime=&lifetime,.root=0x4000,
 .mmio=0xc0000000,.mmio_bytes=0x1000000,.aperture_bytes=0x10000000,.epoch=7,.sequence=1,
 .prepare=999000,.gt=998000,.hda=995000,.outer=990000,.vendor=0x8086,.device=0x1626,.policy=BDW_BOOT_POLICY};
 m.original[0]=0x021ff02f00000001ull;
 if(mode==MANY_FENCES || mode==MIXED_FENCES)for(unsigned i=0;i<32;i++)m.original[i]=((uint64_t)i*8192<<32)|3ull<<32|(uint64_t)i*8192|1;
 memcpy(m.fences,m.original,sizeof(m.fences));memcpy(source.binding.fences,m.original,sizeof(m.original));
 for(unsigned i=0;i<5;i++)m.ctl[i]=3;
 expected(0x941c,1);expected(0x941c,1);
 for(unsigned i=0;i<32;i++)if(m.original[i] && mode!=KEEP_FENCES && (mode!=MIXED_FENCES || !(i&1))){
  expected(0x100000+8*i,0);expected(0x100004+8*i,(uint32_t)(m.original[i]>>32));expected(0x100000+8*i,(uint32_t)m.original[i]);
 }
 for(unsigned i=0;i<5;i++)expected(device_controls[4-i],0x10000);
 if(mode==OVER || mode==EXACT){
  uint64_t cap=domain==0?INTEL_DIAGNOSTIC_PREPARE_US:domain==1?300000:domain==2?400000:500000;
  if(domain){
   source.binding.outer=1000000;source.binding.hda=1000000;source.binding.gt=1000000;
   source.binding.prepare=1000000+cap-50000;
   if(domain>1)source.binding.gt=source.binding.prepare;
   if(domain>2)source.binding.hda=source.binding.prepare;
   m.clock=source.binding.prepare+1000;
  }
  const uint64_t origins[]={source.binding.prepare,source.binding.gt,source.binding.hda,source.binding.outer};m.limit=origins[domain]+cap;
 }
}
static void check(bool ok,bool wanted) {
 ++cases;
 if(ok!=wanted){fprintf(stderr,"case=%u mode=%u at=%u events=%u clocks=%u reason=%u stage=%u wanted=%u got=%u hit=%u\n",cases,m.mode,m.at,m.events,m.clocks,result.reason,result.stage,wanted,ok,m.hit);abort();}
 assert(result.object==&result);
 if(ok){
  assert(result.complete && !result.registered && !result.busy && !result.retained && !result.reason && !result.uncertain);
  assert(result.stage==BDW_BOOT_DONE && result.epoch_attempted && result.epoch_advanced && m.advances==1);
  assert(result.reset_attempts==2 && result.reset_writes==2 && result.reset_completed==2 && result.cancelled==5);
  assert(result.cancel_attempts==5 && result.cancel_writes==5 && result.changed_mask==result.restored_mask);
  assert(result.reads==m.reads && result.read_attempts==m.reads && result.writes==m.writes && result.write_attempts==m.attempts);
  assert(m.writes==m.expected_count && result.preserved && result.preserve_calls==1 && m.preserves==1);
  assert(!memcmp(result.fence_before,m.original,sizeof(m.original)) && !memcmp(result.fence_final,m.original,sizeof(m.original)));
  assert(result.settle_end-result.settle_begin>=50 && result.end>result.begin && result.end==result.last);
  unsigned changed=0;for(unsigned i=0;i<32;i++)changed+=(result.changed_mask>>i)&1;
  assert(result.fence_writes==3*changed && result.fence_attempts==result.fence_writes);
  unsigned reads=301+result.reset_polls[0]+result.reset_polls[1]+2*changed;
  for(unsigned i=0;i<5;i++)reads+=result.cancel_polls[i];
  assert(result.reads==reads);
 }else{
  assert(!result.complete && result.registered && result.retained && result.busy && result.reason && result.stage==BDW_BOOT_FAULT);
  if(m.mode==RESET_NEVER)assert(result.reset_polls[0]==256 && m.writes==1);
  if(m.mode==CANCEL_NEVER)assert(result.cancel_polls[4]==256 && result.cancelled==0 && m.attempts==6);
  if(m.hit && m.mode!=REENTER && m.mode!=EXACT)assert(m.writes<=m.writes_at_hit+(m.hit_tag=='w'));
 }
 unsigned events_before=m.events;assert(!intel_bdw_gt_bootstrap_run(&result,&source,io()));assert(m.events==events_before);
}
static void run_case(enum mode mode,unsigned at,bool wanted,unsigned bit,unsigned domain) {
 setup(mode,at,bit,domain);check(intel_bdw_gt_bootstrap_run(&result,&source,io()),wanted);
}
static void invalid_inputs(void) {
 for(unsigned n=0;n<22;n++){
  setup(GOOD,0,0,0);struct bdw_boot_io callbacks=io();
  switch(n){
   case 0:source.binding.policy=0;break;case 1:source.binding.vendor=0x1234;break;case 2:source.binding.device=0x1616;break;
   case 3:source.binding.root=0;break;case 4:source.binding.root++;break;case 5:source.binding.mmio++;break;
   case 6:source.binding.mmio_bytes=0x100000;break;case 7:source.binding.aperture_bytes=0;break;
   case 8:source.binding.epoch=UINT32_MAX;break;case 9:source.binding.sequence=2;break;
   case 10:source.binding.outer=0;break;case 11:source.binding.prepare=source.binding.gt-1;break;
   case 12:source.binding.fences[0]|=4;break;case 13:source.binding.fences[0]|=1ull<<43;break;
   case 14:source.binding.fences[1]=source.binding.fences[0];break;case 15:source.binding.fences[0]=0x0000000000000001ull;break;
   case 16:callbacks.prerequisite=NULL;break;case 17:callbacks.advance_epoch=NULL;break;case 18:callbacks.preserved=NULL;break;
   case 19:source.binding.provider=&other;break;case 20:source.object=&source_copy;break;case 21:source.consumer=&copy;break;
  }
  assert(!intel_bdw_gt_bootstrap_run(&result,&source,callbacks) && !m.events && !result.registered && !result.retained);++cases;
 }
 /* Never destroy an already consumed source before refusing an overlapping
  * result, including a result beginning at the last inactive fence. */
 for(unsigned i=0;i<3;i++){
  setup(GOOD,0,0,0);
  unsigned char before[sizeof(result)+sizeof(source)+64];
  unsigned char *arena=calloc(1,sizeof(before));assert(arena);
  unsigned source_offset=i==2?128:0;
  unsigned result_offset=i==1?(unsigned)((unsigned char *)&source.binding.fences[31]-(unsigned char *)&source):0;
  struct bdw_boot_source *s=(void *)(arena+source_offset);
  struct intel_bdw_gt_bootstrap *r=(void *)(arena+result_offset);
  memcpy(s,&source,sizeof(source));s->object=s;s->consumer=&result;
  memcpy(before,arena,sizeof(before));
  assert(!intel_bdw_gt_bootstrap_run(r,s,io()) && !m.events && !memcmp(before,arena,sizeof(before)));free(arena);++cases;
 }
}
static void deadlines(void) {
 for(unsigned domain=0;domain<4;domain++){
  run_case(EXACT,baseline_clocks,true,0,domain);run_case(OVER,baseline_clocks,false,0,domain);
 }
 /* Every reset, settle and cancellation phase has an inclusive10ms endpoint.
  * No later phase may buy time for an earlier incomplete one. */
 for(unsigned phase=0;phase<8;phase++){
  unsigned stage=phase<2?BDW_BOOT_RESET:phase==2?BDW_BOOT_SETTLE:BDW_BOOT_CANCEL;
  unsigned index=phase<2?phase:phase==2?1:7-phase;
  unsigned event=0;
  for(unsigned i=1;i<=baseline_events;i++)if(baseline[i].tag=='n' && baseline[i].stage==stage &&
    baseline[i].index==index && baseline[i].phase)event=i;
  assert(event);
  for(unsigned extra=0;extra<2;extra++){
   setup(extra?OVER:EXACT,baseline[event].clocks,0,0);m.limit=baseline[event].phase+10000;
   check(intel_bdw_gt_bootstrap_run(&result,&source,io()),!extra);
   if(extra)assert(m.writes==baseline[event].writes && result.clock_reason==8);
  }
 }
 /* A slow pre-store admission at the exact endpoint may issue one store, but
  * its subsequent overrun still fails. At endpoint+1 it must issue none. */
 for(unsigned cancel=0;cancel<2;cancel++){
  unsigned event=0,stage=cancel?BDW_BOOT_CANCEL:BDW_BOOT_RESET,index=cancel?4:0;
  unsigned prefix=cancel?5:0;
  for(unsigned i=1;i<=baseline_events;i++)if(baseline[i].tag=='n' && baseline[i].stage==stage &&
    baseline[i].index==index && baseline[i].writes==prefix && baseline[i].phase)event=i;
  assert(event);
  for(unsigned extra=0;extra<2;extra++){
   setup(extra?OVER:EXACT,baseline[event].clocks,0,0);m.limit=baseline[event].phase+10000;
   check(intel_bdw_gt_bootstrap_run(&result,&source,io()),false);
   assert(m.writes==prefix+!extra && result.clock_reason==8);
  }
 }
}
int main(int argc,char **argv) {
 bool quick=argc>1 && !strcmp(argv[1],"--sanitizer");
 run_case(GOOD,0,true,0,0);baseline_events=m.events;baseline_clocks=m.clocks;baseline_proofs=m.proofs;baseline_reads=m.reads;baseline_writes=m.writes;
 memcpy(baseline,events,sizeof(events));
 printf("bootstrap baseline events=%u clocks=%u proofs=%u reads=%u writes=%u prepare_us=%llu\n",baseline_events,baseline_clocks,baseline_proofs,baseline_reads,baseline_writes,(unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US);fflush(stdout);
 invalid_inputs();
 const enum mode positives[]={KEEP_FENCES,MANY_FENCES,MIXED_FENCES,RESET_PENDING,CANCEL_PENDING};
 for(unsigned i=0;i<sizeof(positives)/sizeof(*positives);i++)run_case(positives[i],0,true,0,0);
 const enum mode terminal[]={FROZEN,EPOCH_FALSE,PRESERVE_FALSE,FOREIGN_INITIAL,FOREIGN_POST,FENCE_DRIFT,TORN_FENCE,RESET_NEVER,CANCEL_NEVER,LATE_FENCE,LATE_DOMAIN,EARLY_FENCE_ACCESS};
 for(unsigned i=0;i<sizeof(terminal)/sizeof(*terminal);i++)run_case(terminal[i],0,false,0,0);
 deadlines();
 for(unsigned i=1;i<=baseline_events;i++){
  if(quick && i!=1 && i!=baseline_events && i!=baseline_events/2)continue;
  for(enum mode mode=REENTER;mode<=VECTOR;mode++)run_case(mode,i,mode==REENTER,0,0);
 }
 printf("bootstrap sealed callback cases=%u\n",cases);fflush(stdout);
 for(unsigned i=1;i<=baseline_clocks;i++){
  if(quick && i!=1 && i!=baseline_clocks && i!=baseline_clocks/2)continue;
  run_case(ZERO,i,false,0,0);run_case(BACK,i,false,0,0);run_case(OVER,i,false,0,0);
 }
 for(unsigned i=1;i<=baseline_proofs;i++){
  if(quick && i!=1 && i!=baseline_proofs && i!=baseline_proofs/2)continue;
  for(enum mode mode=PROOF_FALSE;mode<=PROOF_FUTURE;mode++){
   if(mode==PROOF_BITS){for(unsigned bit=0;bit<8;bit++)run_case(mode,i,false,bit,0);}
   else run_case(mode,i,false,0,0);
  }
 }
 for(unsigned i=1;i<=baseline_reads;i++){run_case(READ_FALSE,i,false,0,0);run_case(READ_FLOAT,i,false,0,0);}
 for(unsigned i=1;i<=baseline_writes;i++)for(enum mode mode=WRITE_NONE;mode<=WRITE_CALLBACK;mode++)run_case(mode,i,false,0,0);
 printf("bootstrap: PASS cases=%u policies=%llu bounded reset/fences only; no native provider or GPU grant\n",cases,(unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US);
 return 0;
}
