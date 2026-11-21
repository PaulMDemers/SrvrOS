#include <srvros/intel_ring_reset.h>
struct reset_call {struct intel_ring_reset *r;struct intel_takeover_io io;
 const struct intel_ring_transition *entry;struct intel_gt_access *parent;struct intel_takeover_result *takeover;};
static bool identity(const struct reset_call *p) {
 return p->r->object==p->r && p->r->entry==p->entry && p->r->parent==p->parent &&
  p->r->takeover==p->takeover && p->r->provider==p->io.ctx && p->r->source==&p->entry->post;
}
static bool source(const struct intel_ring_reset *r) {
 const struct intel_ring_transition *e=r->entry;
 return r->object==r && e && e->parent==r->parent && r->source==&e->post &&
  intel_ring_entry_reset_ready(e) && r->source->begin_us==r->source_begin &&
  r->source->end_us==r->source_end && r->source->last_us==r->source_last &&
  e->end==r->entry_end && e->epoch_after==r->epoch;
}
static uint64_t sample_time(void *ctx) {
 struct reset_call *p=ctx;struct intel_ring_reset *r=p->r;
 if(!identity(p) || !source(r)){r->reason|=IRH_SOURCE;return 0;}
 uint64_t at=p->io.now(p->io.ctx);
 if(!identity(p) || !source(r)){r->reason|=IRH_SOURCE;return 0;}
 if(!at || at<r->last || at<r->begin || at-r->begin>INTEL_RING_RESET_ADMIT_US ||
    at<=r->source_end || at-r->source_end>INTEL_RING_RESET_ADMIT_US) {
  if(!(r->reason&IRH_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}
  r->reason|=IRH_CLOCK;return 0;
 }
 r->last=at;return at;
}
static bool guard(void *ctx) {
 struct reset_call *p=ctx;struct intel_ring_reset *r=p->r;
 if(!sample_time(p))return false;
 if(!source(r)){r->reason|=IRH_SOURCE;return false;}
 bool ok=p->io.guard(p->io.ctx);
 if(!identity(p) || !source(r) || !ok){r->reason|=IRH_SOURCE;return false;}
 ++r->checks;return true;
}
static bool sample(void *ctx,uint32_t out[INTEL_TAKEOVER_WORDS],unsigned *count) {
 struct reset_call *p=ctx;
 if(!guard(p))return false;
 bool ok=p->io.sample(p->io.ctx,out,count);
 return guard(p) && ok;
}
static bool takeover_bound(const struct intel_ring_reset *r) {
 const struct intel_takeover_result *t=r->takeover;const struct intel_gt_access *g=r->parent;
 return t && t->object==t && t->parent==g && t->census==r->source && t->epoch==r->epoch &&
  t->complete && t->quiescent && t->stable && !t->reason && t->returned &&
  t->borrow.object==&t->borrow && t->borrow.parent==g && !t->borrow.active && !t->borrow.next &&
  t->attempts==2 && t->samples==2 && t->reads==12 && t->counts[0]==6 && t->counts[1]==6 &&
  !g->borrows && !g->dependents && t->begin_us==r->take_begin && t->end_us==r->take_end &&
  t->last_us==r->take_last && r->begin<=t->begin_us && t->begin_us<=t->last_us &&
  t->last_us<=t->end_us && t->end_us>t->begin_us && t->end_us-t->begin_us<=100000 && t->end_us<=r->last;
}
bool intel_ring_reset_admit(struct intel_ring_reset *r,struct intel_gt_access *g,
 const struct intel_ring_transition *e,struct intel_takeover_result *t,bool policy,struct intel_takeover_io io) {
 if(!r || r->object)return false;
 *r=(struct intel_ring_reset){.object=r,.entry=e,.parent=g,.takeover=t,.provider=io.ctx};
 if(!g || !e || !t || t->object || !policy || !io.guard || !io.sample || !io.now ||
    e->kind!=IRT_KIND_DISABLED || !e->complete){r->reason=IRH_INPUT;return false;}
 r->eligible=true;r->source=&e->post;r->source_begin=e->post.begin_us;
 r->source_end=e->post.end_us;r->source_last=e->post.last_us;r->entry_end=e->end;r->epoch=e->epoch_after;
 struct reset_call p={r,io,e,g,t};
 if(!source(r) || g->borrows || g->dependents){r->reason|=IRH_SOURCE;goto done;}
 r->begin=io.now(io.ctx);r->last=r->entry_end;
 if(!r->begin || r->begin<r->entry_end || r->begin<=r->source_end ||
    r->begin-r->source_end>INTEL_RING_RESET_ADMIT_US) {
  r->clock_at=r->begin;r->clock_prev=r->entry_end;r->reason|=IRH_CLOCK;goto done;
 }
 r->last=r->begin;
 if(!sample_time(&p))goto done;
 if(!guard(&p))goto done;
 (void)intel_takeover_observe(t,g,r->source,(struct intel_takeover_io){&p,guard,sample,sample_time});
 if(!t->complete || !t->quiescent){r->reason|=IRH_TAKEOVER;goto done;}
 r->take_begin=t->begin_us;r->take_end=t->end_us;r->take_last=t->last_us;
 if(!takeover_bound(r)){r->reason|=IRH_RETURN;goto done;}
 if(!guard(&p))goto done;
 if(!takeover_bound(r)){r->reason|=IRH_RETURN;goto done;}
 r->end=sample_time(&p);
 if(!r->end || r->end<=r->begin)r->reason|=IRH_CLOCK;
 if(!identity(&p) || !source(r) || !takeover_bound(r)){r->reason|=IRH_RETURN;goto done;}
 if(!r->reason)r->bound=true;
 done:
 r->complete=r->eligible && r->bound && r->end && !r->reason;
 r->uncertain=r->eligible && !r->complete;
 return r->complete;
}
