#include <srvros/intel_interval_claim.h>
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool parent_storage(const struct intel_engine_rehold_child *child,const void *p,uint64_t n) {
 if(!child)return false;
 if(overlap(p,n,child,sizeof(*child)))return true;
 const struct intel_engine_rehold *r=child->parent;
 return r && (overlap(p,n,r,sizeof(*r)) ||
  (r->reset && overlap(p,n,r->reset,sizeof(*r->reset))) ||
  (r->preparation && overlap(p,n,r->preparation,sizeof(*r->preparation))) ||
  (r->parent && overlap(p,n,r->parent,sizeof(*r->parent))));
}
static bool fail(struct intel_interval_claim *c,unsigned reason) {
 c->reason|=reason;c->state=IIC_FAULT;return false;
}
static bool geometry(const struct scanout_handoff_snapshot *s) {
 return s && s->mmio==0xc0000000 && s->aperture==0xb0000000 && s->aperture_bytes==0x10000000 &&
  !s->fence_begin && s->fence_end==0x2200000 && s->first==INTEL_WINDOW_FIRST &&
  s->pages==INTEL_WINDOW_ENTRIES && !s->assurances && s->original[0]!=UINT64_MAX;
}
static bool borrower_source(const struct intel_interval_claim *c) {
 const struct intel_interval_borrow *b=c->dependent;
 if(!b)return !c->registered && c->borrows==c->returns;
 return b==c->registered && c->returns<c->borrows && c->borrows-c->returns==1 &&
  b->object==b && b->claim==c && b->epoch==c->epoch &&
  b->sequence==c->borrows && b->active && !b->fault;
}
static bool source(const struct intel_interval_claim *c) {
 const struct intel_engine_rehold *r=c->rehold;
 return c->object==c && c->pinned && c->child && intel_engine_rehold_child_live(c->child) &&
  c->child->dependent==c && c->child->dependent_borrows==1 && !c->child->dependent_returns &&
  c->child->parent==r && r->reset==c->reset && r->parent==c->parent &&
  r->origin_us==c->origin_us && r->owner_origin_us==c->owner_origin_us &&
  c->parent->begin==c->owner_origin_us && r->epoch==c->epoch && c->parent->engine_epoch==c->epoch &&
  geometry(&c->captured) && c->captured.original[0]==c->expected && borrower_source(c);
}
static uint64_t sample_time(struct intel_interval_claim *c) {
 if(c->reason)return c->last_us;
 if(!source(c)){fail(c,IIC_SOURCE);return 0;}
 uint64_t at=c->io.now(c->io.ctx);
 if(!at || at<c->last_us || at<c->origin_us || at-c->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US ||
    !c->owner_origin_us || at<c->owner_origin_us || at-c->owner_origin_us>GT_ACCESS_TOTAL_US) {
  c->clock_at=at;c->clock_prev=c->last_us;fail(c,IIC_CLOCK);return 0;
 }
 c->last_us=at;return at;
}
static bool guard(struct intel_interval_claim *c) {
 if(c->reason)return false;
 if(!sample_time(c))return false;
 if(!source(c))return fail(c,IIC_SOURCE);
 bool ok=c->io.guard(c->io.ctx);
 if(!source(c))fail(c,IIC_SOURCE);
 if(!ok)fail(c,IIC_GUARD);
 if(c->reason)return false;
 return sample_time(c) && (source(c) || fail(c,IIC_SOURCE));
}
static uint64_t observer_now(void *ctx){return sample_time(ctx);}
static bool observer_guard(void *ctx){return guard(ctx);}
static bool observer_read(void *ctx,uint32_t offset,uint32_t *out) {
 struct intel_interval_claim *c=ctx;
 if(!guard(c))return false;
 bool ok=c->io.read(c->io.ctx,offset,out);
 if(!ok)fail(c,IIC_READ);
 /* The actual observer accounts a completed DWORD before its post-read guard. */
 return ok;
}
static bool observe(struct intel_interval_claim *c,struct intel_held_interval_result *out) {
 bool ok=intel_held_interval_observe(out,c->reset,&c->captured,
  (struct intel_held_interval_io){c,observer_guard,observer_read,observer_now});
 if(!ok) {
  if(out->reason&IHI_CHANGED)fail(c,IIC_CHANGED);
  if((out->reason&IHI_CLOCK) && !c->reason) {
   c->clock_at=out->end_us;c->clock_prev=out->begin_us;fail(c,IIC_CLOCK);
  }
  if(!c->reason)fail(c,IIC_READ);
  return false;
 }
 struct scanout_transfer_manifest m;
 out->bound=intel_held_interval_manifest(out,&m) && m.original==&out->expected &&
  out->expected==c->expected && m.first==INTEL_WINDOW_FIRST && m.pages==1 &&
  !m.assurances && m.obligations==INTEL_COMPLETION_OPEN && m.source_epoch==c->epoch;
 return out->bound || fail(c,IIC_SOURCE);
}
static bool finish(struct intel_interval_claim *c,bool ok) {
 if(!ok && !c->reason)fail(c,IIC_SOURCE);
 c->end_us=c->last_us;c->busy=false;return ok;
}
bool intel_interval_claim_open(struct intel_interval_claim *c,struct intel_engine_rehold_child *child,
 const struct scanout_handoff_snapshot *input,struct intel_held_interval_io io) {
 if(!c || c->object)return false;
 if(parent_storage(child,c,sizeof(*c)))return false;
 if(input && overlap(c,sizeof(*c),input,sizeof(*input)))return false;
 *c=(struct intel_interval_claim){.object=c,.child=child,.io=io,.state=IIC_OPENING,.busy=true};
 if(!geometry(input) || !io.guard || !io.read || !io.now){fail(c,IIC_INPUT);return finish(c,false);}
 if(!intel_engine_rehold_child_live(child) || child->dependent || child->dependent_borrows || child->dependent_returns) {
  fail(c,IIC_CHILD);return finish(c,false);
 }
 c->captured=*input;c->expected=input->original[0];c->rehold=child->parent;
 c->reset=c->rehold->reset;c->parent=c->rehold->parent;c->epoch=c->rehold->epoch;
 c->origin_us=c->rehold->origin_us;c->owner_origin_us=c->rehold->owner_origin_us;c->last_us=c->rehold->last_us;
 child->dependent=c;++child->dependent_borrows;c->pinned=true;
 if(!guard(c))return finish(c,false);
 c->begin_us=c->last_us;
 if(!observe(c,&c->opening) || !guard(c))return finish(c,false);
 if(c->last_us<=c->begin_us){c->clock_at=c->last_us;c->clock_prev=c->begin_us;fail(c,IIC_CLOCK);return finish(c,false);}
 c->state=IIC_ACTIVE;return finish(c,true);
}
bool intel_interval_claim_check(struct intel_interval_claim *c) {
 if(!c || c->object!=c || c->busy || c->reason || c->state!=IIC_ACTIVE)return false;
 c->busy=true;uint64_t before=c->last_us;
 bool ok=guard(c);
 if(ok && c->last_us<=before){c->clock_at=c->last_us;c->clock_prev=before;fail(c,IIC_CLOCK);ok=false;}
 return finish(c,ok);
}
bool intel_interval_claim_manifest(const struct intel_interval_claim *c,struct scanout_transfer_manifest *out) {
 if(!c || c->object!=c || c->busy || c->reason || c->state!=IIC_ACTIVE || !out ||
    (c->dependent && overlap(c->dependent,sizeof(*c->dependent),out,sizeof(*out))) ||
    overlap(c,sizeof(*c),out,sizeof(*out)) || parent_storage(c->child,out,sizeof(*out)) || !source(c) || !c->opening.bound || !c->opening.complete ||
    c->opening.expected!=c->expected || c->opening.manifest.original!=&c->opening.expected ||
    c->last_us<c->origin_us || c->last_us-c->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US ||
    c->last_us<c->owner_origin_us || c->last_us-c->owner_origin_us>GT_ACCESS_TOTAL_US)return false;
 *out=(struct scanout_transfer_manifest){c->captured.mmio,c->captured.aperture,
  c->captured.aperture_bytes,c->captured.fence_begin,c->captured.fence_end,
  c->epoch,INTEL_WINDOW_FIRST,1,0,INTEL_COMPLETION_OPEN,&c->expected};return true;
}
static bool registered_borrow(const struct intel_interval_borrow *b) {
 return b && b->object==b && b->active && !b->fault && b->claim &&
  b->claim->object==b->claim && b->claim->dependent==b &&
  b->claim->state==IIC_ACTIVE && !b->claim->reason && borrower_source(b->claim);
}
static bool borrow_entry(struct intel_interval_borrow *b) {
 if(!b || b->object!=b || !b->active || b->busy || b->fault)return false;
 if(!b->claim || b->claim->object!=b->claim){b->fault=true;return false;}
 if(b->claim->busy)return false;
 if(registered_borrow(b))return true;
 /* A copied consumer cannot fault the original owner. A still-registered
  * original whose other source witness was lost must latch that failure. */
 b->fault=true;
 if(b->claim->registered==b || b->claim->dependent==b)fail(b->claim,IIC_SOURCE);
 return false;
}
bool intel_interval_borrow_open(struct intel_interval_claim *c,struct intel_interval_borrow *b) {
 if(!c || c->object!=c || c->busy || c->reason || c->state!=IIC_ACTIVE ||
    !b || b->object || c->dependent || c->registered || c->borrows!=c->returns || c->borrows==UINT32_MAX ||
    overlap(c,sizeof(*c),b,sizeof(*b)) || parent_storage(c->child,b,sizeof(*b)))return false;
 *b=(struct intel_interval_borrow){.object=b,.claim=c,.epoch=c->epoch,
  .sequence=c->borrows+1,.active=true,.busy=true};
 c->dependent=b;c->registered=b;++c->borrows;
 bool ok=intel_interval_claim_check(c) && registered_borrow(b);
 if(!ok && !c->reason)fail(c,IIC_SOURCE);
 b->busy=false;b->fault=!ok;
 return ok;
}
bool intel_interval_borrow_check(struct intel_interval_borrow *b) {
 if(!borrow_entry(b))return false;
 b->busy=true;
 struct intel_interval_claim *c=b->claim;
 bool ok=intel_interval_claim_check(c) && registered_borrow(b);
 if(!ok && !c->reason)fail(c,IIC_SOURCE);
 b->busy=false;b->fault=!ok;
 return ok;
}
bool intel_interval_borrow_manifest(const struct intel_interval_borrow *b,struct scanout_transfer_manifest *out) {
 if(!registered_borrow(b) || b->busy || !out || overlap(b,sizeof(*b),out,sizeof(*out)))return false;
 return intel_interval_claim_manifest(b->claim,out);
}
bool intel_interval_borrow_page(const struct intel_interval_borrow *b,uint64_t physical,
                               struct intel_bdw_ggtt_page *out) {
 if(!registered_borrow(b) || b->busy || !out ||
    overlap(b,sizeof(*b),out,sizeof(*out)) ||
    overlap(b->claim,sizeof(*b->claim),out,sizeof(*out)) ||
    parent_storage(b->claim->child,out,sizeof(*out)))return false;
 struct scanout_transfer_manifest manifest;
 return intel_interval_borrow_manifest(b,&manifest) &&
  intel_bdw_ggtt_page_plan(&manifest,physical,out);
}
bool intel_interval_borrow_return(struct intel_interval_borrow *b) {
 if(!borrow_entry(b))return false;
 b->busy=true;
 struct intel_interval_claim *c=b->claim;
 bool ok=intel_interval_claim_check(c) && registered_borrow(b);
 if(!ok && !c->reason)fail(c,IIC_SOURCE);
 if(ok) {
  c->dependent=0;c->registered=0;++c->returns;b->active=false;
 }
 b->busy=false;b->fault=!ok;
 return ok;
}
bool intel_interval_claim_close(struct intel_interval_claim *c) {
 if(!c || c->object!=c || c->busy || c->reason || c->state!=IIC_ACTIVE ||
    c->dependent || c->registered || c->borrows!=c->returns)return false;
 c->busy=true;c->state=IIC_CLOSING;uint64_t before=c->last_us;
 if(!guard(c) || !observe(c,&c->closing) || !guard(c))return finish(c,false);
 if(c->last_us<=before){c->clock_at=c->last_us;c->clock_prev=before;fail(c,IIC_CLOCK);return finish(c,false);}
 /* Last guard rechecks the exact registered source; only clean close unpins. */
 c->child->dependent=0;++c->child->dependent_returns;c->pinned=false;c->state=IIC_CLOSED;
 return finish(c,true);
}
