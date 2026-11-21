#include <srvros/intel_vtd_drain.h>

static bool binding_same(const struct intel_vtd_drain_binding *a,const struct intel_vtd_drain_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime && a->root==b->root &&
 a->epoch==b->epoch && a->sequence==b->sequence && a->hash==b->hash && a->unit==b->unit &&
 a->prepare_begin==b->prepare_begin && a->gt_begin==b->gt_begin && a->hda_begin==b->hda_begin &&
 a->outer_begin==b->outer_begin && a->vendor==b->vendor && a->device==b->device;
}
static bool source_valid(const struct intel_vtd_drain_source *s) {
 if(!s || s->object!=s)return false;
 const struct intel_vtd_drain_binding *b=&s->binding;
 return b->owner && b->provider && b->lifetime && b->root && !(b->root&4095) && b->epoch && b->sequence &&
 b->hash==INTEL_VTD_DRAIN_HASH && b->unit==INTEL_VTD_DRAIN_UNIT && b->vendor==0x8086 && b->device==0x1626 &&
 b->outer_begin && b->gt_begin>=b->outer_begin && b->hda_begin>=b->outer_begin &&
 b->prepare_begin>=b->gt_begin && b->prepare_begin>=b->hda_begin;
}
static bool live(const struct intel_vtd_drain *r) {
 return r && r->object==r && r->busy && !r->complete && !r->retained && !r->reason && !r->end &&
 !r->clock_at && !r->clock_previous && !r->clock_reason && source_valid(r->source) && r->source->consumer==r &&
 binding_same(&r->binding,&r->source->binding) && r->p.state>=VTD_DRAIN_OPENING && r->p.state<=VTD_DRAIN_VERIFYING &&
 r->p.attempts<=1 && r->p.writes<=r->p.attempts && r->p.polls<=INTEL_VTD_DRAIN_POLLS && !r->p.uncertain &&
 r->io.ctx==r->binding.provider && r->io.ctx==r->original_io.ctx && r->io.guard==r->original_io.guard && r->io.read==r->original_io.read &&
 r->io.write==r->original_io.write && r->io.now==r->original_io.now && r->io.pause==r->original_io.pause;
}
static bool progress_same(const struct intel_vtd_drain_progress *a,const struct intel_vtd_drain_progress *b) {
 if(a->begin!=b->begin || a->last!=b->last || a->state!=b->state || a->reads!=b->reads || a->guards!=b->guards ||
 a->attempts!=b->attempts || a->writes!=b->writes || a->polls!=b->polls || a->uncertain!=b->uncertain ||
 a->initial!=b->initial || a->final!=b->final)return false;
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<8;j++)if(a->context[i][j]!=b->context[i][j])return false;
 for(unsigned i=0;i<INTEL_VTD_DRAIN_POLLS;i++)if(a->observed[i]!=b->observed[i])return false;
 return true;
}
/* Only a scalar DATA projection is captured, not a live ownership object. */
struct callback_snapshot {
 const struct intel_vtd_drain_source *source;
 struct intel_vtd_drain_binding binding;
 struct intel_vtd_drain_io io;
 struct intel_vtd_drain_progress progress;
};
static struct callback_snapshot capture(const struct intel_vtd_drain *r) {
 return (struct callback_snapshot){r->source,r->binding,r->io,r->p};
}
static bool unchanged(const struct intel_vtd_drain *r,const struct callback_snapshot *s) {
 return live(r) && r->source==s->source && binding_same(&r->binding,&s->binding) &&
 r->io.ctx==s->io.ctx && r->io.guard==s->io.guard && r->io.read==s->io.read && r->io.write==s->io.write &&
 r->io.now==s->io.now && r->io.pause==s->io.pause && progress_same(&r->p,&s->progress);
}
static bool fail(struct intel_vtd_drain *r,unsigned why) {
 r->reason|=why;r->p.state=VTD_DRAIN_FAULT;r->complete=false;r->retained=true;r->end=0;return false;
}
static bool tick(struct intel_vtd_drain *r) {
 if(!live(r))return fail(r,VTD_DRAIN_SOURCE);
 struct callback_snapshot s=capture(r);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,VTD_DRAIN_SOURCE);
 unsigned why=!at?VTD_DRAIN_ZERO:at<r->p.last?VTD_DRAIN_BACKWARD:
 at-r->binding.prepare_begin>INTEL_VTD_DRAIN_PREPARE_US?VTD_DRAIN_PREPARE_EXPIRED:
 at-r->binding.gt_begin>INTEL_VTD_DRAIN_GT_US?VTD_DRAIN_GT_EXPIRED:
 at-r->binding.hda_begin>INTEL_VTD_DRAIN_HDA_US?VTD_DRAIN_HDA_EXPIRED:
 at-r->binding.outer_begin>INTEL_VTD_DRAIN_OUTER_US?VTD_DRAIN_OUTER_EXPIRED:0;
 if(why){r->clock_at=at;r->clock_previous=r->p.last;r->clock_reason=why;return fail(r,VTD_DRAIN_CLOCK);}
 if(!r->p.begin)r->p.begin=at;
 r->p.last=at;return true;
}
static bool guard(struct intel_vtd_drain *r) {
 if(!tick(r))return false;
 struct callback_snapshot s=capture(r);bool ok=r->io.guard(r->io.ctx,r->source);
 if(!unchanged(r,&s))return fail(r,VTD_DRAIN_SOURCE);
 if(!tick(r))return false;
 if(!ok)return fail(r,VTD_DRAIN_GUARD);
 ++r->p.guards;return true;
}
static bool permitted(uint32_t off) {
 return off==0 || off==8 || off==12 || off==16 || off==20 || off==28 || off==32 || off==36 ||
 off==40 || off==44 || off==52 || off==0x508 || off==0x50c;
}
static bool read_word(struct intel_vtd_drain *r,uint32_t off,uint32_t *out) {
 if(!out || !permitted(off) || !guard(r))return false;
 struct callback_snapshot s=capture(r);uint32_t value=UINT32_MAX;
 bool ok=r->io.read(r->io.ctx,off,&value);
 if(!unchanged(r,&s))return fail(r,VTD_DRAIN_SOURCE);
 if(ok)++r->p.reads;
 if(!tick(r))return false;
 if(!ok || value==UINT32_MAX)return fail(r,VTD_DRAIN_READ);
 *out=value;return true;
}
static bool pair(struct intel_vtd_drain *r,uint32_t off,uint64_t *out) {
 uint32_t high,low,again;
 if(!read_word(r,off+4,&high) || !read_word(r,off,&low) || !read_word(r,off+4,&again))return false;
 if(high!=again)return fail(r,VTD_DRAIN_PROFILE);
 *out=(uint64_t)high<<32|low;return true;
}
static bool zero_word(struct intel_vtd_drain *r,uint32_t off) {
 uint32_t v;
 if(!read_word(r,off,&v))return false;
 return !v?true:fail(r,VTD_DRAIN_BUSY);
}
static bool context(struct intel_vtd_drain *r,unsigned n) {
 uint32_t v;uint64_t *c=r->p.context[n];
 if(!read_word(r,28,&v))return false;
 c[0]=v;if(v)return fail(r,VTD_DRAIN_PROFILE);
 if(!read_word(r,0,&v))return false;
 c[1]=v;if(v!=0x10)return fail(r,VTD_DRAIN_PROFILE);
 if(!pair(r,8,&c[2]) || !pair(r,16,&c[3]) || !pair(r,32,&c[4]) || !pair(r,40,&c[5]))return false;
 /* The pinned CAP supports DRD/DWD; ECAP.IRO=0x50, so only0x508/0x50c.
  * Conservative fresh CCMD state (default CAIG=global), no prior request. */
 if(c[2]!=INTEL_VTD_DRAIN_CAP || c[3]!=INTEL_VTD_DRAIN_ECAP || c[4] || c[5]!=0x0800000000000000ull)return fail(r,VTD_DRAIN_PROFILE);
 if(!read_word(r,52,&v))return false;
 c[6]=v;if(v)return fail(r,VTD_DRAIN_PROFILE);
 if(!read_word(r,28,&v))return false;
 c[7]=v;if(v)return fail(r,VTD_DRAIN_PROFILE);
 if(n)for(unsigned i=0;i<8;i++)if(c[i]!=r->p.context[0][i])return fail(r,VTD_DRAIN_PROFILE);
 return true;
}
static bool poll_policy(struct intel_vtd_drain *r) {
 uint64_t c;
 if(!zero_word(r,28) || !pair(r,40,&c))return false;
 return c==r->p.context[0][5]?true:fail(r,VTD_DRAIN_BUSY);
}
static bool request(struct intel_vtd_drain *r) {
 if(!guard(r))return false;
 ++r->p.attempts;struct callback_snapshot s=capture(r);
 enum intel_gt_write w=r->io.write(r->io.ctx,0x50c,INTEL_VTD_DRAIN_REQUEST);
 bool intact=unchanged(r,&s);
 /* Record the possible store before evaluating any subsequent callback. */
 if(w==GT_WRITE_DONE)++r->p.writes;
 else if(w!=GT_WRITE_NONE)r->p.uncertain=1;
 if(!intact)return fail(r,VTD_DRAIN_SOURCE);
 if(w!=GT_WRITE_DONE)return fail(r,VTD_DRAIN_WRITE);
 return tick(r);
}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
bool intel_vtd_drain_run(struct intel_vtd_drain *r,struct intel_vtd_drain_source *s,struct intel_vtd_drain_io io) {
 if(!r || r->object || r->p.state || r->busy || r->retained || r->complete || r->reason || r->end || r->clock_at || r->clock_previous || r->clock_reason || !source_valid(s) || s->consumer || io.ctx!=s->binding.provider ||
 !io.guard || !io.read || !io.write || !io.now || !io.pause || overlap(r,sizeof(*r),s,sizeof(*s)))return false;
 *r=(struct intel_vtd_drain){.object=r,.source=s,.binding=s->binding,.io=io,.original_io=io,.busy=true,
 .p={.state=VTD_DRAIN_OPENING,.last=s->binding.prepare_begin}};s->consumer=r;
 if(!guard(r) || !context(r,0))goto done;
 if(!pair(r,0x508,&r->p.initial))goto done;
 /* Initially reset or a prior exact global DR/DW completion; no command or
  * partially configured/foreign state is overwritten. Both low DWORDs are0. */
 if(r->p.initial!=0x0200000000000000ull && r->p.initial!=0x1203000000000000ull){fail(r,VTD_DRAIN_BUSY);goto done;}
 if(!poll_policy(r) || !zero_word(r,52) || !zero_word(r,28))goto done;
 r->p.state=VTD_DRAIN_REQUESTING;if(!request(r))goto done;
 r->p.state=VTD_DRAIN_POLLING;
 for(unsigned i=0;i<INTEL_VTD_DRAIN_POLLS;i++) {
  uint32_t v;
  if(!poll_policy(r) || !read_word(r,0x50c,&v) || !zero_word(r,28) || !zero_word(r,52))goto done;
  r->p.observed[i]=v;++r->p.polls;r->p.final=(uint64_t)v<<32;
  /* Exactly global+DR+DW; only IVT and IAIG may change in the response. */
  if((v&~0x86000000u)!=0x10030000u){fail(r,VTD_DRAIN_RESULT);goto done;}
  if(!(v&0x80000000u)) {
   if((v&0x06000000u)!=0x02000000u){fail(r,VTD_DRAIN_RESULT);goto done;}
   break;
  }
  if(i+1==INTEL_VTD_DRAIN_POLLS){fail(r,VTD_DRAIN_PENDING);goto done;}
  if(!guard(r))goto done;
  struct callback_snapshot saved=capture(r);r->io.pause(r->io.ctx);
  if(!unchanged(r,&saved)){fail(r,VTD_DRAIN_SOURCE);goto done;}
  if(!tick(r))goto done;
 }
 r->p.state=VTD_DRAIN_VERIFYING;
 if(!context(r,1) || !zero_word(r,0x508))goto done;
 uint32_t final;
 if(!read_word(r,0x50c,&final))goto done;
 if(final!=0x12030000u || r->p.final!=0x1203000000000000ull || r->p.attempts!=1 || r->p.writes!=1 ||
 !r->p.polls || r->p.polls>INTEL_VTD_DRAIN_POLLS || r->p.uncertain){fail(r,VTD_DRAIN_RESULT);goto done;}
 if(!guard(r))goto done;
 if(r->p.reads!=43+7*r->p.polls || r->p.guards!=45+8*r->p.polls){fail(r,VTD_DRAIN_RESULT);goto done;}
 if(r->p.last<=r->p.begin){r->clock_at=r->p.last;r->clock_previous=r->p.begin;r->clock_reason=VTD_DRAIN_FROZEN;fail(r,VTD_DRAIN_CLOCK);goto done;}
 r->end=r->p.last;r->p.state=VTD_DRAIN_COMPLETE;r->complete=true;
 done:r->busy=false;return r->complete;
}
