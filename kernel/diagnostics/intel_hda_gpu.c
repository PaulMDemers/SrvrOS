#include <srvros/intel_hda_gpu.h>
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fault(struct intel_hda_gpu *s,unsigned why) {
 s->reason|=why;s->state=IHG_FAULT;s->busy=false;s->complete=false;
 if(s->pinned || s->hda.retained || s->hda.attempts)s->retained=true;
 s->end_us=s->last_us;return false;
}
static bool gpu_empty(const struct intel_gt_access *g) {
 return g && !g->object && !g->state && !g->pinned && !g->writes && !g->write_attempts &&
  !g->borrows && !g->dependents && !g->engine_epoch;
}
static bool gpu_source(const struct intel_hda_gpu *s) {
 const struct intel_gt_access *g=s->gpu;
 return gpu_empty(g) || (g && g->object==g && g->state!=GT_ACCESS_FAULT &&
  (g->state==GT_ACCESS_OPENING || g->state==GT_ACCESS_AWAKE || g->state==GT_ACCESS_CLOSED) &&
  g->exclusion.epoch==s->exclusion.epoch && g->exclusion.owner==s->exclusion.owner);
}
static bool claim_source(const struct intel_hda_gpu *s) {
 const struct intel_interval_claim *c=s->claim;
 if(!c)return !s->attachments;
 if(s->attachments!=1)return false;
 if(!c->object)return c->state==IIC_EMPTY && !c->pinned && !c->reason;
 return c->object==c && !c->reason && c->parent==s->gpu &&
  (c->state==IIC_OPENING || c->state==IIC_ACTIVE || c->state==IIC_CLOSING || c->state==IIC_CLOSED);
}
bool intel_hda_gpu_live(const struct intel_hda_gpu *s) {
 return s && s->object==s && (s->state==IHG_ACTIVE || s->state==IHG_CLOSING) &&
  !s->reason && !s->retained && s->pinned && display_access_held(&s->exclusion) &&
  intel_hda_owner_borrowed(&s->hda,&s->borrow) && s->hda.pins==1 && s->hda.dependents==&s->borrow &&
  s->token.owner==&s->hda && s->token.epoch==s->hda.epoch &&
  s->borrow.dependent==s && s->borrow.dependent_borrows==1 && !s->borrow.dependent_returns &&
  gpu_source(s) && claim_source(s);
}
bool intel_hda_gpu_time(const struct intel_hda_gpu *s,uint64_t at) {
 return s && s->object==s && s->pinned && !s->reason &&
  (s->state==IHG_OPENING || s->state==IHG_ACTIVE || s->state==IHG_CLOSING) &&
  at && s->outer_origin_us && at>=s->last_us && at>=s->outer_origin_us &&
  at-s->outer_origin_us<=INTEL_HDA_GPU_TOTAL_US &&
  (!s->hda.begin_us || (at>=s->hda.last_us && at>=s->hda.begin_us && at-s->hda.begin_us<=INTEL_HDA_OWNER_TOTAL_US));
}
bool intel_hda_gpu_deadline(struct intel_hda_gpu *s,uint64_t at) {
 if(!s || s->object!=s || !s->pinned || s->reason ||
    (s->state!=IHG_OPENING && s->state!=IHG_ACTIVE && s->state!=IHG_CLOSING))return false;
 if(intel_hda_gpu_time(s,at)){s->last_us=at;return true;}
 if(!s->clock_cause) {
  s->clock_at=at;s->clock_prev=s->last_us;
  s->clock_cause=(!at?1u:0u) |
   ((at<s->last_us || at<s->outer_origin_us || (s->hda.begin_us && at<s->hda.last_us))?2u:0u) |
   ((s->hda.begin_us && at>=s->hda.begin_us && at-s->hda.begin_us>INTEL_HDA_OWNER_TOTAL_US)?4u:0u) |
   ((s->outer_origin_us && at>=s->outer_origin_us && at-s->outer_origin_us>INTEL_HDA_GPU_TOTAL_US)?8u:0u);
  if(!s->clock_cause)s->clock_cause=16; /* malformed original origin */
 }
 return fault(s,IHG_CLOCK);
}
static bool tick(struct intel_hda_gpu *s) {
 uint64_t at=s->io.hda.now(s->io.hda.ctx);
 if(!intel_hda_gpu_deadline(s,at))return false;
 if(!display_access_held(&s->exclusion))return fault(s,IHG_EXCLUSION);
 s->last_us=at;return true;
}
static bool guard(struct intel_hda_gpu *s) {
 if(!tick(s))return false;
 if(!intel_hda_gpu_live(s))return fault(s,IHG_SOURCE);
 if(!intel_hda_owner_validate(&s->hda,&s->borrow))return fault(s,IHG_HDA);
 if(!intel_hda_gpu_live(s))return fault(s,IHG_SOURCE);
 return tick(s);
}
static bool retire(struct intel_hda_gpu *s) {
 if(!tick(s))return false;
 if(!s->io.retire(s->io.ctx))return fault(s,IHG_RETIRE);
 if(!tick(s))return false;
 if(!display_access_unpin(&s->exclusion))return fault(s,IHG_EXCLUSION);
 s->pinned=false;s->retained=false;s->state=IHG_CLOSED;s->busy=false;s->end_us=s->last_us;return true;
}
bool intel_hda_gpu_open(struct intel_hda_gpu *s,struct intel_gt_access *gpu,
 const struct display_access_token *exclusion,uint64_t origin,
 struct intel_hda_owner_facts facts,struct intel_hda_gpu_io io) {
 if(!s || s->object || !gpu || !exclusion || overlap(s,sizeof(*s),gpu,sizeof(*gpu)) ||
    overlap(s,sizeof(*s),exclusion,sizeof(*exclusion)))return false;
 *s=(struct intel_hda_gpu){.object=s,.gpu=gpu,.exclusion=*exclusion,.io=io,.outer_origin_us=origin,.state=IHG_OPENING,.busy=true};
 if(!origin || !gpu_empty(gpu) || !display_access_held(exclusion) || !io.retire || !io.hda.now ||
    !io.hda.inspect || !io.hda.guard || !io.hda.endpoint || !io.hda.command16 ||
    !facts.compiled || !facts.recipe || !facts.efi64 || !facts.loader || !facts.profile ||
    !facts.serialized || !facts.excluded || !facts.prerequisites)return fault(s,IHG_INPUT);
 if(!display_access_pin(exclusion))return fault(s,IHG_EXCLUSION);
 s->pinned=true;
 if(!tick(s))return false;
 s->begin_us=s->last_us;
 if(!intel_hda_owner_open(&s->hda,facts,io.hda,&s->token)) {
  bool clean_unissued=!s->reason && s->state==IHG_OPENING;
  s->reason|=IHG_HDA;
  /* A rejected first read is known unissued. Only adapter-proved retirement
   * can unwind; no HDA store/pin or uncertain mapping may be discarded. */
  if(clean_unissued && !s->hda.attempts && !s->hda.retained && !s->hda.pins && !s->hda.dependents) {
   s->reason=0;
   if(retire(s)){s->reason=IHG_HDA;return false;}
  }
  return fault(s,IHG_HDA);
 }
 if(!intel_hda_owner_pin(&s->hda,&s->token,&s->borrow))return fault(s,IHG_HDA);
 s->borrow.dependent=s;s->borrow.dependent_borrows=1;s->state=IHG_ACTIVE;
 if(!guard(s))return false;
 s->busy=false;return true;
}
bool intel_hda_gpu_check(struct intel_hda_gpu *s) {
 if(!s || s->object!=s || s->busy || s->state!=IHG_ACTIVE || s->reason)return false;
 if(s->checks==UINT32_MAX)return fault(s,IHG_SOURCE);
 if(!intel_hda_gpu_live(s))return fault(s,IHG_SOURCE);
 s->busy=true;
 if(!guard(s))return false;
 ++s->checks;s->busy=false;return true;
}
bool intel_hda_gpu_attach(struct intel_hda_gpu *s,struct intel_interval_claim *claim) {
 if(!s || s->object!=s || s->busy || s->state!=IHG_ACTIVE || s->reason || !claim ||
    !s->gpu || s->attachments || claim->object || overlap(s,sizeof(*s),claim,sizeof(*claim)) ||
    overlap(s->gpu,sizeof(*s->gpu),claim,sizeof(*claim)) || s->gpu->state!=GT_ACCESS_AWAKE)return false;
 if(!intel_hda_gpu_check(s))return false;
 s->claim=claim;s->attachments=1;return true;
}
static bool gpu_retired(const struct intel_hda_gpu *s,bool complete) {
 const struct intel_gt_access *g=s->gpu;const struct intel_interval_claim *c=s->claim;
 if(!g || g->pinned || g->borrows || g->dependents)return false;
 if(!gpu_empty(g) && (g->object!=g || g->state!=GT_ACCESS_CLOSED))return false;
 if(c && (c->object!=c || c->state!=IIC_CLOSED || c->pinned || c->reason ||
    !c->opening.complete || !c->closing.complete || !c->opening.bound || !c->closing.bound))return false;
 return !complete || (c && s->attachments==1 && g->object==g && g->state==GT_ACCESS_CLOSED);
}
bool intel_hda_gpu_close(struct intel_hda_gpu *s,bool complete) {
 if(!s || s->object!=s || s->busy || s->state!=IHG_ACTIVE || s->reason)return false;
 if(!intel_hda_gpu_live(s))return fault(s,IHG_SOURCE);
 s->busy=true;s->state=IHG_CLOSING;
 if(!guard(s))return false;
 if(!gpu_retired(s,complete))return fault(s,IHG_GPU);
 if(!intel_hda_owner_check(&s->hda,&s->borrow,&s->mid))return fault(s,IHG_HDA);
 if(!guard(s) || !gpu_retired(s,complete))return fault(s,IHG_GPU);
 s->borrow.dependent=0;++s->borrow.dependent_returns;
 if(!intel_hda_owner_unpin(&s->hda,&s->borrow) || !intel_hda_owner_close(&s->hda,&s->token))return fault(s,IHG_HDA);
 if(!gpu_retired(s,complete))return fault(s,IHG_GPU);
 if(!retire(s))return false;
 s->gpu_complete=complete;s->complete=complete;return true;
}
