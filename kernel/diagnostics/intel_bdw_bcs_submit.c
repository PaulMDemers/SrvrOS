#include <srvros/intel_bdw_bcs_submit.h>
#include <srvros/intel_bdw_bcs_port.h>

/* Stable even if a failed caller loses its descriptor object. */
static const char poster;

#ifdef SRVROS_BCS_SUBMIT_HOST_TEST
void intel_bdw_bcs_fixture_new_device(void)
{
 (void)intel_bdw_bcs_port_release(&poster);
}
#endif

static bool binding_same(const struct bcs_submit_binding *a,const struct bcs_submit_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime && a->context==b->context &&
 a->root==b->root && a->epoch==b->epoch && a->sequence==b->sequence && a->descriptor==b->descriptor &&
 a->ring_gpu==b->ring_gpu && a->ring_physical==b->ring_physical && a->context_gpu==b->context_gpu && a->context_physical==b->context_physical &&
 a->completion_gpu==b->completion_gpu && a->completion_physical==b->completion_physical &&
 a->aperture_bytes==b->aperture_bytes && a->fence_begin==b->fence_begin && a->fence_end==b->fence_end &&
 a->prepare==b->prepare && a->gt==b->gt && a->hda==b->hda && a->outer==b->outer && a->admitted_at==b->admitted_at &&
 a->vendor==b->vendor && a->device==b->device && a->tail==b->tail;
}
static bool io_same(const struct bcs_submit_io *a,const struct bcs_submit_io *b) {
 return a->ctx==b->ctx && a->ownership==b->ownership && a->write==b->write && a->now==b->now;
}
static bool span(uint64_t p,uint64_t bytes) {return p && !(p&4095) && p<=0x100000000ull-bytes;}
static bool disjoint(uint64_t a,uint64_t an,uint64_t b,uint64_t bn) {return a+an<=b || b+bn<=a;}
static bool valid(const struct bcs_submit_source *s) {
 if(!s || s->object!=s)return false;
 const struct bcs_submit_binding *b=&s->binding;
 if(!b->owner || !b->provider || !b->lifetime || !b->context || !span(b->root,4096) || !b->epoch || !b->sequence ||
 b->vendor!=0x8086 || b->device!=0x1626 || b->tail!=24 || !(b->descriptor&1) ||
 (b->descriptor&0xfffff000ull)!=b->context_gpu || !(b->descriptor>>32) || (b->descriptor>>32)>0x1fffff ||
 (b->descriptor&0xec0) || !b->aperture_bytes || b->aperture_bytes>0x100000000ull || (b->aperture_bytes&4095) ||
 b->fence_begin>=b->fence_end || ((b->fence_begin|b->fence_end)&4095) || b->fence_end>b->aperture_bytes ||
 !b->outer || b->gt<b->outer || b->hda<b->outer || b->prepare<b->gt || b->prepare<b->hda || b->admitted_at<b->prepare)return false;
 const uint64_t gpu[]={b->ring_gpu,b->context_gpu,b->completion_gpu},physical[]={b->ring_physical,b->context_physical,b->completion_physical},bytes[]={4096,8192,4096};
 for(unsigned i=0;i<3;i++) {
  if(!span(gpu[i],bytes[i]) || !span(physical[i],bytes[i]) || gpu[i]+bytes[i]>b->aperture_bytes ||
   !disjoint(gpu[i],bytes[i],b->fence_begin,b->fence_end-b->fence_begin) ||
   !disjoint(physical[i],bytes[i],b->root,4096))return false;
  for(unsigned j=0;j<i;j++)if(!disjoint(gpu[i],bytes[i],gpu[j],bytes[j]) || !disjoint(physical[i],bytes[i],physical[j],bytes[j]))return false;
 }
 return true;
}
static bool progress_same(const struct bcs_submit_progress *a,const struct bcs_submit_progress *b) {
 return a->begin==b->begin && a->last==b->last && a->state==b->state && a->proofs==b->proofs &&
 a->attempts==b->attempts && a->writes==b->writes && a->triggered==b->triggered && a->uncertain==b->uncertain;
}
static bool live(const struct intel_bdw_bcs_submit *r) {
 return r->object==r && r->busy && !r->issued && !r->retained && !r->reason && !r->end &&
 !r->clock_at && !r->clock_previous && !r->clock_reason && valid(r->source) && r->source->consumer==r &&
 binding_same(&r->binding,&r->source->binding) && io_same(&r->io,&r->original_io);
}
/* Only binding, callback and progress DATA are copied; no ownership object. */
struct saved {
 struct bcs_submit_source *source;
 struct bcs_submit_binding binding;
 struct bcs_submit_io io;
 struct bcs_submit_progress p;
 uint32_t words[4];
};
static struct saved capture(const struct intel_bdw_bcs_submit *r) {
 struct saved s={.source=r->source,.binding=r->binding,.io=r->io,.p=r->p};
 for(unsigned i=0;i<4;i++)s.words[i]=r->words[i];
 return s;
}
static bool unchanged(const struct intel_bdw_bcs_submit *r,const struct saved *s) {
 if(!live(r) || r->source!=s->source || !binding_same(&r->binding,&s->binding) || !io_same(&r->io,&s->io) || !progress_same(&r->p,&s->p))return false;
 for(unsigned i=0;i<4;i++)if(r->words[i]!=s->words[i])return false;
 return true;
}
static bool fail(struct intel_bdw_bcs_submit *r,struct bcs_submit_source *s,unsigned why) {
 r->object=r;r->source=s;s->object=s;s->consumer=r;
 r->reason|=why;r->p.state=BCS_PORT_FAULT;r->issued=false;r->retained=true;r->end=0;return false;
}
static bool tick(struct intel_bdw_bcs_submit *r) {
 struct bcs_submit_source *source=r->source;
 if(!live(r))return fail(r,source,BCS_PORT_SOURCE);
 struct saved s=capture(r);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s)){r->p=s.p;return fail(r,s.source,BCS_PORT_SOURCE);}
 unsigned why=!at?1:at<r->p.last?2:at-r->binding.prepare>INTEL_DIAGNOSTIC_PREPARE_US?3:
 at-r->binding.gt>INTEL_DIAGNOSTIC_GT_TOTAL_US?4:at-r->binding.hda>INTEL_DIAGNOSTIC_HDA_US?5:
 at-r->binding.outer>INTEL_DIAGNOSTIC_OUTER_US?6:at-r->p.begin>BCS_SUBMIT_US?7:0;
 if(why){r->clock_at=at;r->clock_previous=r->p.last;r->clock_reason=why;return fail(r,source,BCS_PORT_CLOCK);}
 r->p.last=at;return true;
}
/* Broadwell PRM 2c pp.435-438. The first DWORD marks the selected list
 * valid. Only the fourth submits it. After submission the single context
 * can be waiting, executing in element zero, or already retired. */
static bool queue_matches(const struct bcs_submit_proof *proof, uint32_t *opening)
{
    uint32_t status = proof->queue_status;
    unsigned prefix = proof->prefix;
    const uint32_t full = 1u << 2, list1 = 1u << 3, list0 = 1u << 4;
    unsigned active = (status >> 14) & 3u;
    if (status == UINT32_MAX || (status & full)) return false;
    if (prefix == 0) *opening = status & 3u;

    unsigned selected = (*opening >> 1) & 1u;
    uint32_t valid = status & (list0 | list1);
    uint32_t expected = selected ? list1 : list0;
    if (prefix < 4)
        return !active && (status & 3u) == *opening && valid == (prefix ? expected : 0);

    if (((status >> 1) & 1u) == selected || (valid && valid != expected)) return false;
    return !active || (active == 1 && valid == expected && (status & 1u) == selected &&
                       proof->running_context == (uint32_t)(proof->descriptor >> 32));
}
static bool ownership(struct intel_bdw_bcs_submit *r,uint32_t *opening) {
 if(!tick(r))return false;
 struct saved s=capture(r);struct bcs_submit_proof p={0};
 bool ok=r->io.ownership(r->io.ctx,r->source,r->p.writes,&p);
 if(!unchanged(r,&s)){r->p=s.p;return fail(r,s.source,BCS_PORT_SOURCE);}
 if(!tick(r))return false;
 const struct bcs_submit_binding *b=&r->binding;
 if(!ok || p.source!=r->source || p.owner!=b->owner || p.provider!=b->provider || p.lifetime!=b->lifetime ||
 p.context!=b->context || p.root!=b->root || p.epoch!=b->epoch || p.sequence!=b->sequence || p.descriptor!=b->descriptor ||
 p.requirements!=BCS_SUBMIT_REQUIRED || p.prefix!=r->p.writes || p.checked_at<s.p.last || p.checked_at>r->p.last ||
 !queue_matches(&p,opening))
  return fail(r,r->source,BCS_PORT_OWNERSHIP);
 ++r->p.proofs;return true;
}
static bool write_word(struct intel_bdw_bcs_submit *r,unsigned i,uint32_t *opening) {
 if(i>=4 || r->p.attempts!=i || r->p.writes!=i)return fail(r,r->source,BCS_PORT_RESULT);
 if(!ownership(r,opening))return false;
 ++r->p.attempts;struct saved s=capture(r);
 enum intel_gt_write w=r->io.write(r->io.ctx,BCS_SUBMIT_PORT,r->words[i]);
 bool intact=unchanged(r,&s);r->p=s.p;
 if(w==GT_WRITE_DONE){++r->p.writes;if(i==3)r->p.triggered=1;}
 else if(w!=GT_WRITE_NONE)r->p.uncertain=true;
 if(!intact)return fail(r,s.source,BCS_PORT_SOURCE);
 if(w!=GT_WRITE_DONE)return fail(r,s.source,BCS_PORT_WRITE);
 return tick(r);
}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
bool intel_bdw_bcs_submit_run(struct intel_bdw_bcs_submit *r,struct bcs_submit_source *source,struct bcs_submit_io io) {
 if(!intel_bdw_bcs_port_claim(&poster))return false;
 if(!r || r->object || r->source || r->busy || r->issued || r->retained || r->reason || r->end || r->p.state ||
 r->p.begin || r->p.last || r->p.proofs || r->p.attempts || r->p.writes || r->p.triggered || r->p.uncertain ||
 r->words[0] || r->words[1] || r->words[2] || r->words[3] ||
 r->clock_at || r->clock_previous || r->clock_reason || !valid(source) || source->consumer ||
 io.ctx!=source->binding.provider || !io.ownership || !io.write || !io.now || overlap(r,sizeof(*r),source,sizeof(*source))) {
  intel_bdw_bcs_port_release(&poster);
  return false;
 }
 *r=(struct intel_bdw_bcs_submit){.object=r,.source=source,.binding=source->binding,.io=io,.original_io=io,
 .p={.begin=source->binding.admitted_at,.last=source->binding.admitted_at,.state=BCS_PORT_WRITING},.busy=true};
 source->consumer=r;
 r->words[0]=r->words[1]=0;r->words[2]=(uint32_t)(r->binding.descriptor>>32);r->words[3]=(uint32_t)r->binding.descriptor;
 uint32_t opening=0;
 for(unsigned i=0;i<4;i++)if(!write_word(r,i,&opening))goto done;
 r->p.state=BCS_PORT_VERIFYING;if(!ownership(r,&opening))goto done;
 if(r->p.attempts!=4 || r->p.writes!=4 || r->p.triggered!=1 || r->p.proofs!=5 || r->p.uncertain ||
 r->words[0] || r->words[1] || r->words[2]!=(uint32_t)(r->binding.descriptor>>32) || r->words[3]!=(uint32_t)r->binding.descriptor) {
  fail(r,source,BCS_PORT_RESULT);goto done;
 }
 if(r->p.last<=r->p.begin){r->clock_at=r->p.last;r->clock_previous=r->p.last;r->clock_reason=8;fail(r,source,BCS_PORT_CLOCK);goto done;}
 r->p.state=BCS_PORT_ISSUED;r->issued=true;r->end=r->p.last;
done:
 r->busy=false;
 /* A clean four-word issue permits the next provider to check queue capacity.
  * A known empty prefix is also reusable. Unconfirmed post-trigger state or
  * a partial descriptor quarantines the FIFO even for a different source. */
 bool issued=r->issued;
 if(issued || (!r->p.writes && !r->p.uncertain))
  intel_bdw_bcs_port_release(&poster);
 return issued;
}
