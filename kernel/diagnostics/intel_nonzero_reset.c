#include <srvros/intel_nonzero_reset.h>
struct nz_call {struct intel_nonzero_reset *r;struct intel_nonzero_io io;
 const struct intel_ring_transition *entry;struct intel_gt_access *parent;};
static bool fail(struct intel_nonzero_reset *r,unsigned why){r->reason|=why;return false;}
/* Historical entry/census identity remains immutable after the reset epoch.
 * This is continuity inside the new registered child, not an old ready token. */
static bool source(const struct intel_nonzero_reset *r) {
 if(!r)return false;
 const struct intel_ring_transition *e=r->entry;const struct intel_gt_access *g=r->parent;
 if(!r || r->object!=r || !e || e->object!=e || !g || g->object!=g || e->parent!=g ||
    e->kind!=IRT_KIND_ENABLED || !e->complete || !e->policy || !e->blocked || !e->returned ||
    !e->post_bound || !e->readers_bound || e->reason || e->uncertain ||
    e->epoch_before || e->epoch_after!=1 || !e->epoch_advanced || e->store_calls!=1 ||
    e->checks!=20 || e->read_attempts!=4 || e->reader_attempts!=2 ||
    e->reader_counts[0]!=37 || e->reader_counts[1]!=37 ||
    e->borrow.object!=&e->borrow || e->borrow.parent!=g || e->borrow.active || e->borrow.next ||
    !e->source || e->source->object!=e->source || e->source->parent!=g ||
    e->source->engine_epoch || !e->source->complete || !e->source->stable || e->source->reason ||
    e->source->samples!=2 || e->source->contexts!=2 || e->source->groups!=10 || e->source->reads!=92 ||
    e->source->begin_us!=e->source_begin || e->source->end_us!=e->source_end || e->source->last_us!=e->source_last ||
    e->source->batches!=2 || e->source->enabled!=16 || e->source->pending || e->source->flip_wait ||
    e->source->tlb_pending || e->source->parser_idle!=3 ||
    r->source!=&e->post || e->post.object!=&e->post || e->post.parent!=g || e->post.engine_epoch!=1 ||
    !e->post.complete || !e->post.stable || e->post.reason || e->post.enabled || e->post.pending ||
    e->post.flip_wait || e->post.tlb_pending || e->post.parser_idle!=3 ||
    e->post.samples!=2 || e->post.groups!=10 || e->post.contexts!=2 || e->post.reads!=92 || e->post.batches!=2 ||
    e->end!=r->entry_end || e->post.begin_us!=r->source_begin || e->post.end_us!=r->source_end ||
    e->post.last_us!=r->source_last || e->post_begin!=r->source_begin || e->post_end!=r->source_end ||
    e->post_last!=r->source_last || r->epoch_before!=1 || r->epoch_after!=1+(unsigned)r->epoch_advanced ||
    g->begin!=r->parent_begin || !r->parent_begin || g->engine_epoch!=r->epoch_after || g->write_attempts!=r->parent_attempts ||
    e->parent_attempts!=r->parent_attempts || g->state!=GT_ACCESS_AWAKE || !g->pinned ||
    !display_access_held(&g->exclusion))return false;
 const struct intel_blitter_result *b=&e->bcs;
 if(b->object!=b || !b->admitted || !b->disabled || b->entry_disabled || b->attempts!=1 ||
    b->writes!=1 || b->uncertain || b->reason || b->samples!=4 || b->reads!=32 ||
    !e->begin || e->begin<=e->source_end || !b->begin_us || b->end_us<=b->begin_us ||
    b->end_us-b->begin_us>100000 || b->end_us>e->source_end+100000 ||
    e->post_begin<=b->end_us || e->return_begin<e->post_end || e->end<e->return_begin ||
    e->end!=e->last || e->end<=e->begin || e->end-e->begin>150000 ||
    !r->source_begin || r->source_begin>r->source_last || r->source_last>r->source_end ||
    r->source_end<=r->source_begin || r->source_end-r->source_begin>100000)return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned k=0;k<6;k++)if(e->source_context[n][k]!=e->source->context[n][k] ||
      e->post.context[n][k]!=e->source_context[n][k])return false;
  if(!(e->source_context[n][0]&512) || !(e->source_context[n][3]&512))return false;
  for(unsigned eng=0;eng<5;eng++)for(unsigned i=0;i<8;i++) {
   uint32_t expected=i==4?1u:eng==4 && i==2?0x3eef000u:eng==4 && i==3?0xf001u:0u;
   if(e->source_words[n][eng][i]!=expected || e->source->words[n][eng][i]!=expected ||
      e->post.words[n][eng][i]!=(eng==4 && i==3?0xf000u:expected))return false;
  }
 }
 for(unsigned n=0;n<4;n++) {
  const struct intel_completion_result *c=n<2?e->source:&e->post;unsigned s=n%2;
  for(unsigned i=0;i<8;i++)if(b->words[n][i]!=(i<6?c->words[s][4][i]:c->context[s][i==6?3:5]))return false;
 }
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)if(e->readers[0].planes[p][i]!=e->readers[1].planes[p][i] || e->readers[0].planes[p][i]==UINT32_MAX)return false;
 for(unsigned i=0;i<10;i++)if(e->readers[0].audio[i]!=e->readers[1].audio[i])return false;
 const uint32_t *a=e->readers[1].audio;
 return a[0]==0x160c8086 && a[1]==0x040300 && a[2]==2 && a[3]==1 && !a[4] && a[5]==1;
}
bool intel_nonzero_reset_live(const struct intel_nonzero_reset *r) {
 return r && source(r) && r->policy && r->stage>=INZ_ADMIT && r->stage<INZ_DONE && r->eligible && !r->complete && !r->returned && !r->reason &&
  r->borrow.object==&r->borrow && r->borrow.parent==r->parent && r->borrow.active &&
  !r->borrow.next && r->parent->borrows==1 && r->parent->dependents==&r->borrow;
}
static bool identity(const struct nz_call *p) {
 return p->r->object==p->r && p->r->provider==p->io.ctx &&
  p->r->entry==p->entry && p->r->parent==p->parent;
}
static bool clock_at(struct nz_call *p,uint64_t at) {
 struct intel_nonzero_reset *r=p->r;
 if(!identity(p))return fail(r,INZ_SOURCE);
 const struct intel_gt_access *g=p->parent;
 uint64_t origin=r->begin?r->begin:r->admit_begin;
 if(!at || !origin || at<r->last || at<origin || at-origin>100000 ||
    at<g->last || at<g->begin || at-g->begin>GT_ACCESS_TOTAL_US ||
    (!r->begin && (at<=r->source_end || at-r->source_end>100000))) {
  if(!(r->reason&INZ_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}
  return fail(r,INZ_CLOCK);
 }
 r->last=at;return true;
}
static bool guard(struct nz_call *p) {
 struct intel_nonzero_reset *r=p->r;
 if(!identity(p) || !intel_nonzero_reset_live(r))return fail(r,INZ_SOURCE);
 if(!clock_at(p,p->io.now(p->io.ctx)))return false;
 if(!identity(p) || !intel_nonzero_reset_live(r))return fail(r,INZ_SOURCE);
 bool ok=p->io.guard(p->io.ctx);
 if(!identity(p) || !intel_nonzero_reset_live(r))return fail(r,INZ_SOURCE);
 if(!clock_at(p,p->io.now(p->io.ctx)))return false;
 if(!identity(p) || !intel_nonzero_reset_live(r))return fail(r,INZ_SOURCE);
 if(!ok)return fail(r,INZ_GUARD);
 ++r->checks;return true;
}
static bool valid_ctl(uint32_t v){return v!=UINT32_MAX && !(v&0xfffcu);}
static bool sample(struct nz_call *p,uint32_t out[6],unsigned *count) {
 if(!guard(p))return false;
 bool ok=p->io.sample(p->io.ctx,out,count);
 if(*count>6){*count=0;fail(p->r,INZ_READ);}
 p->r->control_reads+=*count;
 for(unsigned i=0;i<*count;i++)if(i<5?!valid_ctl(out[i]):out[i]!=0)fail(p->r,INZ_STATE);
 bool g=guard(p);if(!ok || *count!=6)fail(p->r,INZ_READ);
 return g && !p->r->reason;
}
static bool ctl_snapshot(struct nz_call *p,unsigned n) {
 bool ok=sample(p,p->r->control[n],&p->r->control_count[n]);
 if(p->r->control_count[n]==6)++p->r->control_samples;
 return ok;
}
bool intel_nonzero_reset_write_ready(const struct intel_nonzero_reset *r,bool reset,unsigned index,uint32_t value) {
 if(!intel_nonzero_reset_live(r) || !r->begin || !r->blocked || !r->admit_end)return false;
 if(reset)return r->stage==INZ_RESET && index<2 && r->index==index &&
  value==INTEL_BCS_RESET_DOMAIN && r->epoch_advanced && r->resets==r->reset_writes+1 &&
  r->reset_writes==index && r->reset_acks==index && r->controls==r->control_writes &&
  r->control_writes==5 && r->control_acks==5 && r->bcs_samples==1;
 unsigned n=r->controls-1;
 return r->controls && n<10 && r->control_writes==n && r->control_acks==n && r->index==index &&
  ((n<5 && r->stage==INZ_PREPARE && index==n && value==INTEL_ENGINE_PREPARE_REQUEST && !r->epoch_advanced) ||
   (n>=5 && r->stage==INZ_CANCEL && index==9-n && value==INTEL_ENGINE_PREPARE_CANCEL &&
    r->epoch_advanced && r->reset_writes==2 && r->reset_acks==2 && r->post_mode));
}
static bool request(struct nz_call *p,unsigned e,bool enable) {
 struct intel_nonzero_reset *r=p->r;unsigned n=r->controls;r->index=e;
 if(!guard(p))return false;
 r->control_begin[n]=r->last;++r->controls;
 uint32_t v=UINT32_MAX;enum intel_gt_write wrote=p->io.request(p->io.ctx,e,enable?INTEL_ENGINE_PREPARE_REQUEST:INTEL_ENGINE_PREPARE_CANCEL,&v);
 if(wrote==GT_WRITE_DONE){++r->control_writes;++r->control_reads;r->ack[n]=r->final[n]=v;if(!valid_ctl(v))fail(r,INZ_STATE);}
 bool g=guard(p);if(wrote!=GT_WRITE_DONE)fail(r,INZ_WRITE);
 if(!g || r->reason)return false;
 for(unsigned i=0;i<256;i++) {
  if(r->last-r->control_begin[n]>10000)return fail(r,INZ_TIMEOUT);
  if((v&3u)==(enable?3u:0u)){++r->control_acks;r->control_end[n]=r->last;return true;}
  if(i==255)return fail(r,INZ_TIMEOUT);
  p->io.pause(p->io.ctx);if(!guard(p))return false;
  bool ok=p->io.control(p->io.ctx,e,&v);if(ok){++r->control_reads;++r->polls[n];r->final[n]=v;if(!valid_ctl(v))fail(r,INZ_STATE);}
  g=guard(p);if(!ok)fail(r,INZ_READ);if(!g || r->reason)return false;
 }
 return fail(r,INZ_TIMEOUT);
}
static bool bcs_snapshot(struct nz_call *p,unsigned n) {
 struct intel_nonzero_reset *r=p->r;if(!guard(p))return false;
 bool ok=p->io.bcs(p->io.ctx,r->bcs[n],&r->domain[n?3:0],&r->bcs_count[n]);
 if(r->bcs_count[n]>9){r->bcs_count[n]=0;fail(r,INZ_READ);}
 r->bcs_reads+=r->bcs_count[n];if(r->bcs_count[n]==9)++r->bcs_samples;
 for(unsigned i=0;i<r->bcs_count[n] && i<8;i++)if(r->bcs[n][i]==UINT32_MAX)fail(r,INZ_STATE);
 if(r->bcs_count[n]==9 && r->domain[n?3:0])fail(r,INZ_STATE);
 bool g=guard(p);if(!ok || r->bcs_count[n]!=9)fail(r,INZ_READ);if(!g || r->reason)return false;
 uint32_t expected[8];for(unsigned i=0;i<8;i++)expected[i]=i<6?r->source->words[1][4][i]:r->source->context[1][i==6?3:5];
 bool same=true,defaults=true;
 for(unsigned i=0;i<8;i++){if(r->bcs[n][i]!=expected[i])same=false;if(r->bcs[n][i]!=(i==2 || i==3?0:expected[i]))defaults=false;}
 if(!n)return same?true:fail(r,INZ_STATE);
 r->post_mode=same?INZ_POST_PRESERVED:defaults?INZ_POST_RESET_DEFAULT:INZ_POST_NONE;
 return r->post_mode?true:fail(r,INZ_STATE);
}
static bool pulse(struct nz_call *p,unsigned n) {
 struct intel_nonzero_reset *r=p->r;r->index=n;if(!guard(p))return false;r->pulse_begin[n]=r->last;
 if(!n){r->epoch_advanced=true;r->epoch_after=r->epoch_before+1;r->parent->engine_epoch=r->epoch_after;}
 ++r->resets;uint32_t v=UINT32_MAX;
 enum intel_gt_write wrote=p->io.reset(p->io.ctx,INTEL_BCS_RESET_DOMAIN,&v);
 if(wrote==GT_WRITE_DONE){++r->reset_writes;++r->bcs_reads;r->reset_ack[n]=r->reset_final[n]=v;if(v&~8u)fail(r,INZ_STATE);}
 bool g=guard(p);if(wrote!=GT_WRITE_DONE)fail(r,INZ_WRITE);if(!g || r->reason)return false;
 for(unsigned i=0;i<256;i++) {
  if(r->last-r->pulse_begin[n]>10000)return fail(r,INZ_TIMEOUT);
  if(!v){++r->reset_acks;r->domain[n+1]=v;r->pulse_end[n]=r->last;return true;}
  if(i==255)return fail(r,INZ_TIMEOUT);
  p->io.pause(p->io.ctx);if(!guard(p))return false;
  bool ok=p->io.domain(p->io.ctx,&v);if(ok){++r->bcs_reads;++r->reset_polls[n];r->reset_final[n]=v;if(v&~8u)fail(r,INZ_STATE);}
  g=guard(p);if(!ok)fail(r,INZ_READ);if(!g || r->reason)return false;
 }
 return fail(r,INZ_TIMEOUT);
}
static bool preserve(struct nz_call *p) {
 struct intel_nonzero_reset *r=p->r;if(!guard(p))return false;
 bool ok=p->io.post(p->io.ctx,&r->post);bool g=guard(p);
 if(!ok || !g || r->reason)return fail(r,INZ_PRESERVE);
 const struct intel_completion_result *c=&r->post;
 if(c->object!=c || c->parent!=r->parent || c->engine_epoch!=r->epoch_after || !c->complete || !c->stable || c->reason ||
    c->enabled || c->pending || c->flip_wait || c->tlb_pending || c->parser_idle!=3 || c->groups!=10 || c->samples!=2 ||
    c->contexts!=2 || c->batches!=2 || c->reads!=92 || c->begin_us<r->begin || c->end_us>r->last ||
    c->end_us<=c->begin_us || c->last_us<c->begin_us || c->last_us>c->end_us)return fail(r,INZ_PRESERVE);
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<5;e++)for(unsigned i=0;i<8;i++) {
   uint32_t expected=r->source->words[n][e][i];
   if(r->post_mode==INZ_POST_RESET_DEFAULT && e==4 && (i==2 || i==3))expected=0;
   if(c->words[n][e][i]!=expected)return fail(r,INZ_PRESERVE);
  }
  for(unsigned i=0;i<6;i++)if(c->context[n][i]!=r->source->context[n][i])return fail(r,INZ_PRESERVE);
 }
 if(!guard(p))return false;
 ok=p->io.readers(p->io.ctx,&r->readers,&r->reader_count);
 if(r->reader_count>37){r->reader_count=0;fail(r,INZ_READ);}
 g=guard(p);if(!ok || !g || r->reader_count!=37 || r->reason)return fail(r,INZ_PRESERVE);
 for(unsigned i=0;i<3;i++)for(unsigned j=0;j<9;j++)if(r->readers.planes[i][j]!=r->entry->readers[1].planes[i][j])return fail(r,INZ_PRESERVE);
 for(unsigned i=0;i<10;i++)if(r->readers.audio[i]!=r->entry->readers[1].audio[i])return fail(r,INZ_PRESERVE);
 r->guard_begin=r->last;if(!guard(p))return false;r->guard_end=r->last;r->preserved=true;return true;
}
static bool run(struct intel_nonzero_reset *r,struct intel_gt_access *g,
 const struct intel_ring_transition *e,bool policy,struct intel_nonzero_io io,
 bool (*observe)(void *,struct intel_nonzero_reset *),void *ctx) {
 if(!r || r->object)return false;
 *r=(struct intel_nonzero_reset){.object=r,.parent=g,.entry=e,.provider=io.ctx,.policy=policy};
 if(!g || !e || !policy || !io.guard || !io.sample || !io.control || !io.request || !io.bcs ||
    !io.domain || !io.reset || !io.post || !io.readers || !io.now || !io.pause ||
    e->kind!=IRT_KIND_ENABLED || !e->complete){r->reason=INZ_INPUT;return false;}
 r->eligible=true;r->uncertain=true;r->source=&e->post;r->source_begin=e->post.begin_us;
 r->source_end=e->post.end_us;r->source_last=e->post.last_us;r->entry_end=e->end;
 r->epoch_before=r->epoch_after=e->epoch_after;r->parent_attempts=g->write_attempts;r->parent_begin=g->begin;
 struct nz_call p={r,io,e,g};
 if(!source(r) || g->borrows || g->dependents){fail(r,INZ_SOURCE);goto done;}
 r->admit_begin=io.now(io.ctx);r->last=r->entry_end;
 if(!clock_at(&p,r->admit_begin))goto done;
 if(!source(r) || !intel_gt_access_borrow(g,&r->borrow)){fail(r,INZ_SOURCE);goto done;}
 unsigned attempts=g->write_attempts;r->blocked=!intel_gt_access_close(g) && attempts==g->write_attempts;
 r->stage=INZ_ADMIT;if(!r->blocked || !guard(&p)){fail(r,INZ_SOURCE);goto done;}
 for(unsigned n=0;n<2;n++) {
  ++r->admission_attempts;if(!sample(&p,r->admission[n],&r->admission_count[n]))goto done;
  for(unsigned i=0;i<6;i++)if(r->admission[n][i]&0xffffu){fail(r,INZ_STATE);goto done;}
 }
 for(unsigned i=0;i<6;i++)if(r->admission[0][i]!=r->admission[1][i]){fail(r,INZ_STATE);goto done;}
 if(!guard(&p))goto done;
 r->admit_end=r->last;
 /* Preparation starts once here, after complete admission. Its100ms includes
  * every readiness/reset/cancel/preservation operation and the final return. */
 uint64_t prepared_at=io.now(io.ctx);
 if(!clock_at(&p,prepared_at) || !identity(&p) || !source(r)){fail(r,INZ_SOURCE);goto done;}
 r->begin=prepared_at;r->stage=INZ_PREPARE;
 if(!ctl_snapshot(&p,0))goto done;
 for(unsigned i=0;i<6;i++)if(r->control[0][i]!=r->admission[1][i]){fail(r,INZ_STATE);goto done;}
 for(unsigned i=0;i<5;i++)if(!request(&p,i,true))goto done;
 r->stage=INZ_HELD;
 for(unsigned n=1;n<3;n++) {
  if(!ctl_snapshot(&p,n))goto done;
  for(unsigned i=0;i<5;i++)if((r->control[n][i]&0xffffu)!=3 || (n==2 && r->control[n][i]!=r->control[1][i])){fail(r,INZ_STATE);goto done;}
 }
 if(!bcs_snapshot(&p,0))goto done;
 r->stage=INZ_RESET;if(!pulse(&p,0) || !pulse(&p,1))goto done;
 r->stage=INZ_SETTLE;r->settle_begin=r->last;
 for(unsigned i=0;i<256;i++) {
  io.pause(io.ctx);if(!guard(&p))goto done;
  if(r->last-r->settle_begin>10000){fail(r,INZ_TIMEOUT);goto done;}
  if(r->last-r->settle_begin>=50){r->settle_end=r->last;break;}
 }
 if(!r->settle_end){fail(r,INZ_TIMEOUT);goto done;}
 r->stage=INZ_POST;if(!bcs_snapshot(&p,1) || !ctl_snapshot(&p,3))goto done;
 for(unsigned i=0;i<5;i++)if(i<4?r->control[3][i]!=r->control[2][i]:(r->control[3][i]&0xffffu)!=0){fail(r,INZ_STATE);goto done;}
 r->stage=INZ_CANCEL;for(unsigned i=5;i-->0;)if(!request(&p,i,false))goto done;
 if(!ctl_snapshot(&p,4))goto done;
 for(unsigned i=0;i<6;i++)if((r->control[4][i]&0xffffu)!=(r->control[0][i]&0xffffu)){fail(r,INZ_STATE);goto done;}
 r->stage=INZ_PRESERVE_STAGE;if(!preserve(&p))goto done;
 if(observe) {
  r->observer_entered=true;
  bool observed=observe(ctx,r);
  if(!guard(&p) || !observed || r->dependent || r->registered ||
     r->child_borrows!=1 || r->child_returns!=1){fail(r,INZ_PRESERVE);goto done;}
  r->observer_complete=true;
  r->guard_begin=r->last;if(!guard(&p))goto done;r->guard_end=r->last;
 }
 if((!observe && (r->child_borrows || r->child_returns || r->observer_entered || r->observer_complete)) || r->dependent || r->registered || r->child_borrows!=r->child_returns){fail(r,INZ_RETURN);goto done;}
 r->stage=INZ_RETURN_STAGE;r->return_begin=io.now(io.ctx);
 if(!clock_at(&p,r->return_begin) || !identity(&p) || !source(r)){fail(r,INZ_RETURN);goto done;}
 r->returned=intel_gt_access_return(&r->borrow);if(!r->returned){fail(r,INZ_RETURN);goto done;}
 r->end=io.now(io.ctx);
 if(!clock_at(&p,r->end) || !identity(&p) || !source(r) || g->borrows || g->dependents ||
    r->borrow.object!=&r->borrow || r->borrow.parent!=g || r->borrow.active || r->borrow.next){fail(r,INZ_RETURN);goto done;}
 r->stage=INZ_DONE;r->complete=true;r->uncertain=false;
 done:
 return r->complete;
}
bool intel_nonzero_reset_run(struct intel_nonzero_reset *r,struct intel_gt_access *g,
 const struct intel_ring_transition *e,bool policy,struct intel_nonzero_io io) {
 return run(r,g,e,policy,io,0,0);
}
bool intel_nonzero_reset_observe(struct intel_nonzero_reset *r,struct intel_gt_access *g,
 const struct intel_ring_transition *e,bool policy,struct intel_nonzero_io io,
 bool (*observe)(void *,struct intel_nonzero_reset *),void *ctx) {
 if(!observe)return false;
 return run(r,g,e,policy,io,observe,ctx);
}
