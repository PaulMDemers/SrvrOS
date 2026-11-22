#include <srvros/intel_ggtt_invalidate.h>

static bool binding_same(const struct intel_ggtt_invalidate_binding *a,const struct intel_ggtt_invalidate_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime &&
 a->root==b->root && a->epoch==b->epoch && a->sequence==b->sequence && a->mmio==b->mmio &&
 a->mmio_bytes==b->mmio_bytes && a->prepare_begin==b->prepare_begin && a->gt_begin==b->gt_begin &&
 a->hda_begin==b->hda_begin && a->outer_begin==b->outer_begin && a->vendor==b->vendor && a->device==b->device;
}
static bool valid(const struct intel_ggtt_invalidate_source *s) {
 if(!s || s->object!=s)return false;
 const struct intel_ggtt_invalidate_binding *b=&s->binding;
 return b->owner && b->provider && b->lifetime && b->root && !(b->root&4095) && b->epoch && b->sequence &&
 b->mmio && !(b->mmio&65535) && b->mmio_bytes>=0x102000 && b->mmio<=UINT64_MAX-b->mmio_bytes &&
 b->vendor==0x8086 && b->device==0x1626 && b->outer_begin && b->gt_begin>=b->outer_begin &&
 b->hda_begin>=b->outer_begin && b->prepare_begin>=b->gt_begin && b->prepare_begin>=b->hda_begin;
}
static bool io_same(const struct intel_ggtt_invalidate_io *a,const struct intel_ggtt_invalidate_io *b) {
 return a->ctx==b->ctx && a->guard==b->guard && a->read==b->read && a->write==b->write && a->now==b->now;
}
static bool live(const struct intel_ggtt_invalidate *r) {
 return r && r->object==r && r->busy && !r->complete && !r->retained && !r->reason && !r->end &&
 !r->clock_at && !r->clock_previous && !r->clock_reason && r->source && valid(r->source) &&
 r->source->consumer==r && binding_same(&r->source->binding,&r->binding) && io_same(&r->io,&r->original_io);
}
static bool progress_same(const struct intel_ggtt_invalidate_progress *a,const struct intel_ggtt_invalidate_progress *b) {
 return a->begin==b->begin && a->last==b->last && a->state==b->state && a->reads==b->reads &&
 a->guards==b->guards && a->attempts==b->attempts && a->writes==b->writes && a->initial==b->initial &&
 a->posted==b->posted && a->uncertain==b->uncertain;
}
struct callback_snapshot {
 const struct intel_ggtt_invalidate_source *source;
 struct intel_ggtt_invalidate_binding binding;
 struct intel_ggtt_invalidate_io io;
 struct intel_ggtt_invalidate_progress progress;
};
static struct callback_snapshot capture(const struct intel_ggtt_invalidate *r) {
 return (struct callback_snapshot){r->source,r->binding,r->io,r->p};
}
static bool unchanged(const struct intel_ggtt_invalidate *r,const struct callback_snapshot *s) {
 return live(r) && r->source==s->source && binding_same(&r->binding,&s->binding) &&
 io_same(&r->io,&s->io) && progress_same(&r->p,&s->progress);
}
static bool fail(struct intel_ggtt_invalidate *r,unsigned why) {
 r->reason|=why;r->p.state=GGTT_INVALIDATE_FAULT;r->complete=false;r->retained=true;r->end=0;
 return false;
}
static bool tick(struct intel_ggtt_invalidate *r) {
 if(!live(r))return fail(r,GGTT_INVALIDATE_SOURCE);
 struct callback_snapshot saved=capture(r);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&saved))return fail(r,GGTT_INVALIDATE_SOURCE);
 unsigned why=!at?GGTT_INVALIDATE_ZERO:at<r->p.last?GGTT_INVALIDATE_BACKWARD:
 at-r->binding.prepare_begin>INTEL_GGTT_INVALIDATE_PREPARE_US?GGTT_INVALIDATE_PREPARE_EXPIRED:
 at-r->binding.gt_begin>INTEL_GGTT_INVALIDATE_GT_US?GGTT_INVALIDATE_GT_EXPIRED:
 at-r->binding.hda_begin>INTEL_GGTT_INVALIDATE_HDA_US?GGTT_INVALIDATE_HDA_EXPIRED:
 at-r->binding.outer_begin>INTEL_GGTT_INVALIDATE_OUTER_US?GGTT_INVALIDATE_OUTER_EXPIRED:0;
 if(why){r->clock_at=at;r->clock_previous=r->p.last;r->clock_reason=why;return fail(r,GGTT_INVALIDATE_CLOCK);}
 if(!r->p.begin)r->p.begin=at;
 r->p.last=at;return true;
}
static bool guard(struct intel_ggtt_invalidate *r) {
 if(!tick(r))return false;
 struct callback_snapshot saved=capture(r);bool ok=r->io.guard(r->io.ctx,r->source);
 if(!unchanged(r,&saved))return fail(r,GGTT_INVALIDATE_SOURCE);
 if(!tick(r))return false;
 if(!ok)return fail(r,GGTT_INVALIDATE_GUARD);
 ++r->p.guards;return true;
}
static bool read_word(struct intel_ggtt_invalidate *r,uint64_t *out) {
 if(!guard(r))return false;
 struct callback_snapshot saved=capture(r);uint32_t value=UINT32_MAX;
 bool ok=r->io.read(r->io.ctx,INTEL_GGTT_INVALIDATE_REGISTER,&value);
 if(!unchanged(r,&saved))return fail(r,GGTT_INVALIDATE_SOURCE);
 if(ok)++r->p.reads;
 if(!tick(r))return false;
 if(!ok)return fail(r,GGTT_INVALIDATE_READ);
 *out=value;
 return !value?true:fail(r,GGTT_INVALIDATE_PROFILE);
}
static bool request(struct intel_ggtt_invalidate *r) {
 if(!guard(r))return false;
 ++r->p.attempts;struct callback_snapshot saved=capture(r);
 enum intel_gt_write w=r->io.write(r->io.ctx,INTEL_GGTT_INVALIDATE_REGISTER,1);
 bool intact=unchanged(r,&saved);
 if(w==GT_WRITE_DONE)++r->p.writes;
 else if(w!=GT_WRITE_NONE)r->p.uncertain=1;
 if(!intact)return fail(r,GGTT_INVALIDATE_SOURCE);
 if(w!=GT_WRITE_DONE)return fail(r,GGTT_INVALIDATE_WRITE);
 return tick(r);
}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
bool intel_ggtt_invalidate_run(struct intel_ggtt_invalidate *r,struct intel_ggtt_invalidate_source *s,struct intel_ggtt_invalidate_io io) {
 if(!r || r->object || r->p.state || r->busy || r->retained || r->complete || r->reason || r->end ||
 r->clock_at || r->clock_previous || r->clock_reason || !valid(s) || s->consumer || io.ctx!=s->binding.provider ||
 !io.guard || !io.read || !io.write || !io.now || overlap(r,sizeof(*r),s,sizeof(*s)))return false;
 *r=(struct intel_ggtt_invalidate){.object=r,.source=s,.binding=s->binding,.io=io,.original_io=io,.busy=true,
 .p={.state=GGTT_INVALIDATE_OPENING,.last=s->binding.prepare_begin}};
 s->consumer=r;
 if(!read_word(r,&r->p.initial))goto done;
 r->p.state=GGTT_INVALIDATE_WRITING;if(!request(r))goto done;
 r->p.state=GGTT_INVALIDATE_POSTING;if(!read_word(r,&r->p.posted))goto done;
 r->p.state=GGTT_INVALIDATE_VERIFYING;if(!guard(r))goto done;
 if(r->p.reads!=2 || r->p.guards!=4 || r->p.attempts!=1 || r->p.writes!=1 || r->p.initial || r->p.posted || r->p.uncertain) {
  fail(r,GGTT_INVALIDATE_RESULT);goto done;
 }
 if(r->p.last<=r->p.begin){r->clock_at=r->p.last;r->clock_previous=r->p.last;r->clock_reason=GGTT_INVALIDATE_FROZEN;fail(r,GGTT_INVALIDATE_CLOCK);goto done;}
 r->p.state=GGTT_INVALIDATE_COMPLETE;r->end=r->p.last;r->complete=true;
done:
 r->busy=false;return r->complete;
}
