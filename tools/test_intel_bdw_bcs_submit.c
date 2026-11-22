#include <srvros/intel_bdw_bcs_submit.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

static struct intel_bdw_bcs_submit result,copy;
static struct bcs_submit_source source,other_source;
static unsigned owner,lifetime,context,other,cases;
static struct {
 unsigned kind,at,field,domain,events,clocks,proofs,calls,stores,triggered,events_at_fault;
 uint64_t time,limit;
 bool hit;
} m;
static struct bcs_submit_io io(void);
enum fault {NONE,SOURCE_FIELD,RESULT_FIELD,BOTH_FIELD,PROGRESS,PACKET,TERMINAL,REGISTRATION,RESULT_OBJECT,
 SOURCE_OBJECT,SOURCE_POINTER,CALLBACK,ORIGINAL_CALLBACK,BOTH_CALLBACKS,REENTER,
 PROOF_FIELD,PROOF_BIT,PROOF_FALSE,PROOF_FUTURE,WRITE_NONE,WRITE_UNKNOWN,WRITE_INVALID,CLOCK_ZERO,CLOCK_BACK,CLOCK_LIMIT,CLOCK_OVER,FROZEN};
struct field {size_t offset,bytes;};
#define FIELD(T,n) {offsetof(struct T,n),sizeof(((struct T *)0)->n)}
static const struct field binding_fields[]={
 FIELD(bcs_submit_binding,owner),FIELD(bcs_submit_binding,provider),FIELD(bcs_submit_binding,lifetime),FIELD(bcs_submit_binding,context),
 FIELD(bcs_submit_binding,root),FIELD(bcs_submit_binding,epoch),FIELD(bcs_submit_binding,sequence),FIELD(bcs_submit_binding,descriptor),
 FIELD(bcs_submit_binding,ring_gpu),FIELD(bcs_submit_binding,ring_physical),FIELD(bcs_submit_binding,context_gpu),FIELD(bcs_submit_binding,context_physical),
 FIELD(bcs_submit_binding,completion_gpu),FIELD(bcs_submit_binding,completion_physical),FIELD(bcs_submit_binding,aperture_bytes),FIELD(bcs_submit_binding,fence_begin),FIELD(bcs_submit_binding,fence_end),
 FIELD(bcs_submit_binding,prepare),FIELD(bcs_submit_binding,gt),FIELD(bcs_submit_binding,hda),FIELD(bcs_submit_binding,outer),FIELD(bcs_submit_binding,admitted_at),
 FIELD(bcs_submit_binding,vendor),FIELD(bcs_submit_binding,device),FIELD(bcs_submit_binding,tail)};
static const struct field progress_fields[]={FIELD(bcs_submit_progress,begin),FIELD(bcs_submit_progress,last),FIELD(bcs_submit_progress,state),
 FIELD(bcs_submit_progress,proofs),FIELD(bcs_submit_progress,attempts),FIELD(bcs_submit_progress,writes),FIELD(bcs_submit_progress,triggered),FIELD(bcs_submit_progress,uncertain)};
static const struct field proof_fields[]={FIELD(bcs_submit_proof,source),FIELD(bcs_submit_proof,owner),FIELD(bcs_submit_proof,provider),FIELD(bcs_submit_proof,lifetime),FIELD(bcs_submit_proof,context),
 FIELD(bcs_submit_proof,root),FIELD(bcs_submit_proof,epoch),FIELD(bcs_submit_proof,sequence),FIELD(bcs_submit_proof,descriptor),FIELD(bcs_submit_proof,checked_at),FIELD(bcs_submit_proof,prefix)};
#define COUNT(a) (sizeof(a)/sizeof((a)[0]))
static void mutate(void *p,struct field f,uint64_t mask) {
 uint64_t value=0;assert(f.bytes<=8);memcpy(&value,(unsigned char *)p+f.offset,f.bytes);value^=mask;memcpy((unsigned char *)p+f.offset,&value,f.bytes);
}
static void hit(void){m.hit=true;m.events_at_fault=m.events;}
static void hook(void) {
 ++m.events;
 if(m.kind==REENTER) {assert(!intel_bdw_bcs_submit_run(&result,&source,io()));return;}
 if(m.events!=m.at)return;
 uint64_t mask=m.field==4?8192:m.field==7?2:1; /* Legal changed root/descriptor. */
 switch(m.kind) {
 case SOURCE_FIELD:mutate(&source.binding,binding_fields[m.field],mask);break;
 case RESULT_FIELD:mutate(&result.binding,binding_fields[m.field],mask);break;
 case BOTH_FIELD:mutate(&source.binding,binding_fields[m.field],mask);mutate(&result.binding,binding_fields[m.field],mask);break;
 case PROGRESS:mutate(&result.p,progress_fields[m.field],1);break;
 case PACKET:result.words[m.field]^=1;break;
 case TERMINAL:
  switch(m.field){case 0:result.busy=false;break;case 1:result.issued=true;break;case 2:result.retained=true;break;
   case 3:result.reason=1;break;case 4:result.end=1;break;case 5:result.clock_at=1;break;case 6:result.clock_previous=1;break;case 7:result.clock_reason=1;break;default:assert(0);}
  break;
 case REGISTRATION:source.consumer=NULL;break;
 case RESULT_OBJECT:result.object=NULL;break;
 case SOURCE_OBJECT:source.object=NULL;break;
 case SOURCE_POINTER:other_source=source;other_source.object=&other_source;result.source=&other_source;break;
 case CALLBACK:result.io.ctx=&other;break;
 case ORIGINAL_CALLBACK:result.original_io.ctx=&other;break;
 case BOTH_CALLBACKS:result.io.ctx=result.original_io.ctx=&other;break;
 default:return;
 }
 hit();
}
static uint64_t now(void *ctx) {
 assert(ctx==&m);++m.clocks;hook();
 if(m.clocks==m.at && m.kind==CLOCK_ZERO){hit();return 0;}
 if(m.clocks==m.at && m.kind==CLOCK_BACK){hit();return result.p.last-1;}
 if(m.clocks==m.at && (m.kind==CLOCK_LIMIT || m.kind==CLOCK_OVER)){if(m.kind==CLOCK_OVER)hit();m.time=m.limit+(m.kind==CLOCK_OVER);return m.time;}
 return m.kind==FROZEN?m.time:++m.time;
}
static bool ownership(void *ctx,const struct bcs_submit_source *s,unsigned prefix,struct bcs_submit_proof *p) {
 assert(ctx==&m && s==&source && prefix==m.stores);++m.proofs;hook();
 const struct bcs_submit_binding *b=&source.binding;
 *p=(struct bcs_submit_proof){&source,&owner,&m,&lifetime,&context,0x1000,9,1,b->descriptor,m.time,BCS_SUBMIT_REQUIRED,prefix};
 if(m.proofs==m.at) {
  if(m.kind==PROOF_FALSE){hit();return false;}
  if(m.kind==PROOF_FIELD){mutate(p,proof_fields[m.field],m.field==9?0xfffff:1);hit();}
  if(m.kind==PROOF_BIT){p->requirements^=1u<<m.field;hit();}
  if(m.kind==PROOF_FUTURE){p->checked_at=m.time+2;hit();}
 }
 return true;
}
static enum intel_gt_write write_word(void *ctx,uint32_t port,uint32_t value) {
 assert(ctx==&m && port==0x22230 && m.calls<4);
 const uint32_t expected[]={0,0,1,0x0220101d};assert(value==expected[m.calls]);
 ++m.calls;hook();
 if(m.calls==m.at && m.kind==WRITE_NONE){hit();return GT_WRITE_NONE;}
 if(m.calls==m.at && (m.kind==WRITE_UNKNOWN || m.kind==WRITE_INVALID)){hit();return m.kind==WRITE_UNKNOWN?GT_WRITE_UNCERTAIN:(enum intel_gt_write)3;}
 ++m.stores;if(m.stores==4)++m.triggered;return GT_WRITE_DONE;
}
static struct bcs_submit_io io(void){return (struct bcs_submit_io){&m,ownership,write_word,now};}
static void setup(unsigned kind,unsigned at,unsigned field,unsigned domain) {
 memset(&result,0,sizeof(result));memset(&copy,0,sizeof(copy));memset(&source,0,sizeof(source));memset(&other_source,0,sizeof(other_source));
 memset(&m,0,sizeof(m));m.kind=kind;m.at=at;m.field=field;m.domain=domain;m.time=2000000;
 source.object=&source;
 source.binding=(struct bcs_submit_binding){&owner,&m,&lifetime,&context,0x1000,9,1,0x000000010220101dull,
  0x2200000,0x1000000,0x2201000,0x1001000,0x2203000,0x1003000,0x10000000,0,0x2200000,
  m.time,m.time,m.time,m.time,m.time,0x8086,0x1626,24};
 uint64_t *origins[]={&source.binding.prepare,&source.binding.gt,&source.binding.hda,&source.binding.outer};
 const uint64_t caps[]={INTEL_DIAGNOSTIC_PREPARE_US,INTEL_DIAGNOSTIC_GT_TOTAL_US,INTEL_DIAGNOSTIC_HDA_US,INTEL_DIAGNOSTIC_OUTER_US};
 if(domain<4){*origins[domain]=m.time-caps[domain]+2000;for(unsigned i=domain+1;i<4;i++)*origins[i]=*origins[domain];m.limit=m.time+2000;}
 else m.limit=m.time+10000;
}
static void run(unsigned kind,unsigned at,unsigned field,unsigned domain,bool expected) {
 setup(kind,at,field,domain);bool ok=intel_bdw_bcs_submit_run(&result,&source,io());++cases;
 assert(ok==expected && result.issued==expected && !result.busy && result.object==&result && result.source==&source && source.object==&source && source.consumer==&result);
 assert(result.p.attempts==m.calls && result.p.writes==m.stores && result.p.triggered==m.triggered);
 assert(!result.p.uncertain || kind==WRITE_UNKNOWN || kind==WRITE_INVALID);
 if(expected) {
  assert(!result.reason && !result.retained && result.p.state==BCS_PORT_ISSUED && result.p.writes==4 && result.p.triggered==1 && result.p.proofs==5 && result.end>result.p.begin);
  assert(m.calls==4 && m.stores==4 && m.triggered==1);
 } else {
  assert(result.reason && result.retained && result.p.state==BCS_PORT_FAULT && !result.end);
  if(m.hit) {
   if(kind>=PROOF_FIELD && kind<=PROOF_FUTURE) {
    /* A final read-only clock accounts the proof's timestamp. No later port
     * access may follow a failed proof, including the final prefix4 check. */
    assert(m.events<=m.events_at_fault+1 && m.calls==m.proofs-1);
   } else assert(m.events==m.events_at_fault);
  }
 }
 unsigned events=m.events;assert(!intel_bdw_bcs_submit_run(&copy,&source,io()) && m.events==events && !copy.object);
 assert(!intel_bdw_bcs_submit_run(&result,&source,io()) && m.events==events);
}
static void invalid(void) {
 for(unsigned i=0;i<21;i++) {
  setup(NONE,0,0,4);struct bcs_submit_io provider=io();
  switch(i){case 0:source.object=NULL;break;case 1:source.consumer=&copy;break;case 2:source.binding.vendor=0;break;case 3:source.binding.device++;break;
   case 4:source.binding.root=UINT64_MAX-4095;break;case 5:source.binding.epoch=0;break;case 6:source.binding.descriptor&=~1ull;break;case 7:source.binding.descriptor^=4096;break;
   case 8:source.binding.descriptor|=1ull<<53;break;case 9:source.binding.descriptor|=1ull<<9;break;case 10:source.binding.tail=16;break;
   case 11:source.binding.ring_gpu=source.binding.context_gpu;break;case 12:source.binding.completion_physical=source.binding.context_physical+4096;break;
   case 13:source.binding.fence_end=0x2201000;break;case 14:source.binding.context_physical=source.binding.root;break;case 15:source.binding.admitted_at--;break;
   case 16:provider.ctx=&other;break;case 17:provider.now=NULL;break;case 18:provider.write=NULL;break;case 19:provider.ownership=NULL;break;
   case 20:result.p.writes=1;break;
  }
  struct intel_bdw_bcs_submit before=result;struct bcs_submit_source before_source=source;
  assert(!intel_bdw_bcs_submit_run(&result,&source,provider) && !m.events && !memcmp(&before,&result,sizeof(result)) && !memcmp(&before_source,&source,sizeof(source)));++cases;
 }
 for(unsigned i=0;i<COUNT(progress_fields);i++) {
  setup(NONE,0,0,4);mutate(&result.p,progress_fields[i],1);struct intel_bdw_bcs_submit before=result;
  assert(!intel_bdw_bcs_submit_run(&result,&source,io()) && !m.events && !source.consumer && !memcmp(&before,&result,sizeof(result)));++cases;
 }
 setup(NONE,0,0,4);struct bcs_submit_source old=source;
 assert(!intel_bdw_bcs_submit_run((void *)&source,&source,io()) && !memcmp(&old,&source,sizeof(source)) && !m.events);++cases;
}
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--positive")){run(NONE,0,0,4,true);puts("bcs-submit: positive PASS");return 0;}
 assert(argc==1);
 run(NONE,0,0,4,true);unsigned events=m.events,clocks=m.clocks,proofs=m.proofs;
 assert(events==23 && clocks==14 && proofs==5);
 for(unsigned n=1;n<=events;n++) {
  for(unsigned i=0;i<COUNT(binding_fields);i++)for(unsigned k=SOURCE_FIELD;k<=BOTH_FIELD;k++)run(k,n,i,4,false);
  for(unsigned i=0;i<COUNT(progress_fields);i++)run(PROGRESS,n,i,4,false);
  for(unsigned i=0;i<4;i++)run(PACKET,n,i,4,false);
  for(unsigned i=0;i<8;i++)run(TERMINAL,n,i,4,false);
  for(unsigned k=REGISTRATION;k<=BOTH_CALLBACKS;k++)run(k,n,0,4,false);
 }
 run(REENTER,0,0,4,true);
 for(unsigned n=1;n<=proofs;n++) {
  run(PROOF_FALSE,n,0,4,false);
  run(PROOF_FUTURE,n,0,4,false);
  for(unsigned i=0;i<COUNT(proof_fields);i++)run(PROOF_FIELD,n,i,4,false);
  for(unsigned i=0;i<5;i++)run(PROOF_BIT,n,i,4,false);
 }
 for(unsigned i=1;i<=4;i++)for(unsigned k=WRITE_NONE;k<=WRITE_INVALID;k++)run(k,i,0,4,false);
 for(unsigned i=1;i<=clocks;i++){run(CLOCK_ZERO,i,0,4,false);run(CLOCK_BACK,i,0,4,false);}
 for(unsigned domain=0;domain<5;domain++){run(CLOCK_LIMIT,clocks,0,domain,true);run(CLOCK_OVER,clocks,0,domain,false);}
 run(FROZEN,0,0,4,false);invalid();
 printf("bcs-submit: PASS cases=%u events=%u clocks=%u proofs=%u prepare_us=%llu; issuance only, synthetic ownership, no native grant\n",cases,events,clocks,proofs,(unsigned long long)INTEL_DIAGNOSTIC_PREPARE_US);
 return 0;
}
