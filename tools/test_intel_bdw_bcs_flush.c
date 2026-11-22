/* Independent synthetic owned-ring/device model; never native acceptance. */
#include <srvros/intel_bdw_bcs_flush.h>
#include <assert.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
static struct intel_bdw_bcs_flush result;
static struct bcs_flush_source source;
static unsigned owner,lifetime,context,cases;
enum fault {NORMAL,SOURCE_DRIFT,RESULT_DRIFT,PACKET_DRIFT,IO_DRIFT,REGISTRATION_DRIFT,
 REENTER,PROOF_FALSE,PROOF_DRIFT,READ_FALSE,STALE_NONCE,STALE_OTHER,WRONG_COMPLETION,
 PUBLISH_NONE,PUBLISH_UNKNOWN,SUBMIT_NONE,SUBMIT_UNKNOWN,NEVER_COMPLETES,
 CLOCK_ZERO,CLOCK_BACK,CLOCK_LIMIT,FROZEN};
static struct {
 uint64_t time,limit;
 unsigned events,clocks,proofs,reads,publishes,submits,pauses,at,field,signal_after;
 enum fault fault;
 bool hit,published,submitted;
 uint32_t packet[6];
} model;
static bool run(void);
static void source_field(unsigned field) {
 switch(field){
 case 0:source.binding.owner=&context;break;
 case 1:source.binding.provider=&owner;break;
 case 2:source.binding.lifetime=&owner;break;
 case 3:source.binding.context=&owner;break;
 default:break;
 }
 /* Change each scalar through its actual type; no aliasing a struct as words. */
 switch(field){
 case 4:source.binding.epoch^=1;break;
 case 5:source.binding.sequence^=1;break;
 case 6:source.binding.root^=4096;break;
 case 7:source.binding.nonce^=1;break;
 case 8:source.binding.ring_gpu^=4096;break;
 case 9:source.binding.ring_physical^=4096;break;
 case 10:source.binding.completion_gpu^=4096;break;
 case 11:source.binding.completion_physical^=4096;break;
 case 12:source.binding.aperture_bytes^=4096;break;
 case 13:source.binding.fence_begin^=4096;break;
 case 14:source.binding.fence_end^=4096;break;
 case 15:source.binding.prepare_begin^=1;break;
 case 16:source.binding.gt_begin^=1;break;
 case 17:source.binding.hda_begin^=1;break;
 case 18:source.binding.outer_begin^=1;break;
 case 19:source.binding.vendor^=1;break;
 case 20:source.binding.device^=1;break;
 default:break;
 }
}
static void result_field(unsigned field) {
 switch(field){
 case 0:result.p.begin^=1;break;case 1:result.p.last^=1;break;
 case 2:result.p.submit_begin^=1;break;case 3:result.p.initial^=1;break;
 case 4:result.p.observed^=1;break;case 5:result.p.state^=1;break;
 case 6:result.p.proofs^=1;break;case 7:result.p.reads^=1;break;
 case 8:result.p.publish_attempts^=1;break;case 9:result.p.published^=1;break;
 case 10:result.p.submit_attempts^=1;break;case 11:result.p.submitted^=1;break;
 case 12:result.p.polls^=1;break;case 13:result.p.uncertain=!result.p.uncertain;break;
 case 14:result.binding.root^=4096;break;case 15:result.original_io.pause=NULL;break;
 case 16:result.source=NULL;break;case 17:result.complete=true;break;
 case 18:result.busy=false;break;case 19:result.end=1;break;
 default:assert(false);
 }
}
static void event(void) {
 ++model.events;
 if(model.events!=model.at)return;
 switch(model.fault){
 case SOURCE_DRIFT:source_field(model.field);break;
 case RESULT_DRIFT:result_field(model.field);break;
 case PACKET_DRIFT:result.packet[model.field]^=1;break;
 case IO_DRIFT:result.io.pause=NULL;break;
 case REGISTRATION_DRIFT:source.consumer=NULL;break;
 case REENTER:{
  struct intel_bdw_bcs_flush saved=result;assert(!run());assert(!memcmp(&saved,&result,sizeof(result)));
  break;
 }
 default:return;
 }
 model.hit=true;
}
static uint64_t now(void *ctx) {
 assert(ctx==&model);++model.clocks;event();
 if(model.clocks==model.at){
  if(model.fault==CLOCK_ZERO)return 0;
  if(model.fault==CLOCK_BACK)return result.p.last-1;
  if(model.fault==CLOCK_LIMIT){model.time=model.limit;return model.limit;}
 }
 return model.fault==FROZEN?model.time:++model.time;
}
static bool ownership(void *ctx,const struct bcs_flush_source *s,struct bcs_flush_proof *p) {
 assert(ctx==&model && s==&source && source.consumer==&result);
 ++model.proofs;event();
 /* Independent model's registered owner, not an echo of the supplied fields. */
 *p=(struct bcs_flush_proof){.source=&source,.owner=&owner,.provider=&model,
 .lifetime=&lifetime,.context=&context,.epoch=7,.sequence=19,.root=0x9000,
 .checked_at=model.time,.requirements=31};
 if(model.proofs==model.at){
  if(model.fault==PROOF_FALSE)return false;
  if(model.fault==PROOF_DRIFT){
   model.hit=true;
   switch(model.field){
   case 0:p->source=NULL;break;case 1:p->owner=NULL;break;case 2:p->provider=NULL;break;
   case 3:p->lifetime=NULL;break;case 4:p->context=NULL;break;case 5:p->epoch^=1;break;
   case 6:p->sequence^=1;break;case 7:p->root^=4096;break;case 8:p->checked_at--;break;
   case 9:p->checked_at+=1000;break;default:p->requirements^=1u<<(model.field-10);break;
   }
  }
 }
 return true;
}
static bool read_value(void *ctx,uint64_t gpu,uint64_t *out) {
 assert(ctx==&model && gpu==0x2401000);++model.reads;event();
 if(model.reads==model.at && model.fault==READ_FALSE)return false;
 if(!model.submitted){
  *out=model.fault==STALE_NONCE?0x1122334455667788ull:model.fault==STALE_OTHER?3:0;return true;
 }
 assert(model.published && model.submits==1);
 *out=model.fault==WRONG_COMPLETION?0x1122334455667789ull:
 model.fault==NEVER_COMPLETES || model.reads-1<model.signal_after?0:0x1122334455667788ull;
 return true;
}
static enum intel_gt_write publish(void *ctx,uint64_t ring,const uint32_t *p,unsigned count) {
 assert(ctx==&model && ring==0x2400000 && count==6 && !model.submitted && !model.published);
 assert(p[0]==0x13044003 && p[1]==0x2401004 && p[2]==0 && p[3]==0x55667788 && p[4]==0x11223344 && p[5]==0);
 ++model.publishes;event();
 if(model.fault==PUBLISH_NONE)return GT_WRITE_NONE;
 memcpy(model.packet,p,sizeof(model.packet));model.published=true;
 return model.fault==PUBLISH_UNKNOWN?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static enum intel_gt_write submit(void *ctx,const void *c,uint64_t ring,uint32_t tail) {
 assert(ctx==&model && c==&context && ring==0x2400000 && tail==24 && model.published && !model.submitted);
 assert(model.packet[0]==0x13044003 && model.packet[5]==0);
 ++model.submits;event();
 if(model.fault==SUBMIT_NONE)return GT_WRITE_NONE;
 model.submitted=true;
 return model.fault==SUBMIT_UNKNOWN?GT_WRITE_UNCERTAIN:GT_WRITE_DONE;
}
static void pause_once(void *ctx) {assert(ctx==&model);++model.pauses;event();}
static struct bcs_flush_io io(void) {
 return (struct bcs_flush_io){&model,ownership,read_value,publish,submit,now,pause_once};
}
static void setup(enum fault fault,unsigned at,unsigned field) {
 memset(&result,0,sizeof(result));memset(&source,0,sizeof(source));memset(&model,0,sizeof(model));
 model.time=1000000;model.fault=fault;model.at=at;model.field=field;model.signal_after=3;
 source=(struct bcs_flush_source){.object=&source,.binding={.owner=&owner,.provider=&model,
 .lifetime=&lifetime,.context=&context,.epoch=7,.sequence=19,.root=0x9000,.nonce=0x1122334455667788ull,
 .ring_gpu=0x2400000,.ring_physical=0x1000000,.completion_gpu=0x2401000,.completion_physical=0x1001000,
 .aperture_bytes=0x10000000,.fence_begin=0x100000,.fence_end=0x2300000,
 .prepare_begin=1000000,.gt_begin=1000000,.hda_begin=1000000,.outer_begin=1000000,
 .vendor=0x8086,.device=0x1626}};
}
static bool run(void) {return intel_bdw_bcs_flush_run(&result,&source,io());}
static void failed(void) {
 ++cases;assert(!run());assert(result.object==&result && !result.complete && result.retained && result.reason && !result.busy && !result.end);
 assert(model.publishes<=1 && model.submits<=1 && model.reads<=257);
 struct intel_bdw_bcs_flush saved=result;assert(!run());assert(!memcmp(&saved,&result,sizeof(result)));
}
static void passed(void) {
 ++cases;bool ok=run();
 if(!ok)fprintf(stderr,"positive case=%u fault=%u at=%u field=%u reason=%u clock=%u events=%u clocks=%u reads=%u polls=%u proofs=%u\n",
 cases,model.fault,model.at,model.field,result.reason,result.clock_reason,model.events,model.clocks,result.p.reads,result.p.polls,result.p.proofs);
 assert(ok);assert(result.complete && !result.retained && !result.reason && !result.busy && source.consumer==&result);
 assert(result.p.initial==0 && result.p.observed==0x1122334455667788ull);
 assert(result.p.published==1 && result.p.submitted==1 && model.publishes==1 && model.submits==1);
 assert(result.p.reads==result.p.polls+1 && result.p.proofs==result.p.reads+3);
 struct intel_bdw_bcs_flush saved=result;assert(!run());assert(!memcmp(&saved,&result,sizeof(result)));
}
static void codec(void) {
 uint32_t words[6];
 for(uint64_t a=64;a<=0xffffffc0ull;a=a<4096?a+64:a*2){
  assert(intel_bdw_bcs_flush_packet(a,0x0102030405060708ull,words));
  /* Independently decode each field, including reserved bits and the NOOP. */
  assert(words[0]>>29==0 && ((words[0]>>23)&63)==0x26 && (words[0]&63)==3);
  assert((words[0]&~((63u<<23)|(1u<<18)|(1u<<14)|63u))==0);
  assert(words[0]&(1u<<18));assert(words[0]&(1u<<14));
  assert((words[1]&7)==4 && (words[1]&~7u)==a && words[2]==0);
  assert(words[3]==0x05060708 && words[4]==0x01020304 && words[5]==0);
  ++cases;
 }
 for(unsigned i=0;i<64;i++)if(i){memset(words,0xa5,sizeof(words));uint32_t before[6];memcpy(before,words,sizeof(words));
  assert(!intel_bdw_bcs_flush_packet(0x1000+i,1,words));assert(!memcmp(before,words,sizeof(words)));++cases;}
 assert(!intel_bdw_bcs_flush_packet(0,1,words));assert(!intel_bdw_bcs_flush_packet(0x1000,0,words));
 assert(!intel_bdw_bcs_flush_packet(0x100000000ull,1,words));assert(!intel_bdw_bcs_flush_packet(UINT64_MAX,1,words));
 assert(!intel_bdw_bcs_flush_packet(0x1000,1,NULL));
 assert(!intel_bdw_bcs_flush_packet(0x1000,UINT64_MAX,words));
 assert(intel_bdw_bcs_flush_packet(0xffffffc0ull,UINT64_MAX-1,words));++cases;
}
static void input_cases(void) {
 for(unsigned i=0;i<22;i++){
  setup(NORMAL,0,0);
  switch(i){
  case 0:source.binding.owner=NULL;break;case 1:source.binding.provider=NULL;break;
  case 2:source.binding.lifetime=NULL;break;case 3:source.binding.context=NULL;break;
  case 4:source.binding.epoch=0;break;case 5:source.binding.sequence=0;break;
  case 6:source.binding.root|=1;break;case 7:source.binding.nonce=0;break;
  case 8:source.binding.ring_gpu=source.binding.completion_gpu;break;
  case 9:source.binding.ring_physical=source.binding.completion_physical;break;
  case 10:source.binding.completion_gpu=source.binding.fence_begin;break;
  case 11:source.binding.ring_gpu=source.binding.fence_end-4096;break;
  case 12:source.binding.completion_gpu=source.binding.aperture_bytes;break;
  case 13:source.binding.ring_physical=0x100000000ull;break;
  case 14:source.binding.fence_end=source.binding.fence_begin;break;
  case 15:source.binding.aperture_bytes=UINT64_MAX;break;
  case 16:source.binding.gt_begin=source.binding.prepare_begin+1;break;
  case 17:source.binding.outer_begin=0;break;case 18:source.binding.vendor=0;break;
  case 19:source.binding.device=0;break;case 20:source.object=NULL;break;
  case 21:source.binding.nonce=UINT64_MAX;break;
  }
  assert(!run() && !model.events && !source.consumer && !result.object);++cases;
 }
 for(unsigned i=0;i<7;i++){
  setup(NORMAL,0,0);struct bcs_flush_io bad=io();
  switch(i){case 0:bad.ctx=NULL;break;case 1:bad.ownership=NULL;break;case 2:bad.read=NULL;break;
  case 3:bad.publish=NULL;break;case 4:bad.submit=NULL;break;case 5:bad.now=NULL;break;case 6:bad.pause=NULL;break;}
  assert(!intel_bdw_bcs_flush_run(&result,&source,bad) && !model.events && !source.consumer);++cases;
 }
 setup(NORMAL,0,0);struct bcs_flush_source copied=source;
 assert(!intel_bdw_bcs_flush_run(&result,&copied,io()) && !model.events);++cases;
 setup(NORMAL,0,0);source.consumer=&result;assert(!run() && !model.events);++cases;
 setup(NORMAL,0,0);assert(!intel_bdw_bcs_flush_run((void *)&source,&source,io()) && !model.events);++cases;
 setup(NORMAL,0,0);assert(!intel_bdw_bcs_flush_run(NULL,&source,io()));assert(!intel_bdw_bcs_flush_run(&result,NULL,io()));++cases;
}
static void boundaries(unsigned clocks) {
 for(unsigned which=0;which<5;which++)for(unsigned over=0;over<2;over++){
  setup(CLOCK_LIMIT,clocks,0);uint64_t end;
  if(which==0)end=1000000+BCS_FLUSH_PREPARE_US;
  else if(which==1){end=source.binding.gt_begin+BCS_FLUSH_GT_US;source.binding.prepare_begin=end-50000;}
  else if(which==2){end=source.binding.hda_begin+BCS_FLUSH_HDA_US;source.binding.prepare_begin=end-50000;source.binding.gt_begin=end-100000;}
  else if(which==3){end=source.binding.outer_begin+BCS_FLUSH_OUTER_US;source.binding.prepare_begin=end-50000;source.binding.gt_begin=source.binding.hda_begin=end-200000;}
  else end=0;
  if(which<4)model.time=end-clocks;
  if(which==4){
   /* Measure the original submission time once; the fault is on the final
    * clock only, so proof timestamps and prior progress are still real. */
   setup(NORMAL,0,0);passed();end=result.p.submit_begin+10000;
   setup(CLOCK_LIMIT,clocks,0);
  }
  model.limit=end+over;
  if(over){failed();assert(result.clock_at==end+1);assert(result.clock_reason==(which==0?BCS_CLOCK_PREPARE:which==1?BCS_CLOCK_GT:which==2?BCS_CLOCK_HDA:which==3?BCS_CLOCK_OUTER:BCS_CLOCK_WAIT));}
  else {passed();assert(result.end==end);}
 }
}
static void private_barrier_cases(void)
{
    const uint32_t addr=0xfffff040, lo=0x89abcdef, hi=0x12345678;
    const uint64_t marker=(uint64_t)hi<<32 | lo;
    const uint32_t render[]={0x7a000004,0x1050a1,addr,0,lo,hi};
    const uint32_t other[]={0x13004003,addr,0,lo,hi,0};
    uint32_t out[6];
    for (unsigned engine=0; engine<BDW_BARRIER_ENGINES; ++engine) {
        assert(intel_bdw_ppgtt_barrier(engine,addr,marker,out));
        assert(!memcmp(out,engine==BDW_BARRIER_RCS?render:other,sizeof(out)));
    }
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_ENGINES,addr,marker,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,addr,0,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,addr,UINT64_MAX,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,0,marker,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,addr+8,marker,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,1ull<<32,marker,out));
    assert(!intel_bdw_ppgtt_barrier(BDW_BARRIER_RCS,addr,marker,NULL));
    assert(!memcmp(out,other,sizeof(out)));
    const uint32_t wait[]={0x0e00c002,lo,addr,0};
    assert(intel_bdw_ppgtt_wait_equal(addr,lo,out) && !memcmp(wait,out,sizeof(wait)));
    assert(!intel_bdw_ppgtt_wait_equal(addr+1,lo,out));
    assert(!intel_bdw_ppgtt_wait_equal(1ull<<32,lo,out));
    assert(!intel_bdw_ppgtt_wait_equal(0,lo,out));
    assert(!intel_bdw_ppgtt_wait_equal(addr,0,out));
    assert(!intel_bdw_ppgtt_wait_equal(addr,lo,NULL));
    assert(!memcmp(wait,out,sizeof(wait)));
    uint32_t list[BDW_GATE_WORDS_MAX];
    const uint32_t result = 0xfffff000, gate = 0x1000;
    for (unsigned e = 0; e < BDW_BARRIER_ENGINES; ++e) {
        memset(list, 0xaa, sizeof(list));
        unsigned count = intel_bdw_ppgtt_drain_and_hold(e, result, gate, marker, list);
        bool rcs = e == BDW_BARRIER_RCS;
        assert(count == (rcs ? 26u : 16u));
        assert(intel_bdw_ppgtt_barrier(e, result + 64, marker, out));
        assert(!memcmp(list, out, sizeof(out)));
        unsigned common = 6;
        if (rcs) {
            const uint32_t first[] = {0x0e00c002, 1, gate + 64, 0};
            assert(!memcmp(list + 6, first, sizeof(first)));
            assert(intel_bdw_ppgtt_barrier(e, result + 192, marker, out));
            assert(!memcmp(list + 10, out, sizeof(out)));
            common = 16;
        }
        const uint32_t held[] = {0x0e00c002, 1, gate, 0};
        assert(!memcmp(list + common, held, sizeof(held)));
        assert(intel_bdw_ppgtt_barrier(e, result + 128, ~marker, out));
        assert(!memcmp(list + common + 4, out, sizeof(out)));
        for (unsigned i = count; i < BDW_GATE_WORDS_MAX; ++i) assert(list[i] == 0xaaaaaaaa);
    }
    uint32_t before[BDW_GATE_WORDS_MAX];
    memcpy(before, list, sizeof(list));
    assert(!intel_bdw_ppgtt_drain_and_hold(0, gate, gate, marker, list));
    assert(!intel_bdw_ppgtt_drain_and_hold(0, result, gate + 1, marker, list));
    assert(!intel_bdw_ppgtt_drain_and_hold(0, result + 4096ull, gate, marker, list));
    assert(!intel_bdw_ppgtt_drain_and_hold(BDW_BARRIER_ENGINES, result, gate, marker, list));
    assert(!intel_bdw_ppgtt_drain_and_hold(0, result, gate, UINT64_MAX, list));
    assert(!memcmp(before, list, sizeof(list)));

}

int main(void) {
 private_barrier_cases();codec();input_cases();setup(NORMAL,0,0);passed();
 unsigned events=model.events,clocks=model.clocks,proofs=model.proofs,reads=model.reads;
 for(unsigned f=0;f<21;f++)for(unsigned at=1;at<=events;at++){
  setup(SOURCE_DRIFT,at,f);failed();assert(model.hit && model.events==at);
 }
 for(unsigned f=0;f<20;f++)for(unsigned at=1;at<=events;at++){
  setup(RESULT_DRIFT,at,f);failed();assert(model.hit && model.events==at);
 }
 for(unsigned f=0;f<6;f++)for(unsigned at=1;at<=events;at++){
  setup(PACKET_DRIFT,at,f);failed();assert(model.hit && model.events==at);
 }
 for(unsigned at=1;at<=events;at++){
  setup(IO_DRIFT,at,0);failed();assert(model.hit && model.events==at);
  setup(REGISTRATION_DRIFT,at,0);failed();assert(model.hit && model.events==at);
  setup(REENTER,at,0);passed();assert(model.hit);
 }
 for(unsigned at=1;at<=proofs;at++){
  setup(PROOF_FALSE,at,0);failed();
  for(unsigned field=0;field<16;field++){setup(PROOF_DRIFT,at,field);failed();assert(model.hit);}
 }
 for(unsigned at=1;at<=reads;at++){setup(READ_FALSE,at,0);failed();}
 for(unsigned at=1;at<=clocks;at++){
  setup(CLOCK_ZERO,at,0);failed();assert(result.clock_reason==BCS_CLOCK_ZERO);
  setup(CLOCK_BACK,at,0);failed();assert(result.clock_reason==BCS_CLOCK_BACKWARD);
 }
 for(enum fault f=STALE_NONCE;f<=NEVER_COMPLETES;f++){
  setup(f,0,0);failed();
  if(f==STALE_NONCE || f==STALE_OTHER)assert(!model.publishes && !model.submits);
  if(f==PUBLISH_NONE || f==PUBLISH_UNKNOWN)assert(!model.submits);
  if(f==SUBMIT_NONE || f==SUBMIT_UNKNOWN)assert(model.reads==1);
 }
 setup(NORMAL,0,0);model.signal_after=1;passed();assert(result.p.polls==1);
 setup(NORMAL,0,0);model.signal_after=256;passed();assert(result.p.polls==256);
 setup(NEVER_COMPLETES,0,0);failed();assert(result.p.polls==256 && result.reason==BCS_FLUSH_TIMEOUT);
 setup(FROZEN,0,0);failed();assert(result.clock_reason==BCS_CLOCK_FROZEN);
 boundaries(clocks);
 printf("bcs-flush: PASS cases=%u events=%u clocks=%u prepare_us=%llu; synthetic only, no native provider or GPU grant\n",cases,events,clocks,(unsigned long long)BCS_FLUSH_PREPARE_US);
}
