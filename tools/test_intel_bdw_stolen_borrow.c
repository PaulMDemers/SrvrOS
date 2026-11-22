/* Execute the real reservation backend, memory lifetime and CPU exclusion.
 * Device ownership is independently modeled; no native producer or RAM store. */
#include <srvros/intel_bdw_stolen_borrow.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/display_access.c"
static struct intel_bdw_stolen_borrow b,forged;
static struct bdw_stolen_source src,other;
static struct scanout_memory memory[2];
static struct scanout_memory_pool memories;
static struct scanout_memory_token mt[2];
static struct bdw_stolen_borrow_token token;
static struct display_access_token exclusion;
static struct scanout_memory_layout child_layout;
static uint64_t ptes[2][3][16384];
static unsigned lifetime,cases;
enum kind {NORMAL,PROVIDER_FALSE,PROOF_RIGHT,PROOF_FIELD,ZERO_TIME,BACK_TIME,LONG_TIME,EXPIRED_TIME,FROZEN_TIME,
 BINDING_FIELD,BOTH_BINDINGS,SLOT_FIELD,SOURCE_OBJECT,SOURCE_CONSUMER,SOURCE_RIGHTS,SOURCE_CONSUMED,
 OWNER_OBJECT,SOURCE_POINTER,CALLBACK,PROGRESS,REENTER};
static struct {
 uint64_t time;
 struct bdw_stolen_binding original;
 unsigned calls,proofs,now_calls,at,field,kind,hit_call;
 bool hit;
} model;
struct field {size_t offset,bytes;};
#define F(T,n) {offsetof(struct T,n),sizeof(((struct T *)0)->n)}
#define COUNT(x) (sizeof(x)/sizeof((x)[0]))
static const struct field bindings[]={
 F(bdw_stolen_binding,provider),F(bdw_stolen_binding,lifetime),F(bdw_stolen_binding,owner),
 F(bdw_stolen_binding,root),F(bdw_stolen_binding,epoch),F(bdw_stolen_binding,admitted_at),F(bdw_stolen_binding,expires_at),
 F(bdw_stolen_binding,facts.host),F(bdw_stolen_binding,facts.gpu),F(bdw_stolen_binding,facts.subsystem),
 F(bdw_stolen_binding,facts.ggc),F(bdw_stolen_binding,facts.bdsm),F(bdw_stolen_binding,facts.bgsm),F(bdw_stolen_binding,facts.tolud),
 F(bdw_stolen_binding,facts.reserved),F(bdw_stolen_binding,facts.aperture_bytes),F(bdw_stolen_binding,facts.ggtt_bytes),
 F(bdw_stolen_binding,facts.fence_begin),F(bdw_stolen_binding,facts.fence_end),F(bdw_stolen_binding,facts.width),
 F(bdw_stolen_binding,facts.height),F(bdw_stolen_binding,facts.stride),F(bdw_stolen_binding,layout.checked),F(bdw_stolen_binding,layout.classified),
 F(bdw_stolen_binding,layout.physical[0]),F(bdw_stolen_binding,layout.physical[1]),F(bdw_stolen_binding,layout.physical[2]),
 F(bdw_stolen_binding,layout.physical_end[0]),F(bdw_stolen_binding,layout.physical_end[1]),F(bdw_stolen_binding,layout.physical_end[2]),
 F(bdw_stolen_binding,layout.plan.data_begin),F(bdw_stolen_binding,layout.plan.data_end),F(bdw_stolen_binding,layout.plan.usable_end),
 F(bdw_stolen_binding,layout.plan.gtt_begin),F(bdw_stolen_binding,layout.plan.gtt_end),F(bdw_stolen_binding,layout.plan.pixel_bytes),
 F(bdw_stolen_binding,layout.plan.translation_bytes),
 F(bdw_stolen_binding,layout.plan.range[0].gpu),F(bdw_stolen_binding,layout.plan.range[0].bytes),F(bdw_stolen_binding,layout.plan.range[0].pages),
 F(bdw_stolen_binding,layout.plan.range[1].gpu),F(bdw_stolen_binding,layout.plan.range[1].bytes),F(bdw_stolen_binding,layout.plan.range[1].pages),
 F(bdw_stolen_binding,layout.plan.range[2].gpu),F(bdw_stolen_binding,layout.plan.range[2].bytes),F(bdw_stolen_binding,layout.plan.range[2].pages)};
static const struct field proofs[]={F(bdw_stolen_proof,source),F(bdw_stolen_proof,provider),F(bdw_stolen_proof,lifetime),
 F(bdw_stolen_proof,owner),F(bdw_stolen_proof,root),F(bdw_stolen_proof,epoch),F(bdw_stolen_proof,checked_at),F(bdw_stolen_proof,phase)};
static const struct field slots[]={F(bdw_stolen_slot,object),F(bdw_stolen_slot,parent),F(bdw_stolen_slot,child),
 F(bdw_stolen_slot,index),F(bdw_stolen_slot,pages),F(bdw_stolen_slot,claimed),F(bdw_stolen_slot,returned)};
static void mutate(void *p,struct field f){uint64_t v=0;assert(f.bytes<=8);memcpy(&v,(char *)p+f.offset,f.bytes);v^=1;memcpy((char *)p+f.offset,&v,f.bytes);}
static struct bdw_stolen_borrow_io io(void);
static void hook(void){
 ++model.calls;
 if(model.kind==REENTER){
  assert(!intel_bdw_stolen_borrow_check(&b,&token));assert(!intel_bdw_stolen_borrow_close(&b,&token));
  assert(!intel_bdw_stolen_borrow_memory(&b,&token,0,&memory[0],&memories,&child_layout,&mt[0]));return;
 }
 if(model.calls!=model.at)return;
 switch(model.kind){
 case BINDING_FIELD:mutate(&src.binding,bindings[model.field]);break;
 case BOTH_BINDINGS:mutate(&src.binding,bindings[model.field]);mutate(&b.binding,bindings[model.field]);break;
 case SLOT_FIELD:mutate(&b.slot[model.field/COUNT(slots)],slots[model.field%COUNT(slots)]);break;
 case SOURCE_OBJECT:src.object=&other;break;
 case SOURCE_CONSUMER:src.consumer=&forged;break;
 case SOURCE_RIGHTS:src.rights^=1;break;
 case SOURCE_CONSUMED:src.consumed=false;break;
 case OWNER_OBJECT:b.object=&forged;break;
 case SOURCE_POINTER:b.source=&other;break;
 case CALLBACK:b.io.now=NULL;break;
 case PROGRESS:b.checks++;break;
 default:return;
 }
 model.hit=true;model.hit_call=model.calls;
}
static uint64_t now(void *ctx){
 assert(ctx==&model);model.now_calls++;hook();
 if(model.kind==FROZEN_TIME && model.hit)return model.time;
 if(model.calls==model.at){
  if(model.kind==LONG_TIME && (model.now_calls&1))return ++model.time;
  uint64_t v=0;
  switch(model.kind){
  case ZERO_TIME:v=0;break;case BACK_TIME:v=b.last-1;break;
  case LONG_TIME:v=model.time+BDW_BORROW_OPERATION_US+1;model.time=v;break;
  case EXPIRED_TIME:v=model.original.expires_at+1;break;
  case FROZEN_TIME:v=b.last;model.time=v;break;default:return ++model.time;
  }
  model.hit=true;model.hit_call=model.calls;return v;
 }
 return ++model.time;
}
static bool validate_source(void *ctx,const struct bdw_stolen_source *s,enum bdw_borrow_phase phase,struct bdw_stolen_proof *p){
 assert(ctx==&model && s==&src && b.busy && pins==1 && !display_access_allowed());
 assert(src.consumer==&b && src.consumed);++model.proofs;hook();
 const struct bdw_stolen_binding *original=&model.original;
 *p=(struct bdw_stolen_proof){&src,&model,&lifetime,original->owner,original->root,original->epoch,model.time,BDW_BORROW_ALL,phase};
 if(model.calls==model.at){
  if(model.kind==PROVIDER_FALSE){model.hit=true;model.hit_call=model.calls;return false;}
  if(model.kind==PROOF_RIGHT){p->rights^=1u<<model.field;model.hit=true;model.hit_call=model.calls;}
  if(model.kind==PROOF_FIELD){mutate(p,proofs[model.field]);if(model.field==6)p->checked_at=0;model.hit=true;model.hit_call=model.calls;}
 }
 return true;
}
static struct bdw_stolen_borrow_io io(void){return (struct bdw_stolen_borrow_io){&model,validate_source,now};}
static void setup(void){
 state=epoch=owner=pins=0;fault=false;
 memset(&b,0,sizeof b);memset(&forged,0,sizeof forged);memset(&src,0,sizeof src);memset(&other,0,sizeof other);
 memset(memory,0,sizeof memory);memset(mt,0,sizeof mt);memset(&memories,0,sizeof memories);memset(&token,0,sizeof token);memset(&model,0,sizeof model);
 model.time=100;
 assert(display_access_acquire(7,&exclusion));
 src.object=&src;src.rights=BDW_BORROW_ALL;
 src.binding=(struct bdw_stolen_binding){.provider=&model,.lifetime=&lifetime,.owner=7,.root=0x1000,.epoch=9,.admitted_at=100,.expires_at=1000000,
 .facts={.host=0x16048086,.gpu=0x16268086,.subsystem=0x011b106b,.ggc=0x2c1,.bdsm=0x8c000001,.bgsm=0x8b800001,.tolud=0x90000001,
 .reserved=0x8ff00007,.aperture_bytes=0x10000000,.ggtt_bytes=0x800000,.fence_begin=0,.fence_end=0x2200000,.width=1440,.height=900,.stride=6144}};
 struct bdw_stolen_plan plan;assert(intel_bdw_stolen_scanout_plan(&src.binding.facts,&plan));
 struct bdw_stolen_vectors vectors={0};const uint64_t physical[]={0x8c001000,0x8e201000,0x8e801000};
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++){
  vectors.pte[n][r]=ptes[n][r];vectors.count[n][r]=plan.range[r].pages;
  for(unsigned i=0;i<plan.range[r].pages;i++)ptes[n][r][i]=(physical[r]+4096ull*i)|1;
 }
 assert(intel_bdw_stolen_scanout_check(&src.binding.facts,&vectors,&src.binding.layout));model.original=src.binding;
 assert(scanout_memory_layout(1440,900,6144,false,&child_layout));assert(child_layout.pages==1350);
}
static bool scenario(void){
 struct scanout_memory_cpu cpu,unused;struct scanout_memory_hold hold={0};
 if(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token))return false;
 assert(!display_access_release(&exclusion,true) && pins==1);
 if(!intel_bdw_stolen_borrow_memory(&b,&token,0,&memory[0],&memories,&child_layout,&mt[0]))return false;
 if(!scanout_memory_cpu_begin(&memory[0],&mt[0],&cpu))return false;
 if(!intel_bdw_stolen_borrow_memory(&b,&token,1,&memory[1],&memories,&child_layout,&mt[1]))return false;
 if(!scanout_memory_hold_begin(&memory[1],&mt[1],&hold))return false;
 assert(!intel_bdw_stolen_borrow_close(&b,&token));
 if(b.state==BDW_BORROW_FAULT)return false;
 assert(b.state==BDW_BORROW_CLOSING && pins==1 && src.consumer==&b);
 assert(!scanout_memory_cpu_begin(&memory[1],&mt[1],&unused));
 assert(!intel_bdw_stolen_borrow_memory(&b,&token,0,&memory[0],&memories,&child_layout,&mt[0]));
 if(!scanout_memory_cpu_end(&memory[0],&cpu))return false;
 assert(!scanout_memory_cpu_begin(&memory[0],&mt[0],&unused));
 if(!scanout_memory_close(&memory[0],&mt[0]))return false;
 assert(pins==1 && !display_access_release(&exclusion,true));
 if(!scanout_memory_hold_end(&hold))return false;
 assert(!scanout_memory_cpu_begin(&memory[1],&mt[1],&unused));
 if(!scanout_memory_close(&memory[1],&mt[1]))return false;
 if(!intel_bdw_stolen_borrow_close(&b,&token))return false;
 assert(!pins && !src.consumer && src.consumed && b.state==BDW_BORROW_CLOSED);
 assert(memory[0].releases==1 && memory[1].releases==1 && b.slot[0].returned && b.slot[1].returned);
 assert(!intel_bdw_stolen_borrow_close(&b,&token));assert(display_access_release(&exclusion,true));
 return true;
}
int main(void){
 setup();assert(scenario());unsigned calls=model.calls;assert(calls>30);cases++;
 for(unsigned k=PROVIDER_FALSE;k<=REENTER;k++){
  unsigned fields=k==PROOF_RIGHT?7:k==PROOF_FIELD?COUNT(proofs):(k==BINDING_FIELD || k==BOTH_BINDINGS)?COUNT(bindings):k==SLOT_FIELD?2*COUNT(slots):1;
  for(unsigned f=0;f<fields;f++)for(unsigned at=1;at<=calls;at++){
   setup();model.kind=k;model.at=at;model.field=f;bool ok=scenario();
   if(model.hit){if(ok)fprintf(stderr,"undetected kind=%u field=%u at=%u calls=%u\n",k,f,at,model.calls);assert(!ok);assert(b.state==BDW_BORROW_FAULT && b.pinned && pins==1 && !display_access_allowed());if(model.calls>model.hit_call+2)fprintf(stderr,"continuation kind=%u field=%u at=%u calls=%u\n",k,f,at,model.calls);assert(model.calls<=model.hit_call+2);}
   else assert(ok);
   cases++;
  }
 }
 for(unsigned missing=0;missing<8;missing++){
  setup();src.rights=missing==7?0:BDW_BORROW_ALL^(1u<<missing);
  assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token));assert(!model.calls && !pins && !src.consumer && !src.consumed && !b.object);cases++;
 }
 setup();assert(intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token));
 forged=b;struct bdw_stolen_borrow_token counterfeit={&forged,token.owner,token.epoch};
 assert(!intel_bdw_stolen_borrow_check(&forged,&counterfeit));assert(!intel_bdw_stolen_borrow_close(&forged,&counterfeit));
 struct bdw_stolen_borrow_token stale=token;stale.epoch++;assert(!intel_bdw_stolen_borrow_close(&b,&stale));
 assert(intel_bdw_stolen_borrow_close(&b,&token));assert(display_access_release(&exclusion,true));cases++;
 setup();assert(intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token));
 assert(intel_bdw_stolen_borrow_memory(&b,&token,0,&memory[0],&memories,&child_layout,&mt[0]));
 unsigned before=model.calls;
 assert(!memory[0].allocator.release(memory[0].allocator.ctx,memory[0].physical+4096,memory[0].layout.pages));
 assert(!memory[0].allocator.release(memory[0].allocator.ctx,memory[0].physical,memory[0].layout.pages+1));
 assert(!b.slot[0].returned && before==model.calls);
 assert(!intel_bdw_stolen_borrow_memory(&b,&token,1,&memory[0],&memories,&child_layout,&mt[1]));
 assert(scanout_memory_close(&memory[0],&mt[0]));
 assert(!memory[0].allocator.release(memory[0].allocator.ctx,memory[0].physical,memory[0].layout.pages));
 assert(intel_bdw_stolen_borrow_close(&b,&token) && display_access_release(&exclusion,true));cases++;
 setup();src.binding.layout.physical[2]=src.binding.layout.physical[1];assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token) && !pins && !model.calls);cases++;
 setup();src.binding.expires_at=model.original.expires_at=101;assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token));assert(b.state==BDW_BORROW_FAULT && model.proofs==1 && pins==1);cases++;
 setup();src.binding.expires_at=model.original.expires_at=101;model.time=102;assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token));assert(!model.proofs && pins==1);cases++;
 setup();b.slot[1].pages=1;assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),&token) && !pins);cases++;
 setup();assert(!intel_bdw_stolen_borrow_begin(&b,&src,&exclusion,io(),(void *)&src));assert(!pins && !model.calls);cases++;
 printf("stolen-borrow PASS cases=%u real-memory=1 real-exclusion=1 native_grants=0 ram_stores=0 pte_stores=0 firmware_frees=0\n",cases);
}
