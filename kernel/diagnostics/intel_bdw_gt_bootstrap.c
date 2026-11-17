#include <srvros/intel_bdw_gt_bootstrap.h>

static const uint32_t controls[5]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0};
static bool bytes_equal(const void *a,const void *b,unsigned n) {
 const unsigned char *x=a,*y=b;for(unsigned i=0;i<n;i++)if(x[i]!=y[i])return false;return true;
}
static void bytes_copy(void *to,const void *from,unsigned n) {
 unsigned char *a=to;const unsigned char *b=from;for(unsigned i=0;i<n;i++)a[i]=b[i];
}
static bool overlaps(const void *a,unsigned an,const void *b,unsigned bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool same_io(const struct bdw_boot_io *a,const struct bdw_boot_io *b) {
 return a->ctx==b->ctx && a->prerequisite==b->prerequisite && a->read==b->read && a->write==b->write &&
 a->advance_epoch==b->advance_epoch && a->preserved==b->preserved && a->now==b->now && a->pause==b->pause;
}
static bool fail(struct intel_bdw_gt_bootstrap *r,unsigned why) {
 /* Every call is after source registration. A callback changing registration
  * must not make the failed dependent appear retired or reusable. */
 r->object=r;r->reason|=why;r->busy=true;r->registered=true;r->retained=true;
 r->complete=false;r->stage=BDW_BOOT_FAULT;return false;
}
static bool live(const struct intel_bdw_gt_bootstrap *r) {
 return r && r->object==r && r->source && r->source->object==r->source && r->source->consumer==r &&
 r->busy && r->registered && !r->retained && !r->reason && !r->complete && !r->end &&
 r->stage>=BDW_BOOT_CAPTURE && r->stage<BDW_BOOT_DONE && same_io(&r->io,&r->original_io) &&
 bytes_equal(&r->binding,&r->source->binding,sizeof(r->binding));
}
/* Whole bounded POD snapshots include vectors/counters and callback tables.
 * Keep this non-inlined so nested admissions cannot multiply the 2KiB frame. */
struct saved {unsigned char bytes[sizeof(struct intel_bdw_gt_bootstrap)];};
_Static_assert(sizeof(struct saved)<=1792,"bounded bootstrap callback snapshot");
static void capture(const struct intel_bdw_gt_bootstrap *r,struct saved *s) {
 bytes_copy(s->bytes,r,sizeof(*r));
}
static bool unchanged(const struct intel_bdw_gt_bootstrap *r,const struct saved *s) {
 return live(r) && bytes_equal(r,s->bytes,sizeof(*r));
}
static __attribute__((noinline)) bool tick(struct intel_bdw_gt_bootstrap *r) {
 if(!live(r))return fail(r,BDW_BOOT_SOURCE);
 struct saved s;capture(r,&s);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 unsigned why=!at?1:at<r->last?2:at<r->binding.prepare || at-r->binding.prepare>INTEL_DIAGNOSTIC_PREPARE_US?3:
 at<r->binding.gt || at-r->binding.gt>INTEL_DIAGNOSTIC_GT_TOTAL_US?4:at<r->binding.hda || at-r->binding.hda>INTEL_DIAGNOSTIC_HDA_US?5:
 at<r->binding.outer || at-r->binding.outer>INTEL_DIAGNOSTIC_OUTER_US?6:0;
 if(why){r->clock_at=at;r->clock_previous=r->last;r->clock_reason=why;return fail(r,BDW_BOOT_CLOCK);}
 if(!why && r->phase_begin && (r->stage==BDW_BOOT_RESET || r->stage==BDW_BOOT_SETTLE || r->stage==BDW_BOOT_CANCEL) &&
 (at<r->phase_begin || at-r->phase_begin>BDW_BOOT_PHASE_US)) {
  r->clock_at=at;r->clock_previous=r->last;r->clock_reason=8;return fail(r,BDW_BOOT_CLOCK|BDW_BOOT_TIMEOUT);
 }
 if(!r->begin)r->begin=at;
 r->last=at;return true;
}
static __attribute__((noinline)) bool guard(struct intel_bdw_gt_bootstrap *r) {
 if(!tick(r))return false;
 struct saved s;capture(r,&s);uint64_t before=r->last;struct bdw_boot_proof p={0};
 bool ok=r->io.prerequisite(r->io.ctx,r->source,&p);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 if(!tick(r))return false;
 if(!ok || p.source!=r->source || p.owner!=r->binding.owner || p.provider!=r->binding.provider ||
 p.lifetime!=r->binding.lifetime || p.root!=r->binding.root || p.sequence!=r->binding.sequence ||
 p.epoch!=r->binding.epoch+(unsigned)r->epoch_advanced || p.checked_at<before || p.checked_at>r->last ||
 p.prerequisites!=BDW_BOOT_REQUIRED)return fail(r,BDW_BOOT_PROOF);
 ++r->proofs;return true;
}
static __attribute__((noinline)) bool load(struct intel_bdw_gt_bootstrap *r,uint32_t offset,uint32_t *out) {
 if(!out || !guard(r))return false;
 ++r->read_attempts;struct saved s;capture(r,&s);uint32_t value=UINT32_MAX;
 bool ok=r->io.read(r->io.ctx,offset,&value);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 if(ok)++r->reads;
 if(!guard(r))return false;
 if(!ok)return fail(r,BDW_BOOT_READ);
 *out=value;return true;
}
bool intel_bdw_gt_bootstrap_write_ready(const struct intel_bdw_gt_bootstrap *r,uint32_t offset,uint32_t value) {
 if(!live(r) || !r->epoch_advanced || r->write_attempts!=r->writes+1)return false;
 if((r->stage==BDW_BOOT_RESET || r->stage==BDW_BOOT_CANCEL) &&
 (!r->phase_begin || r->last<r->phase_begin || r->last-r->phase_begin>BDW_BOOT_PHASE_US))return false;
 if(r->stage==BDW_BOOT_RESET)return r->index<2 && r->reset_attempts==r->index+1 && r->reset_writes==r->index &&
 r->reset_completed==r->index && offset==BDW_BOOT_GDRST && value==BDW_BOOT_FULL_RESET;
 if(r->stage==BDW_BOOT_RESTORE && r->index<32 && r->step<3 &&
 (r->changed_mask&(1u<<r->index)) && !(r->restored_mask&(1u<<r->index))) {
  uint64_t v=r->fence_before[r->index];uint32_t expected=r->step==0?0:r->step==1?(uint32_t)(v>>32):(uint32_t)v;
  return offset==BDW_BOOT_FENCE_BASE+8*r->index+(r->step==1?4:0) && value==expected;
 }
 return r->stage==BDW_BOOT_CANCEL && r->index<5 && r->cancel_attempts==5-r->index &&
 r->cancel_writes==4-r->index && r->cancelled==4-r->index && offset==controls[r->index] && value==0x10000u;
}
static __attribute__((noinline)) bool store(struct intel_bdw_gt_bootstrap *r,uint32_t offset,uint32_t value) {
 if(!guard(r))return false;
 ++r->write_attempts;
 if(r->stage==BDW_BOOT_RESET)++r->reset_attempts;
 else if(r->stage==BDW_BOOT_RESTORE)++r->fence_attempts;
 else if(r->stage==BDW_BOOT_CANCEL)++r->cancel_attempts;
 if(!intel_bdw_gt_bootstrap_write_ready(r,offset,value))return fail(r,BDW_BOOT_STATE);
 struct saved s;capture(r,&s);enum intel_gt_write wrote=r->io.write(r->io.ctx,offset,value);
 if(!unchanged(r,&s)){r->uncertain=true;return fail(r,BDW_BOOT_SOURCE);}
 if(wrote==GT_WRITE_DONE){++r->writes;if(r->stage==BDW_BOOT_RESET)++r->reset_writes;else if(r->stage==BDW_BOOT_RESTORE)++r->fence_writes;else ++r->cancel_writes;}
 else {r->uncertain=wrote!=GT_WRITE_NONE;return fail(r,BDW_BOOT_WRITE);}
 return guard(r);
}
static __attribute__((noinline)) bool pause_once(struct intel_bdw_gt_bootstrap *r) {
 if(!guard(r))return false;
 struct saved s;capture(r,&s);r->io.pause(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 return guard(r);
}
static bool phase_ok(struct intel_bdw_gt_bootstrap *r) {
 return r->last>=r->phase_begin && r->last-r->phase_begin<=BDW_BOOT_PHASE_US?true:fail(r,BDW_BOOT_TIMEOUT);
}
static bool fence_read(struct intel_bdw_gt_bootstrap *r,unsigned i,uint64_t *out) {
 uint32_t hi,lo,again;uint32_t address=BDW_BOOT_FENCE_BASE+8*i;
 if(!load(r,address+4,&hi) || !load(r,address,&lo) || !load(r,address+4,&again))return false;
 if(hi!=again)return fail(r,BDW_BOOT_STATE);
 *out=(uint64_t)hi<<32|lo;return true;
}
static bool fence_valid(uint64_t value,uint64_t aperture) {
 if(value&~0xfffff7fffffff003ull)return false;
 if(!(value&1))return true;
 uint64_t low=value&0xfffff000ull,high=(value>>32)&0xfffff000ull;
 unsigned pitch=(unsigned)((value>>32)&2047)+1;
 return low<=high && high+4096<=aperture && ((value&2) || !(pitch&3));
}
static bool input(const struct bdw_boot_source *s,struct bdw_boot_io io) {
 if(!s || s->object || s->consumer || !io.ctx || !io.prerequisite || !io.read || !io.write ||
 !io.advance_epoch || !io.preserved || !io.now || !io.pause)return false;
 const struct bdw_boot_binding *b=&s->binding;
 if(!b->owner || !b->provider || !b->lifetime || b->provider!=io.ctx || b->vendor!=0x8086 || b->device!=0x1626 ||
 b->policy!=BDW_BOOT_POLICY || !b->root || (b->root&4095) || !b->mmio || (b->mmio&0xffffff) ||
 b->mmio_bytes<BDW_BOOT_FENCE_BASE+256 || b->mmio_bytes>0x1000000 || b->mmio>UINT64_MAX-b->mmio_bytes ||
 !b->aperture_bytes || b->aperture_bytes>0x100000000ull || (b->aperture_bytes&4095) ||
 b->epoch>=UINT32_MAX || b->sequence!=1 || !b->outer || b->outer>b->hda || b->hda>b->gt || b->gt>b->prepare)return false;
 for(unsigned i=0;i<32;i++){
  uint64_t v=b->fences[i];if(!fence_valid(v,b->aperture_bytes))return false;
  if(!(v&1))continue;
  uint64_t low=v&0xfffff000ull,high=((v>>32)&0xfffff000ull)+4096;
  for(unsigned j=0;j<i;j++){uint64_t p=b->fences[j];if((p&1) && low<((p>>32)&0xfffff000ull)+4096 && (p&0xfffff000ull)<high)return false;}
 }
 return true;
}
static __attribute__((noinline)) bool advance(struct intel_bdw_gt_bootstrap *r) {
 r->stage=BDW_BOOT_EPOCH;if(!guard(r))return false;
 r->epoch_attempted=true;struct saved s;capture(r,&s);
 bool ok=r->io.advance_epoch(r->io.ctx,r->source,r->binding.epoch,r->binding.epoch+1);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 if(!ok)return fail(r,BDW_BOOT_SOURCE);
 r->epoch_advanced=true;return guard(r);
}
static bool pulse(struct intel_bdw_gt_bootstrap *r,unsigned n) {
 r->stage=BDW_BOOT_RESET;r->index=n;r->phase_begin=0;if(!guard(r))return false;
 r->phase_begin=r->last;
 if(!store(r,BDW_BOOT_GDRST,BDW_BOOT_FULL_RESET) || !phase_ok(r))return false;
 for(unsigned i=0;i<BDW_BOOT_POLLS;i++){
  uint32_t value;if(!load(r,BDW_BOOT_GDRST,&value) || !phase_ok(r))return false;
  if(!i)r->reset_first[n]=value;
  r->reset_final[n]=value;++r->reset_polls[n];
  if(value&~1u)return fail(r,BDW_BOOT_STATE);
  if(!value){++r->reset_completed;return true;}
  if(i+1==BDW_BOOT_POLLS)return fail(r,BDW_BOOT_TIMEOUT);
  if(!pause_once(r) || !phase_ok(r))return false;
 }
 return fail(r,BDW_BOOT_TIMEOUT);
}
static bool settle(struct intel_bdw_gt_bootstrap *r) {
 r->stage=BDW_BOOT_SETTLE;r->phase_begin=0;if(!guard(r))return false;
 r->settle_begin=r->last;r->phase_begin=r->last;
 for(unsigned i=0;i<BDW_BOOT_POLLS;i++){
  if(!pause_once(r) || !phase_ok(r))return false;
  if(r->last-r->settle_begin>=BDW_BOOT_SETTLE_US){r->settle_end=r->last;return true;}
 }
 return fail(r,BDW_BOOT_TIMEOUT);
}
static bool restore(struct intel_bdw_gt_bootstrap *r,unsigned i) {
 r->stage=BDW_BOOT_RESTORE;r->index=i;
 uint32_t base=BDW_BOOT_FENCE_BASE+8*i,posted;
 r->step=0;if(!store(r,base,0) || !load(r,base,&posted))return false;
 if(posted)return fail(r,BDW_BOOT_STATE);
 r->step=1;if(!store(r,base+4,(uint32_t)(r->fence_before[i]>>32)))return false;
 r->step=2;if(!store(r,base,(uint32_t)r->fence_before[i]) || !load(r,base,&posted))return false;
 if(posted!=(uint32_t)r->fence_before[i])return fail(r,BDW_BOOT_STATE);
 r->restored_mask|=1u<<i;return true;
}
static bool cancel(struct intel_bdw_gt_bootstrap *r,unsigned i) {
 r->stage=BDW_BOOT_CANCEL;r->index=i;r->phase_begin=0;if(!guard(r))return false;
 r->phase_begin=r->last;
 if(!store(r,controls[i],0x10000u) || !phase_ok(r))return false;
 for(unsigned n=0;n<BDW_BOOT_POLLS;n++){
  uint32_t value;if(!load(r,controls[i],&value) || !phase_ok(r))return false;
  if(!n)r->cancel_first[i]=value;
  r->cancel_final[i]=value;++r->cancel_polls[i];
  if(value==UINT32_MAX || (value&0xfffcu))return fail(r,BDW_BOOT_STATE);
  if(!(value&3u)){++r->cancelled;return true;}
  if(n+1==BDW_BOOT_POLLS)return fail(r,BDW_BOOT_TIMEOUT);
  if(!pause_once(r) || !phase_ok(r))return false;
 }
 return fail(r,BDW_BOOT_TIMEOUT);
}
static __attribute__((noinline)) bool preserved(struct intel_bdw_gt_bootstrap *r) {
 r->stage=BDW_BOOT_VERIFY;if(!guard(r))return false;
 ++r->preserve_calls;struct saved s;capture(r,&s);bool ok=r->io.preserved(r->io.ctx,r);
 if(!unchanged(r,&s))return fail(r,BDW_BOOT_SOURCE);
 if(!guard(r))return false;
 if(!ok)return fail(r,BDW_BOOT_PRESERVE);
 r->preserved=true;return true;
}
bool intel_bdw_gt_bootstrap_run(struct intel_bdw_gt_bootstrap *r,struct bdw_boot_source *s,struct bdw_boot_io io) {
 if(!r || !s || overlaps(r,sizeof(*r),s,sizeof(*s)) || r->object)return false;
 *r=(struct intel_bdw_gt_bootstrap){.object=r};
 if(!input(s,io)){r->reason=BDW_BOOT_INPUT;r->stage=BDW_BOOT_FAULT;return false;}
 r->source=s;bytes_copy(&r->binding,&s->binding,sizeof(r->binding));r->io=r->original_io=io;
 s->object=s;s->consumer=r;r->stage=BDW_BOOT_CAPTURE;r->busy=true;r->registered=true;
 if(!guard(r))return false;
 for(unsigned i=0;i<32;i++){
  if(!fence_read(r,i,&r->fence_before[i]))return false;
  if(r->fence_before[i]!=r->binding.fences[i])return fail(r,BDW_BOOT_STATE);
 }
 for(unsigned i=0;i<5;i++){
  if(!load(r,controls[i],&r->control_before[i]))return false;
  if(r->control_before[i]==UINT32_MAX || (r->control_before[i]&65535)!=3)return fail(r,BDW_BOOT_STATE);
 }
 if(!load(r,BDW_BOOT_GDRST,&r->reset_initial))return false;
 if(r->reset_initial)return fail(r,BDW_BOOT_STATE);
 if(!advance(r) || !pulse(r,0) || !pulse(r,1) || !settle(r))return false;
 r->stage=BDW_BOOT_POST;
 if(!load(r,BDW_BOOT_GDRST,&r->reset_post))return false;
 if(r->reset_post)return fail(r,BDW_BOOT_STATE);
 for(unsigned i=0;i<5;i++){
  if(!load(r,controls[i],&r->control_after[i]))return false;
  if(r->control_after[i]==UINT32_MAX || (r->control_after[i]&65535))return fail(r,BDW_BOOT_STATE);
 }
 for(unsigned i=0;i<32;i++){
  if(!fence_read(r,i,&r->fence_after[i]))return false;
  if(r->fence_after[i]!=r->fence_before[i]){
   if(r->fence_after[i])return fail(r,BDW_BOOT_STATE);
   r->changed_mask|=1u<<i;
  }
 }
 for(unsigned i=0;i<32;i++)if((r->changed_mask&(1u<<i)) && !restore(r,i))return false;
 for(unsigned n=0;n<5;n++)if(!cancel(r,4-n))return false;
 r->stage=BDW_BOOT_VERIFY;
 for(unsigned i=0;i<32;i++){
  if(!fence_read(r,i,&r->fence_final[i]))return false;
  if(r->fence_final[i]!=r->fence_before[i])return fail(r,BDW_BOOT_STATE);
 }
 if(!preserved(r) || !load(r,BDW_BOOT_GDRST,&r->reset_close))return false;
 if(r->reset_close)return fail(r,BDW_BOOT_STATE);
 if(!guard(r))return false;
 if(r->last<=r->begin){r->clock_at=r->last;r->clock_previous=r->begin;r->clock_reason=7;return fail(r,BDW_BOOT_CLOCK);}
 if(!r->epoch_attempted || !r->epoch_advanced || r->reset_attempts!=2 || r->reset_writes!=2 || r->reset_completed!=2 ||
 r->changed_mask!=r->restored_mask || r->fence_attempts!=r->fence_writes || r->cancel_attempts!=5 || r->cancel_writes!=5 ||
 r->cancelled!=5 || r->write_attempts!=r->writes || !r->preserved || r->preserve_calls!=1)return fail(r,BDW_BOOT_STATE);
 r->end=r->last;r->busy=false;r->registered=false;r->complete=true;r->stage=BDW_BOOT_DONE;return true;
}
