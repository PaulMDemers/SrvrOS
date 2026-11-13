#include <srvros/intel_source_transfer.h>
#ifndef SRVROS_INTEL_SOURCE_TRANSFER
#error "Source transfer requires its separate image"
#endif
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool fail(struct intel_source_transfer *t,unsigned why) {
 t->reason|=why;t->state=IST_FAULT;return false;
}
static uint64_t clock_owner(void *);
static bool provider(void *,const struct scanout_transfer_manifest *,enum scanout_transfer_phase);
bool intel_source_transfer_live(const struct intel_source_transfer *t) {
 if(!t || t->object!=t || !t->registered || t->reason || t->closed ||
    t->state<IST_OPENING || t->state>IST_CLOSING || !t->buffers ||
    !intel_source_buffers_live(t->buffers))return false;
 const struct intel_source_buffers *b=t->buffers;const struct intel_nonzero_source *s=b->source;
 if(b->dependent!=t || b->registered_child!=t || b->child_borrows!=1 || b->child_returns ||
    !b->observer_entered || b->observer_complete || s!=t->source || s->input!=t->input ||
    s->provider!=t->provider || b->lifetime!=t->lifetime || b->expected!=t->expected ||
    b->origin!=t->origin || b->parent_begin!=t->parent_begin || b->epoch!=t->epoch ||
    b->kind!=t->kind || b->post_mode!=t->post_mode || b->last!=t->last ||
    !t->begin || t->begin>t->last || t->end)return false;
 if(t->io.ctx!=t->original_io.ctx || t->io.guard!=t->original_io.guard ||
    t->io.now!=t->original_io.now || t->io.revalidate!=t->original_io.revalidate ||
    t->assurances!=t->expected_assurances || t->obligations!=t->expected_obligations)return false;
 if(t->exclusion.epoch!=s->parent->exclusion.epoch || t->exclusion.owner!=s->parent->exclusion.owner ||
    !display_access_held(&t->exclusion))return false;
 for(unsigned i=0;i<2;i++) {
  const struct scanout_memory_hold *h=b->hold[i];
  if(h!=t->hold[i] || h->memory.object!=t->memory[i].object || h->memory.epoch!=t->memory[i].epoch ||
     h->memory.owner!=t->memory[i].owner || h->physical!=t->physical[i] || h->serial!=t->serial[i])return false;
 }
 if(t->state==IST_REFUSED && (t->owner.object || t->pool.epoch || t->space_pool.epoch ||
    t->token.object || t->callbacks || t->clocks || !t->refused || t->opened))return false;
 if(t->state==IST_ACTIVE || t->state==IST_CLOSING) {
  const struct scanout_transfer *o=&t->owner;
  if(!t->opened || t->refused || o->object!=o || t->token.object!=o ||
     t->token.epoch!=o->epoch || t->pool.epoch!=o->epoch || t->token.owner!=t->exclusion.owner ||
     o->owner!=t->exclusion.owner || o->arena_epoch!=t->space_pool.epoch ||
     o->io.ctx!=t || o->io.revalidate!=provider || o->io.now!=clock_owner ||
     o->manifest.original!=o->original || o->original[0]!=t->expected ||
     o->manifest.source_epoch!=t->epoch || o->manifest.first!=t->input->first || o->manifest.pages!=1 ||
     o->manifest.mmio!=t->input->mmio || o->manifest.aperture!=t->input->aperture ||
     o->manifest.aperture_bytes!=t->input->aperture_bytes ||
     o->manifest.fence_begin!=t->input->fence_begin || o->manifest.fence_end!=t->input->fence_end ||
     o->manifest.assurances!=t->assurances || o->manifest.obligations!=t->obligations)return false;
 }
 return true;
}
static bool tick(struct intel_source_transfer *t) {
 if(!t->busy || !intel_source_transfer_live(t))return fail(t,IST_SOURCE);
 unsigned state=t->state;struct intel_source_transfer_io io=t->io;uint64_t at=io.now(io.ctx);
 if(t->io.ctx!=io.ctx || t->io.now!=io.now || t->io.guard!=io.guard ||
    t->io.revalidate!=io.revalidate || !t->busy || t->state!=state || !intel_source_transfer_live(t))return fail(t,IST_SOURCE);
 struct intel_nonzero_source *s=t->source;
 if(!at || at<t->last || at<s->last || at<s->reset->last || at<s->parent->last ||
    at<t->origin || at-t->origin>100000 || at<t->parent_begin || at-t->parent_begin>300000) {
  t->clock_at=at;t->clock_prev=t->last;return fail(t,IST_CLOCK);
 }
 t->last=t->buffers->last=s->last=s->reset->last=s->parent->last=at;return true;
}
static bool guard(struct intel_source_transfer *t) {
 if(!tick(t))return false;
 unsigned state=t->state;struct intel_source_transfer_io io=t->io;bool ok=io.guard(io.ctx);
 if(t->io.ctx!=io.ctx || t->io.now!=io.now || t->io.guard!=io.guard ||
    t->io.revalidate!=io.revalidate || !t->busy || t->state!=state || !intel_source_transfer_live(t))return fail(t,IST_SOURCE);
 return ok ? tick(t) : fail(t,IST_GUARD);
}
static uint64_t clock_owner(void *ctx) {
 struct intel_source_transfer *t=ctx;++t->clocks;
 return tick(t)?t->last:0;
}
static bool provider(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
 struct intel_source_transfer *t=ctx;
 if(!intel_source_transfer_live(t) || !m || m->source_epoch!=t->epoch || m->pages!=1 ||
    m->first!=t->input->first || m->mmio!=t->input->mmio || m->aperture!=t->input->aperture ||
    m->aperture_bytes!=t->input->aperture_bytes || m->fence_begin!=t->input->fence_begin ||
    m->fence_end!=t->input->fence_end || !m->original || m->original[0]!=t->expected ||
    m->assurances!=t->assurances || m->obligations!=t->obligations || !guard(t))return false;
 unsigned state=t->state;struct intel_source_transfer_io io=t->io;++t->callbacks;
 bool ok=io.revalidate(io.ctx,m,phase);
 if(t->io.ctx!=io.ctx || t->io.now!=io.now || t->io.guard!=io.guard ||
    t->io.revalidate!=io.revalidate || !t->busy || t->state!=state || !intel_source_transfer_live(t))return fail(t,IST_SOURCE);
 return ok && guard(t);
}
static bool transfer_open(struct intel_source_transfer *t,struct intel_source_buffers *b,
 const struct display_access_token *exclusion,struct intel_source_transfer_evidence evidence,
 struct intel_source_transfer_io io) {
 if(!t || t->object || !b || !exclusion || !io.guard || !io.now || !io.revalidate ||
    evidence.assurances&~HANDOFF_ALL || evidence.obligations&~255u ||
    overlap(t,sizeof(*t),b,sizeof(*b)) || overlap(t,sizeof(*t),exclusion,sizeof(*exclusion)) ||
    !b->source || overlap(t,sizeof(*t),b->source,sizeof(*b->source)))return false;
 if(!intel_source_buffers_live(b) || !b->observer_entered || b->observer_complete ||
    b->dependent || b->registered_child || b->child_borrows || b->child_returns)return false;
 for(unsigned i=0;i<2;i++)if(overlap(t,sizeof(*t),b->hold[i],sizeof(*b->hold[i])) ||
    overlap(t,sizeof(*t),b->hold[i]->memory.object,sizeof(*b->hold[i]->memory.object)))return false;
 struct intel_nonzero_source *s=b->source;
 if(exclusion->epoch!=s->parent->exclusion.epoch || exclusion->owner!=s->parent->exclusion.owner ||
    !display_access_held(exclusion))return false;
 if(overlap(t,sizeof(*t),s->reset,sizeof(*s->reset)) || overlap(t,sizeof(*t),s->parent,sizeof(*s->parent)) ||
    overlap(t,sizeof(*t),s->entry,sizeof(*s->entry)) || overlap(t,sizeof(*t),s->input,sizeof(*s->input)))return false;
 *t=(struct intel_source_transfer){.object=t,.buffers=b,.source=s,.input=s->input,.provider=s->provider,
  .lifetime=b->lifetime,.expected=b->expected,.origin=b->origin,.parent_begin=b->parent_begin,
  .begin=b->last,.last=b->last,.epoch=b->epoch,.kind=b->kind,.post_mode=b->post_mode,
  .state=IST_OPENING,.busy=true,.assurances=evidence.assurances,.obligations=evidence.obligations,
  .expected_assurances=evidence.assurances,.expected_obligations=evidence.obligations,.exclusion=*exclusion,.io=io,.original_io=io};
 for(unsigned i=0;i<2;i++){t->hold[i]=b->hold[i];t->memory[i]=b->hold[i]->memory;t->physical[i]=b->physical[i];t->serial[i]=b->serial[i];}
 b->dependent=b->registered_child=t;++b->child_borrows;t->registered=true;++t->borrows;
 t->source_blocked=s->dependent==b && b->dependent==t && b->child_borrows!=b->child_returns;
 t->holds_blocked=!scanout_memory_hold_end(b->hold[0]) && !scanout_memory_hold_end(b->hold[1]);
 if(!t->source_blocked || !t->holds_blocked || !guard(t))return false;
 struct scanout_transfer_manifest m={s->input->mmio,s->input->aperture,s->input->aperture_bytes,
  s->input->fence_begin,s->input->fence_end,s->epoch,s->input->first,1,t->assurances,t->obligations,&t->expected};
 bool opened=scanout_transfer_open(&t->owner,&t->pool,&t->space_pool,&t->exclusion,&m,
  (struct scanout_transfer_io){t,provider,clock_owner},&t->token);
 if(!opened) {
  if(t->owner.object || t->pool.epoch || t->space_pool.epoch || t->token.object || t->callbacks || t->clocks ||
     (t->assurances==HANDOFF_ALL && !t->obligations))return fail(t,IST_OWNER);
  t->refused=true;t->state=IST_REFUSED;return guard(t);
 }
 t->opened=true;t->state=IST_ACTIVE;return guard(t);
}
static bool transfer_check(struct intel_source_transfer *t) {
 if(!t || t->object!=t || !guard(t))return false;
 if(t->state==IST_REFUSED)return t->checked=true;
 if(t->state!=IST_ACTIVE && t->state!=IST_CLOSING)return fail(t,IST_OWNER);
 if(!scanout_transfer_check(&t->owner,&t->token))return fail(t,IST_OWNER);
 if(!guard(t))return false;
 return t->checked=true;
}
static bool transfer_close(struct intel_source_transfer *t) {
 if(!t || t->object!=t || !guard(t))return false;
 if(t->state!=IST_REFUSED) {
  if(t->state!=IST_ACTIVE && t->state!=IST_CLOSING)return fail(t,IST_OWNER);
  t->state=IST_CLOSING;
  if(!scanout_transfer_close(&t->owner,&t->token)) {
   if(t->owner.state==TRANSFER_CLOSING && intel_source_transfer_live(t))return false;
   return fail(t,IST_OWNER);
  }
 }
 if(!guard(t))return false;
 if(t->last<=t->begin) {
  t->clock_at=t->last;t->clock_prev=t->begin;return fail(t,IST_CLOCK);
 }
 struct intel_source_buffers *b=t->buffers;
 b->dependent=b->registered_child=0;++b->child_returns;++t->returns;
 t->end=t->last;t->registered=false;t->closed=true;t->state=IST_CLOSED;return true;
}

bool intel_source_transfer_open(struct intel_source_transfer *t,struct intel_source_buffers *b,
 const struct display_access_token *e,struct intel_source_transfer_evidence evidence,struct intel_source_transfer_io io) {
 if(!t || t->busy)return false;
 bool ok=transfer_open(t,b,e,evidence,io);
 if(t->object==t)t->busy=false;
 return ok;
}
bool intel_source_transfer_check(struct intel_source_transfer *t) {
 if(!t || t->object!=t || t->busy || t->state==IST_CLOSED || t->state==IST_FAULT)return false;
 t->busy=true;bool ok=transfer_check(t);t->busy=false;return ok;
}
bool intel_source_transfer_close(struct intel_source_transfer *t) {
 if(!t || t->object!=t || t->busy || t->state==IST_CLOSED || t->state==IST_FAULT)return false;
 t->busy=true;bool ok=transfer_close(t);t->busy=false;return ok;
}
