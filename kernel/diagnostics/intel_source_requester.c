#include <srvros/intel_source_requester.h>
#ifndef SRVROS_INTEL_SOURCE_REQUESTER
#error "Original-source requester requires its separate image"
#endif
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fail(struct intel_source_requester *r,unsigned reason) {
 r->reason|=reason;r->state=ISR_FAULT;return false;
}
bool intel_source_requester_live(const struct intel_source_requester *r) {
 if(!r || r->object!=r || !r->registered || r->reason || !r->busy || r->borrows!=1 || r->returns ||
    r->state<ISR_OPENING || r->state>ISR_RESTORING || !r->transfer ||
    !intel_source_transfer_live(r->transfer))return false;
 const struct intel_source_transfer *t=r->transfer;
 if(t->busy || t->state!=IST_REFUSED || !t->refused || t->opened || t->dependent!=r ||
    t->registered_child!=r || t->child_borrows!=1 || t->child_returns ||
    t->buffers!=r->buffers || t->source!=r->source || t->provider!=r->provider ||
    t->input!=r->input || t->lifetime!=r->lifetime || t->epoch!=r->epoch ||
    t->kind!=r->kind || t->post_mode!=r->post_mode || t->origin!=r->origin ||
    t->parent_begin!=r->parent_begin || t->last!=r->last || !r->begin || r->begin>r->last || r->end)return false;
#ifdef SRVROS_INTEL_SOURCE_ENGINES
 if(r->child_returns>r->child_borrows || r->child_borrows>1 ||
    (r->dependent && (r->dependent!=r->registered_child || r->child_borrows!=1 || r->child_returns ||
                     r->state!=ISR_INHIBITED || !r->child_entered || r->child_complete)) ||
    (!r->dependent && (r->registered_child || r->child_borrows!=r->child_returns)) ||
    (r->child_complete && !intel_source_requester_child_returned(r)))return false;
#endif
 return r->io.ctx==r->original_io.ctx && r->io.guard==r->original_io.guard &&
  r->io.sample==r->original_io.sample && r->io.command16==r->original_io.command16 &&
  r->io.pending==r->original_io.pending && r->io.now==r->original_io.now && r->io.pause==r->original_io.pause;
}
bool intel_source_requester_time(const struct intel_source_requester *r,uint64_t at) {
 return intel_source_requester_live(r) && at && at>=r->last && at>=r->source->last &&
  at>=r->source->reset->last && at>=r->source->parent->last &&
  at>=r->origin && at-r->origin<=100000 && at>=r->parent_begin && at-r->parent_begin<=300000;
}
bool intel_source_requester_deadline(struct intel_source_requester *r,uint64_t at) {
 if(!r || r->object!=r || r->reason)return false;
 if(!intel_source_requester_live(r))return fail(r,ISR_SOURCE);
 if(!intel_source_requester_time(r,at)) {
  r->clock_at=at;r->clock_prev=r->last;return fail(r,ISR_CLOCK);
 }
 r->last=r->transfer->last=r->buffers->last=r->source->last=r->source->reset->last=r->source->parent->last=at;
 return true;
}
#ifdef SRVROS_INTEL_SOURCE_ENGINES
bool intel_source_requester_child_returned(const struct intel_source_requester *r) {
 return r && r->object==r && !r->dependent && !r->registered_child && r->child_borrows==1 && r->child_returns==1;
}
#endif
static bool tick(struct intel_source_requester *r) {
 if(!intel_source_requester_live(r))return fail(r,ISR_SOURCE);
 unsigned state=r->state;struct intel_source_requester_io io=r->io;uint64_t at=io.now(io.ctx);
 if(!intel_source_requester_live(r) || r->state!=state)return fail(r,ISR_SOURCE);
 return intel_source_requester_deadline(r,at);
}
static bool guard(struct intel_source_requester *r) {
 if(!tick(r))return false;
 unsigned state=r->state;struct intel_source_requester_io io=r->io;bool ok=io.guard(io.ctx);
 if(!intel_source_requester_live(r) || r->state!=state)return fail(r,ISR_SOURCE);
 return ok?tick(r):fail(r,ISR_GUARD);
}
static bool profile(struct intel_source_requester *r,unsigned n) {
 const uint32_t *w=r->words[n];
 if(!intel_pci_requester_prefix(w,9) || (w[1]&65535)!=(n==1 || n==2?3u:7u))return false;
 if(n)for(unsigned i=0;i<8;i++)if((w[i]&(i==1?~4u:UINT32_MAX))!=(r->words[0][i]&(i==1?~4u:UINT32_MAX)))return false;
 return n<2 || !w[8];
}
static bool sample(struct intel_source_requester *r,unsigned n) {
 r->phase=n;if(!guard(r))return false;
 r->sample_begin[n]=r->last;unsigned state=r->state;
 bool ok=r->io.sample(r->io.ctx,r->words[n],&r->counts[n]);
 if(!intel_source_requester_live(r) || r->state!=state || r->phase!=n)return fail(r,ISR_SOURCE);
 if(r->counts[n]>9){r->counts[n]=0;return fail(r,ISR_READ);}
 r->reads+=r->counts[n];if(r->counts[n]==9)++r->samples;
 if(!guard(r))return false;
 r->sample_end[n]=r->last;
 if(!ok || r->counts[n]!=9)return fail(r,ISR_READ);
 return profile(r,n)?true:fail(r,ISR_PROFILE);
}
static bool command(struct intel_source_requester *r,uint16_t value) {
 if(!guard(r))return false;
 unsigned state=r->state;++r->attempts;
 enum intel_gt_write issued=r->io.command16(r->io.ctx,value);
 if(!intel_source_requester_live(r) || r->state!=state)return fail(r,ISR_SOURCE);
 if(issued==GT_WRITE_DONE)++r->writes;
 if(issued!=GT_WRITE_DONE)return fail(r,ISR_WRITE);
 return tick(r);
}
static bool drain(struct intel_source_requester *r) {
 uint32_t pending=r->words[1][8];r->drain_begin=r->last;
 while(pending) {
  if(r->polls==INTEL_SOURCE_REQUESTER_POLLS)return fail(r,ISR_PENDING);
  if(!guard(r))return false;
  if(r->last-r->drain_begin>INTEL_SOURCE_REQUESTER_DRAIN_US)return fail(r,ISR_PENDING);
  unsigned p=r->polls++;bool ok=r->io.pending(r->io.ctx,r->poll_words[p],&r->poll_counts[p]);
  if(!intel_source_requester_live(r) || r->state!=ISR_INHIBITED)return fail(r,ISR_SOURCE);
  if(r->poll_counts[p]>2){r->poll_counts[p]=0;return fail(r,ISR_READ);}
  r->reads+=r->poll_counts[p];if(!tick(r))return false;r->poll_at[p]=r->last;
  if(r->last-r->drain_begin>INTEL_SOURCE_REQUESTER_DRAIN_US)return fail(r,ISR_PENDING);
  if(!ok || r->poll_counts[p]!=2)return fail(r,ISR_READ);
  if(r->poll_words[p][0]!=r->words[1][1] || (r->poll_words[p][1]&~0x100u))return fail(r,ISR_PROFILE);
  pending=r->poll_words[p][1];if(pending){r->io.pause(r->io.ctx);if(!tick(r))return false;}
 }
 return true;
}
bool intel_source_requester_run(struct intel_source_requester *r,struct intel_source_transfer *t,
 struct intel_source_requester_io io,bool (*observe)(void *,struct intel_source_requester *),void *ctx) {
 if(!r || r->object || !t || !io.guard || !io.sample || !io.command16 || !io.pending || !io.now || !io.pause || !observe ||
    overlap(r,sizeof(*r),t,sizeof(*t)) || !intel_source_transfer_live(t) || t->busy ||
    t->state!=IST_REFUSED || !t->refused || t->opened || t->dependent || t->registered_child || t->child_borrows || t->child_returns)return false;
 if(overlap(r,sizeof(*r),t->buffers,sizeof(*t->buffers)) || overlap(r,sizeof(*r),t->source,sizeof(*t->source)) ||
    overlap(r,sizeof(*r),t->source->reset,sizeof(*t->source->reset)) || overlap(r,sizeof(*r),t->source->parent,sizeof(*t->source->parent)) ||
    overlap(r,sizeof(*r),t->input,sizeof(*t->input)) || overlap(r,sizeof(*r),t->source->entry,sizeof(*t->source->entry)))return false;
 for(unsigned i=0;i<2;i++)if(overlap(r,sizeof(*r),t->hold[i],sizeof(*t->hold[i])) ||
    overlap(r,sizeof(*r),t->memory[i].object,sizeof(*t->memory[i].object)))return false;
 *r=(struct intel_source_requester){.object=r,.transfer=t,.buffers=t->buffers,.source=t->source,.provider=t->provider,
  .lifetime=t->lifetime,.input=t->input,.io=io,.original_io=io,.origin=t->origin,.parent_begin=t->parent_begin,
  .begin=t->last,.last=t->last,.epoch=t->epoch,.kind=t->kind,.post_mode=t->post_mode,.state=ISR_OPENING,.busy=true,.registered=true,.borrows=1};
 t->dependent=t->registered_child=r;++t->child_borrows;
 r->parent_blocked=!intel_source_transfer_check(t) && !intel_source_transfer_close(t);
 r->holds_blocked=!scanout_memory_hold_end(t->hold[0]) && !scanout_memory_hold_end(t->hold[1]);
 if(!r->parent_blocked || !r->holds_blocked){fail(r,ISR_SOURCE);goto done;}
 if(!sample(r,0))goto done;
 r->state=ISR_INHIBITING;
 if(!command(r,3) || !sample(r,1))goto done;
 r->state=ISR_INHIBITED;
 if(!drain(r) || !guard(r))goto done;
 r->child_begin=r->last;r->child_entered=true;
 if(!observe(ctx,r)){fail(r,ISR_CHILD);goto done;}
 if(!intel_source_requester_live(r) || r->state!=ISR_INHIBITED){fail(r,ISR_SOURCE);goto done;}
#ifdef SRVROS_INTEL_SOURCE_ENGINES
 if(!intel_source_requester_child_returned(r)){fail(r,ISR_CHILD);goto done;}
#endif
 if(!guard(r))goto done;
 r->child_end=r->last;r->child_complete=true;
 /* Fresh inhibited profile after the child, before restoring the requester. */
 if(!sample(r,2))goto done;
 r->state=ISR_RESTORING;
 if(!command(r,7) || !sample(r,3) || !guard(r))goto done;
 if(r->last<=r->begin){r->clock_at=r->last;r->clock_prev=r->begin;fail(r,ISR_CLOCK);goto done;}
 r->restored=true;t->dependent=t->registered_child=0;++t->child_returns;++r->returns;
 r->end=r->last;r->registered=false;r->state=ISR_CLOSED;r->complete=true;
done:r->busy=false;return r->complete;
}
