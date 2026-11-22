#include <srvros/intel_entry_admit.h>
struct entry_call {struct intel_entry_result *r;struct intel_entry_io io;};
static bool census_valid(const struct intel_entry_result *r) {
 const struct intel_completion_result *c=r->census;
 return c && c->object==c && c->parent==r->parent && c->engine_epoch==r->epoch &&
  c->complete && c->stable && !c->reason && !c->enabled && !c->pending &&
  !c->flip_wait && !c->tlb_pending && c->parser_idle==3 && c->batches==2 &&
  c->groups==10 && c->samples==2 && c->contexts==2 && c->reads==92 &&
  c->begin_us && c->end_us>c->begin_us && c->end_us-c->begin_us<=INTEL_ENTRY_TOTAL_US;
}
static bool source_valid(const struct intel_entry_result *r) {
 const struct intel_gt_access *g=r->parent;const struct intel_completion_result *c=r->census;
 if(r->object!=r || !g || g->object!=g || g->state!=GT_ACCESS_AWAKE || !g->pinned ||
    g->engine_epoch!=r->epoch || g->write_attempts!=r->parent_attempts_before ||
    !display_access_held(&g->exclusion) || !census_valid(r) ||
    c->begin_us!=r->source_begin || c->end_us!=r->source_end || c->last_us!=r->source_last ||
    c->begin_us<=g->begin || c->end_us>g->last)return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++)for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)
   if(c->words[n][e][i]!=r->source_words[n][e][i])return false;
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(c->context[n][i]!=r->source_context[n][i])return false;
 }
 return true;
}
static bool live(const struct intel_entry_result *r) {
 const struct intel_gt_access *g=r->parent;
 return source_valid(r) && r->borrow.object==&r->borrow && r->borrow.parent==g &&
  r->borrow.active && g->borrows==1 && g->dependents==&r->borrow && !r->borrow.next;
}
static bool clock_ok(struct entry_call *p) {
 struct intel_entry_result *r=p->r;uint64_t at=p->io.now(p->io.ctx);
 if(!at || at<r->last_us || at<r->begin_us || at-r->begin_us>INTEL_ENTRY_TOTAL_US ||
    at<=r->source_end || at-r->source_end>INTEL_ENTRY_TOTAL_US) {
  if(!(r->reason&IEA_CLOCK)){r->clock_at=at;r->clock_prev=r->last_us;}
  r->reason|=IEA_CLOCK;return false;
 }
 r->last_us=at;return true;
}
static bool guard(void *ctx) {
 struct entry_call *p=ctx;struct intel_entry_result *r=p->r;
 if(!clock_ok(p))return false;
 if(!live(r)){r->reason|=IEA_SOURCE;return false;}
 bool ok=p->io.guard(p->io.ctx);
 if(!live(r) || !ok){r->reason|=IEA_SOURCE;return false;}
 ++r->checks;return true;
}
static bool read_group(void *ctx,uint32_t out[INTEL_BCS_WORDS]) {
 struct entry_call *p=ctx;struct intel_entry_result *r=p->r;
 if(!guard(p))return false;
 if(!p->io.read(p->io.ctx,out)){r->reason|=IEA_READ;return false;}
 if(!guard(p))return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned i=0;i<INTEL_ENGINE_WORDS;i++)if(out[i]!=r->source_words[n][4][i]){r->reason|=IEA_ENTRY;return false;}
  if(out[6]!=r->source_context[n][3] || out[7]!=r->source_context[n][5]){r->reason|=IEA_ENTRY;return false;}
 }
 return true;
}
static uint64_t sample_time(void *ctx) {struct entry_call *p=ctx;return p->io.now(p->io.ctx);}
bool intel_entry_admit(struct intel_entry_result *r,struct intel_gt_access *g,
 const struct intel_completion_result *c,bool policy,struct intel_entry_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_entry_result){.object=r,.parent=g,.census=c,.policy=policy};
 if(!g || !policy || !io.guard || !io.read || !io.now){r->reason=IEA_INPUT;return false;}
 r->epoch=r->epoch_after=g->engine_epoch;r->parent_attempts_before=r->parent_attempts_after=g->write_attempts;
 if(!census_valid(r)){r->reason=IEA_INPUT;return false;}
 /* Preserve exact original zero geometry/idle gates; never clear a field. */
 for(unsigned n=0;n<2;n++) {
  const uint32_t *w=c->words[n][4];
  if(w[0] || w[1] || w[2] || w[3] || w[4]!=1 || w[5] || c->context[n][3]!=512 || c->context[n][5]){r->reason=IEA_ENTRY;return false;}
  for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++)for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)r->source_words[n][e][i]=c->words[n][e][i];
  for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)r->source_context[n][i]=c->context[n][i];
 }
 r->source_begin=c->begin_us;r->source_end=c->end_us;r->source_last=c->last_us;
 if(g->borrows || g->dependents || !intel_gt_access_check(g) || !source_valid(r)){r->reason=IEA_SOURCE;return false;}
 r->begin_us=r->last_us=io.now(io.ctx);
 struct entry_call call={r,io};
 if(!r->begin_us){r->reason=IEA_CLOCK;return false;}
 if(!clock_ok(&call))return false;
 if(!intel_gt_access_borrow(g,&r->borrow)){r->reason|=IEA_SOURCE;return false;}
 r->blocked=!intel_gt_access_close(g) && g->state==GT_ACCESS_AWAKE && g->pinned && g->write_attempts==r->parent_attempts_before;
 if(!r->blocked){r->reason|=IEA_SOURCE;goto done;}
 (void)intel_blitter_observe_disabled(&r->bcs,g,policy,(struct intel_blitter_io){&call,guard,read_group,0,sample_time});
 if(!r->bcs.disabled && !r->reason)r->reason|=IEA_ENTRY;
 r->bound=r->bcs.disabled && !r->reason && guard(&call);
 done:
 if(live(r) && io.guard(io.ctx) && live(r))r->returned=intel_gt_access_return(&r->borrow);
 if(!r->returned)r->reason|=IEA_RETURN;
 r->epoch_after=g->engine_epoch;r->parent_attempts_after=g->write_attempts;
 if(r->returned) {
  if(!source_valid(r) || g->borrows || g->dependents)r->reason|=IEA_SOURCE;
  if(clock_ok(&call))r->end_us=r->last_us;
 }
 if(r->returned && (!r->end_us || r->end_us<=r->begin_us))r->reason|=IEA_CLOCK;
 r->complete=r->blocked && r->bound && r->returned && !r->reason &&
  r->bcs.disabled && r->bcs.entry_disabled && !r->bcs.attempts && !r->bcs.writes &&
  !r->bcs.uncertain && r->bcs.samples==4 && r->bcs.reads==32;
 return r->complete;
}
