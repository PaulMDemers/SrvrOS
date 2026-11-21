#include <srvros/intel_bdw_tlb.h>
const uint32_t intel_bdw_tlb_registers[5]={0x4260,0x4264,0x4270,0x4268,0x426c};
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool same(const struct intel_bdw_tlb_binding *a,const struct intel_bdw_tlb_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime &&
 a->epoch==b->epoch && a->sequence==b->sequence && a->root==b->root && a->physical==b->physical &&
 a->aperture_bytes==b->aperture_bytes && a->fence_begin==b->fence_begin && a->fence_end==b->fence_end &&
 a->prepare_begin==b->prepare_begin && a->gt_begin==b->gt_begin && a->hda_begin==b->hda_begin && a->outer_begin==b->outer_begin &&
 a->vendor==b->vendor && a->device==b->device && a->first==b->first && a->pages==b->pages;
}
static bool valid_source(const struct intel_bdw_tlb_source *input) {
 if(!input || input->object!=input)return false;
 const struct intel_bdw_tlb_binding *s=&input->binding;
 if(!s->owner || !s->provider || !s->lifetime || !s->epoch || !s->sequence ||
    s->vendor!=0x8086 || s->device!=0x1626 || !s->root || (s->root&4095) || !s->physical || (s->physical&4095) ||
    s->physical>=0x100000000ull || !s->pages || s->pages>1536 ||
    (uint64_t)s->pages*4096>0x100000000ull-s->physical || !s->aperture_bytes ||
    (s->aperture_bytes&4095) || s->aperture_bytes>0x100000000ull ||
    s->first>=s->aperture_bytes/4096 || s->pages>s->aperture_bytes/4096-s->first ||
    s->fence_begin>=s->fence_end || (s->fence_begin&4095) || (s->fence_end&4095) ||
    s->fence_end>s->aperture_bytes || !s->outer_begin || s->hda_begin<s->outer_begin || s->gt_begin<s->outer_begin ||
    s->prepare_begin<s->gt_begin || s->prepare_begin<s->hda_begin)return false;
 uint64_t begin=(uint64_t)s->first*4096,end=begin+(uint64_t)s->pages*4096;
 return begin>=s->fence_end || s->fence_begin>=end;
}
static bool live(const struct intel_bdw_tlb *r) {
 return r && r->object==r && r->input && r->input->consumer==r && r->busy && !r->complete && !r->reason &&
 !r->end && !r->clock_at && !r->clock_previous && !r->clock_reason && !r->retained &&
 r->state>=BDW_TLB_OPENING && r->state<=BDW_TLB_POLLING && valid_source(r->input) && same(&r->input->binding,&r->binding) &&
 r->index<5 && r->attempts<=5 && r->writes<=r->attempts && r->completed<=r->writes &&
 !(r->requested_mask&~31u) && !(r->completed_mask&~r->requested_mask) && r->reads<=5+5*INTEL_BDW_TLB_POLLS &&
 r->io.ctx==r->original_io.ctx && r->io.prerequisite==r->original_io.prerequisite && r->io.read==r->original_io.read &&
 r->io.write==r->original_io.write && r->io.now==r->original_io.now && r->io.pause==r->original_io.pause;
}
/* Copy only scalar progress/binding data, never an ownership object. Every
 * external callback is forbidden to rewrite the protocol's original progress. */
struct progress {
 const struct intel_bdw_tlb_source *input;
 struct intel_bdw_tlb_binding binding;
 uint64_t begin,last;
 unsigned state,index,reads,attempts,writes,completed,proofs,requested,done;
 uint32_t initial[5],final[5],polls[5];
 bool uncertain;
};
static struct progress capture(const struct intel_bdw_tlb *r) {
 struct progress p={.input=r->input,.binding=r->binding,.begin=r->begin,.last=r->last,
 .state=r->state,.index=r->index,.reads=r->reads,.attempts=r->attempts,.writes=r->writes,
 .completed=r->completed,.proofs=r->proofs,.requested=r->requested_mask,.done=r->completed_mask,.uncertain=r->uncertain};
 for(unsigned i=0;i<5;i++){p.initial[i]=r->initial[i];p.final[i]=r->final[i];p.polls[i]=r->polls[i];}
 return p;
}
static bool unchanged(const struct intel_bdw_tlb *r,const struct progress *p) {
 if(!live(r) || r->input!=p->input || !same(&r->binding,&p->binding) || r->begin!=p->begin || r->last!=p->last ||
 r->state!=p->state || r->index!=p->index || r->reads!=p->reads || r->attempts!=p->attempts || r->writes!=p->writes ||
 r->completed!=p->completed || r->proofs!=p->proofs || r->requested_mask!=p->requested || r->completed_mask!=p->done || r->uncertain!=p->uncertain)return false;
 for(unsigned i=0;i<5;i++)if(r->initial[i]!=p->initial[i] || r->final[i]!=p->final[i] || r->polls[i]!=p->polls[i])return false;
 return true;
}
static bool fail(struct intel_bdw_tlb *r,unsigned reason) {
 r->reason|=reason;r->state=BDW_TLB_FAULT;r->complete=false;r->end=0;
 r->retained=r->attempts || (reason&(BDW_TLB_SOURCE|BDW_TLB_CLOCK|BDW_TLB_STATE));return false;
}
static bool tick(struct intel_bdw_tlb *r) {
 if(!live(r))return fail(r,BDW_TLB_SOURCE);
 struct progress before=capture(r);
 uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&before))return fail(r,BDW_TLB_SOURCE);
 unsigned why=!at?BDW_TLB_ZERO:at<r->last?BDW_TLB_BACKWARD:
 at-r->binding.prepare_begin>INTEL_BDW_TLB_PREPARE_US?BDW_TLB_PREPARE_EXPIRED:
 at-r->binding.gt_begin>INTEL_BDW_TLB_GT_US?BDW_TLB_GT_EXPIRED:
 at-r->binding.hda_begin>INTEL_BDW_TLB_HDA_US?BDW_TLB_HDA_EXPIRED:
 at-r->binding.outer_begin>INTEL_BDW_TLB_OUTER_US?BDW_TLB_OUTER_EXPIRED:0;
 if(why){r->clock_at=at;r->clock_previous=r->last;r->clock_reason=why;return fail(r,BDW_TLB_CLOCK);}
 if(!r->begin)r->begin=at;
 r->last=at;return true;
}
static bool prerequisite(struct intel_bdw_tlb *r) {
 if(!tick(r))return false;
 struct intel_bdw_tlb_proof p={0};uint64_t before=r->last;
 struct progress saved=capture(r);
 bool ok=r->io.prerequisite(r->io.ctx,r->input,&p);
 if(!unchanged(r,&saved))return fail(r,BDW_TLB_SOURCE);
 if(!tick(r))return false;
 if(!ok || p.source!=r->input || p.owner!=r->binding.owner || p.provider!=r->binding.provider || p.lifetime!=r->binding.lifetime ||
    p.epoch!=r->binding.epoch || p.sequence!=r->binding.sequence || p.root!=r->binding.root || p.checked_at<before || p.checked_at>r->last ||
    p.engines!=31 || p.pipelines_flushed!=31 || p.memory_retired!=31)return fail(r,BDW_TLB_PREREQUISITE);
 ++r->proofs;return true;
}
static bool read_control(struct intel_bdw_tlb *r,uint32_t *out) {
 if(!prerequisite(r))return false;
 struct progress saved=capture(r);uint32_t value=UINT32_MAX;
 bool ok=r->io.read(r->io.ctx,r->index,intel_bdw_tlb_registers[r->index],&value);
 if(!unchanged(r,&saved))return fail(r,BDW_TLB_SOURCE);
 if(ok){++r->reads;*out=value;}
 if(!tick(r))return false;
 return ok && !(*out&~1u)?true:fail(r,BDW_TLB_READ);
}
static bool request(struct intel_bdw_tlb *r) {
 if(!prerequisite(r))return false;
 unsigned index=r->index;++r->attempts;struct progress saved=capture(r);
 enum intel_gt_write issued=r->io.write(r->io.ctx,index,intel_bdw_tlb_registers[index],1);
 bool intact=unchanged(r,&saved);
 /* Count the possible store before any callback/state validation. */
 if(issued==GT_WRITE_DONE) {++r->writes;r->requested_mask|=1u<<index;}
 else if(issued!=GT_WRITE_NONE)r->uncertain=true;
 if(!intact || !live(r))return fail(r,BDW_TLB_SOURCE);
 if(!tick(r))return false;
 return issued==GT_WRITE_DONE?true:fail(r,BDW_TLB_WRITE);
}
bool intel_bdw_tlb_invalidate(struct intel_bdw_tlb *r,struct intel_bdw_tlb_source *s,struct intel_bdw_tlb_io io) {
 if(!r || r->object || !valid_source(s) || s->consumer || io.ctx!=s->binding.provider || !io.prerequisite || !io.read || !io.write || !io.now || !io.pause ||
    overlap(r,sizeof(*r),s,sizeof(*s)))return false;
 *r=(struct intel_bdw_tlb){.object=r,.input=s,.binding=s->binding,.io=io,.original_io=io,.last=s->binding.prepare_begin,.state=BDW_TLB_OPENING,.busy=true};
 s->consumer=r;
 for(unsigned i=0;i<5;i++) {
  r->state=BDW_TLB_READING;r->index=i;
  if(!read_control(r,&r->initial[i]))goto done;
  if(r->initial[i]){fail(r,BDW_TLB_BUSY);goto done;}
 }
 for(unsigned i=0;i<5;i++) {
  r->state=BDW_TLB_WRITING;r->index=i;
  if(!request(r))goto done;
  r->state=BDW_TLB_POLLING;
  for(unsigned n=0;n<INTEL_BDW_TLB_POLLS;n++) {
   ++r->polls[i];
   if(!read_control(r,&r->final[i]))goto done;
   if(!r->final[i]){++r->completed;r->completed_mask|=1u<<i;break;}
   if(n+1<INTEL_BDW_TLB_POLLS) {
    struct progress saved=capture(r);
    r->io.pause(r->io.ctx);
    if(!unchanged(r,&saved)){fail(r,BDW_TLB_SOURCE);goto done;}
    if(!tick(r))goto done;
   }
  }
  if(r->final[i]){fail(r,BDW_TLB_PENDING);goto done;}
 }
 if(!prerequisite(r))goto done;
 if(r->last<=r->begin){r->clock_at=r->last;r->clock_previous=r->begin;r->clock_reason=BDW_TLB_FROZEN;fail(r,BDW_TLB_CLOCK);goto done;}
 if(r->attempts!=5 || r->writes!=5 || r->completed!=5 || r->requested_mask!=31 || r->completed_mask!=31 || r->uncertain){fail(r,BDW_TLB_STATE);goto done;}
 unsigned polls=0;
 for(unsigned i=0;i<5;i++) {
  if(r->initial[i] || r->final[i] || !r->polls[i] || r->polls[i]>INTEL_BDW_TLB_POLLS){fail(r,BDW_TLB_STATE);goto done;}
  polls+=r->polls[i];
 }
 if(r->reads!=5+polls || r->proofs!=r->reads+r->attempts+1){fail(r,BDW_TLB_STATE);goto done;}
 r->end=r->last;r->state=BDW_TLB_COMPLETE;r->complete=true;
 done:r->busy=false;return r->complete;
}
