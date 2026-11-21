#include <srvros/intel_held_interval.h>
static bool fail(struct intel_held_interval_result *r,unsigned why) {r->reason|=why;return false;}
static bool source(const struct intel_held_interval_result *r) {
 const struct intel_engine_held_reset_result *x=r->reset;
 return r->object==r && x && intel_engine_prepare_reset_live(x) && x->entered && x->stage==4 &&
  !x->reason && x->provider_enter && !x->provider_exit && x->epoch_advanced &&
  x->epoch_before==r->epoch_before && x->epoch_after==r->epoch_after &&
  x->epoch_before!=UINT32_MAX && x->epoch_after==x->epoch_before+1 &&
  x->preparation==r->preparation && x->parent==r->parent &&
  r->borrow==&r->preparation->borrow && r->preparation->parent==r->parent &&
  x->origin_us==r->origin_us && x->attempts==2 && x->writes==2 && x->acks==2 &&
  x->snapshots==2 && x->snapshot_attempts==2 && x->counts[0]==9 && x->counts[1]==9 &&
  x->settle_begin_us && x->settle_end_us>=x->settle_begin_us+INTEL_BCS_RESET_SETTLE_US &&
  x->last_us>=x->settle_end_us && r->parent->engine_epoch==r->epoch_after &&
  r->manifest.original==&r->expected && r->manifest.source_epoch==r->epoch_after;
}
static bool clock_ok(struct intel_held_interval_result *r,struct intel_held_interval_io io) {
 uint64_t at=io.now(io.ctx);
 if(!at || at<r->last_us || at<r->origin_us || at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US) {
  if(!(r->reason&IHI_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  return fail(r,IHI_CLOCK);
 }
 r->last_us=at;return true;
}
static bool guard(struct intel_held_interval_result *r,struct intel_held_interval_io io) {
 if(!clock_ok(r,io))return false;
 if(!source(r))return fail(r,IHI_PARENT);
 bool ok=io.guard(io.ctx);
 if(!source(r))return fail(r,IHI_PARENT);
 if(!clock_ok(r,io))return false;
 if(!source(r))return fail(r,IHI_PARENT);
 return ok?true:fail(r,IHI_GUARD);
}
bool intel_held_interval_observe(struct intel_held_interval_result *r,
 const struct intel_engine_held_reset_result *x,const struct scanout_handoff_snapshot *input,struct intel_held_interval_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_held_interval_result){.object=r,.reset=x};
 if(!x || !x->preparation || !x->parent || !input || !io.guard || !io.read || !io.now ||
    input->mmio!=0xc0000000 || input->aperture!=0xb0000000 || input->aperture_bytes!=0x10000000 ||
    input->fence_begin || input->fence_end!=0x2200000 || input->first!=INTEL_WINDOW_FIRST ||
    input->pages!=INTEL_WINDOW_ENTRIES || input->assurances || input->original[0]==UINT64_MAX)
  return fail(r,IHI_INPUT);
 /* Capture only one raw original, before any provider may change caller data.
  * A populated entry or the epoch transition is never a memory grant. */
 r->expected=input->original[0];r->origin_us=x->origin_us;r->last_us=x->last_us;
 r->preparation=x->preparation;r->parent=x->parent;r->borrow=&x->preparation->borrow;
 r->epoch_before=x->epoch_before;r->epoch_after=x->epoch_after;
 r->manifest=(struct scanout_transfer_manifest){input->mmio,input->aperture,input->aperture_bytes,
  input->fence_begin,input->fence_end,x->epoch_after,input->first,1,0,INTEL_COMPLETION_OPEN,&r->expected};
 if(!source(r))return fail(r,IHI_PARENT);
 if(!guard(r,io))goto done;
 r->begin_us=r->last_us;
 for(unsigned n=0;n<2;n++) {
  r->sample_begin[n]=r->last_us;
  for(unsigned i=0;i<3;i++) {
   if(!guard(r,io)){r->sample_end[n]=r->last_us;goto done;}
   uint32_t expected=(uint32_t)(r->expected>>(i==1?0:32)),value=0;
   ++r->attempts;
   bool ok=io.read(io.ctx,INTEL_WINDOW_PAGE+(i==1?0:4),&value);
   if(ok){r->words[n][i]=value;++r->counts[n];++r->reads;}
   /* Preserve drift even if a simultaneous post-read guard is lost. */
   bool match=ok && value==expected;
   if(ok && !match)fail(r,IHI_CHANGED);
   bool guarded=guard(r,io);
   r->sample_end[n]=r->last_us;
   if(!ok)fail(r,IHI_READ);
   if(!guarded || !match)goto done;
  }
  r->sample_end[n]=r->last_us;++r->samples;
 }
 r->stable=true;
done:
 (void)clock_ok(r,io);r->end_us=r->last_us;
 if(!source(r))fail(r,IHI_PARENT);
 if(!r->begin_us || r->end_us<=r->begin_us)fail(r,IHI_CLOCK);
 r->complete=!r->reason && r->stable && r->samples==2 && r->counts[0]==3 && r->counts[1]==3 && r->reads==6;
 return r->complete;
}
bool intel_held_interval_manifest(const struct intel_held_interval_result *r,struct scanout_transfer_manifest *out) {
 if(!r || !out || !r->complete || r->reason || !source(r))return false;
 uintptr_t a=(uintptr_t)r,b=(uintptr_t)out;
 if(a>UINTPTR_MAX-sizeof(*r) || b>UINTPTR_MAX-sizeof(*out) || (a<b+sizeof(*out) && b<a+sizeof(*r)))return false;
 *out=r->manifest;return true;
}
