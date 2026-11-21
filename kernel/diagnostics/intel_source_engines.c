#include <srvros/intel_source_engines.h>
#ifndef SRVROS_INTEL_SOURCE_ENGINES
#error "Source engines require their separate image"
#endif
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fail(struct intel_source_engines *r,unsigned why){r->complete=false;r->restored=false;r->reason|=why;r->state=ISE_FAULT;return false;}
bool intel_source_engines_live(const struct intel_source_engines *r) {
 if(!r || r->object!=r || !r->registered || !r->busy || r->reason || r->state<ISE_OPENING || r->state>ISE_CANCELLING ||
    r->complete || r->restored || r->phase>=4 || r->step>=10 || r->index>=5 || r->samples>4 ||
    r->attempts>10 || r->writes>r->attempts || r->acks>r->writes || r->polls>2560 ||
    r->borrows!=1 || r->returns || !r->requester || !intel_source_requester_live(r->requester))return false;
 for(unsigned n=0;n<4;n++)if(r->counts[n]>6)return false;
 const struct intel_source_requester *p=r->requester;
 if(p->state!=ISR_INHIBITED || !p->child_entered || p->child_complete || p->dependent!=r || p->registered_child!=r ||
    p->child_borrows!=1 || p->child_returns || p->transfer!=r->transfer || p->source!=r->source ||
    p->provider!=r->provider || p->lifetime!=r->lifetime || p->epoch!=r->epoch || p->kind!=r->kind ||
    p->post_mode!=r->post_mode || p->origin!=r->origin || p->parent_begin!=r->parent_begin ||
    p->last!=r->last || !r->begin || r->begin>r->last || r->end)return false;
 return r->io.ctx==r->original_io.ctx && r->io.guard==r->original_io.guard && r->io.sample==r->original_io.sample &&
  r->io.control==r->original_io.control && r->io.transition==r->original_io.transition &&
  r->io.now==r->original_io.now && r->io.pause==r->original_io.pause;
}
bool intel_source_engines_deadline(struct intel_source_engines *r,uint64_t at) {
 if(!r || r->object!=r || r->reason || !r->registered || !r->busy)return false;
 if(!intel_source_engines_live(r))return fail(r,ISE_SOURCE);
 if(!at || at<r->last || !intel_source_requester_time(r->requester,at)) {
  r->clock_at=at;r->clock_prev=r->last;return fail(r,ISE_CLOCK);
 }
 if(!intel_source_requester_deadline(r->requester,at))return fail(r,ISE_SOURCE);
 r->last=at;return true;
}
static bool tick(struct intel_source_engines *r) {
 if(!intel_source_engines_live(r))return fail(r,ISE_SOURCE);
 unsigned state=r->state;uint64_t at=r->io.now(r->io.ctx);
 if(!intel_source_engines_live(r) || r->state!=state)return fail(r,ISE_SOURCE);
 return intel_source_engines_deadline(r,at);
}
static bool guard(struct intel_source_engines *r) {
 if(!tick(r))return false;
 unsigned state=r->state;bool ok=r->io.guard(r->io.ctx);
 if(!intel_source_engines_live(r) || r->state!=state)return fail(r,ISE_SOURCE);
 return ok?tick(r):fail(r,ISE_GUARD);
}
static bool valid(uint32_t v){return v!=UINT32_MAX && !(v&0xfffcu);}
/* Completed hardware profiles must still be complete at each pure admission. */
static bool profiles_current(const struct intel_source_engines *r,unsigned count) {
 if(!count || count>4)return false;
 for(unsigned n=0;n<count;n++) {
  if(r->counts[n]!=6)return false;
  for(unsigned e=0;e<6;e++) {
   uint32_t v=r->words[n][e];unsigned want=n==1 || n==2?3u:0u;
   if(e==5?v!=0:!valid(v) || (v&3u)!=want)return false;
   if(n && (v&~3u)!=(r->words[0][e]&~3u))return false;
   if((n==2 && v!=r->words[1][e]) || (n==3 && v!=r->words[0][e]))return false;
  }
 }
 return true;
}
static bool sample(struct intel_source_engines *r,unsigned n) {
 r->phase=n;if(!guard(r))return false;
 r->sample_begin[n]=r->last;unsigned state=r->state;
 bool ok=r->io.sample(r->io.ctx,r->words[n],&r->counts[n]);
 if(!intel_source_engines_live(r) || r->state!=state || r->phase!=n)return fail(r,ISE_SOURCE);
 if(r->counts[n]>6){r->counts[n]=0;return fail(r,ISE_READ);}
 r->reads+=r->counts[n];if(r->counts[n]==6)++r->samples;
 if(!guard(r))return false;
 r->sample_end[n]=r->last;
 if(!ok || r->counts[n]!=6)return fail(r,ISE_READ);
 for(unsigned e=0;e<6;e++) {
  uint32_t v=r->words[n][e];unsigned want=n==1 || n==2?3u:0u;
  if(e==5?v!=0:!valid(v) || (v&3u)!=want)return fail(r,ISE_STATE);
  if(n && (v&~3u)!=(r->words[0][e]&~3u))return fail(r,ISE_STATE);
  if((n==2 && v!=r->words[1][e]) || (n==3 && v!=r->words[0][e]))return fail(r,ISE_STATE);
 }
 return true;
}
bool intel_source_engines_write_ready(const struct intel_source_engines *r,unsigned e,uint32_t value) {
 if(!intel_source_engines_live(r) || r->step>=10 || r->index!=e || e>=5 ||
    r->attempts!=r->step+1 || r->writes!=r->step || r->acks!=r->step || !profiles_current(r,r->step<5?1:3))return false;
 if(r->step<5)return r->state==ISE_PREPARING && r->index==r->step && r->samples==1 && r->phase==0 && value==0x10001;
 return r->state==ISE_CANCELLING && r->index==9-r->step && r->samples==3 && r->phase==2 &&
  r->child_complete && value==0x10000;
}
static bool transition(struct intel_source_engines *r,unsigned n) {
 r->step=n;r->index=n<5?n:9-n;if(!guard(r))return false;
 const uint64_t phase_origin=r->last;
 r->phase_begin[n]=phase_origin;++r->attempts;uint32_t v=UINT32_MAX;
 enum intel_gt_write wrote=r->io.transition(r->io.ctx,r->index,n<5?0x10001u:0x10000u,&v);
 if(!intel_source_engines_live(r) || r->step!=n || r->index!=(n<5?n:9-n) || r->phase_begin[n]!=phase_origin)return fail(r,ISE_SOURCE);
 if(wrote==GT_WRITE_DONE){++r->writes;++r->reads;r->ack[n]=r->final[n]=v;}
 if(wrote!=GT_WRITE_DONE)return fail(r,ISE_WRITE);
 if(!guard(r))return false;
 for(unsigned i=0;i<=256;i++) {
  if(r->phase_begin[n]!=phase_origin || r->step!=n || r->index!=(n<5?n:9-n) || r->last<phase_origin)return fail(r,ISE_SOURCE);
  if(r->last-phase_origin>10000)return fail(r,ISE_PENDING);
  if(!valid(v) || (v&1u)!=(unsigned)(n<5) || (v&~3u)!=(r->words[0][r->index]&~3u))return fail(r,ISE_STATE);
  if((v&3u)==(n<5?3u:0u)){++r->acks;r->phase_end[n]=r->last;return true;}
  if(i==256)return fail(r,ISE_PENDING);
  r->io.pause(r->io.ctx);if(!guard(r))return false;
  if(r->phase_begin[n]!=phase_origin || r->step!=n || r->index!=(n<5?n:9-n))return fail(r,ISE_SOURCE);
  bool ok=r->io.control(r->io.ctx,r->index,&v);
  if(!intel_source_engines_live(r) || r->step!=n || r->index!=(n<5?n:9-n) || r->phase_begin[n]!=phase_origin)return fail(r,ISE_SOURCE);
  if(ok){++r->reads;++r->polls;++r->phase_polls[n];r->final[n]=v;}
  if(!guard(r))return false;
  if(!ok)return fail(r,ISE_READ);
 }
 return fail(r,ISE_PENDING);
}
bool intel_source_engines_run(struct intel_source_engines *r,struct intel_source_requester *p,
 struct intel_source_engines_io io,bool (*observe)(void *,struct intel_source_engines *),void *ctx) {
 if(!r || r->object || !p || !io.guard || !io.sample || !io.control || !io.transition || !io.now || !io.pause || !observe ||
    !intel_source_requester_live(p) || p->state!=ISR_INHIBITED || !p->child_entered || p->child_complete ||
    p->dependent || p->registered_child || p->child_borrows || p->child_returns)return false;
 const struct intel_source_transfer *t=p->transfer;
 if(overlap(r,sizeof(*r),p,sizeof(*p)) || overlap(r,sizeof(*r),t,sizeof(*t)) ||
    overlap(r,sizeof(*r),t->buffers,sizeof(*t->buffers)) || overlap(r,sizeof(*r),t->source,sizeof(*t->source)) ||
    overlap(r,sizeof(*r),t->source->reset,sizeof(*t->source->reset)) || overlap(r,sizeof(*r),t->source->parent,sizeof(*t->source->parent)) ||
    overlap(r,sizeof(*r),t->input,sizeof(*t->input)) || overlap(r,sizeof(*r),t->source->entry,sizeof(*t->source->entry)))return false;
 for(unsigned i=0;i<2;i++)if(overlap(r,sizeof(*r),t->hold[i],sizeof(*t->hold[i])) ||
    overlap(r,sizeof(*r),t->memory[i].object,sizeof(*t->memory[i].object)))return false;
 *r=(struct intel_source_engines){.object=r,.requester=p,.transfer=p->transfer,.source=p->source,.provider=p->provider,
  .lifetime=p->lifetime,.io=io,.original_io=io,.origin=p->origin,.parent_begin=p->parent_begin,.begin=p->last,.last=p->last,
  .epoch=p->epoch,.kind=p->kind,.post_mode=p->post_mode,.state=ISE_OPENING,.busy=true,.registered=true,.borrows=1};
 p->dependent=p->registered_child=r;++p->child_borrows;
 r->parent_blocked=!intel_source_requester_child_returned(p) && !intel_source_transfer_check(p->transfer) && !intel_source_transfer_close(p->transfer);
 r->holds_blocked=!scanout_memory_hold_end(p->buffers->hold[0]) && !scanout_memory_hold_end(p->buffers->hold[1]);
 if(!r->parent_blocked || !r->holds_blocked){fail(r,ISE_SOURCE);goto done;}
 if(!sample(r,0))goto done;
 r->state=ISE_PREPARING;
 for(unsigned i=0;i<5;i++)if(!transition(r,i))goto done;
 r->state=ISE_HELD;if(!sample(r,1) || !guard(r))goto done;
 if(!profiles_current(r,2)){fail(r,ISE_STATE);goto done;}
 r->child_begin=r->last;r->child_entered=true;
 if(!observe(ctx,r)){fail(r,ISE_CHILD);goto done;}
 if(!intel_source_engines_live(r) || r->state!=ISE_HELD){fail(r,ISE_SOURCE);goto done;}
 if(!guard(r))goto done;
 r->child_end=r->last;r->child_complete=true;
 if(!sample(r,2))goto done;
 r->state=ISE_CANCELLING;
 for(unsigned i=5;i<10;i++)if(!transition(r,i))goto done;
 if(!sample(r,3) || !guard(r))goto done;
 if(r->last<=r->begin){r->clock_at=r->last;r->clock_prev=r->begin;fail(r,ISE_CLOCK);goto done;}
 unsigned polls=0;
 for(unsigned n=0;n<10;n++) {
  if(r->phase_polls[n]>256){fail(r,ISE_STATE);goto done;}
  polls+=r->phase_polls[n];
 }
 if(!intel_source_engines_live(r) || r->state!=ISE_CANCELLING || r->phase!=3 || r->step!=9 || r->index ||
    r->samples!=4 || r->attempts!=10 || r->writes!=10 || r->acks!=10 || !r->child_entered || !r->child_complete ||
    !r->parent_blocked || !r->holds_blocked || !profiles_current(r,4) || polls!=r->polls ||
    r->reads!=24+r->writes+r->polls){fail(r,ISE_STATE);goto done;}
 p->dependent=p->registered_child=0;++p->child_returns;++r->returns;
 r->restored=true;r->end=r->last;r->registered=false;r->state=ISE_CLOSED;r->complete=true;
 done:r->busy=false;return r->complete;
}
