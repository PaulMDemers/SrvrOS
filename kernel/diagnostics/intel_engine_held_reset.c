#include <srvros/intel_engine_held_reset.h>
static bool fail(struct intel_engine_held_reset_result *r,unsigned why) {r->reason|=why;return false;}
static bool clock_ok(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io) {
 uint64_t at=io.now(io.ctx);
 if(!at || at<r->last_us || at<r->origin_us || at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US) {
  if(!(r->reason&IHR_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  return fail(r,IHR_CLOCK);
 }
 r->last_us=at;return true;
}
static bool guard(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io) {
 if(!clock_ok(r,io))return false;
 if(!intel_engine_prepare_reset_live(r))return fail(r,IHR_PARENT);
 bool ok=io.guard(io.ctx);
 if(!intel_engine_prepare_reset_live(r))return fail(r,IHR_PARENT);
 if(!clock_ok(r,io))return false;
 if(!intel_engine_prepare_reset_live(r))return fail(r,IHR_PARENT);
 return ok?true:fail(r,IHR_GUARD);
}
static bool snapshot(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io,unsigned n) {
 if(!guard(r,io))return false;
 /* Copy expected data while the original source prerequisite is admitted;
  * value failure must survive a simultaneous lost post-provider lifetime. */
 const struct intel_completion_result *c=r->preparation->preflight->census;
 uint32_t expected[8];
 for(unsigned i=0;i<8;i++)expected[i]=i<6?c->words[1][4][i]:c->context[1][i==6?3:5];
 ++r->snapshot_attempts;
 bool ok=io.snapshot(io.ctx,r->words[n],&r->domains[n?3:0],&r->counts[n]);
 if(r->counts[n]>9){r->counts[n]=0;fail(r,IHR_READ);}
 r->reads+=r->counts[n];if(r->counts[n]==9)++r->snapshots;
 bool values=true;
 for(unsigned i=0;i<r->counts[n] && i<8;i++)if(r->words[n][i]!=expected[i] ||
    (n && r->words[n][i]!=r->words[0][i]))values=false;
 if(r->counts[n]==9 && r->domains[n?3:0])values=false;
 if(!values)fail(r,IHR_STATE);
 bool guarded=guard(r,io);
 if(!ok || r->counts[n]!=9)fail(r,IHR_READ);
 return guarded && values && ok && r->counts[n]==9;
}
static bool pulse_run(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io,unsigned n) {
 if(!guard(r,io))return false;
 uint64_t phase=r->last_us;r->phase_begin_us[n]=phase;
 /* Consume the software epoch even when the provider definitely issues no
  * store. No callback lies between invalidation and the possible store. */
 if(!n){r->epoch_advanced=true;r->epoch_after=r->epoch_before+1;r->parent->engine_epoch=r->epoch_after;}
 if(!intel_engine_prepare_reset_live(r))return fail(r,IHR_PARENT);
 ++r->attempts;r->uncertain=true;
 uint32_t v=UINT32_MAX;
 enum intel_gt_write wrote=io.transition(io.ctx,INTEL_BCS_RESET_DOMAIN,&v);
 if(wrote==GT_WRITE_DONE){++r->writes;++r->reads;r->ack[n]=r->final[n]=v;if(v&~INTEL_BCS_RESET_DOMAIN)fail(r,IHR_STATE);}
 bool guarded=guard(r,io);
 if(wrote!=GT_WRITE_DONE)fail(r,IHR_WRITE);
 if(!guarded || wrote!=GT_WRITE_DONE)return false;
 for(unsigned i=0;i<INTEL_ENGINE_PREPARE_POLLS;i++) {
  ++r->polls;
  if(v&~INTEL_BCS_RESET_DOMAIN)return fail(r,IHR_STATE);
  if(r->last_us-phase>INTEL_ENGINE_PREPARE_PHASE_US)return fail(r,IHR_TIMEOUT);
  if(!v){r->domains[n+1]=v;++r->acks;return true;}
  if(i+1==INTEL_ENGINE_PREPARE_POLLS)return fail(r,IHR_TIMEOUT);
  io.pause(io.ctx);
  if(!guard(r,io))return false;
  bool read=io.domain(io.ctx,&v);
  if(read){++r->reads;++r->phase_polls[n];r->final[n]=v;if(v&~INTEL_BCS_RESET_DOMAIN)fail(r,IHR_STATE);}
  guarded=guard(r,io);
  if(!read)fail(r,IHR_READ);
  if(!guarded || !read)return false;
 }
 return fail(r,IHR_TIMEOUT);
}
static bool pulse(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io,unsigned n) {
 bool ok=pulse_run(r,io,n);
 if(r->phase_begin_us[n])r->phase_end_us[n]=r->last_us;
 return ok;
}
bool intel_engine_held_reset_run(struct intel_engine_held_reset_result *r,struct intel_engine_held_reset_io io) {
 if(!r || r->object!=r || !r->attempted || !r->active || r->entered || r->begin_us || r->complete || r->reason ||
    !io.guard || !io.snapshot || !io.domain || !io.transition || !io.begin || !io.end || !io.now || !io.pause ||
    !intel_engine_prepare_reset_live(r))return false;
 /* Seal entry before the very first clock/provider callback: begin_us is not
  * yet available to prevent recursive entry during initial admission. */
 r->entered=true;
 if(!guard(r,io))return false;
 r->begin_us=r->last_us;r->stage=1;
 bool entered=io.begin(io.ctx);if(entered)r->provider_enter=true;
 bool guarded=guard(r,io);
 if(!entered)fail(r,IHR_PROVIDER);
 if(!guarded || !entered)goto done;
 if(!snapshot(r,io,0))goto done;
 r->stage=2;
 if(!pulse(r,io,0) || !pulse(r,io,1))goto done;
 r->stage=3;r->settle_begin_us=r->last_us;
 for(unsigned i=0;i<INTEL_ENGINE_PREPARE_POLLS;i++) {
  io.pause(io.ctx);
  if(!guard(r,io))goto done;
  if(r->last_us-r->settle_begin_us>INTEL_ENGINE_PREPARE_PHASE_US){fail(r,IHR_TIMEOUT);goto done;}
  if(r->last_us-r->settle_begin_us>=INTEL_BCS_RESET_SETTLE_US){r->settle_end_us=r->last_us;break;}
 }
 if(!r->settle_end_us){fail(r,IHR_TIMEOUT);goto done;}
 r->stage=4;
 if(!snapshot(r,io,1) || !guard(r,io))goto done;
 bool exited=io.end(io.ctx);if(exited)r->provider_exit=true;
 guarded=guard(r,io);
 if(!exited)fail(r,IHR_PROVIDER);
 if(!guarded || !exited)goto done;
 r->stage=5;
done:
 (void)clock_ok(r,io);r->end_us=r->last_us;
 if(!intel_engine_prepare_reset_live(r))fail(r,IHR_PARENT);
 if(r->end_us<=r->begin_us)fail(r,IHR_CLOCK);
 r->complete=!r->reason && r->stage==5 && r->provider_enter && r->provider_exit && r->epoch_advanced &&
  r->attempts==2 && r->writes==2 && r->acks==2 && r->snapshots==2 && r->counts[0]==9 && r->counts[1]==9 &&
  r->settle_end_us>=r->settle_begin_us+INTEL_BCS_RESET_SETTLE_US;
 if(r->complete)r->uncertain=false;
 return r->complete;
}
