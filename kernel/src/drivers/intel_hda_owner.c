#include <srvros/intel_hda_owner.h>
static bool clock_ok(struct intel_hda_owner_result *r,struct intel_hda_owner_io io) {
 uint64_t t=io.now(io.ctx);
 if(!t || t<r->last_us || t-r->begin_us>400000){r->reason=2;return false;}
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
 if(!io.guard(io.ctx) || !io.endpoint(io.ctx,&e) || !endpoint_ok(&r->phase[0].endpoint,&e,command)) {
  r->reason=4;return false;
 }
 return clock_ok(r,io);
}
static bool inspect(struct intel_hda_owner_result *r,struct intel_hda_owner_io io,unsigned command) {
 struct intel_hda_result *s=&r->phase[r->phases];
 if(!clock_ok(r,io))return false;
 bool observed=io.inspect(io.ctx,s);
 if(s->retained || s->maps!=s->retired)r->retained=true;
 if(!observed || !intel_hda_owner_idle(s,command) || s->begin_us<r->last_us ||
    (r->phases && !endpoint_ok(&r->phase[0].endpoint,&s->endpoint,command))) {r->reason=3;return false;}
 if(!clock_ok(r,io))return false;
 if(s->end_us>r->last_us){r->reason=2;return false;}
 r->phases++;return true;
}
static bool transition(struct intel_hda_owner_result *r,struct intel_hda_owner_io io,unsigned from,unsigned to) {
 if(!guard(r,io,from))return false;
 /* Mark uncertainty before dispatch. Never undo or retry an ambiguous store. */
 r->retained=true;r->attempts++;
 if(!io.command16(io.ctx,(uint16_t)to)){r->reason=5;return false;}
 r->writes++;
 if(!guard(r,io,to))return false;
 return true;
}
static uint64_t owner_epochs;
static bool fault(struct intel_hda_owner_result *r) {
 r->state=INTEL_HDA_OWNER_FAULT;
 if(r->attempts)r->retained=true;
 if(!r->reason)r->reason=4;
 return false;
}
bool intel_hda_owner_live(const struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t) {
 return r && t && r->object==r && t->owner==r && t->epoch && t->epoch==r->epoch &&
  r->state==INTEL_HDA_OWNER_LIVE && r->inhibited && !r->retained && !r->reason;
}
bool intel_hda_owner_open(struct intel_hda_owner_result *r,struct intel_hda_owner_facts f,struct intel_hda_owner_io io,struct intel_hda_owner_token *t) {
 if(!r || r->object)return false;
 *r=(struct intel_hda_owner_result){.object=r};
 r->admitted=f.compiled && f.recipe && f.efi64 && f.loader && f.profile && f.serialized && f.excluded && f.prerequisites;
 if(!t || !r->admitted || !io.inspect || !io.guard || !io.endpoint || !io.command16 || !io.now || owner_epochs==UINT64_MAX){r->reason=1;return fault(r);}
 r->io=io;r->epoch=++owner_epochs;
 r->begin_us=r->last_us=io.now(io.ctx);
 if(!r->begin_us){r->reason=2;return fault(r);}
 if(!inspect(r,io,6) || !transition(r,io,6,2) || !inspect(r,io,2))return fault(r);
 r->inhibited=true;r->retained=false;r->state=INTEL_HDA_OWNER_LIVE;
 *t=(struct intel_hda_owner_token){r,r->epoch};return true;
}
bool intel_hda_owner_pin(struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t,struct intel_hda_owner_borrow *b) {
 if(!intel_hda_owner_live(r,t) || !b || b->object || r->pins==UINT32_MAX)return false;
 *b=(struct intel_hda_owner_borrow){.object=b,.owner=r,.epoch=r->epoch};
 if(!guard(r,r->io,2))return fault(r);
 r->pins++;b->active=true;return true;
}
static bool borrowed(struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *b) {
 return b && b->object==b && b->owner==r && b->active &&
  intel_hda_owner_live(r,&(struct intel_hda_owner_token){r,b->epoch}) && r->pins;
}
bool intel_hda_owner_check(struct intel_hda_owner_result *r,const struct intel_hda_owner_borrow *b,struct intel_hda_result *out) {
 if(!borrowed(r,b) || !out || out->object ||
    out==&r->phase[0] || out==&r->phase[1] || out==&r->phase[2] || r->checks==UINT32_MAX)return false;
 if(!guard(r,r->io,2))return fault(r);
 bool observed=r->io.inspect(r->io.ctx,out);
 if(out->retained || out->maps!=out->retired)r->retained=true;
 if(!observed || !intel_hda_owner_idle(out,2) || out->begin_us<r->last_us || !endpoint_ok(&r->phase[0].endpoint,&out->endpoint,2)){r->reason=3;return fault(r);}
 if(!clock_ok(r,r->io))return fault(r);
 if(out->end_us>r->last_us){r->reason=2;return fault(r);}
 r->checks++;return true;
}
bool intel_hda_owner_unpin(struct intel_hda_owner_result *r,struct intel_hda_owner_borrow *b) {
 if(!borrowed(r,b))return false;
 if(!guard(r,r->io,2))return fault(r);
 r->pins--;b->active=false;return true;
}
bool intel_hda_owner_close(struct intel_hda_owner_result *r,const struct intel_hda_owner_token *t) {
 if(!intel_hda_owner_live(r,t))return false;
 if(r->pins){if(r->blocked_closes==UINT32_MAX)return fault(r);r->blocked_closes++;return false;}
 struct intel_hda_owner_io io=r->io;
 if(!transition(r,io,2,6) || !inspect(r,io,6) || !guard(r,io,6))return fault(r);
 r->end_us=io.now(io.ctx);
 if(r->end_us<r->last_us || r->end_us<=r->begin_us || r->end_us-r->begin_us>400000){r->reason=2;return fault(r);}
 r->restored=true;r->retained=false;r->complete=true;r->state=INTEL_HDA_OWNER_CLOSED;return true;
}
bool intel_hda_owner_trial(struct intel_hda_owner_result *r,struct intel_hda_owner_facts f,struct intel_hda_owner_io io) {
 struct intel_hda_owner_token t={0};
 return intel_hda_owner_open(r,f,io,&t) && intel_hda_owner_close(r,&t);
}
