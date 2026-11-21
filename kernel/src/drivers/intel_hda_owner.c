#include <srvros/intel_hda_owner.h>
/* One exact display-HDA endpoint. A live/faulted registration cannot be
 * duplicated by rebinding a structure copy; no forced owner replacement. */
static struct intel_hda_owner_result *active_owner;
static uint64_t owner_epochs;
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool registered(const struct intel_hda_owner_result *r) {
 return r && r==active_owner && r->object==r && r->epoch &&
  (r->state==INTEL_HDA_OWNER_NEW || r->state==INTEL_HDA_OWNER_LIVE);
}
static bool fault(struct intel_hda_owner_result *r) {
 r->state=INTEL_HDA_OWNER_FAULT;r->busy=false;
 if(r->attempts || r->pins || r->dependents)r->retained=true;
 if(!r->reason)r->reason=4;
 return false;
}
static bool clock_failed(struct intel_hda_owner_result *r,uint64_t t,unsigned why) {
 if(!r->clock_cause){r->clock_sample=t;r->clock_previous=r->last_us;r->clock_cause=why;}
 r->reason=2;return false;
}
static bool clock_ok(struct intel_hda_owner_result *r,struct intel_hda_owner_io io) {
 if(!registered(r)){r->reason=4;return false;}
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t<r->begin_us || t-r->begin_us>INTEL_HDA_OWNER_TOTAL_US)
  return clock_failed(r,t,!t?1:(t<r->last_us || t<r->begin_us)?2:4);
 if(!registered(r)){r->reason=4;return false;}
 r->last_us=t;return true;
}
bool intel_hda_owner_idle(const struct intel_hda_result *s,unsigned command) {
 if(!s || s->object!=s || !s->complete || s->retained || s->reason || !s->stable ||
    !intel_hda_endpoint_valid(&s->endpoint) || s->endpoint.command!=command ||
    s->samples!=2 || s->reads!=58 || s->maps!=1 || s->retired!=1 || s->run_mask || s->dma_mask ||
    !s->begin_us || s->end_us<=s->begin_us || s->end_us-s->begin_us>200000)return false;
 for(unsigned n=0;n<2;n++) {
  const struct intel_hda_sample *p=&s->sample[n];
  if(p->streams!=3 || p->global[0]!=0x3001 || p->global[1]!=1)return false;
  for(unsigned j=2;j<INTEL_HDA_GLOBALS;j++)if(p->global[j])return false;
  for(unsigned i=0;i<3;i++) {
   if(p->stream[i][0]!=0x40000)return false;
   for(unsigned j=1;j<5;j++)if(p->stream[i][j])return false;
  }
 }
 return true;
}
static bool endpoint_ok(const struct intel_hda_endpoint *a,const struct intel_hda_endpoint *b,unsigned command) {
 return intel_hda_endpoint_valid(b) && b->command==command && a->id==b->id && a->class_rev==b->class_rev &&
  a->subsystem==b->subsystem && a->flags==b->flags && a->pm==b->pm && a->state==b->state &&
  a->bar==b->bar && a->bar_high==b->bar_high && a->reserved==b->reserved;
}
static bool guard(struct intel_hda_owner_result *r,struct intel_hda_owner_io io,unsigned command) {
 struct intel_hda_endpoint e={0};
 if(!clock_ok(r,io))return false;
 if(!io.guard(io.ctx) || !registered(r) || !io.endpoint(io.ctx,&e) || !registered(r) ||
    !endpoint_ok(&r->phase[0].endpoint,&e,command)){r->reason=4;return false;}
 return clock_ok(r,io);
}
static bool inspect(struct intel_hda_owner_result *r,struct intel_hda_owner_io io,unsigned command) {
 if(r->phases>=3){r->reason=3;return false;}
 struct intel_hda_result *s=&r->phase[r->phases];
 if(!clock_ok(r,io))return false;
 bool observed=io.inspect(io.ctx,s);
 if(s->retained || s->maps!=s->retired)r->retained=true;
 if(!registered(r) || !observed || !intel_hda_owner_idle(s,command) || s->begin_us<r->last_us ||
    (r->phases && !endpoint_ok(&r->phase[0].endpoint,&s->endpoint,command))) {r->reason=3;return false;}
 if(!clock_ok(r,io))return false;
 if(s->end_us>r->last_us){r->reason=2;return false;}
 r->phases++;return true;
}
static bool transition(struct intel_hda_owner_result *r,struct intel_hda_owner_io io,unsigned from,unsigned to) {
 if(!guard(r,io,from))return false;
 r->retained=true;r->attempts++;
 if(!io.command16(io.ctx,(uint16_t)to)){r->reason=5;return false;}
 r->writes++;
 return guard(r,io,to);
}
/* A complete bounded registry, not a count inferred from a supplied copy. */
static bool registry(const struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *wanted) {
 if(r->pins>INTEL_HDA_OWNER_BORROWS)return false;
 const struct intel_hda_owner_borrow *b=r->dependents;bool found=!wanted;
 for(unsigned n=0;n<r->pins;n++) {
  if(!b || b->object!=b || b->owner!=r || b->epoch!=r->epoch || !b->active)return false;
  if(b==wanted)found=true;
  b=b->next;
 }
 return !b && found;
}
bool intel_hda_owner_live(const struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t) {
 return registered(r) && t && t->owner==r && t->epoch==r->epoch &&
  r->state==INTEL_HDA_OWNER_LIVE && r->inhibited && !r->retained && !r->reason;
}
bool intel_hda_owner_open(struct intel_hda_owner_result *r,struct intel_hda_owner_facts f,struct intel_hda_owner_io io,struct intel_hda_owner_token *t) {
 if(!r || r->object || active_owner || (t && overlap(r,sizeof(*r),t,sizeof(*t))))return false;
 *r=(struct intel_hda_owner_result){.object=r,.busy=true};
 r->admitted=f.compiled && f.recipe && f.efi64 && f.loader && f.profile && f.serialized && f.excluded && f.prerequisites;
 if(!t || !r->admitted || !io.inspect || !io.guard || !io.endpoint || !io.command16 || !io.now || owner_epochs==UINT64_MAX){r->reason=1;return fault(r);}
 r->io=io;r->epoch=++owner_epochs;active_owner=r;
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us || r->begin_us==UINT64_MAX){clock_failed(r,r->begin_us,!r->begin_us?1:8);return fault(r);}
 if(!inspect(r,io,6) || !transition(r,io,6,2) || !inspect(r,io,2))return fault(r);
 r->inhibited=true;r->retained=false;r->state=INTEL_HDA_OWNER_LIVE;r->busy=false;
 *t=(struct intel_hda_owner_token){r,r->epoch};return true;
}
bool intel_hda_owner_pin(struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t,struct intel_hda_owner_borrow *b) {
 if(!intel_hda_owner_live(r,t) || r->busy || !b || overlap(r,sizeof(*r),b,sizeof(*b)) || b->object ||
    r->pins>=INTEL_HDA_OWNER_BORROWS)return false;
 if(!registry(r,0)){r->reason=4;return fault(r);}
 for(const struct intel_hda_owner_borrow *p=r->dependents;p;p=p->next)if(overlap(p,sizeof(*p),b,sizeof(*b)))return false;
 r->busy=true;
 *b=(struct intel_hda_owner_borrow){.object=b,.owner=r,.epoch=r->epoch,.active=true,.next=r->dependents};
 r->dependents=b;r->pins++;
 if(!guard(r,r->io,2) || !registry(r,b))return fault(r);
 r->busy=false;return true;
}
bool intel_hda_owner_borrowed(const struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *b) {
 return r && b && b->object==b && b->owner==r && b->active &&
  intel_hda_owner_live(r,&(struct intel_hda_owner_token){r,b->epoch}) && registry(r,b);
}
bool intel_hda_owner_validate(struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *b) {
 if(!intel_hda_owner_borrowed(r,b) || r->busy)return false;
 r->busy=true;
 if(!guard(r,r->io,2) || !intel_hda_owner_borrowed(r,b))return fault(r);
 r->busy=false;return true;
}
bool intel_hda_owner_check(struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *b,struct intel_hda_result *out) {
 if(!intel_hda_owner_borrowed(r,b) || r->busy || !out || overlap(r,sizeof(*r),out,sizeof(*out)) ||
    out->object || r->checks==UINT32_MAX)return false;
 for(const struct intel_hda_owner_borrow *p=r->dependents;p;p=p->next)if(overlap(p,sizeof(*p),out,sizeof(*out)))return false;
 r->busy=true;
 if(!guard(r,r->io,2) || !intel_hda_owner_borrowed(r,b))return fault(r);
 bool observed=r->io.inspect(r->io.ctx,out);
 if(out->retained || out->maps!=out->retired)r->retained=true;
 if(!observed || !registered(r) || !intel_hda_owner_borrowed(r,b) || !intel_hda_owner_idle(out,2) ||
    out->begin_us<r->last_us || !endpoint_ok(&r->phase[0].endpoint,&out->endpoint,2)){r->reason=3;return fault(r);}
 if(!clock_ok(r,r->io))return fault(r);
 if(out->end_us>r->last_us){r->reason=2;return fault(r);}
 r->checks++;r->busy=false;return true;
}
bool intel_hda_owner_unpin(struct intel_hda_owner_result *r,struct intel_hda_owner_borrow *b) {
 if(!intel_hda_owner_borrowed(r,b) || r->busy || b->dependent || b->dependent_borrows>1 ||
    b->dependent_borrows!=b->dependent_returns)return false;
 r->busy=true;
 if(!guard(r,r->io,2) || !intel_hda_owner_borrowed(r,b))return fault(r);
 struct intel_hda_owner_borrow **link=&r->dependents;
 while(*link!=b)link=&(*link)->next;
 *link=b->next;b->next=0;r->pins--;b->active=false;r->busy=false;return true;
}
bool intel_hda_owner_close(struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t) {
 if(!intel_hda_owner_live(r,t) || r->busy)return false;
 if(!registry(r,0)){r->reason=4;return fault(r);}
 if(r->pins){if(r->blocked_closes==UINT32_MAX)return fault(r);r->blocked_closes++;return false;}
 r->busy=true;struct intel_hda_owner_io io=r->io;
 if(!transition(r,io,2,6) || !inspect(r,io,6) || !guard(r,io,6))return fault(r);
 if(!clock_ok(r,io))return fault(r);
 r->end_us=r->last_us;
 if(r->end_us<=r->begin_us){clock_failed(r,r->end_us,8);return fault(r);}
 r->restored=true;r->retained=false;r->complete=true;r->state=INTEL_HDA_OWNER_CLOSED;r->busy=false;active_owner=0;return true;
}
bool intel_hda_owner_trial(struct intel_hda_owner_result *r,struct intel_hda_owner_facts f,struct intel_hda_owner_io io) {
 struct intel_hda_owner_token t={0};
 return intel_hda_owner_open(r,f,io,&t) && intel_hda_owner_close(r,&t);
}
