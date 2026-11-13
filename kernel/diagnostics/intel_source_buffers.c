#include <srvros/intel_source_buffers.h>
#ifndef SRVROS_INTEL_STARTUP_BUFFERS
#error "Source buffers require their separate startup image"
#endif
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fail(struct intel_source_buffers *b,unsigned why){b->reason|=why;return false;}
static bool full(const struct scanout_memory_hold *h) {
 return scanout_memory_hold_live(h) && h->layout.width==1440 && h->layout.height==900 &&
  h->layout.pitch==6144 && h->layout.rows==904 && h->layout.pages==1356 && h->layout.tiled==1 &&
  h->layout.bytes==5554176 && h->layout.visible_bytes==5529600 && h->bytes==5554176 &&
  h->physical && !(h->physical&4095) && h->physical<=0x100000000ull-h->bytes;
}
bool intel_source_buffers_live(const struct intel_source_buffers *b) {
 if(!b || b->object!=b || !b->registered || b->returned || b->complete || b->reason ||
    !b->source || !intel_nonzero_source_live(b->source))return false;
 const struct intel_nonzero_source *s=b->source;
 if(s->dependent!=b || s->registered_child!=b || s->child_borrows!=1 || s->child_returns ||
    !s->observer_entered || s->observer_complete || !s->stable || s->count!=6 ||
    s->expected!=b->expected || s->origin!=b->origin || s->parent_begin!=b->parent_begin ||
    s->epoch!=b->epoch || s->post_mode!=b->post_mode || s->entry->kind!=b->kind ||
    b->last!=s->last || b->end || b->planned>2 ||
    (b->begin && (b->begin<s->read_end || b->begin>b->last)))return false;
 for(unsigned i=0;i<2;i++) {
  const struct scanout_memory_hold *h=b->hold[i];
  if(!full(h) || !b->lifetime || h->lifetime!=b->lifetime || h->dependent!=b || h->registered!=b || h->physical!=b->physical[i] ||
     h->serial!=b->serial[i] || h->memory.object!=b->memory[i].object ||
     h->memory.epoch!=b->memory[i].epoch || h->memory.owner!=b->memory[i].owner)return false;
  if(i<b->planned) {
   const struct intel_bdw_ggtt_page *p=&b->page[i];
   if(p->physical!=b->physical[i] || p->original!=b->expected || p->replacement!=(b->physical[i]|1) ||
      p->index!=s->input->first || p->gpu_address!=(uint64_t)s->input->first*4096 ||
      p->mmio_offset!=0x800000+s->input->first*8)return false;
  }
 }
 return b->hold[0]!=b->hold[1] && b->hold[0]->memory.object!=b->hold[1]->memory.object &&
  b->hold[0]->memory.owner==b->hold[1]->memory.owner &&
  (b->physical[0]+b->hold[0]->bytes<=b->physical[1] || b->physical[1]+b->hold[1]->bytes<=b->physical[0]);
}
static bool tick(struct intel_source_buffers *b) {
 if(!intel_source_buffers_live(b))return fail(b,ISB_SOURCE);
 struct intel_source_buffers_io io=b->io;uint64_t at=io.now(io.ctx);
 if(b->io.ctx!=io.ctx || b->io.now!=io.now || b->io.guard!=io.guard || !intel_source_buffers_live(b))return fail(b,ISB_SOURCE);
 struct intel_nonzero_source *s=b->source;
 if(!at || at<b->last || at<s->last || at<s->reset->last || at<s->parent->last || at<b->origin ||
    at-b->origin>100000 || at<b->parent_begin || at-b->parent_begin>300000) {
  b->clock_at=at;b->clock_prev=b->last;return fail(b,ISB_CLOCK);
 }
 b->last=s->last=s->reset->last=s->parent->last=at;return true;
}
static bool guard(struct intel_source_buffers *b) {
 if(!tick(b))return false;
 struct intel_source_buffers_io io=b->io;bool ok=io.guard(io.ctx);
 if(b->io.ctx!=io.ctx || b->io.now!=io.now || b->io.guard!=io.guard || !intel_source_buffers_live(b))return fail(b,ISB_SOURCE);
 if(!ok)return fail(b,ISB_GUARD);
 return tick(b);
}
bool intel_source_buffers_run(struct intel_source_buffers *b,struct intel_nonzero_source *s,
 struct scanout_memory_hold *a,struct scanout_memory_hold *c,struct intel_source_buffers_io io) {
 if(!b || b->object)return false;
 if(!s || !a || !c || !io.guard || !io.now || a==c ||
    overlap(a,sizeof(*a),c,sizeof(*c)) ||
    overlap(b,sizeof(*b),s,sizeof(*s)) || overlap(b,sizeof(*b),a,sizeof(*a)) || overlap(b,sizeof(*b),c,sizeof(*c)) ||
    !s->reset || overlap(b,sizeof(*b),s->reset,sizeof(*s->reset)) ||
    !s->parent || overlap(b,sizeof(*b),s->parent,sizeof(*s->parent)) ||
    !s->entry || overlap(b,sizeof(*b),s->entry,sizeof(*s->entry)) ||
    !s->input || overlap(b,sizeof(*b),s->input,sizeof(*s->input)) ||
    !a->memory.object || !c->memory.object || overlap(b,sizeof(*b),a->memory.object,sizeof(*a->memory.object)) ||
    overlap(b,sizeof(*b),c->memory.object,sizeof(*c->memory.object)))return false;
 *b=(struct intel_source_buffers){.object=b,.source=s,.hold={a,c},.io=io};
 if(!full(a) || !full(c) || !a->lifetime || a->lifetime!=c->lifetime || a->dependent || a->registered || c->dependent || c->registered)return fail(b,ISB_BACKING);
 if(!intel_nonzero_source_live(s) || s->dependent || s->registered_child || s->child_borrows || s->child_returns ||
    !s->observer_entered || s->observer_complete || !s->stable || s->count!=6)return fail(b,ISB_SOURCE);
 b->lifetime=a->lifetime;b->epoch=s->epoch;b->kind=s->entry->kind;b->post_mode=s->post_mode;b->expected=s->expected;
 b->origin=s->origin;b->parent_begin=s->parent_begin;b->last=s->last;
 for(unsigned i=0;i<2;i++) {
  struct scanout_memory_hold *h=b->hold[i];b->physical[i]=h->physical;b->serial[i]=h->serial;b->memory[i]=h->memory;
 }
 if(a->memory.object==c->memory.object || a->memory.owner!=c->memory.owner ||
    !(a->physical+a->bytes<=c->physical || c->physical+c->bytes<=a->physical))return fail(b,ISB_BACKING);
 s->dependent=s->registered_child=b;++s->child_borrows;b->registered=true;
 a->dependent=a->registered=c->dependent=c->registered=b;
 b->blocked=s->child_borrows!=s->child_returns && !scanout_memory_hold_end(a) && !scanout_memory_hold_end(c);
 if(!b->blocked || !guard(b))return false;
 b->begin=b->last;
 struct scanout_transfer_manifest m={s->input->mmio,s->input->aperture,s->input->aperture_bytes,
  s->input->fence_begin,s->input->fence_end,b->epoch,s->input->first,1,0,255,&b->expected};
 for(unsigned i=0;i<2;i++) {
  if(!intel_bdw_ggtt_page_plan(&m,b->physical[i],&b->page[i]))return fail(b,ISB_INPUT);
  ++b->planned;
 }
 if(!guard(b))return false;
 b->end=b->last;if(b->end<=b->begin){b->clock_at=b->end;b->clock_prev=b->begin;return fail(b,ISB_CLOCK);}
 /* Only the final same-origin guard permits this original registered return. */
 s->dependent=s->registered_child=0;++s->child_returns;
 a->dependent=a->registered=c->dependent=c->registered=0;
 b->registered=false;b->returned=true;b->complete=true;return true;
}
