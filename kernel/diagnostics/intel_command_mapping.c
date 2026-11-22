#include <srvros/intel_command_mapping.h>
#include <srvros/intel_bdw_traffic.h>
#include <srvros/intel_ggtt_invalidate.h>
#include <srvros/vmm.h>
#include <srvros/x86_cache.h>

static bool io_same(const struct command_map_io *a,const struct command_map_io *b) {
 return a->ctx==b->ctx && a->ownership==b->ownership && a->read==b->read && a->write==b->write &&
 a->complete==b->complete && a->readers_retired==b->readers_retired && a->now==b->now && a->pause==b->pause;
}
static bool cpu_io_same(const struct intel_command_io *a,const struct intel_command_io *b) {
 return a->ram.ctx==b->ram.ctx && a->ram.allocate==b->ram.allocate && a->ram.validate==b->ram.validate &&
 a->ram.release==b->ram.release && a->map==b->map && a->unmap==b->unmap && a->observe==b->observe &&
 a->initialize==b->initialize && a->preserved==b->preserved && a->guard==b->guard && a->retired==b->retired && a->now==b->now;
}
static bool lease_same(const struct intel_command_lease *a,const struct intel_command_lease *b) {
 return a->owner==b->owner && a->provider==b->provider && a->lifetime==b->lifetime && a->root==b->root &&
 a->epoch==b->epoch && a->sequence==b->sequence && a->prepare==b->prepare && a->gt==b->gt && a->hda==b->hda && a->outer==b->outer;
}
static bool manifest_same(const struct scanout_transfer_manifest *a,const struct scanout_transfer_manifest *b) {
 return a->mmio==b->mmio && a->aperture==b->aperture && a->aperture_bytes==b->aperture_bytes &&
 a->fence_begin==b->fence_begin && a->fence_end==b->fence_end && a->source_epoch==b->source_epoch &&
 a->first==b->first && a->pages==b->pages && a->assurances==b->assurances && a->obligations==b->obligations && a->original==b->original;
}
static bool overlap(const void *a,uint64_t an,const void *b,uint64_t bn) {
 uintptr_t x=(uintptr_t)a,y=(uintptr_t)b;
 return !a || !b || x>UINTPTR_MAX-an || y>UINTPTR_MAX-bn || (x<y+bn && y<x+an);
}
static bool transfer_live(const struct intel_command_mapping *r) {
 const struct scanout_transfer *t=r->transfer;
 if(!t || t->object!=t || (t->state!=TRANSFER_ACTIVE && t->state!=TRANSFER_CLOSING) || t->busy!=r->checking_transfer || !t->pinned ||
 t->error || r->token.object!=t || r->token.epoch!=t->epoch || r->token.owner!=t->owner ||
 t->manifest.original!=t->original || !manifest_same(&r->manifest,&t->manifest) ||
 t->io.ctx!=r->transfer_io.ctx || t->io.now!=r->transfer_io.now || t->io.revalidate!=r->transfer_io.revalidate ||
 !display_access_held(&t->exclusion) || t->space.object!=&t->space || t->space.epoch!=t->arena_epoch ||
 t->space.owner!=t->owner || (t->space.state!=SPACE_ACTIVE && t->space.state!=SPACE_CLOSING))return false;
 for(unsigned i=0;i<4;i++)if(t->original[i]!=r->original[i])return false;
 return true;
}
static bool range_live(const struct intel_command_mapping *r) {
 const struct scanout_space *s=&r->transfer->space;const struct scanout_space_token *t=&r->range;
 if(t->object!=s || t->epoch!=s->epoch || !t->serial || t->slot>=SCANOUT_SPACE_LEASES || t->first!=r->manifest.first || t->pages!=4)return false;
 const struct scanout_space_entry *e=&s->leases[t->slot];
 return e->serial==t->serial && e->first==t->first && e->pages==t->pages &&
 (e->use==SPACE_RESERVED || e->use==SPACE_EXPOSED || e->use==SPACE_RETIRED);
}
static bool live(const struct intel_command_mapping *r) {
 return r && r->object==r && r->busy && r->registered && !r->retained && !r->reason && !r->complete && !r->end &&
 io_same(&r->io,&r->original_io) && intel_command_memory_consumer_live(r->cpu,r) &&
 lease_same(&r->lease,&r->cpu->lease) && cpu_io_same(&r->cpu_io,&r->cpu->io) &&
 r->cpu->physical==r->physical && r->cpu->serial==r->serial && r->cpu->end==r->cpu_end && transfer_live(r) && range_live(r);
}
/* Snapshot actual storage, including callback tables/registration/counters. No
 * callback is allowed to mutate either owner. Only four publication entries
 * belong to this transaction; the large unused vector never enters a frame. */
struct saved {
 unsigned char owner[offsetof(struct intel_command_mapping,publication)];
 unsigned char publication[offsetof(struct scanout_publication,before)];
 uint64_t before[4],after[4];
};
static void capture(const struct intel_command_mapping *r,struct saved *s) {
 const unsigned char *p=(const unsigned char *)r;
 for(unsigned i=0;i<sizeof(s->owner);i++)s->owner[i]=p[i];
 p=(const unsigned char *)&r->publication;
 for(unsigned i=0;i<sizeof(s->publication);i++)s->publication[i]=p[i];
 for(unsigned i=0;i<4;i++){s->before[i]=r->publication.before[i];s->after[i]=r->publication.after[i];}
}
static bool unchanged(const struct intel_command_mapping *r,const struct saved *s) {
 if(!live(r))return false;
 const unsigned char *p=(const unsigned char *)r;
 for(unsigned i=0;i<sizeof(s->owner);i++)if(s->owner[i]!=p[i])return false;
 p=(const unsigned char *)&r->publication;
 for(unsigned i=0;i<sizeof(s->publication);i++)if(s->publication[i]!=p[i])return false;
 for(unsigned i=0;i<4;i++)if(s->before[i]!=r->publication.before[i] || s->after[i]!=r->publication.after[i])return false;
 return true;
}
static bool fail(struct intel_command_mapping *r,unsigned why) {
 r->reason|=why;r->retained=true;r->complete=false;r->state=COMMAND_MAP_FAULT;return false;
}
static uint64_t sample(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!live(r)){fail(r,COMMAND_MAP_SOURCE);return 0;}
 struct saved s;capture(r,&s);uint64_t at=r->io.now(r->io.ctx);
 if(!unchanged(r,&s)){fail(r,COMMAND_MAP_SOURCE);return 0;}
 unsigned why=!at?1:at<r->last?2:
 at-r->lease.prepare>INTEL_DIAGNOSTIC_PREPARE_US?3:at-r->lease.gt>INTEL_DIAGNOSTIC_GT_TOTAL_US?4:
 at-r->lease.hda>INTEL_DIAGNOSTIC_HDA_US?5:at-r->lease.outer>INTEL_DIAGNOSTIC_OUTER_US?6:0;
 if(why){r->clock_at=at;r->clock_previous=r->last;r->clock_reason=why;fail(r,COMMAND_MAP_CLOCK);return 0;}
 if(!r->begin)r->begin=at;
 r->last=at;return at;
}
struct nested_clock {struct intel_command_mapping *owner;uint64_t last;};
static uint64_t nested_sample(void *ctx) {
 struct nested_clock *c=ctx;
 if(c->owner->last!=c->last){fail(c->owner,COMMAND_MAP_SOURCE);return 0;}
 uint64_t at=sample(c->owner);if(at)c->last=at;return at;
}
static bool guard(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!sample(r))return false;
 struct saved s;capture(r,&s);struct nested_clock clock={r,r->last};r->checking_transfer=true;
 bool owned=scanout_transfer_check_clocked(r->transfer,&r->token,nested_sample,&clock);
 r->checking_transfer=false;
 /* Only the sealed local clock observer may advance this header field.
  * A provider changing last directly is caught before the next sample. */
 const unsigned char *last=(const unsigned char *)&clock.last;
 for(unsigned i=0;i<sizeof(clock.last);i++)s.owner[offsetof(struct intel_command_mapping,last)+i]=last[i];
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(!owned)return fail(r,COMMAND_MAP_OWNERSHIP);
 if(!sample(r))return false;
 capture(r,&s);uint64_t checked_after=r->last;struct command_map_proof p={0};bool ok=r->io.ownership(r->io.ctx,r,r->phase,&p);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(!sample(r))return false;
 if(!ok || p.object!=r || p.cpu!=r->cpu || p.transfer!=r->transfer || p.root!=r->lease.root ||
 p.epoch!=r->lease.epoch || p.physical!=r->physical || p.first!=r->range.first || p.pages!=4 ||
 p.requirements!=COMMAND_MAP_REQUIRED || p.checked_at<checked_after || p.checked_at>r->last)
  return fail(r,COMMAND_MAP_OWNERSHIP);
 ++r->proofs;return true;
}
static bool window_pinned(const struct intel_command_mapping *r)
{
 const struct command_map_window *w=&r->window;
 uint64_t off=0x800000ull+8ull*r->range.first;
 uint64_t aperture=r->manifest.aperture+4096ull*r->range.first;
 return w->guard && r->range.pages==4 && (off&4095)<=4096-32 &&
        r->manifest.mmio<=UINT64_MAX-(off&~4095ull) && aperture>=r->manifest.aperture &&
        !(w->aperture_pin&4095) && w->aperture_pin>=r->manifest.aperture &&
        w->aperture_pin-r->manifest.aperture<r->manifest.aperture_bytes && w->guard_epoch==w->guard->epoch &&
        vmm_reserved_guard_register_pinned(w->guard,r->lease.root,w->aperture_pin,r->manifest.mmio+(off&~4095ull),
                                          w->alias,w->pin) &&
        vmm_reserved_guard_io_range_pinned(w->guard,r->lease.root,aperture,4,w->pin);
}

static bool window_epoch(struct intel_command_mapping *r)
{
 if(!window_pinned(r) || !live(r))return false;
 struct saved saved;capture(r,&saved);
 uint64_t epoch=r->window.traffic.held_epoch(r->transfer,&r->publication);
 return unchanged(r,&saved) && epoch==r->block_epoch && window_pinned(r);
}

struct window_check {struct intel_command_mapping *mapping;uint64_t begin;};

static bool window_live(struct window_check *check)
{
 struct intel_command_mapping *r=check->mapping;
 uint64_t now=sample(r);
 return now && now>=check->begin && now-check->begin<BDW_TRAFFIC_BLOCK_US && window_epoch(r);
}

static bool window_read(void *ctx,uint32_t offset,uint32_t *value)
{
 struct window_check *check=ctx;
 struct intel_command_mapping *r=check->mapping;
 if(!window_live(check))return false;
 struct saved saved;capture(r,&saved);
 *value=UINT32_MAX;
 bool ok=r->window.traffic.read(r->transfer,offset,value);
 return ok && *value!=UINT32_MAX && (offset!=0x40d0 || !(*value&0xff00u)) &&
        unchanged(r,&saved) && window_live(check);
}

static bool window_select(void *ctx,uint32_t from,uint32_t to)
{
 struct window_check *check=ctx;
 struct intel_command_mapping *r=check->mapping;
 if(!window_live(check))return false;
 struct saved saved;capture(r,&saved);
 enum intel_gt_write issued=r->window.traffic.select_l3(r->transfer,from,to);
 return issued==GT_WRITE_DONE && unchanged(r,&saved) && window_live(check);
}

static bool window_plane(struct window_check *check)
{
 uint32_t control,queued,live_surface;
 struct intel_command_mapping *r=check->mapping;
 if(!window_read(check,0x70180,&control) || !(control&(1u<<31)) ||
    !window_read(check,0x7019c,&queued) || !window_read(check,0x701ac,&live_surface) ||
    (live_surface&4095) || (queued&~8u)!=live_surface || live_surface>=r->manifest.fence_end)return false;
 if(r->plane_control)return r->plane_control==control && r->live_surface==live_surface;
 r->plane_control=control;r->live_surface=live_surface;
 return true;
}

static bool window_blocked(struct window_check *check)
{
 return check->mapping->block_epoch && window_plane(check) &&
        intel_bdw_traffic_still_blocked(check,window_read);
}

static bool window_acquire(struct intel_command_mapping *r)
{
 struct window_check check={r,r->last};
 struct bdw_traffic_sample observations={.ctx=&check,.read=window_read};
 if(!window_plane(&check) || !intel_bdw_traffic_l3_policy(&observations,window_select) ||
    !intel_bdw_traffic_open(&observations) || !guard(r) || !window_live(&check))return false;
 uint64_t before=r->last,epoch=0;
 struct saved saved;capture(r,&saved);
 enum intel_gt_write issued=r->window.traffic.request(r->transfer,&r->publication,&epoch);
 if(issued!=GT_WRITE_DONE || !epoch || !unchanged(r,&saved))return false;
 r->block_epoch=epoch;
 for(unsigned poll=0;poll<GT_ACCESS_POLL_CAP;poll++){
  bool drained=false;
  if(!intel_bdw_traffic_poll(&observations,&drained))return false;
  if(drained && r->last>before)return window_blocked(&check);
 }
 return false;
}

static bool window_release(struct intel_command_mapping *r)
{
 struct window_check check={r,r->last};
 struct bdw_traffic_sample observations={.ctx=&check,.read=window_read};
 if(r->publication.state!=PUB_RETIRED || !intel_bdw_traffic_l3_policy(&observations,window_select) ||
    !window_blocked(&check))return false;
 uint64_t before=r->last;
 struct saved saved;capture(r,&saved);
 enum intel_gt_write issued=r->window.traffic.release(r->transfer,&r->publication,r->block_epoch);
 if(issued!=GT_WRITE_DONE || !unchanged(r,&saved))return false;
 r->block_epoch=0;
 for(unsigned poll=0;poll<GT_ACCESS_POLL_CAP;poll++){
  bool arbiter,engines;
  uint32_t l3;
  if(!bdw_traffic_gam(&observations,BDW_GAM_CLIENTS,&arbiter) ||
     !bdw_traffic_engines(&observations,false,&engines) || !window_read(&check,0x8038,&l3))return false;
  if(arbiter && engines && !(l3&3u) && before<r->last)return window_plane(&check);
 }
 return false;
}

static volatile uint64_t *window_entry(const struct intel_command_mapping *r,uint32_t index)
{
 return (volatile uint64_t *)(uintptr_t)(r->window.alias+((8ull*index)&4095));
}

static enum intel_gt_write window_write(struct intel_command_mapping *r,uint32_t index,
                                        uint64_t expected,uint64_t replacement)
{
 struct window_check check={r,r->last};
 uint32_t pending;
 if(!intel_cache_probe_uc_page(r->manifest.mmio+((0x800000ull+8ull*index)&~4095ull)) ||
    !window_read(&check,INTEL_GGTT_INVALIDATE_REGISTER,&pending) || pending || !window_blocked(&check))return GT_WRITE_NONE;
 enum intel_gt_write issued=intel_bdw_ggtt_exchange(window_entry(r,index),expected,replacement);
 if(issued==GT_WRITE_DONE && !window_blocked(&check))return GT_WRITE_UNCERTAIN;
 return issued;
}

static bool window_invalidate(struct intel_command_mapping *r)
{
 struct window_check check={r,r->last};
 struct bdw_traffic_sample observations={.ctx=&check,.read=window_read};
 uint32_t posted;
 if(!intel_bdw_traffic_l3_policy(&observations,window_select) || !window_blocked(&check) ||
    !window_read(&check,INTEL_GGTT_INVALIDATE_REGISTER,&posted) || posted)return false;
 struct saved saved;capture(r,&saved);
 enum intel_gt_write issued=r->window.traffic.invalidate_ggtt(r->transfer,&r->publication,r->block_epoch,
                                                               INTEL_GGTT_INVALIDATE_REGISTER,1);
 return issued==GT_WRITE_DONE && unchanged(r,&saved) &&
        window_read(&check,INTEL_GGTT_INVALIDATE_REGISTER,&posted) && !posted && window_blocked(&check);
}

static bool read_entry(void *ctx,uint32_t index,uint64_t *out) {
 struct intel_command_mapping *r=ctx;
 if(!out || index<r->range.first || index-r->range.first>=4 || !guard(r))return false;
 uint64_t value=UINT64_MAX;
 bool ok;
 if(r->window.guard){
  struct window_check check={r,r->last};
  ok=r->block_epoch ? window_blocked(&check) : window_epoch(r);
  if(ok)value=*window_entry(r,index);
 }else{
  struct saved s;capture(r,&s);
  ok=r->io.read(r->io.ctx,0x800000u+8u*index,&value);
  if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 }
 if(ok)++r->reads;
 if(!sample(r))return false;
 if(!ok)return fail(r,COMMAND_MAP_READ);
 *out=value;return true;
}
static bool write_entry(void *ctx,uint32_t index,uint64_t value) {
 struct intel_command_mapping *r=ctx;
 if(index<r->range.first || index-r->range.first>=4 || !guard(r))return false;
 unsigned i=index-r->range.first;
 if((r->publication.state!=PUB_WRITING || value!=r->replacement[i]) &&
 (r->publication.state!=PUB_RESTORING || value!=r->original[i]))return fail(r,COMMAND_MAP_SOURCE);
 ++r->attempts;bool intact;
 enum intel_gt_write result;
 if(r->window.guard){
  uint64_t expected=r->publication.state==PUB_RESTORING ? r->replacement[i] : r->original[i];
  result=window_write(r,index,expected,value);
  intact=live(r);
 }else{
  struct saved s;capture(r,&s);
  result=r->io.write(r->io.ctx,0x800000u+8u*index,value);
  intact=unchanged(r,&s);
 }
 if(result==GT_WRITE_DONE)++r->writes;else if(result!=GT_WRITE_NONE)r->uncertain=true;
 if(!intact)return fail(r,COMMAND_MAP_SOURCE);
 if(result!=GT_WRITE_DONE)return fail(r,COMMAND_MAP_WRITE);
 return sample(r)!=0;
}
static bool completion(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!guard(r))return false;
 if(r->window.guard && !window_invalidate(r))return fail(r,COMMAND_MAP_COMPLETE);
 struct saved s;capture(r,&s);bool ok=r->io.complete(r->io.ctx,r,r->phase);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(ok)++r->completions;
 if(!sample(r))return false;
 return ok?true:fail(r,COMMAND_MAP_COMPLETE);
}
static bool readers(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!guard(r))return false;
 struct saved s;capture(r,&s);bool ok=r->io.readers_retired(r->io.ctx,r);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 ++r->reader_checks;
 return sample(r) && ok;
}
static bool pause_once(struct intel_command_mapping *r) {
 if(!guard(r))return false;
 struct saved s;capture(r,&s);r->io.pause(r->io.ctx);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 return sample(r)!=0;
}
bool intel_command_mapping_page(const struct intel_command_mapping *r,unsigned i,uint64_t *gpu,uint64_t *physical) {
 if(!r || r->state!=COMMAND_MAP_USING || !live(r) || i>=4 || !gpu || !physical ||
 overlap(gpu,8,r,sizeof(*r)) || overlap(physical,8,r,sizeof(*r)) || overlap(gpu,8,physical,8) ||
 overlap(gpu,8,r->cpu,sizeof(*r->cpu)) || overlap(physical,8,r->cpu,sizeof(*r->cpu)) ||
 overlap(gpu,8,r->transfer,sizeof(*r->transfer)) || overlap(physical,8,r->transfer,sizeof(*r->transfer)))return false;
 *gpu=(uint64_t)(r->range.first+i)*4096;*physical=r->physical+4096*i;return true;
}
/* One CPU0 register window. A failed nested publication retains the original
 * mapping, both backing objects and the device block until recovery. */
static struct intel_command_mapping *slot_owner;

static bool slot_live(struct intel_command_mapping *r)
{
 const struct command_map_slot *s=&r->slot;
 const struct scanout_transfer *t=s->transfer;
 struct window_check check={r,r->last};
 return r==slot_owner && r->state==COMMAND_MAP_USING && r->publication.state==PUB_PUBLISHED &&
        t->object==t && t->state==TRANSFER_ACTIVE && t->pinned && !t->error &&
        t->exclusion.owner==r->transfer->exclusion.owner && t->exclusion.epoch==r->transfer->exclusion.epoch &&
        t->manifest.mmio==r->manifest.mmio && t->manifest.aperture==r->manifest.aperture &&
        t->manifest.aperture_bytes==r->manifest.aperture_bytes && t->manifest.fence_end==r->manifest.fence_end &&
        t->manifest.source_epoch==r->manifest.source_epoch && t->manifest.assurances==HANDOFF_ALL &&
        !t->manifest.obligations && t->manifest.first==8704 && t->manifest.pages==1 &&
        t->manifest.original==t->original &&
        vmm_reserved_guard_io_range_pinned(r->window.guard,r->lease.root,r->window.aperture_pin,1,r->window.pin) &&
        guard(r) && window_blocked(&check);
}
static bool slot_guard(void *ctx)
{
 struct intel_command_mapping *r=slot_owner;
 if(!r || ctx!=r->io.ctx || !slot_live(r))return false;
 struct saved saved;capture(r,&saved);
 return r->slot.io.guard(ctx) && unchanged(r,&saved) &&
        scanout_transfer_check(r->slot.transfer,&r->slot.grant) && unchanged(r,&saved) && slot_live(r);
}
static bool slot_read(void *ctx,uint32_t index,uint64_t *value)
{
 struct intel_command_mapping *r=slot_owner;
 if(index!=8704 || !value || !slot_guard(ctx))return false;
 *value=*window_entry(r,index);
 return true;
}
static bool slot_write(void *ctx,uint32_t index,uint64_t value)
{
 struct intel_command_mapping *r=slot_owner;
 if(index!=8704 || !slot_guard(ctx))return false;
 const struct scanout_memory *m=r->slot.memory;
 const struct scanout_publication *p=&m->publication;
 bool restoring=p->state==PUB_RESTORING;
 struct intel_bdw_ggtt_page entry;
 uint64_t original;
 if(!intel_bdw_scanout_page_store_active(m,&r->slot.token,r->slot.transfer,index) ||
    !scanout_transfer_original(r->slot.transfer,&r->slot.grant,index,&original) ||
    !intel_bdw_ggtt_plan_entry(&r->slot.transfer->manifest,index,m->physical,&entry) ||
    (entry.original&4095)!=1 || !(entry.original&~4095ull) || entry.original>>39 ||
    original!=entry.original || !scanout_space_verify_original(m->space,&p->range,0,original) ||
    entry.original!=p->before[0] || entry.replacement!=p->after[0] ||
    value!=(restoring?entry.original:entry.replacement))return false;
 struct saved saved;capture(r,&saved);
 if(!m->allocator.validate || !m->allocator.validate(m->allocator.ctx,entry.physical,1) ||
    !unchanged(r,&saved) || !slot_live(r) || m->physical!=entry.physical ||
    !intel_bdw_scanout_page_store_active(m,&r->slot.token,r->slot.transfer,index) ||
    restoring!=(p->state==PUB_RESTORING) || p->before[0]!=entry.original || p->after[0]!=entry.replacement ||
    r->last<p->phase_us || r->last-p->phase_us>SCANOUT_PUBLISH_DEADLINE_US)return false;
 return window_write(r,index,restoring?entry.replacement:entry.original,value)==GT_WRITE_DONE;
}
static bool slot_flush(void *ctx)
{
 struct intel_command_mapping *r=slot_owner;
 if(!slot_guard(ctx) || !window_invalidate(r))return false;
 struct saved saved;capture(r,&saved);
 return r->slot.io.flush(ctx) && unchanged(r,&saved) && slot_live(r);
}
static bool slot_retired(void *ctx)
{
 struct intel_command_mapping *r=slot_owner;
 if(!slot_guard(ctx))return false;
 struct saved saved;capture(r,&saved);
 return r->slot.io.readers_retired(ctx) && unchanged(r,&saved) && slot_live(r);
}
static bool slot_roundtrip(struct intel_command_mapping *r)
{
 struct command_map_slot *s=&r->slot;
 if(slot_owner || !s->memory->physical ||
    (s->memory->physical>=r->physical && s->memory->physical-r->physical<INTEL_COMMAND_BYTES))return false;
 slot_owner=r;
 struct scanout_publish_io io={r->io.ctx,slot_guard,slot_read,slot_write,slot_flush,slot_retired,r->io.now};
 if(!slot_guard(r->io.ctx) ||
    !intel_bdw_scanout_prepare_test_page(s->memory,&s->token,s->transfer,&s->grant,s->pool,io) ||
    !scanout_memory_apply(s->memory,&s->token) || !slot_guard(r->io.ctx) ||
    !scanout_memory_close(s->memory,&s->token) || !slot_guard(r->io.ctx) ||
    s->memory->publication.state!=PUB_RETIRED || s->memory->publication.writes!=2 ||
    *window_entry(r,8704)!=s->transfer->original[0])return false;
 slot_owner=NULL;
 return true;
}

static bool mapping_run(struct intel_command_mapping *r,struct intel_command_memory *cpu,
 struct scanout_transfer *t,const struct scanout_transfer_token *token,struct command_map_io io, const struct command_map_window *window, const struct command_map_slot *slot,
 bool (*child)(void *,struct intel_command_mapping *),void *child_ctx) {
 if(window && (!window->guard || !window->pin || !window->aperture_pin || !window->guard_epoch || !window->alias || (window->alias&4095) ||
               !window->traffic.held_epoch || !window->traffic.request || !window->traffic.release ||
               !window->traffic.read || !window->traffic.select_l3 || !window->traffic.invalidate_ggtt))return false;
 if(!r || r->object || r->state || r->busy || r->registered || r->retained || r->complete || r->reason ||
 r->range.object || r->publication.object || r->begin || r->last || r->end || !intel_command_memory_returned(cpu) || !t || !token || (!slot && (!child || !child_ctx)) ||
 (window && overlap(r,sizeof(*r),window,sizeof(*window))) || (slot && overlap(r,sizeof(*r),slot,sizeof(*slot))) ||
 overlap(r,sizeof(*r),cpu,sizeof(*cpu)) || overlap(r,sizeof(*r),t,sizeof(*t)) || overlap(r,sizeof(*r),token,sizeof(*token)) ||
 overlap(cpu,sizeof(*cpu),t,sizeof(*t)) || overlap(token,sizeof(*token),cpu,sizeof(*cpu)) || overlap(token,sizeof(*token),t,sizeof(*t)) ||
 t->object!=t || t->state!=TRANSFER_ACTIVE || t->busy || !t->pinned || t->error ||
 token->object!=t || token->epoch!=t->epoch || token->owner!=t->owner || t->manifest.pages!=4 ||
 t->manifest.original!=t->original || t->manifest.assurances!=HANDOFF_ALL || t->manifest.obligations ||
 t->manifest.source_epoch!=cpu->lease.epoch || io.ctx!=cpu->lease.provider || io.ctx!=t->io.ctx || io.now!=t->io.now ||
 !io.ownership || (window ? (io.read || io.write) : (!io.read || !io.write)) || !io.complete || !io.readers_retired || !io.now || !io.pause ||
 (cpu->lease.root>=cpu->physical && cpu->lease.root<cpu->physical+16384))return false;
 *r=(struct intel_command_mapping){.object=r,.cpu=cpu,.transfer=t,.token=*token,.manifest=t->manifest,.transfer_io=t->io,
 .lease=cpu->lease,.cpu_io=cpu->io,.io=io,.original_io=io,.physical=cpu->physical,.serial=cpu->serial,.cpu_end=cpu->end,
 .last=cpu->end,.state=COMMAND_MAP_OPENING,.busy=true};
 if(window)r->window=*window;
 if(slot)r->slot=*slot;
 for(unsigned i=0;i<4;i++)r->original[i]=t->original[i];
 if(!intel_command_memory_consumer_begin(cpu,r))goto source_fail;
 r->registered=true;
 if(!scanout_space_reserve(&t->space,t->owner,t->arena_epoch,4,1,&r->range))goto source_fail;
 for(unsigned i=0;i<4;i++){
  struct scanout_transfer_manifest one=r->manifest;one.first=r->range.first+i;one.pages=1;one.original=&r->original[i];
  struct intel_bdw_ggtt_page p;
  if(!intel_bdw_ggtt_page_plan(&one,r->physical+4096*i,&p))goto source_fail;
  r->replacement[i]=p.replacement;
 }
 if(window && (!window_pinned(r) ||
    !intel_cache_probe_uc_page(r->manifest.mmio+((0x800000ull+8ull*r->range.first)&~4095ull))))goto source_fail;
 struct scanout_publish_io provider={r,guard,read_entry,write_entry,completion,readers,sample};
 if(!scanout_publish_prepare(&r->publication,&r->pool,t->owner,&t->space,&r->range,r->replacement,4,&provider,&r->publication_token))goto source_fail;
 for(unsigned i=0;i<4;i++)if(r->publication.before[i]!=r->original[i])goto source_fail;
 if(window && !window_acquire(r))goto source_fail;
 r->phase=COMMAND_MAP_REPLACEMENT;
 if(!scanout_publish_apply(&r->publication,&r->publication_token) || !guard(r))goto source_fail;
 if(window){
  struct window_check check={r,r->last};
  if(!window_blocked(&check))goto source_fail;
 }
 r->state=COMMAND_MAP_USING;
 bool used;
 if(slot)used=slot_roundtrip(r);
 else{
  struct saved saved;capture(r,&saved);
  used=child(child_ctx,r);
  if(!unchanged(r,&saved))goto source_fail;
 }
 if(!used){fail(r,COMMAND_MAP_CHILD);goto done;}
 if(!guard(r))goto done;
 if(window){
  struct window_check check={r,r->last};
  if(!window_blocked(&check))goto source_fail;
 }
 r->state=COMMAND_MAP_RESTORING;r->phase=COMMAND_MAP_ORIGINAL;
 for(unsigned i=0;i<COMMAND_MAP_POLLS;i++){
  ++r->polls;
  if(scanout_publish_restore(&r->publication,&r->publication_token))break;
  if(r->retained || r->publication.state!=PUB_PUBLISHED || !pause_once(r))goto source_fail;
 }
 if(r->publication.state!=PUB_RETIRED || !guard(r) || !sample(r) || r->last<=r->begin ||
 r->attempts!=8 || r->writes!=8 || r->completions!=2 || r->publication.reads!=20 || r->publication.writes!=8 ||
 r->publication.flushes!=2 || r->reader_checks!=r->polls+1 || r->uncertain)goto source_fail;
 if(window && !window_release(r))goto source_fail;
 if(!scanout_space_release(&t->space,&r->range) || !intel_command_memory_consumer_end(cpu,r))goto source_fail;
 r->registered=false;r->end=r->last;r->complete=true;r->state=COMMAND_MAP_CLOSED;
 goto done;
source_fail:
 if(!r->retained)fail(r,COMMAND_MAP_SOURCE);
done:
 r->busy=false;return r->complete;
}

bool intel_command_mapping_run(struct intel_command_mapping *r,struct intel_command_memory *cpu,
 struct scanout_transfer *t,const struct scanout_transfer_token *token,struct command_map_io io,
 bool (*child)(void *,struct intel_command_mapping *),void *ctx)
{
 return mapping_run(r,cpu,t,token,io,NULL,NULL,child,ctx);
}

bool intel_command_mapping_run_window(struct intel_command_mapping *r,struct intel_command_memory *cpu,
 struct scanout_transfer *t,const struct scanout_transfer_token *token,struct command_map_io io,
 const struct command_map_window *window,bool (*child)(void *,struct intel_command_mapping *),void *ctx)
{
 return window && mapping_run(r,cpu,t,token,io,window,NULL,child,ctx);
}

bool intel_command_mapping_run_slot(struct intel_command_mapping *r,struct intel_command_memory *cpu,
 struct scanout_transfer *t,const struct scanout_transfer_token *token,struct command_map_io io,
 const struct command_map_window *window,const struct command_map_slot *slot)
{
 if(!window || !slot || !t || !slot->transfer || !slot->memory || !slot->pool || slot_owner ||
    slot->transfer==t || slot->transfer->manifest.first!=8704 || slot->transfer->manifest.pages!=1 ||
    t->manifest.first!=8705 || slot->io.ctx!=io.ctx || slot->io.now!=io.now ||
    !slot->io.guard || !slot->io.flush || !slot->io.readers_retired ||
    slot->io.read || slot->io.write || window->aperture_pin!=t->manifest.aperture+8704ull*4096)return false;
 return mapping_run(r,cpu,t,token,io,window,slot,NULL,NULL);
}
