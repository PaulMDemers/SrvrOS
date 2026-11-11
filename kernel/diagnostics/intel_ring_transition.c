#include <srvros/intel_ring_transition.h>
struct ring_call {struct intel_ring_transition *r;struct intel_ring_io io;};
static bool census_valid(const struct intel_completion_result *c,const struct intel_gt_access *g,uint32_t epoch) {
 return c && c->object==c && c->parent==g && c->engine_epoch==epoch && c->complete && c->stable && !c->reason &&
  c->batches==2 && c->groups==10 && c->samples==2 && c->contexts==2 && c->reads==92 &&
  !c->pending && !c->flip_wait && !c->tlb_pending && c->parser_idle==3 &&
  c->begin_us && c->begin_us<=c->last_us && c->last_us<=c->end_us && c->end_us>c->begin_us && c->end_us-c->begin_us<=100000;
}
static bool source_valid(const struct intel_ring_transition *r) {
 const struct intel_gt_access *g=r->parent;const struct intel_completion_result *c=r->source;
 if(r->object!=r || !g || g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned ||
    r->epoch_before==UINT32_MAX || r->epoch_after!=r->epoch_before+(unsigned)r->epoch_advanced ||
    r->store_calls!=(unsigned)r->epoch_advanced || (r->kind==IRT_KIND_DISABLED && r->epoch_advanced) ||
    g->engine_epoch!=r->epoch_after || g->write_attempts!=r->parent_attempts ||
    !display_access_held(&g->exclusion) || !census_valid(c,g,r->epoch_before) || c->enabled!=(r->kind==IRT_KIND_ENABLED?16u:0u) ||
    (r->kind!=IRT_KIND_ENABLED && r->kind!=IRT_KIND_DISABLED) ||
    c->begin_us!=r->source_begin || c->end_us!=r->source_end || c->last_us!=r->source_last || c->begin_us<=g->begin || c->end_us>g->last)return false;
 for(unsigned n=0;n<2;n++) {
  if(r->source_words[n][4][2]!=(r->kind==IRT_KIND_ENABLED?0x3eef000u:0u) ||
     r->source_words[n][4][3]!=(r->kind==IRT_KIND_ENABLED?0xf001u:0u))return false;
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++)for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)if(c->words[n][e][i]!=r->source_words[n][e][i])return false;
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(c->context[n][i]!=r->source_context[n][i])return false;
 }
 return true;
}
static bool post_valid(const struct intel_ring_transition *);
static bool live(const struct intel_ring_transition *r) {
 const struct intel_gt_access *g=r->parent;
 return source_valid(r) && (!r->post_bound || post_valid(r)) && r->borrow.object==&r->borrow && r->borrow.parent==g && r->borrow.active &&
  g->borrows==1 && g->dependents==&r->borrow && !r->borrow.next;
}
static bool clock_ok(struct ring_call *p,bool admission) {
 struct intel_ring_transition *r=p->r;uint64_t at=p->io.now(p->io.ctx);
 if(!at || at<r->last || at<r->begin || at-r->begin>INTEL_RING_TOTAL_US ||
    (admission && (at<=r->source_end || at-r->source_end>INTEL_RING_SOURCE_US))) {
  if(!(r->reason&IRT_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}
  r->reason|=IRT_CLOCK;return false;
 }
 r->last=at;return true;
}
static bool guard(void *ctx) {
 struct ring_call *p=ctx;struct intel_ring_transition *r=p->r;
 if(!clock_ok(p,!r->epoch_advanced && !r->bcs.disabled))return false;
 if(!live(r)){r->reason|=IRT_SOURCE;return false;}
 bool ok=p->io.guard(p->io.ctx);
 if(!live(r) || !ok){r->reason|=IRT_SOURCE;return false;}
 ++r->checks;return true;
}
static bool read_bcs(void *ctx,uint32_t out[INTEL_BCS_WORDS]) {
 struct ring_call *p=ctx;struct intel_ring_transition *r=p->r;
 if(!guard(p))return false;
 ++r->read_attempts;
 if(!p->io.read(p->io.ctx,out)){r->reason|=IRT_READ;return false;}
 if(!guard(p))return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned i=0;i<6;i++) {
   uint32_t expected=r->source_words[n][4][i];if(i==3 && r->epoch_advanced)expected&=~1u;
   if(out[i]!=expected){r->reason|=IRT_READ;return false;}
  }
  if(out[6]!=r->source_context[n][3] || out[7]!=r->source_context[n][5]){r->reason|=IRT_READ;return false;}
 }
 return true;
}
bool intel_ring_transition_store_ready(const struct intel_ring_transition *r) {
 if(!r || r->kind!=IRT_KIND_ENABLED || !live(r) || !r->policy || !r->blocked || !r->epoch_advanced || r->epoch_before==UINT32_MAX ||
    r->epoch_after!=r->epoch_before+1 || r->store_calls!=1 || r->post_bound || r->post.object ||
    r->reader_attempts!=1 || r->reader_counts[0]!=INTEL_RING_READER_WORDS || r->reason ||
    r->bcs.object!=&r->bcs || !r->bcs.admitted || r->bcs.attempts!=1 || r->bcs.writes || r->bcs.samples!=2)return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned i=0;i<6;i++)if(r->bcs.words[n][i]!=r->source_words[n][4][i])return false;
  if(r->bcs.words[n][6]!=r->source_context[n][3] || r->bcs.words[n][7]!=r->source_context[n][5])return false;
 }
 return true;
}
static enum intel_gt_write write_bcs(void *ctx,uint32_t value) {
 struct ring_call *p=ctx;struct intel_ring_transition *r=p->r;
 if(r->kind!=IRT_KIND_ENABLED || value!=INTEL_BCS_DISABLED || r->epoch_advanced || r->store_calls || r->epoch_before==UINT32_MAX ||
    r->bcs.object!=&r->bcs || !r->bcs.admitted || r->bcs.samples!=2 || r->bcs.attempts!=1 || r->bcs.writes ||
    !guard(p))return GT_WRITE_NONE;
 /* The original source remains immutable history after this point. */
 r->epoch_advanced=true;r->epoch_after=++r->parent->engine_epoch;
 r->store_calls=1;
 enum intel_gt_write result=p->io.write(p->io.ctx,value);
 if(result!=GT_WRITE_NONE)r->uncertain=true;
 return result;
}
static uint64_t bcs_now(void *ctx) {
 struct ring_call *p=ctx;struct intel_ring_transition *r=p->r;uint64_t at=p->io.now(p->io.ctx);
 if(!at || at<r->last || at<r->begin || at-r->begin>INTEL_RING_TOTAL_US ||
    (r->bcs.begin_us && (at<r->bcs.last_us || at<r->bcs.begin_us || at-r->bcs.begin_us>100000)) ||
    (!r->epoch_advanced && !r->bcs.disabled && (at<=r->source_end || at-r->source_end>INTEL_RING_SOURCE_US))) {
  if(!(r->reason&IRT_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}
  r->reason|=IRT_CLOCK;
 } else r->last=at;
 return at;
}
static bool reader(struct ring_call *p,unsigned n) {
 struct intel_ring_transition *r=p->r;
 if(n>1 || n!=r->reader_attempts || !guard(p))return false;
 ++r->reader_attempts;
 bool ok=p->io.reader(p->io.ctx,n,&r->readers[n],&r->reader_counts[n]);
 if(r->reader_counts[n]>INTEL_RING_READER_WORDS){r->reader_counts[n]=0;ok=false;}
 if(!ok || r->reader_counts[n]!=INTEL_RING_READER_WORDS){r->reason|=IRT_READER;return false;}
 if(!guard(p))return false;
 const uint32_t *a=r->readers[n].audio;
 if(a[0]!=0x160c8086 || a[1]!=0x040300 || a[2]!=2 || a[3]!=1 || a[4] || a[5]!=1){r->reason|=IRT_READER;return false;}
 for(unsigned e=0;e<3;e++)for(unsigned i=0;i<9;i++)if(r->readers[n].planes[e][i]==UINT32_MAX){r->reason|=IRT_READER;return false;}
 if(n) {
  for(unsigned e=0;e<3;e++)for(unsigned i=0;i<9;i++)if(r->readers[0].planes[e][i]!=r->readers[1].planes[e][i]){r->reason|=IRT_READER;return false;}
  for(unsigned i=0;i<10;i++)if(r->readers[0].audio[i]!=a[i]){r->reason|=IRT_READER;return false;}
 }
 return true;
}
static bool post_valid(const struct intel_ring_transition *r) {
 const struct intel_completion_result *c=&r->post;
 if(!census_valid(c,r->parent,r->epoch_after) || c->enabled || c->begin_us<=r->bcs.end_us)return false;
 if(r->post_bound && (c->begin_us!=r->post_begin || c->end_us!=r->post_end || c->last_us!=r->post_last))return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++)for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++) {
   uint32_t expected=r->source_words[n][e][i];if(e==4 && i==3)expected&=~1u;
   if(c->words[n][e][i]!=expected)return false;
  }
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(c->context[n][i]!=r->source_context[n][i])return false;
  for(unsigned i=0;i<6;i++)if(c->words[n][4][i]!=r->bcs.words[n+2][i])return false;
 }
 return true;
}
static bool run(struct intel_ring_transition *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_ring_io io,bool allow_disabled) {
 if(!r || r->object)return false;
 *r=(struct intel_ring_transition){.object=r,.parent=g,.source=c,.policy=policy};
 if(!g || !policy || !io.guard || !io.read || !io.write || !io.census || !io.reader || !io.now){r->reason=IRT_INPUT;return false;}
 r->epoch_before=r->epoch_after=g->engine_epoch;r->parent_attempts=g->write_attempts;
 if(g->engine_epoch==UINT32_MAX || !census_valid(c,g,r->epoch_before)){r->reason=IRT_INPUT;return false;}
 if(c->enabled==16)r->kind=IRT_KIND_ENABLED;
 else if(allow_disabled && !c->enabled)r->kind=IRT_KIND_DISABLED;
 else {r->reason=IRT_INPUT;return false;}
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++)for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++) {
   uint32_t expected=i==4?1:0;if(r->kind==IRT_KIND_ENABLED && e==4 && i==2)expected=0x3eef000;if(r->kind==IRT_KIND_ENABLED && e==4 && i==3)expected=0xf001;
   if(c->words[n][e][i]!=expected){r->reason=IRT_INPUT;return false;}
   r->source_words[n][e][i]=c->words[n][e][i];
  }
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)r->source_context[n][i]=c->context[n][i];
  if(c->context[n][3]!=512 || c->context[n][5]){r->reason=IRT_INPUT;return false;}
 }
 r->source_begin=c->begin_us;r->source_end=c->end_us;r->source_last=c->last_us;
 if(g->borrows || g->dependents || !intel_gt_access_check(g) || !source_valid(r)){r->reason=IRT_SOURCE;return false;}
 r->begin=r->last=io.now(io.ctx);struct ring_call call={r,io};
 if(!r->begin || !clock_ok(&call,true)){r->reason|=IRT_CLOCK;return false;}
 if(!intel_gt_access_borrow(g,&r->borrow)){r->reason|=IRT_SOURCE;return false;}
 r->blocked=!intel_gt_access_close(g) && g->state==GT_ACCESS_AWAKE && g->pinned && g->write_attempts==r->parent_attempts;
 if(!r->blocked){r->reason|=IRT_SOURCE;goto done;}
 if(!reader(&call,0))goto done;
 if(r->kind==IRT_KIND_ENABLED)
  (void)intel_blitter_disable(&r->bcs,g,policy,(struct intel_blitter_io){&call,guard,read_bcs,write_bcs,bcs_now});
 else
  (void)intel_blitter_observe_disabled(&r->bcs,g,policy,(struct intel_blitter_io){&call,guard,read_bcs,0,bcs_now});
 if(!r->bcs.disabled || r->bcs.uncertain || r->reason){r->reason|=IRT_WRITE;goto done;}
 if(!guard(&call))goto done;
 if(!io.census(io.ctx,&r->post)){r->reason|=IRT_POST;goto done;}
 if(!guard(&call) || !post_valid(r)){r->reason|=IRT_POST;goto done;}
 r->post_begin=r->post.begin_us;r->post_end=r->post.end_us;r->post_last=r->post.last_us;r->post_bound=true;
 if(!reader(&call,1))goto done;
 r->readers_bound=true;
 done:;
 /* A possible store remains quarantined until all postconditions and clocks.
  * A source/provider loss cannot be cleaned up using only superficially equal data. */
 bool proven=!r->reason && r->post_bound && r->readers_bound && r->bcs.disabled && !r->bcs.uncertain;
 bool possible=r->uncertain || r->bcs.uncertain || r->bcs.writes;
 if(((!possible && !r->bcs.disabled) || proven) && live(r) && io.guard(io.ctx) && live(r) && clock_ok(&call,false)) {
  r->return_begin=r->last;r->returned=intel_gt_access_return(&r->borrow);
 }
 if(!r->returned)r->reason|=IRT_RETURN;
 if(r->returned) {
  if(!source_valid(r) || (r->post_bound && !post_valid(r)) || !io.guard(io.ctx) ||
     !source_valid(r) || (r->post_bound && !post_valid(r)) || g->borrows || g->dependents)r->reason|=IRT_SOURCE;
  else if(clock_ok(&call,false))r->end=r->last;
  if(r->end && r->end<=r->begin)r->reason|=IRT_CLOCK;
 }
 bool branch=r->kind==IRT_KIND_ENABLED ?
  r->bcs.attempts==1 && r->bcs.writes==1 && r->store_calls==1 && r->epoch_advanced && r->epoch_after==r->epoch_before+1 && !r->bcs.entry_disabled :
  r->kind==IRT_KIND_DISABLED && !r->bcs.attempts && !r->bcs.writes && !r->store_calls && !r->epoch_advanced && r->epoch_after==r->epoch_before && r->bcs.entry_disabled;
 r->complete=proven && r->returned && r->end && !r->reason && branch && g->engine_epoch==r->epoch_after;
 r->uncertain=((possible || r->bcs.disabled) && !r->complete) || r->borrow.active || (r->reason&IRT_SOURCE);
 return r->complete;
}

bool intel_ring_transition_run(struct intel_ring_transition *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_ring_io io) {return run(r,g,c,policy,io,false);}
bool intel_ring_entry_run(struct intel_ring_transition *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_ring_io io) {return run(r,g,c,policy,io,true);}
