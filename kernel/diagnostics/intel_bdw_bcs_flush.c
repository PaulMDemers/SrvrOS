#include <srvros/intel_bdw_bcs_flush.h>

bool intel_bdw_bcs_flush_packet(uint64_t address,uint64_t nonce,uint32_t out[BCS_FLUSH_DWORDS]) {
 if(!out || !nonce || nonce==UINT64_MAX || !address || address>=0x100000000ull || (address&63))return false;
 /* BDW PRM2a pp834-837: MI opcode26, post-sync immediate QWord, BCS TLB
  * invalidate, length5-2; GGTT selector in DW1. Direct address, no HWS index.
  * MI_NOOP makes the submission tail24 bytes, not an unaligned20 bytes. */
 out[0]=0x13044003u;out[1]=(uint32_t)address|4u;out[2]=0;
 out[3]=(uint32_t)nonce;out[4]=(uint32_t)(nonce>>32);out[5]=0;
 return true;
}
/* Gen8 explicit command barriers for a private 32-bit per-process GPU
 * address. Do not use these packets with the GGTT-only bootstrap context. */
bool intel_bdw_ppgtt_barrier(enum bdw_barrier_engine engine, uint64_t address, uint64_t marker,
                             uint32_t packet[BCS_FLUSH_DWORDS])
{
    if (!packet || (unsigned)engine >= BDW_BARRIER_ENGINES || !marker || marker == UINT64_MAX ||
        !address || address >= (1ull << 32) || (address & 63u))
        return false;

    if (engine == BDW_BARRIER_RCS) {
        /* PIPE_CONTROL: CS stall, QWord post-sync, render target, depth and
         * DC cache flush, plus generic flush enable. No global-address bit. */
        packet[0] = 0x7a000004u;
        packet[1] = (1u<<20) | (1u<<14) | (1u<<12) | (1u<<7) | (1u<<5) | 1u;
        packet[2] = (uint32_t)address;
        packet[3] = 0;
        packet[4] = (uint32_t)marker;
        packet[5] = (uint32_t)(marker >> 32);
    } else {
        /* MI_FLUSH_DW, post-sync immediate QWord, direct PPGTT address,
         * followed by MI_NOOP. This barrier does not invalidate translations. */
        packet[0] = 0x13004003u;
        packet[1] = (uint32_t)address;
        packet[2] = 0;
        packet[3] = (uint32_t)marker;
        packet[4] = (uint32_t)(marker >> 32);
        packet[5] = 0;
    }
    return true;
}
bool intel_bdw_ppgtt_wait_equal(uint64_t address, uint32_t release_value, uint32_t packet[4])
{
    if (!packet || !release_value || !address || address >= (1ull << 32) || (address & 3u))
        return false;
    /* MI_SEMAPHORE_WAIT: PPGTT, memory polling, SAD == SDD, four DWORDs. */
    packet[0] = (0x1cu << 23) | (1u << 15) | (4u << 12) | 2u;
    packet[1] = release_value;
    packet[2] = (uint32_t)address;
    packet[3] = 0;
    return true;
}

unsigned intel_bdw_ppgtt_drain_and_hold(enum bdw_barrier_engine engine, uint64_t result,
                                        uint64_t gate, uint64_t nonce,
                                        uint32_t packet[BDW_GATE_WORDS_MAX])
{
    if (!result || !gate || result == gate || ((result | gate) & 4095u) ||
        (result | gate) >= (1ull << 32) || !packet ||
        !intel_bdw_ppgtt_barrier(engine, result + BDW_QUEUE_DRAIN, nonce, packet))
        return 0;
    unsigned used = BCS_FLUSH_DWORDS;
    if (engine == BDW_BARRIER_RCS) {
        if (!intel_bdw_ppgtt_wait_equal(gate + BDW_GATE_RENDER_START, 1, packet + used) ||
            !intel_bdw_ppgtt_barrier(engine, result + BDW_QUEUE_FINAL_FLUSH, nonce, packet + used + 4))
            return 0;
        used += 4 + BCS_FLUSH_DWORDS;
    }
    if (!intel_bdw_ppgtt_wait_equal(gate, 1, packet + used) ||
        !intel_bdw_ppgtt_barrier(engine, result + BDW_QUEUE_RESUMED, ~nonce, packet + used + 4))
        return 0;
    return used + 4 + BCS_FLUSH_DWORDS;
}

static bool binding_same(const struct bcs_flush_binding *a,const struct bcs_flush_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime && a->context==b->context &&
 a->epoch==b->epoch && a->sequence==b->sequence && a->root==b->root && a->nonce==b->nonce &&
 a->ring_gpu==b->ring_gpu && a->ring_physical==b->ring_physical &&
 a->completion_gpu==b->completion_gpu && a->completion_physical==b->completion_physical &&
 a->aperture_bytes==b->aperture_bytes && a->fence_begin==b->fence_begin && a->fence_end==b->fence_end &&
 a->prepare_begin==b->prepare_begin && a->gt_begin==b->gt_begin && a->hda_begin==b->hda_begin &&
 a->outer_begin==b->outer_begin && a->vendor==b->vendor && a->device==b->device;
}
static bool page(uint64_t p) {return p && !(p&4095) && p<0x100000000ull;}
static bool outside(uint64_t p,uint64_t first,uint64_t end) {return p+4096<=first || p>=end;}
static bool valid(const struct bcs_flush_source *s) {
 if(!s || s->object!=s)return false;
 const struct bcs_flush_binding *b=&s->binding;
 return b->owner && b->provider && b->lifetime && b->context && b->epoch && b->sequence &&
 b->root && !(b->root&4095) && b->nonce && b->nonce!=UINT64_MAX && b->vendor==0x8086 && b->device==0x1626 &&
 page(b->ring_gpu) && page(b->completion_gpu) && page(b->ring_physical) && page(b->completion_physical) &&
 b->ring_gpu!=b->completion_gpu && b->ring_physical!=b->completion_physical &&
 b->aperture_bytes && b->aperture_bytes<=0x100000000ull &&
 b->ring_gpu+4096<=b->aperture_bytes && b->completion_gpu+4096<=b->aperture_bytes &&
 b->fence_begin<b->fence_end && b->fence_end<=b->aperture_bytes &&
 outside(b->ring_gpu,b->fence_begin,b->fence_end) && outside(b->completion_gpu,b->fence_begin,b->fence_end) &&
 b->outer_begin && b->gt_begin>=b->outer_begin && b->hda_begin>=b->outer_begin &&
 b->prepare_begin>=b->gt_begin && b->prepare_begin>=b->hda_begin;
}
static bool io_same(const struct bcs_flush_io *a,const struct bcs_flush_io *b) {
 return a->ctx==b->ctx && a->ownership==b->ownership && a->read==b->read && a->publish==b->publish &&
 a->submit==b->submit && a->now==b->now && a->pause==b->pause;
}
static bool packet_same(const uint32_t *a,const uint32_t *b) {
 for(unsigned i=0;i<BCS_FLUSH_DWORDS;i++)if(a[i]!=b[i])return false;
 return true;
}
static bool progress_same(const struct bcs_flush_progress *a,const struct bcs_flush_progress *b) {
 return a->begin==b->begin && a->last==b->last && a->submit_begin==b->submit_begin &&
 a->initial==b->initial && a->observed==b->observed && a->state==b->state && a->proofs==b->proofs &&
 a->reads==b->reads && a->publish_attempts==b->publish_attempts && a->published==b->published &&
 a->submit_attempts==b->submit_attempts && a->submitted==b->submitted && a->polls==b->polls && a->uncertain==b->uncertain;
}
static bool live(const struct intel_bdw_bcs_flush *r) {
 return r && r->object==r && r->busy && !r->complete && !r->retained && !r->reason && !r->end &&
 !r->clock_at && !r->clock_previous && !r->clock_reason && valid(r->source) && r->source->consumer==r &&
 binding_same(&r->binding,&r->source->binding) && io_same(&r->io,&r->original_io);
}
struct saved_callback {
 const struct bcs_flush_source *source;
 struct bcs_flush_binding binding;
 struct bcs_flush_io io;
 struct bcs_flush_progress progress;
 uint32_t packet[BCS_FLUSH_DWORDS];
};
static struct saved_callback capture(const struct intel_bdw_bcs_flush *r) {
 struct saved_callback s={.source=r->source,.binding=r->binding,.io=r->io,.progress=r->p};
 for(unsigned i=0;i<BCS_FLUSH_DWORDS;i++)s.packet[i]=r->packet[i];
 return s;
}
static bool unchanged(const struct intel_bdw_bcs_flush *r,const struct saved_callback *s) {
 return live(r) && r->source==s->source && binding_same(&r->binding,&s->binding) &&
 io_same(&r->io,&s->io) && progress_same(&r->p,&s->progress) && packet_same(r->packet,s->packet);
}
static bool fail(struct intel_bdw_bcs_flush *r,unsigned reason) {
 r->reason|=reason;r->p.state=BCS_FLUSH_FAULT;r->complete=false;r->retained=true;r->end=0;return false;
}
static bool tick(struct intel_bdw_bcs_flush *r) {
 if(!live(r))return fail(r,BCS_FLUSH_SOURCE);
 struct saved_callback s=capture(r);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,BCS_FLUSH_SOURCE);
 unsigned why=!at?BCS_CLOCK_ZERO:at<r->p.last?BCS_CLOCK_BACKWARD:
 at-r->binding.prepare_begin>BCS_FLUSH_PREPARE_US?BCS_CLOCK_PREPARE:
 at-r->binding.gt_begin>BCS_FLUSH_GT_US?BCS_CLOCK_GT:
 at-r->binding.hda_begin>BCS_FLUSH_HDA_US?BCS_CLOCK_HDA:
 at-r->binding.outer_begin>BCS_FLUSH_OUTER_US?BCS_CLOCK_OUTER:
 r->p.submit_begin && at-r->p.submit_begin>BCS_FLUSH_WAIT_US?BCS_CLOCK_WAIT:0;
 if(why){r->clock_at=at;r->clock_previous=r->p.last;r->clock_reason=why;return fail(r,BCS_FLUSH_CLOCK);}
 if(!r->p.begin)r->p.begin=at;
 r->p.last=at;return true;
}
static bool ownership(struct intel_bdw_bcs_flush *r) {
 if(!tick(r))return false;
 struct saved_callback s=capture(r);struct bcs_flush_proof p={0};
 bool ok=r->io.ownership(r->io.ctx,r->source,&p);
 if(!unchanged(r,&s))return fail(r,BCS_FLUSH_SOURCE);
 if(!tick(r))return false;
 const struct bcs_flush_binding *b=&r->binding;
 if(!ok || p.source!=r->source || p.owner!=b->owner || p.provider!=b->provider || p.lifetime!=b->lifetime ||
 p.context!=b->context || p.epoch!=b->epoch || p.sequence!=b->sequence || p.root!=b->root ||
 p.requirements!=BCS_FLUSH_REQUIRED || p.checked_at<s.progress.last || p.checked_at>r->p.last)return fail(r,BCS_FLUSH_OWNERSHIP);
 ++r->p.proofs;return true;
}
static bool read_value(struct intel_bdw_bcs_flush *r,uint64_t *out) {
 if(!ownership(r))return false;
 struct saved_callback s=capture(r);uint64_t value=UINT64_MAX;
 bool ok=r->io.read(r->io.ctx,r->binding.completion_gpu,&value);
 if(!unchanged(r,&s))return fail(r,BCS_FLUSH_SOURCE);
 if(ok)++r->p.reads;
 if(!tick(r))return false;
 if(!ok)return fail(r,BCS_FLUSH_READ);
 *out=value;return true;
}
static bool write_phase(struct intel_bdw_bcs_flush *r,bool submission) {
 if(!ownership(r))return false;
 if(submission){++r->p.submit_attempts;r->p.submit_begin=r->p.last;}
 else ++r->p.publish_attempts;
 struct saved_callback s=capture(r);
 enum intel_gt_write w=submission?r->io.submit(r->io.ctx,r->binding.context,r->binding.ring_gpu,BCS_FLUSH_DWORDS*4):
 r->io.publish(r->io.ctx,r->binding.ring_gpu,r->packet,BCS_FLUSH_DWORDS);
 bool intact=unchanged(r,&s);
 if(w==GT_WRITE_DONE){if(submission)++r->p.submitted;else ++r->p.published;}
 else if(w!=GT_WRITE_NONE)r->p.uncertain=true;
 if(!intact)return fail(r,BCS_FLUSH_SOURCE);
 if(w!=GT_WRITE_DONE)return fail(r,submission?BCS_FLUSH_SUBMIT:BCS_FLUSH_PUBLISH);
 return tick(r);
}
static bool delay(struct intel_bdw_bcs_flush *r) {
 if(!tick(r))return false;
 struct saved_callback s=capture(r);r->io.pause(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,BCS_FLUSH_SOURCE);
 return tick(r);
}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
bool intel_bdw_bcs_flush_run(struct intel_bdw_bcs_flush *r,struct bcs_flush_source *source,struct bcs_flush_io io) {
 if(!r || r->object || r->p.state || r->busy || r->complete || r->retained || r->reason || r->end ||
 r->clock_at || r->clock_previous || r->clock_reason || !valid(source) || source->consumer ||
 io.ctx!=source->binding.provider || !io.ownership || !io.read || !io.publish || !io.submit || !io.now || !io.pause ||
 overlap(r,sizeof(*r),source,sizeof(*source)))return false;
 *r=(struct intel_bdw_bcs_flush){.object=r,.source=source,.binding=source->binding,.io=io,.original_io=io,
 .busy=true,.p={.state=BCS_FLUSH_OPENING,.last=source->binding.prepare_begin}};
 source->consumer=r;
 if(!intel_bdw_bcs_flush_packet(r->binding.completion_gpu,r->binding.nonce,r->packet)){
  fail(r,BCS_FLUSH_RESULT);goto done;
 }
 if(!read_value(r,&r->p.initial))goto done;
 if(r->p.initial){fail(r,BCS_FLUSH_STALE);goto done;}
 r->p.state=BCS_FLUSH_PUBLISHING;if(!write_phase(r,false))goto done;
 r->p.state=BCS_FLUSH_SUBMITTING;if(!write_phase(r,true))goto done;
 r->p.state=BCS_FLUSH_POLLING;
 for(unsigned i=0;i<BCS_FLUSH_POLLS;i++){
  ++r->p.polls;
  if(!read_value(r,&r->p.observed))goto done;
  if(r->p.observed==r->binding.nonce)break;
  if(r->p.observed){fail(r,BCS_FLUSH_RESULT);goto done;}
  if(i+1==BCS_FLUSH_POLLS){fail(r,BCS_FLUSH_TIMEOUT);goto done;}
  if(!delay(r))goto done;
 }
 r->p.state=BCS_FLUSH_VERIFYING;if(!ownership(r))goto done;
 uint32_t expected[BCS_FLUSH_DWORDS];
 if(!intel_bdw_bcs_flush_packet(r->binding.completion_gpu,r->binding.nonce,expected) || !packet_same(r->packet,expected) ||
 r->p.publish_attempts!=1 || r->p.published!=1 || r->p.submit_attempts!=1 || r->p.submitted!=1 ||
 !r->p.polls || r->p.polls>BCS_FLUSH_POLLS || r->p.reads!=r->p.polls+1 || r->p.proofs!=r->p.reads+3 ||
 r->p.initial || r->p.observed!=r->binding.nonce || r->p.uncertain || !r->p.submit_begin){fail(r,BCS_FLUSH_RESULT);goto done;}
 if(r->p.last<=r->p.begin){r->clock_at=r->p.last;r->clock_previous=r->p.last;r->clock_reason=BCS_CLOCK_FROZEN;fail(r,BCS_FLUSH_CLOCK);goto done;}
 r->p.state=BCS_FLUSH_COMPLETE;r->end=r->p.last;r->complete=true;
done:
 r->busy=false;return r->complete;
}
