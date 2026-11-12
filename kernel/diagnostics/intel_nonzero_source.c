#include <srvros/intel_nonzero_source.h>
static bool fail(struct intel_nonzero_source *s,unsigned reason){s->reason|=reason;return false;}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool geometry(const struct scanout_handoff_snapshot *s) {
 return s && s->mmio==0xc0000000 && s->aperture==0xb0000000 && s->aperture_bytes==0x10000000 &&
  !s->fence_begin && s->fence_end==0x2200000 && s->first==INTEL_WINDOW_FIRST &&
  s->pages==INTEL_WINDOW_ENTRIES && !s->assurances && s->original[0]!=UINT64_MAX;
}
bool intel_nonzero_source_live(const struct intel_nonzero_source *s) {
 if(!s || s->object!=s || !s->registered || s->returned || s->complete || s->reason ||
    !s->reset || !intel_nonzero_reset_live(s->reset))return false;
 const struct intel_nonzero_reset *r=s->reset;
 const struct intel_completion_result *p=&r->post;
 if(r->stage!=INZ_PRESERVE_STAGE || !r->preserved || !r->observer_entered || r->observer_complete ||
    r->dependent!=s || r->registered!=s || r->child_borrows!=1 || r->child_returns ||
    r->entry!=s->entry || r->parent!=s->parent || s->post!=p || s->epoch!=2 ||
    r->epoch_after!=s->epoch || s->parent->engine_epoch!=s->epoch ||
    s->origin!=r->begin || s->parent_begin!=r->parent_begin ||
    s->parent_begin!=s->parent->begin || s->post_mode!=r->post_mode ||
    p->object!=p || p->parent!=s->parent || p->engine_epoch!=s->epoch ||
    !p->complete || !p->stable || p->reason || p->enabled || p->pending || p->flip_wait ||
    p->tlb_pending || p->parser_idle!=3 || p->groups!=10 || p->samples!=2 || p->contexts!=2 || p->reads!=92 ||
    p->batches!=2 || p->begin_us!=s->post_begin || p->end_us!=s->post_end || p->last_us!=s->post_last ||
    r->reader_count!=37 || !geometry(s->input) || s->expected!=s->input->original[0] ||
    !s->origin || s->post_begin<s->origin || s->post_end<=s->post_begin ||
    s->post_last<s->post_begin || s->post_last>s->post_end || s->post_end>r->last)return false;
 if(s->post_mode!=INZ_POST_PRESERVED && s->post_mode!=INZ_POST_RESET_DEFAULT)return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<5;e++)for(unsigned i=0;i<8;i++) {
   uint32_t want=r->source->words[n][e][i];
   if(s->post_mode==INZ_POST_RESET_DEFAULT && e==4 && (i==2 || i==3))want=0;
   if(p->words[n][e][i]!=want)return false;
  }
  for(unsigned i=0;i<6;i++)if(p->context[n][i]!=r->source->context[n][i])return false;
 }
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)if(r->readers.planes[p][i]!=r->entry->readers[1].planes[p][i])return false;
 for(unsigned i=0;i<10;i++)if(r->readers.audio[i]!=r->entry->readers[1].audio[i])return false;
 return true;
}
static bool clock_at(struct intel_nonzero_source *s,struct intel_nonzero_source_io io) {
 uint64_t at=io.now(io.ctx);
 if(s->provider!=io.ctx || !intel_nonzero_source_live(s))return fail(s,INS_PARENT);
 if(!at || at<s->last || at<s->reset->last || at<s->parent->last ||
    at<s->origin || at-s->origin>100000 || at<s->parent_begin || at-s->parent_begin>300000) {
  if(!(s->reason&INS_CLOCK)){s->clock_at=at;s->clock_prev=s->last;}
  return fail(s,INS_CLOCK);
 }
 s->last=at;s->reset->last=at;s->parent->last=at;return true;
}
static bool guard(struct intel_nonzero_source *s,struct intel_nonzero_source_io io) {
 if(s->provider!=io.ctx || !intel_nonzero_source_live(s))return fail(s,INS_PARENT);
 if(!clock_at(s,io))return false;
 bool ok=io.guard(io.ctx);
 if(s->provider!=io.ctx || !intel_nonzero_source_live(s))return fail(s,INS_PARENT);
 if(!clock_at(s,io))return false;
 return ok || fail(s,INS_GUARD);
}
bool intel_nonzero_source_run(struct intel_nonzero_source *s,struct intel_nonzero_reset *r,
 const struct scanout_handoff_snapshot *input,struct intel_nonzero_source_io io) {
 if(!s || s->object)return false;
 if(r && (overlap(s,sizeof(*s),r,sizeof(*r)) || (r->parent && overlap(s,sizeof(*s),r->parent,sizeof(*r->parent))) ||
    (r->entry && overlap(s,sizeof(*s),r->entry,sizeof(*r->entry))) || (r->source && overlap(s,sizeof(*s),r->source,sizeof(*r->source)))))return false;
 if(input && overlap(s,sizeof(*s),input,sizeof(*input)))return false;
 *s=(struct intel_nonzero_source){.object=s,.reset=r,.input=input,.provider=io.ctx};
 if(!r || !geometry(input) || !io.guard || !io.sample || !io.now)return fail(s,INS_INPUT);
 if(!intel_nonzero_reset_live(r) || r->dependent || r->registered || r->child_borrows || r->child_returns ||
    r->stage!=INZ_PRESERVE_STAGE || !r->preserved || !r->observer_entered || r->observer_complete)return fail(s,INS_PARENT);
 s->post=&r->post;s->entry=r->entry;s->parent=r->parent;s->expected=input->original[0];
 s->origin=r->begin;s->parent_begin=r->parent_begin;s->epoch=r->epoch_after;s->post_mode=r->post_mode;
 s->post_begin=r->post.begin_us;s->post_end=r->post.end_us;s->post_last=r->post.last_us;s->last=r->last;
 r->dependent=r->registered=s;++r->child_borrows;s->registered=true;
 /* The registered child makes the enclosing reset's normal return invalid. */
 s->blocked=r->dependent==s && r->child_borrows!=r->child_returns;
 if(!guard(s,io))return false;
 s->begin=s->last;s->read_begin=s->last;
 bool ok=io.sample(io.ctx,s->words,&s->count);
 if(s->count>6){s->count=0;fail(s,INS_READ);}
 bool same=true;
 for(unsigned i=0;i<s->count;i++)if(s->words[i/3][i%3]!=(uint32_t)(s->expected>>(i%3==1?0:32)))same=false;
 if(!same)fail(s,INS_CHANGED);
 /* Keep drift/prefix evidence even when the post-read guard also fails. */
 if(s->reason){return false;}
 bool guarded=guard(s,io);s->read_end=s->last;
 if(!ok || s->count!=6)fail(s,INS_READ);
 if(!guarded || s->reason)return false;
 s->stable=true;s->return_begin=s->last;
 if(!guard(s,io))return false;
 s->end=s->last;
 if(s->end<=s->begin || s->read_end<=s->read_begin)return fail(s,INS_CLOCK);
 r->dependent=r->registered=0;++r->child_returns;s->registered=false;s->returned=true;s->complete=true;
 return true;
}
