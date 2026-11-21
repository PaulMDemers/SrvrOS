#include <srvros/intel_bdw_primary_flip.h>

static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool binding_same(const struct bdw_flip_binding *a,const struct bdw_flip_binding *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime &&
 a->from_mapping==b->from_mapping && a->to_mapping==b->to_mapping &&
 a->root==b->root && a->epoch==b->epoch && a->sequence==b->sequence && a->admitted_at==b->admitted_at &&
 a->aperture_bytes==b->aperture_bytes && a->from_gpu==b->from_gpu && a->to_gpu==b->to_gpu &&
 a->mapped_bytes==b->mapped_bytes && a->vendor==b->vendor && a->device==b->device &&
 a->source_size==b->source_size && a->control==b->control && a->stride==b->stride;
}
static bool io_same(const struct bdw_flip_io *a,const struct bdw_flip_io *b) {
 return a->ctx==b->ctx && a->ownership==b->ownership && a->read==b->read && a->write==b->write && a->now==b->now;
}
static bool valid(const struct bdw_flip_source *s) {
 if(!s || s->object!=s)return false;
 const struct bdw_flip_binding *b=&s->binding;
 if(!b->owner || !b->provider || !b->lifetime || !b->from_mapping || !b->to_mapping || b->from_mapping==b->to_mapping ||
 !b->root || (b->root&4095) || !b->epoch || !b->sequence || !b->admitted_at ||
 b->vendor!=0x8086 || b->device!=0x1626 || !b->aperture_bytes || b->aperture_bytes>0x100000000ull ||
 (b->aperture_bytes&4095) || !b->mapped_bytes || (b->mapped_bytes&4095) ||
 b->from_gpu>=b->aperture_bytes || b->to_gpu>=b->aperture_bytes ||
 ((b->from_gpu|b->to_gpu)&0x3ffff) || b->mapped_bytes>b->aperture_bytes-b->from_gpu ||
 b->mapped_bytes>b->aperture_bytes-b->to_gpu ||
 (b->from_gpu<b->to_gpu+b->mapped_bytes && b->to_gpu<b->from_gpu+b->mapped_bytes) ||
 (b->source_size&~0x1fff0fffu) || !(b->control&0x80000000u) ||
 ((b->control>>26)&15)!=6 || (b->control&~0xfd000400u))return false;
 uint32_t width=(b->source_size>>16)+1,height=(b->source_size&4095)+1,tiled=(b->control>>10)&1;
 if(width>4096 || !b->stride || b->stride>32768 || b->stride<width*4 || (b->stride&(tiled?511u:63u)))return false;
 uint64_t rows=tiled?((uint64_t)height+7)&~7ull:height;
 /* BDW PRI_SURF workaround: 256KiB surface alignment and 128 extra PTEs.
  * mapped_bytes includes translation tail; it is not a RAM-size claim. */
 return b->mapped_bytes>=((rows*b->stride+4095)&~4095ull)+128*4096ull;
}
static bool progress_same(const struct bdw_flip_progress *a,const struct bdw_flip_progress *b) {
 return a->begin==b->begin && a->last==b->last && a->state==b->state && a->proofs==b->proofs &&
 a->reads==b->reads && a->attempts==b->attempts && a->writes==b->writes && a->polls==b->polls &&
 a->frame==b->frame && a->before_frame==b->before_frame && a->after_frame==b->after_frame &&
 a->programmed==b->programmed && a->live[0]==b->live[0] && a->live[1]==b->live[1] && a->uncertain==b->uncertain;
}
static bool live(const struct intel_bdw_primary_flip *r) {
 return r->object==r && !r->rollback && r->busy && !r->retained && !r->reason && !r->end && !r->clock_at && !r->clock_previous &&
 (r->p.state==BDW_FLIP_ARMING || r->p.state==BDW_FLIP_PENDING) && valid(r->source) &&
 r->source->consumer==r && binding_same(&r->binding,&r->source->binding) && io_same(&r->io,&r->original_io);
}
struct saved {
 struct bdw_flip_source *source;
 struct bdw_flip_binding binding;
 struct bdw_flip_io io;
 struct bdw_flip_progress p;
};
static struct saved capture(const struct intel_bdw_primary_flip *r) {
 return (struct saved){r->source,r->binding,r->io,r->p};
}
static bool unchanged(const struct intel_bdw_primary_flip *r,const struct saved *s) {
 return live(r) && r->source==s->source && binding_same(&r->binding,&s->binding) &&
 io_same(&r->io,&s->io) && progress_same(&r->p,&s->p);
}
static bool fail(struct intel_bdw_primary_flip *r,const struct saved *s,unsigned why) {
 /* Restore only recorded identity/progress DATA, never a live owner copy or
  * hardware state. A mutated callback cannot discard a known store prefix. */
 r->object=r;r->source=s->source;r->binding=s->binding;r->io=s->io;r->original_io=s->io;r->p=s->p;
 if(r->source){r->source->object=r->source;r->source->consumer=r;}
 r->reason|=why;r->p.state=BDW_FLIP_FAULT;r->retained=true;r->end=0;return false;
}
static bool tick(struct intel_bdw_primary_flip *r) {
 struct saved s=capture(r);
 if(!live(r))return fail(r,&s,BDW_FLIP_SOURCE);
 uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,&s,BDW_FLIP_SOURCE);
 if(!at || at<r->p.last || at-r->p.begin>BDW_FLIP_US) {
  r->clock_at=at;r->clock_previous=r->p.last;return fail(r,&s,BDW_FLIP_CLOCK);
 }
 r->p.last=at;return true;
}
static bool ownership(struct intel_bdw_primary_flip *r) {
 if(!tick(r))return false;
 struct saved s=capture(r);struct bdw_flip_proof p={0};
 bool ok=r->io.ownership(r->io.ctx,r->source,&p);
 if(!unchanged(r,&s))return fail(r,&s,BDW_FLIP_SOURCE);
 if(!tick(r))return false;
 const struct bdw_flip_binding *b=&r->binding;
 if(!ok || p.source!=r->source || p.owner!=b->owner || p.provider!=b->provider || p.lifetime!=b->lifetime ||
 p.from_mapping!=b->from_mapping || p.to_mapping!=b->to_mapping || p.root!=b->root || p.epoch!=b->epoch ||
 p.sequence!=b->sequence || p.requirements!=BDW_FLIP_REQUIRED || p.state!=r->p.state ||
 p.checked_at<s.p.last || p.checked_at>r->p.last)return fail(r,&s,BDW_FLIP_OWNERSHIP);
 ++r->p.proofs;return true;
}
static bool read_word(struct intel_bdw_primary_flip *r,uint32_t offset,uint32_t *out) {
 if(!ownership(r))return false;
 struct saved s=capture(r);uint32_t value=0;bool ok=r->io.read(r->io.ctx,offset,&value);
 if(!unchanged(r,&s))return fail(r,&s,BDW_FLIP_SOURCE);
 if(!ok)return fail(r,&s,BDW_FLIP_READ);
 ++r->p.reads;if(!tick(r))return false;*out=value;return true;
}
static bool geometry(struct intel_bdw_primary_flip *r) {
 const uint32_t off[]={0x70180,0x70188,0x6001c,0x701a4};
 const uint32_t expected[]={r->binding.control,r->binding.stride,r->binding.source_size,0};
 for(unsigned i=0;i<4;i++) {
  uint32_t value;
  if(!read_word(r,off[i],&value))return false;
  if(value!=expected[i]){struct saved s=capture(r);return fail(r,&s,BDW_FLIP_LAYOUT);}
 }
 return true;
}
static bool observation(struct intel_bdw_primary_flip *r) {
 if(!read_word(r,0x70040,&r->p.before_frame))return false;
 if(!geometry(r))return false;
 /* Raw SURF bit3 records a prior ring flip source, not part of the address.
  * Other low bits are reserved. SURFLIVE is the full live base, not SURF. */
 if(!read_word(r,0x7019c,&r->p.programmed) || !read_word(r,0x701ac,&r->p.live[0]) ||
 !read_word(r,0x701ac,&r->p.live[1]) || !read_word(r,0x70040,&r->p.after_frame))return false;
 uint32_t from=(uint32_t)r->binding.from_gpu,to=(uint32_t)r->binding.to_gpu;
 bool arming=r->p.state==BDW_FLIP_ARMING;
 if((uint32_t)(r->p.after_frame-r->p.before_frame)>1 ||
 (r->p.programmed&~8u)!=(arming?from:to) ||
 (arming?(r->p.live[0]!=from || r->p.live[1]!=from):
 ((r->p.live[0]!=from && r->p.live[0]!=to) || (r->p.live[1]!=from && r->p.live[1]!=to) ||
 (r->p.live[0]==to && r->p.live[1]==from)))) {
  struct saved s=capture(r);return fail(r,&s,BDW_FLIP_LAYOUT);
 }
 return true;
}
bool intel_bdw_primary_flip_begin(struct intel_bdw_primary_flip *r,struct bdw_flip_source *source,struct bdw_flip_io io) {
 if(!r || r->object || r->rollback || r->source || r->busy || r->retained || r->reason || r->end || r->clock_at || r->clock_previous ||
 r->p.state || r->p.begin || r->p.last || r->p.proofs || r->p.reads || r->p.attempts || r->p.writes || r->p.polls ||
 r->p.frame || r->p.before_frame || r->p.after_frame || r->p.programmed || r->p.live[0] || r->p.live[1] || r->p.uncertain ||
 !valid(source) || source->consumer || io.ctx!=source->binding.provider || !io.ownership || !io.read || !io.write || !io.now ||
 overlap(r,sizeof(*r),source,sizeof(*source)))return false;
 *r=(struct intel_bdw_primary_flip){.object=r,.source=source,.binding=source->binding,.io=io,.original_io=io,
 .p={.begin=source->binding.admitted_at,.last=source->binding.admitted_at,.state=BDW_FLIP_ARMING},.busy=true};
 source->consumer=r;
 if(!observation(r))goto done;
 r->p.frame=r->p.after_frame;
 if(!ownership(r))goto done;
 ++r->p.attempts;struct saved s=capture(r);
 enum intel_gt_write w=r->io.write(r->io.ctx,0x7019c,(uint32_t)r->binding.to_gpu);
 bool intact=unchanged(r,&s);
 if(w==GT_WRITE_DONE)++s.p.writes;else if(w!=GT_WRITE_NONE)s.p.uncertain=true;
 r->p=s.p;
 if(!intact){fail(r,&s,BDW_FLIP_SOURCE);goto done;}
 if(w!=GT_WRITE_DONE){fail(r,&s,BDW_FLIP_WRITE);goto done;}
 if(!tick(r))goto done;
 r->p.state=BDW_FLIP_PENDING;
 /* A posting read is an ordering check, never a live latch or DMA fence. */
 uint32_t posted;
 if(!read_word(r,0x7019c,&posted))goto done;
 if((posted&~8u)!=(uint32_t)r->binding.to_gpu){s=capture(r);fail(r,&s,BDW_FLIP_LAYOUT);goto done;}
 if(!ownership(r))goto done;
 if(r->p.last<=r->p.begin){s=capture(r);fail(r,&s,BDW_FLIP_CLOCK);}
done:
 r->busy=false;return r->p.state==BDW_FLIP_PENDING && !r->retained;
}
enum bdw_flip_status intel_bdw_primary_flip_poll(struct intel_bdw_primary_flip *r) {
 if(!r || r->object!=r || r->busy || !r->source || r->source->consumer!=r)return BDW_FLIP_INVALID;
 if(r->p.state==BDW_FLIP_FAULT)return BDW_FLIP_FAILED;
 if(r->p.state!=BDW_FLIP_PENDING)return BDW_FLIP_INVALID;
 uint64_t poll_begin=r->p.last;
 uint32_t previous_frame=r->p.after_frame,previous_live=r->p.live[1];
 r->busy=true;
 if(!live(r)){struct saved s=capture(r);fail(r,&s,BDW_FLIP_SOURCE);goto done;}
 if(r->p.attempts!=1 || r->p.writes!=1 || r->p.uncertain || r->p.polls>=BDW_FLIP_POLLS) {
  struct saved s=capture(r);fail(r,&s,BDW_FLIP_LIMIT);goto done;
 }
 ++r->p.polls;
 if(!observation(r))goto done;
 uint32_t delta=r->p.after_frame-r->p.frame;
 if(delta>8 || (uint32_t)(r->p.before_frame-previous_frame)>8 ||
 (previous_live==(uint32_t)r->binding.to_gpu && r->p.live[0]!=(uint32_t)r->binding.to_gpu)) {
  struct saved s=capture(r);fail(r,&s,BDW_FLIP_LAYOUT);goto done;
 }
 if(!ownership(r))goto done;
 if(r->p.last<=poll_begin){struct saved s=capture(r);fail(r,&s,BDW_FLIP_CLOCK);goto done;}
 if(delta && r->p.live[0]==(uint32_t)r->binding.to_gpu && r->p.live[1]==(uint32_t)r->binding.to_gpu) {
  r->p.state=BDW_FLIP_LATCHED;r->end=r->p.last;
 } else if(r->p.polls==BDW_FLIP_POLLS){struct saved s=capture(r);fail(r,&s,BDW_FLIP_LIMIT);}
done:
 r->busy=false;return r->retained?BDW_FLIP_FAILED:r->p.state==BDW_FLIP_LATCHED?BDW_FLIP_DONE:BDW_FLIP_WAIT;
}

bool intel_bdw_primary_flip_rollback(struct intel_bdw_primary_flip *forward,
                                    struct intel_bdw_primary_flip *back,
                                    struct bdw_flip_source *reverse)
{
    if (!forward || forward->object != forward || forward->rollback || forward->busy ||
        forward->retained || forward->reason || forward->p.state != BDW_FLIP_LATCHED ||
        !forward->end || forward->end != forward->p.last ||
        !valid(forward->source) || forward->source->consumer != forward ||
        !binding_same(&forward->binding, &forward->source->binding) ||
        !io_same(&forward->io, &forward->original_io))
        return false;

    if (!back || !valid(reverse) || reverse->consumer ||
        overlap(back, sizeof(*back), forward, sizeof(*forward)) ||
        overlap(back, sizeof(*back), forward->source, sizeof(*forward->source)) ||
        overlap(reverse, sizeof(*reverse), forward, sizeof(*forward)) ||
        overlap(reverse, sizeof(*reverse), forward->source, sizeof(*forward->source)))
        return false;

    const struct bdw_flip_binding *old = &forward->binding;
    struct bdw_flip_binding expected = *old;
    expected.from_mapping = old->to_mapping;
    expected.to_mapping = old->from_mapping;
    expected.from_gpu = old->to_gpu;
    expected.to_gpu = old->from_gpu;
    expected.sequence = reverse->binding.sequence;
    expected.admitted_at = reverse->binding.admitted_at;
    if (expected.sequence <= old->sequence || expected.admitted_at < forward->end ||
        !binding_same(&expected, &reverse->binding))
        return false;

    /* Reserve the return before entering provider callbacks. A started
     * failure retains its source even for an indeterminate SURF write. */
    forward->rollback = back;
    bool pending = intel_bdw_primary_flip_begin(back, reverse, forward->original_io);
    if (reverse->consumer != back)
        forward->rollback = 0;
    return pending;
}
