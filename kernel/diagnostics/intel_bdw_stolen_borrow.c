#include <srvros/intel_bdw_stolen_borrow.h>

static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool plan_same(const struct bdw_stolen_plan *a,const struct bdw_stolen_plan *b) {
#define EQ(f) if(a->f!=b->f)return false
 EQ(data_begin);EQ(data_end);EQ(usable_end);EQ(gtt_begin);EQ(gtt_end);EQ(pixel_bytes);EQ(translation_bytes);
#undef EQ
 for(unsigned i=0;i<3;i++)if(a->range[i].gpu!=b->range[i].gpu || a->range[i].bytes!=b->range[i].bytes || a->range[i].pages!=b->range[i].pages)return false;
 return true;
}
static bool binding_same(const struct bdw_stolen_binding *a,const struct bdw_stolen_binding *b) {
#define EQ(f) if(a->f!=b->f)return false
 EQ(provider);EQ(lifetime);EQ(owner);EQ(root);EQ(epoch);EQ(admitted_at);EQ(expires_at);
 EQ(facts.host);EQ(facts.gpu);EQ(facts.subsystem);EQ(facts.ggc);EQ(facts.bdsm);EQ(facts.bgsm);EQ(facts.tolud);EQ(facts.reserved);
 EQ(facts.aperture_bytes);EQ(facts.ggtt_bytes);EQ(facts.fence_begin);EQ(facts.fence_end);EQ(facts.width);EQ(facts.height);EQ(facts.stride);
 EQ(layout.checked);EQ(layout.classified);
#undef EQ
 for(unsigned i=0;i<3;i++)if(a->layout.physical[i]!=b->layout.physical[i] || a->layout.physical_end[i]!=b->layout.physical_end[i])return false;
 return plan_same(&a->layout.plan,&b->layout.plan);
}
static bool geometry(const struct bdw_stolen_binding *b) {
 struct bdw_stolen_plan p;
 if(!b->provider || !b->lifetime || !b->owner || !b->root || (b->root&4095) || !b->epoch ||
 !b->admitted_at || b->expires_at<=b->admitted_at || !b->layout.classified ||
 !intel_bdw_stolen_scanout_plan(&b->facts,&p) || !plan_same(&p,&b->layout.plan))return false;
 uint64_t checked=0;
 for(unsigned i=0;i<3;i++) {
  uint64_t lo=b->layout.physical[i],hi=b->layout.physical_end[i];
  if((lo&4095) || lo<p.data_begin+4096 || lo>=hi || hi>p.usable_end || hi-lo!=p.range[i].bytes)return false;
  for(unsigned j=0;j<i;j++)if(lo<b->layout.physical_end[j] && b->layout.physical[j]<hi)return false;
  checked+=2ull*p.range[i].pages;
 }
 return checked==b->layout.checked;
}
static bool io_same(struct bdw_stolen_borrow_io a,struct bdw_stolen_borrow_io b) {
 return a.ctx==b.ctx && a.validate==b.validate && a.now==b.now;
}
static bool empty(const struct intel_bdw_stolen_borrow *b) {
 const struct bdw_stolen_binding zero={0};const struct bdw_stolen_borrow_io no_io={0};
 if(b->object || b->source || b->last || b->state || b->reason || b->checks || b->pinned || b->busy || b->creating ||
 b->exclusion.owner || b->exclusion.epoch || !binding_same(&b->binding,&zero) || !io_same(b->io,no_io) || !io_same(b->original_io,no_io))return false;
 for(unsigned i=0;i<2;i++)if(b->slot[i].object || b->slot[i].parent || b->slot[i].child || b->slot[i].index || b->slot[i].pages || b->slot[i].claimed || b->slot[i].returned)return false;
 return true;
}
static bool identity(const struct intel_bdw_stolen_borrow *b) {
 if(!b || b->object!=b || !b->source || b->source->object!=b->source ||
 b->source->consumer!=b || !b->source->consumed || b->source->rights!=BDW_BORROW_ALL ||
 !binding_same(&b->binding,&b->source->binding) || !io_same(b->io,b->original_io) ||
 b->io.ctx!=b->binding.provider || !b->io.validate || !b->io.now ||
 !b->pinned || b->exclusion.owner!=b->binding.owner || !display_access_held(&b->exclusion))return false;
 for(unsigned i=0;i<2;i++)if(b->slot[i].object!=&b->slot[i] || b->slot[i].parent!=b || b->slot[i].index!=i)return false;
 return b->state==BDW_BORROW_ACTIVE || b->state==BDW_BORROW_CLOSING;
}
/* Value-only callback snapshot, never a copy of the live owner or children. */
struct saved {
 struct bdw_stolen_source *source;
 struct bdw_stolen_binding binding;
 struct bdw_stolen_borrow_io io;
 struct display_access_token exclusion;
 struct {struct scanout_memory *child;uint32_t pages;bool claimed,returned;} slot[2];
 uint64_t last;
 uint32_t state,checks;
 bool creating;
};
static struct saved capture(const struct intel_bdw_stolen_borrow *b) {
 struct saved s={.source=b->source,.binding=b->binding,.io=b->io,.exclusion=b->exclusion,
 .last=b->last,.state=b->state,.checks=b->checks,.creating=b->creating};
 for(unsigned i=0;i<2;i++){s.slot[i].child=b->slot[i].child;s.slot[i].pages=b->slot[i].pages;s.slot[i].claimed=b->slot[i].claimed;s.slot[i].returned=b->slot[i].returned;}
 return s;
}
static bool intact(const struct intel_bdw_stolen_borrow *b,const struct saved *s) {
 if(b->object!=b || b->source!=s->source || !b->busy || b->reason || !binding_same(&b->binding,&s->binding) ||
 !io_same(b->io,s->io) || b->last!=s->last || b->state!=s->state || b->checks!=s->checks || b->creating!=s->creating ||
 b->exclusion.owner!=s->exclusion.owner || b->exclusion.epoch!=s->exclusion.epoch)return false;
 for(unsigned i=0;i<2;i++)if(b->slot[i].child!=s->slot[i].child || b->slot[i].pages!=s->slot[i].pages || b->slot[i].claimed!=s->slot[i].claimed || b->slot[i].returned!=s->slot[i].returned)return false;
 return identity(b);
}
static bool fault(struct intel_bdw_stolen_borrow *b,const struct saved *s,unsigned why) {
 b->object=b;b->source=s->source;b->binding=s->binding;b->io=b->original_io=s->io;b->exclusion=s->exclusion;
 for(unsigned i=0;i<2;i++)b->slot[i]=(struct bdw_stolen_slot){&b->slot[i],b,s->slot[i].child,i,s->slot[i].pages,s->slot[i].claimed,s->slot[i].returned};
 b->last=s->last;b->checks=s->checks;b->creating=s->creating;b->pinned=true;b->state=BDW_BORROW_FAULT;b->reason|=why;b->busy=false;
 return false;
}
static bool proof(struct intel_bdw_stolen_borrow *b,enum bdw_borrow_phase phase) {
 struct saved s=capture(b);
 if(!identity(b) || b->busy || b->reason || b->checks==UINT32_MAX)return fault(b,&s,BDW_BORROW_IDENTITY);
 b->busy=true;
 uint64_t begin=s.io.now(s.io.ctx);
 if(!intact(b,&s))return fault(b,&s,BDW_BORROW_IDENTITY);
 if(!begin || begin<s.last || begin<s.binding.admitted_at || begin>s.binding.expires_at)return fault(b,&s,BDW_BORROW_CLOCK);
 struct bdw_stolen_proof p={0};bool ok=s.io.validate(s.io.ctx,s.source,phase,&p);
 if(!intact(b,&s))return fault(b,&s,BDW_BORROW_IDENTITY);
 uint64_t end=s.io.now(s.io.ctx);
 if(!intact(b,&s))return fault(b,&s,BDW_BORROW_IDENTITY);
 if(end<=begin || end-begin>BDW_BORROW_OPERATION_US || end>s.binding.expires_at)return fault(b,&s,BDW_BORROW_CLOCK);
 if(!ok || p.source!=s.source || p.provider!=s.binding.provider || p.lifetime!=s.binding.lifetime ||
 p.owner!=s.binding.owner || p.root!=s.binding.root || p.epoch!=s.binding.epoch || p.rights!=BDW_BORROW_ALL ||
 p.phase!=(uint32_t)phase || p.checked_at<begin || p.checked_at>end)return fault(b,&s,BDW_BORROW_PROOF);
 b->last=end;++b->checks;b->busy=false;return true;
}
static bool auth(const struct intel_bdw_stolen_borrow *b,const struct bdw_stolen_borrow_token *t) {
 return b && t && b->object==b && !b->busy && !b->creating && t->object==b && t->owner==b->binding.owner && t->epoch==b->binding.epoch &&
 (b->state==BDW_BORROW_ACTIVE || b->state==BDW_BORROW_CLOSING);
}
bool intel_bdw_stolen_borrow_begin(struct intel_bdw_stolen_borrow *b,struct bdw_stolen_source *source,
 const struct display_access_token *exclusion,struct bdw_stolen_borrow_io io,struct bdw_stolen_borrow_token *out) {
 if(!b || !empty(b) ||
 overlap(b,sizeof(*b),source,sizeof(*source)) || overlap(b,sizeof(*b),out,sizeof(*out)) ||
 overlap(source,sizeof(*source),out,sizeof(*out)) || overlap(b,sizeof(*b),exclusion,sizeof(*exclusion)) ||
 overlap(exclusion,sizeof(*exclusion),out,sizeof(*out)) || source->object!=source || source->consumer || source->consumed ||
 source->rights!=BDW_BORROW_ALL || !geometry(&source->binding) || io.ctx!=source->binding.provider || !io.validate || !io.now ||
 exclusion->owner!=source->binding.owner || !display_access_held(exclusion))return false;
 if(!display_access_pin(exclusion))return false;
 *b=(struct intel_bdw_stolen_borrow){.object=b,.source=source,.binding=source->binding,.io=io,.original_io=io,
 .exclusion=*exclusion,.last=source->binding.admitted_at,.state=BDW_BORROW_ACTIVE,.pinned=true};
 for(unsigned i=0;i<2;i++)b->slot[i]=(struct bdw_stolen_slot){.object=&b->slot[i],.parent=b,.index=i};
 source->consumer=b;source->consumed=true;
 if(!proof(b,BDW_BORROW_ADMIT))return false;
 *out=(struct bdw_stolen_borrow_token){b,b->binding.owner,b->binding.epoch};return true;
}
bool intel_bdw_stolen_borrow_check(struct intel_bdw_stolen_borrow *b,const struct bdw_stolen_borrow_token *t) {
 return auth(b,t) && proof(b,BDW_BORROW_LIVE);
}
static bool slot_valid(const struct bdw_stolen_slot *s) {
 return s && s->parent && s->index<2 && s==&s->parent->slot[s->index] && s->object==s &&
 s->child && s->parent->object==s->parent && !s->parent->busy && s->parent->state!=BDW_BORROW_FAULT;
}
static uint64_t allocate(void *ctx,uint32_t pages) {
 struct bdw_stolen_slot *s=ctx;
 if(!slot_valid(s) || !s->parent->creating || s->parent->state!=BDW_BORROW_ACTIVE || s->claimed || s->returned ||
 !pages || pages>s->parent->binding.layout.plan.range[s->index+1].pages || !proof(s->parent,BDW_BORROW_LIVE))return 0;
 s->claimed=true;s->pages=pages;return s->parent->binding.layout.physical[s->index+1];
}
static bool validate(void *ctx,uint64_t physical,uint32_t pages) {
 struct bdw_stolen_slot *s=ctx;
 return slot_valid(s) && s->claimed && !s->returned && s->pages==pages &&
 physical==s->parent->binding.layout.physical[s->index+1] && proof(s->parent,BDW_BORROW_LIVE);
}
static bool release(void *ctx,uint64_t physical,uint32_t pages) {
 struct bdw_stolen_slot *s=ctx;
 if(!validate(ctx,physical,pages))return false;
 s->returned=true;return true; /* Return child reservation only; never PMM/free. */
}
static bool child_identity(const struct bdw_stolen_slot *s) {
 const struct scanout_memory *m=s->child;
 return !m || (m->object==m && m->owner==s->parent->binding.owner && m->allocator.ctx==s &&
 m->allocator.allocate==allocate && m->allocator.validate==validate && m->allocator.release==release &&
 m->physical==s->parent->binding.layout.physical[s->index+1] && m->layout.pages==s->pages);
}
bool intel_bdw_stolen_borrow_memory(struct intel_bdw_stolen_borrow *b,const struct bdw_stolen_borrow_token *t,
 unsigned index,struct scanout_memory *m,struct scanout_memory_pool *pool,const struct scanout_memory_layout *layout,struct scanout_memory_token *out) {
 if(!auth(b,t) || b->state!=BDW_BORROW_ACTIVE || index>=2 || b->slot[index].child || !m || !layout ||
 overlap(b,sizeof(*b),m,sizeof(*m)) || overlap(b->source,sizeof(*b->source),m,sizeof(*m)) ||
 overlap(b,sizeof(*b),pool,sizeof(*pool)) || overlap(b->source,sizeof(*b->source),pool,sizeof(*pool)) ||
 overlap(b,sizeof(*b),out,sizeof(*out)) || overlap(b->source,sizeof(*b->source),out,sizeof(*out)) ||
 overlap(t,sizeof(*t),out,sizeof(*out)) || overlap(pool,sizeof(*pool),out,sizeof(*out)) || overlap(m,sizeof(*m),t,sizeof(*t)))return false;
 for(unsigned i=0;i<2;i++)if(b->slot[i].child && (overlap(m,sizeof(*m),b->slot[i].child,sizeof(*m)) ||
 overlap(out,sizeof(*out),b->slot[i].child,sizeof(*m)) || overlap(pool,sizeof(*pool),b->slot[i].child,sizeof(*m))))return false;
 if(!proof(b,BDW_BORROW_LIVE))return false;
 b->slot[index].child=m;b->creating=true;
 bool ok=scanout_memory_create(m,pool,b->binding.owner,layout,b->binding.layout.physical_end[index+1],
 (struct scanout_memory_io){&b->slot[index],allocate,validate,release},out);
 b->creating=false;
 if(!ok && !b->slot[index].claimed && b->state==BDW_BORROW_ACTIVE)b->slot[index].child=0;
 return ok;
}
bool intel_bdw_stolen_borrow_close(struct intel_bdw_stolen_borrow *b,const struct bdw_stolen_borrow_token *t) {
 if(!auth(b,t))return false;
 struct saved s=capture(b);
 if(!identity(b))return fault(b,&s,BDW_BORROW_IDENTITY);
 b->state=BDW_BORROW_CLOSING;
 for(unsigned i=0;i<2;i++) {
  if(!child_identity(&b->slot[i])){s=capture(b);return fault(b,&s,BDW_BORROW_CHILD);}
  if(b->slot[i].child)b->slot[i].child->closing=true;
 }
 for(unsigned i=0;i<2;i++)if(b->slot[i].child && (!b->slot[i].returned || b->slot[i].child->state!=MEM_RELEASED))return false;
 if(!proof(b,BDW_BORROW_RETURN))return false;
 if(!display_access_unpin(&b->exclusion)){s=capture(b);return fault(b,&s,BDW_BORROW_EXCLUSION);}
 b->source->consumer=0;b->pinned=false;b->state=BDW_BORROW_CLOSED;return true;
}
