#include <srvros/intel_command_mapping.h>

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
static struct saved capture(const struct intel_command_mapping *r) {
 struct saved s;const unsigned char *p=(const unsigned char *)r;
 for(unsigned i=0;i<sizeof(s.owner);i++)s.owner[i]=p[i];
 p=(const unsigned char *)&r->publication;
 for(unsigned i=0;i<sizeof(s.publication);i++)s.publication[i]=p[i];
 for(unsigned i=0;i<4;i++){s.before[i]=r->publication.before[i];s.after[i]=r->publication.after[i];}
 return s;
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
 struct saved s=capture(r);uint64_t at=r->io.now(r->io.ctx);
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
 struct saved s=capture(r);struct nested_clock clock={r,r->last};r->checking_transfer=true;
 bool owned=scanout_transfer_check_clocked(r->transfer,&r->token,nested_sample,&clock);
 r->checking_transfer=false;
 /* Only the sealed local clock observer may advance this header field.
  * A provider changing last directly is caught before the next sample. */
 const unsigned char *last=(const unsigned char *)&clock.last;
 for(unsigned i=0;i<sizeof(clock.last);i++)s.owner[offsetof(struct intel_command_mapping,last)+i]=last[i];
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(!owned)return fail(r,COMMAND_MAP_OWNERSHIP);
 if(!sample(r))return false;
 s=capture(r);uint64_t checked_after=r->last;struct command_map_proof p={0};bool ok=r->io.ownership(r->io.ctx,r,r->phase,&p);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(!sample(r))return false;
 if(!ok || p.object!=r || p.cpu!=r->cpu || p.transfer!=r->transfer || p.root!=r->lease.root ||
 p.epoch!=r->lease.epoch || p.physical!=r->physical || p.first!=r->range.first || p.pages!=4 ||
 p.requirements!=COMMAND_MAP_REQUIRED || p.checked_at<checked_after || p.checked_at>r->last)
  return fail(r,COMMAND_MAP_OWNERSHIP);
 ++r->proofs;return true;
}
static bool read_entry(void *ctx,uint32_t index,uint64_t *out) {
 struct intel_command_mapping *r=ctx;
 if(!out || index<r->range.first || index-r->range.first>=4 || !guard(r))return false;
 struct saved s=capture(r);uint64_t value=UINT64_MAX;
 bool ok=r->io.read(r->io.ctx,0x800000u+8u*index,&value);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
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
 ++r->attempts;struct saved s=capture(r);
 enum intel_gt_write result=r->io.write(r->io.ctx,0x800000u+8u*index,value);
 bool intact=unchanged(r,&s);
 if(result==GT_WRITE_DONE)++r->writes;else if(result!=GT_WRITE_NONE)r->uncertain=true;
 if(!intact)return fail(r,COMMAND_MAP_SOURCE);
 if(result!=GT_WRITE_DONE)return fail(r,COMMAND_MAP_WRITE);
 return sample(r)!=0;
}
static bool completion(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!guard(r))return false;
 struct saved s=capture(r);bool ok=r->io.complete(r->io.ctx,r,r->phase);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 if(ok)++r->completions;
 if(!sample(r))return false;
 return ok?true:fail(r,COMMAND_MAP_COMPLETE);
}
static bool readers(void *ctx) {
 struct intel_command_mapping *r=ctx;
 if(!guard(r))return false;
 struct saved s=capture(r);bool ok=r->io.readers_retired(r->io.ctx,r);
 if(!unchanged(r,&s))return fail(r,COMMAND_MAP_SOURCE);
 ++r->reader_checks;
 return sample(r) && ok;
}
static bool pause_once(struct intel_command_mapping *r) {
 if(!guard(r))return false;
 struct saved s=capture(r);r->io.pause(r->io.ctx);
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
bool intel_command_mapping_run(struct intel_command_mapping *r,struct intel_command_memory *cpu,
 struct scanout_transfer *t,const struct scanout_transfer_token *token,struct command_map_io io,
 bool (*child)(void *,struct intel_command_mapping *),void *child_ctx) {
 if(!r || r->object || r->state || r->busy || r->registered || r->retained || r->complete || r->reason ||
 r->range.object || r->publication.object || r->begin || r->last || r->end || !intel_command_memory_returned(cpu) || !t || !token || !child || !child_ctx ||
 overlap(r,sizeof(*r),cpu,sizeof(*cpu)) || overlap(r,sizeof(*r),t,sizeof(*t)) || overlap(r,sizeof(*r),token,sizeof(*token)) ||
 overlap(cpu,sizeof(*cpu),t,sizeof(*t)) || overlap(token,sizeof(*token),cpu,sizeof(*cpu)) || overlap(token,sizeof(*token),t,sizeof(*t)) ||
 t->object!=t || t->state!=TRANSFER_ACTIVE || t->busy || !t->pinned || t->error ||
 token->object!=t || token->epoch!=t->epoch || token->owner!=t->owner || t->manifest.pages!=4 ||
 t->manifest.original!=t->original || t->manifest.assurances!=HANDOFF_ALL || t->manifest.obligations ||
 t->manifest.source_epoch!=cpu->lease.epoch || io.ctx!=cpu->lease.provider || io.ctx!=t->io.ctx || io.now!=t->io.now ||
 !io.ownership || !io.read || !io.write || !io.complete || !io.readers_retired || !io.now || !io.pause ||
 (cpu->lease.root>=cpu->physical && cpu->lease.root<cpu->physical+16384))return false;
 *r=(struct intel_command_mapping){.object=r,.cpu=cpu,.transfer=t,.token=*token,.manifest=t->manifest,.transfer_io=t->io,
 .lease=cpu->lease,.cpu_io=cpu->io,.io=io,.original_io=io,.physical=cpu->physical,.serial=cpu->serial,.cpu_end=cpu->end,
 .last=cpu->end,.state=COMMAND_MAP_OPENING,.busy=true};
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
 struct scanout_publish_io provider={r,guard,read_entry,write_entry,completion,readers,sample};
 if(!scanout_publish_prepare(&r->publication,&r->pool,t->owner,&t->space,&r->range,r->replacement,4,&provider,&r->publication_token))goto source_fail;
 for(unsigned i=0;i<4;i++)if(r->publication.before[i]!=r->original[i])goto source_fail;
 r->phase=COMMAND_MAP_REPLACEMENT;
 if(!scanout_publish_apply(&r->publication,&r->publication_token) || !guard(r))goto source_fail;
 r->state=COMMAND_MAP_USING;
 struct saved saved=capture(r);bool used=child(child_ctx,r);
 if(!unchanged(r,&saved))goto source_fail;
 if(!used){fail(r,COMMAND_MAP_CHILD);goto done;}
 if(!guard(r))goto done;
 r->state=COMMAND_MAP_RESTORING;r->phase=COMMAND_MAP_ORIGINAL;
 for(unsigned i=0;i<COMMAND_MAP_POLLS;i++){
  ++r->polls;
  if(scanout_publish_restore(&r->publication,&r->publication_token))break;
  if(r->retained || r->publication.state!=PUB_PUBLISHED || !pause_once(r))goto source_fail;
 }
 if(r->publication.state!=PUB_RETIRED || !guard(r) || !sample(r) || r->last<=r->begin ||
 r->attempts!=8 || r->writes!=8 || r->completions!=2 || r->publication.reads!=20 || r->publication.writes!=8 ||
 r->publication.flushes!=2 || r->reader_checks!=r->polls+1 || r->uncertain)goto source_fail;
 if(!scanout_space_release(&t->space,&r->range) || !intel_command_memory_consumer_end(cpu,r))goto source_fail;
 r->registered=false;r->end=r->last;r->complete=true;r->state=COMMAND_MAP_CLOSED;
 goto done;
source_fail:
 if(!r->retained)fail(r,COMMAND_MAP_SOURCE);
done:
 r->busy=false;return r->complete;
}
