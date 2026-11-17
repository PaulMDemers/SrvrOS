/* Actual post-reset child of the original full-GT owner. These two narrow
 * operations do NOT prove GPU pipeline/cache/all-reader retirement. */
#include <srvros/intel_vtd_drain.h>
#include <srvros/intel_ggtt_invalidate.h>
#if !defined(SRVROS_INTEL_GT_PRACTICAL)
#error "Full-GT memory preparation requires its separately selected practical policy"
#endif
#define GT_MEMORY_VA (BACKING_VA+0x80000ull)
struct gt_memory_data {
 const void *object;
 uint64_t begin,last,end,root,epoch,maintain_begin[2],maintain_end[2],maintain_phase[2];
 unsigned state,reason,maps,retired,guards,reads,writes,cache_checks,alias_checks,resource;
 unsigned maintain_before[2],maintain_after[2];
 unsigned mtrr[2],pat[2],effective[2];
 bool mapped[2];
 struct vmm_alias_observation alias[2];
};
static struct {
 struct gt_memory_data d;
 struct intel_vtd_drain vtd;
 struct intel_vtd_drain_source vs;
 struct intel_ggtt_invalidate ggtt;
 struct intel_ggtt_invalidate_source gs;
 struct intel_completion_result post;
} gt_memory;
static bool memory_fail(unsigned why) {
 gt_memory.d.reason|=why;gt_memory.d.state=3;return bootstrap_fail(512);
}
static bool memory_identity(void) {
 const struct gt_memory_data *d=&gt_memory.d;
 return d->object==&gt_memory && d->state==1 && !d->reason && !d->end &&
 gt_bootstrap_native.memory_original==&gt_memory && gt_bootstrap_native.memory_child==&gt_memory &&
 gt_bootstrap_native.memory_borrows==1 && !gt_bootstrap_native.memory_returns &&
 bootstrap_parent() && gt_bootstrap_native.reset.complete && gt_bootstrap_native.full_after &&
 !gt_bootstrap_native.reset.registered && !gt_bootstrap_native.reset.retained &&
 gt_bootstrap_native.command_writes==1 && d->root==gt_bootstrap_native.root &&
 d->epoch==gt_bootstrap_native.g->engine_epoch && d->epoch==gt_bootstrap_native.epoch+1 &&
 d->maps<=2 && d->retired<=d->maps && wake_irq_off();
}
static bool memory_tick(void) {
 if(!memory_identity())return memory_fail(1);
 unsigned char saved[sizeof(gt_memory.d)];
 const unsigned char *p=(const unsigned char *)&gt_memory.d;
 for(unsigned i=0;i<sizeof(saved);i++)saved[i]=p[i];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_memory_boundary();
#endif
 if(!memory_identity())return memory_fail(1);
 for(unsigned i=0;i<sizeof(saved);i++)if(saved[i]!=p[i])return memory_fail(1);
 if(!bootstrap_tick() || !memory_identity() || gt_bootstrap_native.last<gt_memory.d.last)return memory_fail(2);
 for(unsigned i=0;i<sizeof(saved);i++)if(saved[i]!=p[i])return memory_fail(1);
 gt_memory.d.last=gt_bootstrap_native.last;return true;
}
static uint64_t memory_physical(unsigned i) {
 return i?result.mmio_base+0x101000:INTEL_VTD_DRAIN_UNIT;
}
static bool memory_translation(unsigned i,uint64_t *physical) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_memory_translate(i,physical);
#else
 return vmm_virt_to_phys_in_space(gt_memory.d.root,GT_MEMORY_VA+i*4096,physical);
#endif
}
static bool memory_alias(unsigned i) {
 uint64_t physical;
 if(i>1 || !memory_identity() || !gt_memory.d.mapped[i] || !memory_translation(i,&physical) || !memory_identity() || physical!=memory_physical(i))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_memory_alias(i,&gt_memory.d.alias[i]);
#else
 bool ok=vmm_diagnostic_kernel_alias(gt_memory.d.root,GT_MEMORY_VA+i*4096,physical,1,
  gt_bootstrap_native.cache.cpu.physical_bits,&gt_memory.d.alias[i],NULL);
#endif
 const struct vmm_alias_observation *a=&gt_memory.d.alias[i];
 if(!memory_identity() || !ok || a->pages!=1 || a->leaves!=1 || a->pat_index!=2 || a->writable!=1 || a->user || a->no_execute!=1 || a->leaf_sizes!=1)return false;
 ++gt_memory.d.alias_checks;return true;
}
static bool memory_policy(unsigned i) {
 if(i>1 || !memory_identity())return false;
 struct x86_cache_snapshot cache;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool resource=intel_display_test_memory_resource(&gt_memory.d.resource);
 if(!resource || !memory_identity() || (gt_memory.d.resource!=1 && gt_memory.d.resource!=2))return false;
 bool ok=intel_display_test_bootstrap_cache(&cache);
#else
 bool resource=dma_resource() && (dma_resource_declared || dma_table_current());
 gt_memory.d.resource=dma_resource_source;
 if(!resource || !memory_identity() || (gt_memory.d.resource!=1 && gt_memory.d.resource!=2))return false;
 bool ok=intel_cache_probe_context(&cache);
#endif
 if(!memory_identity() || !ok ||
 !x86_cache_equal(&cache,&gt_bootstrap_native.cache) ||
 !x86_cache_page(&cache,memory_physical(i),2,&gt_memory.d.mtrr[i],&gt_memory.d.pat[i],&gt_memory.d.effective[i]) ||
 gt_memory.d.effective[i])return false;
 ++gt_memory.d.cache_checks;return true;
}
static bool memory_guard(unsigned i) {
 if(i>1 || !memory_tick() || !bootstrap_guard() || !memory_identity() || !memory_alias(i) || !memory_identity() || !memory_policy(i) ||
 !memory_tick() || !memory_identity())return memory_fail(4);
 ++gt_memory.d.guards;return true;
}
static bool memory_vtd_binding(const struct intel_vtd_drain_binding *b) {
 return b->owner==gt_bootstrap_native.g && b->provider==&gt_memory && b->lifetime==&gt_bootstrap_native.borrow &&
 b->root==gt_memory.d.root && b->epoch==gt_memory.d.epoch && b->sequence==1 && b->hash==INTEL_DMA_DMAR_HASH &&
 b->unit==INTEL_VTD_DRAIN_UNIT && b->prepare_begin==gt_bootstrap_native.begin && b->gt_begin==gt_bootstrap_native.gt_origin &&
 b->hda_begin==gt_bootstrap_native.hda_origin && b->outer_begin==gt_bootstrap_native.outer_origin && b->vendor==0x8086 && b->device==0x1626;
}
static bool memory_ggtt_binding(const struct intel_ggtt_invalidate_binding *b) {
 return b->owner==gt_bootstrap_native.g && b->provider==&gt_memory && b->lifetime==&gt_bootstrap_native.borrow &&
 b->root==gt_memory.d.root && b->epoch==gt_memory.d.epoch && b->sequence==1 && b->mmio==result.mmio_base && b->mmio_bytes==0x200000 &&
 b->prepare_begin==gt_bootstrap_native.begin && b->gt_begin==gt_bootstrap_native.gt_origin &&
 b->hda_begin==gt_bootstrap_native.hda_origin && b->outer_begin==gt_bootstrap_native.outer_origin && b->vendor==0x8086 && b->device==0x1626;
}
static bool memory_vtd_guard(void *ctx,const struct intel_vtd_drain_source *s) {
 return ctx==&gt_memory && s==&gt_memory.vs && s->object==s && s->consumer==&gt_memory.vtd &&
 gt_memory.vtd.source==s && memory_vtd_binding(&s->binding) && memory_vtd_binding(&gt_memory.vtd.binding) && memory_guard(0);
}
static bool memory_ggtt_guard(void *ctx,const struct intel_ggtt_invalidate_source *s) {
 return ctx==&gt_memory && s==&gt_memory.gs && s->object==s && s->consumer==&gt_memory.ggtt &&
 gt_memory.ggtt.source==s && memory_ggtt_binding(&s->binding) && memory_ggtt_binding(&gt_memory.ggtt.binding) && memory_guard(1);
}
static uint64_t memory_now(void *ctx) {return ctx==&gt_memory && memory_tick()?gt_memory.d.last:0;}
static void memory_pause(void *ctx) {if(ctx==&gt_memory)wake_pause(gt_bootstrap_native.a);}
/* The operation cores check their complete original binding/callback/progress
 * snapshots. The adapter independently revalidates real hardware AFTER the
 * final clock and immediately before each access, including both UC aliases. */
static bool memory_vtd_read(void *,uint32_t,uint32_t *);
static bool memory_ggtt_read(void *,uint32_t,uint32_t *);
static enum intel_gt_write memory_vtd_write(void *,uint32_t,uint32_t);
static enum intel_gt_write memory_ggtt_write(void *,uint32_t,uint32_t);
static bool memory_sources(unsigned i) {
 if(i==0) {
  const struct intel_vtd_drain *r=&gt_memory.vtd;const struct intel_vtd_drain_io *a=&r->io,*b=&r->original_io;
  return r->object==r && r->busy && !r->complete && !r->retained && !r->reason && !r->end &&
   r->source==&gt_memory.vs && gt_memory.vs.object==&gt_memory.vs && gt_memory.vs.consumer==r &&
   memory_vtd_binding(&r->binding) && memory_vtd_binding(&gt_memory.vs.binding) &&
   a->ctx==&gt_memory && a->guard==memory_vtd_guard && a->read==memory_vtd_read && a->write==memory_vtd_write && a->now==memory_now && a->pause==memory_pause &&
   b->ctx==a->ctx && b->guard==a->guard && b->read==a->read && b->write==a->write && b->now==a->now && b->pause==a->pause;
 }
 const struct intel_ggtt_invalidate *r=&gt_memory.ggtt;const struct intel_ggtt_invalidate_io *a=&r->io,*b=&r->original_io;
 return i==1 && r->object==r && r->busy && !r->complete && !r->retained && !r->reason && !r->end &&
  r->source==&gt_memory.gs && gt_memory.gs.object==&gt_memory.gs && gt_memory.gs.consumer==r &&
  memory_ggtt_binding(&r->binding) && memory_ggtt_binding(&gt_memory.gs.binding) &&
  a->ctx==&gt_memory && a->guard==memory_ggtt_guard && a->read==memory_ggtt_read && a->write==memory_ggtt_write && a->now==memory_now &&
  b->ctx==a->ctx && b->guard==a->guard && b->read==a->read && b->write==a->write && b->now==a->now;
}
static bool memory_access(unsigned i) {
 if(!memory_sources(i))return false;
 /* Snapshot progress DATA, never the live source or ownership object. */
 unsigned char saved[sizeof(struct intel_vtd_drain_progress)];
 const unsigned char *p=i?(const unsigned char *)&gt_memory.ggtt.p:(const unsigned char *)&gt_memory.vtd.p;
 unsigned count=i?sizeof(gt_memory.ggtt.p):sizeof(gt_memory.vtd.p);
 for(unsigned n=0;n<count;n++)saved[n]=p[n];
 if(!memory_guard(i) || !memory_now(&gt_memory) || !bootstrap_guard() || !memory_identity() || !memory_alias(i) || !memory_sources(i))return false;
 for(unsigned n=0;n<count;n++)if(saved[n]!=p[n])return memory_fail(4);
 return memory_identity() && wake_irq_off() && vmm_current_address_space()==gt_memory.d.root;
}
static bool memory_load(unsigned i,uint32_t off,uint32_t *out) {
 if(!out || !memory_access(i))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_memory_read(i,off,out);
#else
 __asm__ volatile("mfence":::"memory");*out=*(volatile const uint32_t *)(GT_MEMORY_VA+i*4096+off);
 __asm__ volatile("mfence":::"memory");bool ok=true;
#endif
 if(ok)++gt_memory.d.reads;
 return ok && memory_identity();
}
static bool memory_vtd_read(void *ctx,uint32_t off,uint32_t *out) {
 if(ctx!=&gt_memory || !(off==0 || off==8 || off==12 || off==16 || off==20 || off==28 || off==32 || off==36 || off==40 || off==44 || off==52 || off==0x508 || off==0x50c) ||
 !memory_vtd_guard(ctx,&gt_memory.vs))return false;
 return memory_load(0,off,out);
}
static bool memory_ggtt_read(void *ctx,uint32_t off,uint32_t *out) {
 return ctx==&gt_memory && off==0x101008 && memory_ggtt_guard(ctx,&gt_memory.gs) && memory_load(1,8,out);
}
static enum intel_gt_write memory_store(unsigned i,uint32_t off,uint32_t value) {
 if(!memory_access(i))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write w=intel_display_test_memory_write(i,off,value);
#else
 __asm__ volatile("mfence":::"memory");*(volatile uint32_t *)(GT_MEMORY_VA+i*4096+off)=value;
 __asm__ volatile("mfence":::"memory");enum intel_gt_write w=GT_WRITE_DONE;
#endif
 if(w==GT_WRITE_DONE)++gt_memory.d.writes;
 if(w!=GT_WRITE_DONE || !memory_identity())memory_fail(8);
 return w;
}
static enum intel_gt_write memory_vtd_write(void *ctx,uint32_t off,uint32_t value) {
 const struct intel_vtd_drain *r=&gt_memory.vtd;
 if(ctx!=&gt_memory || off!=0x50c || value!=INTEL_VTD_DRAIN_REQUEST || !r->busy || r->complete || r->retained || r->reason ||
 r->p.state!=VTD_DRAIN_REQUESTING || r->p.attempts!=1 || r->p.writes || r->p.uncertain || !memory_vtd_guard(ctx,&gt_memory.vs))return GT_WRITE_NONE;
 return memory_store(0,off,value);
}
static enum intel_gt_write memory_ggtt_write(void *ctx,uint32_t off,uint32_t value) {
 const struct intel_ggtt_invalidate *r=&gt_memory.ggtt;
 if(ctx!=&gt_memory || off!=0x101008 || value!=1 || !r->busy || r->complete || r->retained || r->reason ||
 r->p.state!=GGTT_INVALIDATE_WRITING || r->p.attempts!=1 || r->p.writes || r->p.uncertain || !memory_ggtt_guard(ctx,&gt_memory.gs))return GT_WRITE_NONE;
 return memory_store(1,8,value);
}
static bool memory_records(void) {
 const struct intel_vtd_drain *v=&gt_memory.vtd;const struct intel_ggtt_invalidate *g=&gt_memory.ggtt;
 if(v->object!=v || v->source!=&gt_memory.vs || gt_memory.vs.object!=&gt_memory.vs || gt_memory.vs.consumer!=v ||
 !v->complete || v->busy || v->retained || v->reason || v->clock_reason || v->clock_at || v->clock_previous ||
 v->p.state!=VTD_DRAIN_COMPLETE || v->p.uncertain || v->p.attempts!=1 || v->p.writes!=1 || !v->p.polls || v->p.polls>256 ||
 v->p.reads!=43+7*v->p.polls || v->p.guards!=45+8*v->p.polls || v->p.final!=0x1203000000000000ull ||
 (v->p.initial!=0x0200000000000000ull && v->p.initial!=0x1203000000000000ull) || v->end!=v->p.last || v->end<=v->p.begin ||
 !memory_vtd_binding(&v->binding) || !memory_vtd_binding(&gt_memory.vs.binding) ||
 g->object!=g || g->source!=&gt_memory.gs || gt_memory.gs.object!=&gt_memory.gs || gt_memory.gs.consumer!=g ||
 !g->complete || g->busy || g->retained || g->reason || g->clock_reason || g->clock_at || g->clock_previous ||
 g->p.state!=GGTT_INVALIDATE_COMPLETE || g->p.uncertain || g->p.reads!=2 || g->p.guards!=4 || g->p.attempts!=1 || g->p.writes!=1 ||
 g->p.initial || g->p.posted || g->end!=g->p.last || g->end<=g->p.begin || g->p.begin<v->end ||
 !memory_ggtt_binding(&g->binding) || !memory_ggtt_binding(&gt_memory.gs.binding))return false;
 const struct intel_vtd_drain_io *a=&v->io,*b=&v->original_io;
 if(a->ctx!=&gt_memory || a->guard!=memory_vtd_guard || a->read!=memory_vtd_read || a->write!=memory_vtd_write || a->now!=memory_now || a->pause!=memory_pause ||
 b->ctx!=a->ctx || b->guard!=a->guard || b->read!=a->read || b->write!=a->write || b->now!=a->now || b->pause!=a->pause)return false;
 const struct intel_ggtt_invalidate_io *c=&g->io,*d=&g->original_io;
 if(c->ctx!=&gt_memory || c->guard!=memory_ggtt_guard || c->read!=memory_ggtt_read || c->write!=memory_ggtt_write || c->now!=memory_now ||
 d->ctx!=c->ctx || d->guard!=c->guard || d->read!=c->read || d->write!=c->write || d->now!=c->now)return false;
 const uint64_t expected[8]={0,0x10,INTEL_VTD_DRAIN_CAP,INTEL_VTD_DRAIN_ECAP,0,0x0800000000000000ull,0,0};
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<8;i++)if(v->p.context[n][i]!=expected[i])return false;
 for(unsigned i=0;i<256;i++)if(i>=v->p.polls?v->p.observed[i]!=0:i+1==v->p.polls?v->p.observed[i]!=0x12030000u:(v->p.observed[i]&~0x06000000u)!=0x90030000u)return false;
 return gt_memory.d.reads==v->p.reads+g->p.reads && gt_memory.d.writes==2;
}
static bool memory_maintain(unsigned i) {
 if(i>1 || !memory_tick() || !bootstrap_guard())return false;
 struct intel_gt_access *g=gt_bootstrap_native.g;
 gt_memory.d.maintain_begin[i]=gt_memory.d.last;gt_memory.d.maintain_phase[i]=g->last;
 gt_memory.d.maintain_before[i]=g->snapshots;
 if(!intel_gt_access_check(g) || !memory_tick() || !bootstrap_guard())return false;
 gt_memory.d.maintain_end[i]=g->last;gt_memory.d.maintain_after[i]=g->snapshots;
 return memory_identity() && gt_memory.d.maintain_after[i]==gt_memory.d.maintain_before[i]+1 &&
 gt_memory.d.maintain_end[i]>=gt_memory.d.maintain_begin[i] &&
 gt_memory.d.maintain_end[i]-gt_memory.d.maintain_phase[i]<=GT_ACCESS_PHASE_US;
}
static bool bootstrap_memory_open(void) {
 if(gt_memory.d.object || gt_bootstrap_native.memory_original || !bootstrap_parent() || !gt_bootstrap_native.reset.complete || !bootstrap_tick() || gt_bootstrap_native.last<gt_bootstrap_native.reset.end)return memory_fail(16);
 gt_memory.d.object=&gt_memory;gt_memory.d.state=1;gt_memory.d.root=gt_bootstrap_native.root;gt_memory.d.epoch=gt_bootstrap_native.g->engine_epoch;
 gt_bootstrap_native.memory_original=gt_bootstrap_native.memory_child=&gt_memory;++gt_bootstrap_native.memory_borrows;
 gt_memory.d.begin=gt_memory.d.last=gt_bootstrap_native.last;
 return memory_maintain(0)?true:memory_fail(16);
}
static bool bootstrap_memory_returned(void) {
 const struct gt_memory_data *d=&gt_memory.d;uint64_t physical;
 unsigned expected_guards=gt_memory.vtd.p.guards+gt_memory.ggtt.p.guards+2*(d->reads+d->writes)+4;
 if(d->guards!=expected_guards || d->cache_checks!=expected_guards+4 ||
 d->alias_checks!=expected_guards+d->reads+d->writes+2 || (d->resource!=1 && d->resource!=2))return false;
 if(d->object!=&gt_memory || d->state!=2 || d->reason || d->mapped[0] || d->mapped[1] || d->maps!=2 || d->retired!=2 ||
 gt_bootstrap_native.memory_original!=&gt_memory || gt_bootstrap_native.memory_child || gt_bootstrap_native.memory_borrows!=1 || gt_bootstrap_native.memory_returns!=1 ||
 d->root!=gt_bootstrap_native.root || d->epoch!=gt_bootstrap_native.g->engine_epoch || !memory_records() || !d->begin || d->end!=d->last || d->end<=d->begin ||
 d->begin<gt_bootstrap_native.reset.end || d->begin>gt_memory.vtd.p.begin ||
 gt_memory.ggtt.end>gt_memory.post.begin_us || gt_memory.post.begin_us<gt_bootstrap_native.post.end_us ||
 !gt_memory.post.stable || gt_memory.post.enabled || gt_memory.post.pending || gt_memory.post.flip_wait || gt_memory.post.tlb_pending ||
 gt_memory.post.groups!=10 || gt_memory.post.contexts!=2 || gt_memory.post.reads!=92 || gt_memory.post.samples!=2 ||
 d->end>gt_bootstrap_native.last || d->end-gt_bootstrap_native.begin>INTEL_DIAGNOSTIC_PREPARE_US ||
 !gt_memory.post.complete || gt_memory.post.object!=&gt_memory.post || gt_memory.post.parent!=gt_bootstrap_native.g ||
 gt_memory.post.engine_epoch!=d->epoch || gt_memory.post.reason || gt_memory.post.end_us> d->end ||
 gt_memory.post.begin_us>=gt_memory.post.end_us || gt_memory.post.last_us>gt_memory.post.end_us ||
 d->maintain_end[0]>gt_bootstrap_native.post.begin_us || d->maintain_begin[1]<gt_memory.post.end_us ||
 memory_translation(0,&physical) || memory_translation(1,&physical))return false;
 for(unsigned i=0;i<2;i++)if(d->alias[i].pages!=1 || d->alias[i].leaves!=1 || d->alias[i].pat_index!=2 || !d->alias[i].writable || d->alias[i].user || !d->alias[i].no_execute || d->alias[i].leaf_sizes!=1 || d->effective[i] ||
 d->maintain_after[i]!=d->maintain_before[i]+1 || d->maintain_end[i]<d->maintain_begin[i] || d->maintain_end[i]-d->maintain_phase[i]>GT_ACCESS_PHASE_US)return false;
 for(unsigned n=0;n<2;n++)for(unsigned e=0;e<5;e++)for(unsigned i=0;i<8;i++)if(gt_memory.post.words[n][e][i]!=gt_bootstrap_native.post.words[n][e][i])return false;
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<6;i++)if(gt_memory.post.context[n][i]!=gt_bootstrap_native.post.context[n][i])return false;
 return true;
}
static bool bootstrap_memory_run(void) {
 if(!memory_identity() || !gt_bootstrap_native.post_bound || !memory_tick())return memory_fail(32);
 for(unsigned i=0;i<2;i++) {
  uint64_t physical;if(memory_translation(i,&physical) || !memory_policy(i) || !memory_tick() || !bootstrap_guard())return memory_fail(32);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_memory_map(i,memory_physical(i));
#else
  bool ok=vmm_map_page_in_space(gt_memory.d.root,GT_MEMORY_VA+i*4096,memory_physical(i),VMM_PAGE_WRITABLE|VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE);
#endif
  if(!ok)return memory_fail(32);
  gt_memory.d.mapped[i]=true;++gt_memory.d.maps;
  if(!memory_guard(i))return memory_fail(32);
 }
 gt_memory.vs.object=&gt_memory.vs;
 gt_memory.vs.binding=(struct intel_vtd_drain_binding){gt_bootstrap_native.g,&gt_memory,&gt_bootstrap_native.borrow,gt_memory.d.root,gt_memory.d.epoch,1,INTEL_DMA_DMAR_HASH,INTEL_VTD_DRAIN_UNIT,
 gt_bootstrap_native.begin,gt_bootstrap_native.gt_origin,gt_bootstrap_native.hda_origin,gt_bootstrap_native.outer_origin,0x8086,0x1626};
 if(!intel_vtd_drain_run(&gt_memory.vtd,&gt_memory.vs,(struct intel_vtd_drain_io){&gt_memory,memory_vtd_guard,memory_vtd_read,memory_vtd_write,memory_now,memory_pause}))return memory_fail(64);
 gt_memory.gs.object=&gt_memory.gs;
 gt_memory.gs.binding=(struct intel_ggtt_invalidate_binding){gt_bootstrap_native.g,&gt_memory,&gt_bootstrap_native.borrow,gt_memory.d.root,gt_memory.d.epoch,1,result.mmio_base,0x200000,
 gt_bootstrap_native.begin,gt_bootstrap_native.gt_origin,gt_bootstrap_native.hda_origin,gt_bootstrap_native.outer_origin,0x8086,0x1626};
 if(!intel_ggtt_invalidate_run(&gt_memory.ggtt,&gt_memory.gs,(struct intel_ggtt_invalidate_io){&gt_memory,memory_ggtt_guard,memory_ggtt_read,memory_ggtt_write,memory_now}) || !memory_records())return memory_fail(64);
 if(!intel_completion_observe(&gt_memory.post,gt_bootstrap_native.g,(struct intel_completion_io){gt_bootstrap_native.a,engine_guard,completion_group,completion_context,now,COMPLETION_SAMPLE}) ||
 !memory_maintain(1))return memory_fail(128);
 for(unsigned n=0;n<2;n++) {
  unsigned i=1-n;uint64_t physical;
  if(!memory_guard(i) || !memory_now(&gt_memory) || !bootstrap_guard() || !memory_alias(i) || !memory_identity())return memory_fail(128);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_memory_unmap(i,&physical);
#else
  bool ok=vmm_unmap_page_in_space(gt_memory.d.root,GT_MEMORY_VA+i*4096,&physical);
#endif
  if(!ok || physical!=memory_physical(i))return memory_fail(128);
  gt_memory.d.mapped[i]=false;++gt_memory.d.retired;
  if(memory_translation(i,&physical) || !memory_policy(i) || !memory_tick() || !bootstrap_guard())return memory_fail(128);
 }
 if(!memory_records() || !memory_tick() || !bootstrap_guard() || !memory_tick())return memory_fail(256);
 gt_memory.d.end=gt_memory.d.last;gt_memory.d.state=2;gt_bootstrap_native.memory_child=NULL;++gt_bootstrap_native.memory_returns;
 return bootstrap_memory_returned()?true:memory_fail(256);
}
static void bootstrap_memory_report(void) {
 const struct gt_memory_data *d=&gt_memory.d;
 const struct intel_vtd_drain *v=&gt_memory.vtd;
 const struct intel_ggtt_invalidate *g=&gt_memory.ggtt;
 console_printf("intel-memory-summary: v=1 state=%u reason=%u complete=%u records=%u object=%u owner=%u child=%u borrows=%u returns=%u root=%u epoch=%u begin=%u end=%u last=%u maps=%u retired=%u guards=%u reads=%u writes=%u cache_checks=%u alias_checks=%u resource=%u\n",
  (uint64_t)(d->state),(uint64_t)(d->reason),(uint64_t)(d->state==2),(uint64_t)(memory_records()),(uint64_t)(d->object==&gt_memory),(uint64_t)(gt_bootstrap_native.memory_original==&gt_memory),(uint64_t)(gt_bootstrap_native.memory_child!=NULL),(uint64_t)(gt_bootstrap_native.memory_borrows),(uint64_t)(gt_bootstrap_native.memory_returns),(uint64_t)(d->root),(uint64_t)(d->epoch),(uint64_t)(d->begin),(uint64_t)(d->end),(uint64_t)(d->last),(uint64_t)(d->maps),(uint64_t)(d->retired),(uint64_t)(d->guards),(uint64_t)(d->reads),(uint64_t)(d->writes),(uint64_t)(d->cache_checks),(uint64_t)(d->alias_checks),(uint64_t)(d->resource));
 console_printf("intel-memory-vtd: v=1 state=%u reason=%u complete=%u busy=%u retained=%u begin=%u end=%u last=%u reads=%u guards=%u attempts=%u writes=%u polls=%u uncertain=%u initial=%u final=%u clock=%u at=%u previous=%u\n",
  (uint64_t)(v->p.state),(uint64_t)(v->reason),(uint64_t)(v->complete),(uint64_t)(v->busy),(uint64_t)(v->retained),(uint64_t)(v->p.begin),(uint64_t)(v->end),(uint64_t)(v->p.last),(uint64_t)(v->p.reads),(uint64_t)(v->p.guards),(uint64_t)(v->p.attempts),(uint64_t)(v->p.writes),(uint64_t)(v->p.polls),(uint64_t)(v->p.uncertain),(uint64_t)(v->p.initial),(uint64_t)(v->p.final),(uint64_t)(v->clock_reason),(uint64_t)(v->clock_at),(uint64_t)(v->clock_previous));
 console_printf("intel-memory-ggtt: v=1 state=%u reason=%u complete=%u busy=%u retained=%u begin=%u end=%u last=%u reads=%u guards=%u attempts=%u writes=%u initial=%u posted=%u uncertain=%u clock=%u at=%u previous=%u\n",
  (uint64_t)(g->p.state),(uint64_t)(g->reason),(uint64_t)(g->complete),(uint64_t)(g->busy),(uint64_t)(g->retained),(uint64_t)(g->p.begin),(uint64_t)(g->end),(uint64_t)(g->p.last),(uint64_t)(g->p.reads),(uint64_t)(g->p.guards),(uint64_t)(g->p.attempts),(uint64_t)(g->p.writes),(uint64_t)(g->p.initial),(uint64_t)(g->p.posted),(uint64_t)(g->p.uncertain),(uint64_t)(g->clock_reason),(uint64_t)(g->clock_at),(uint64_t)(g->clock_previous));
 for(unsigned i=0;i<2;i++)console_printf("intel-memory-maintain: v=1 i=%u begin=%u end=%u phase=%u before=%u after=%u\n",
  (uint64_t)(i),(uint64_t)(d->maintain_begin[i]),(uint64_t)(d->maintain_end[i]),(uint64_t)(d->maintain_phase[i]),(uint64_t)(d->maintain_before[i]),(uint64_t)(d->maintain_after[i]));
 for(unsigned i=0;i<2;i++)console_printf("intel-memory-alias: v=1 i=%u physical=%u va=%u mapped=%u pages=%u leaves=%u pat_index=%u writable=%u user=%u no_execute=%u leaf_sizes=%u mtrr=%u pat=%u effective=%u\n",
  (uint64_t)(i),(uint64_t)(memory_physical(i)),(uint64_t)(GT_MEMORY_VA+i*4096),(uint64_t)(d->mapped[i]),(uint64_t)(d->alias[i].pages),(uint64_t)(d->alias[i].leaves),(uint64_t)(d->alias[i].pat_index),(uint64_t)(d->alias[i].writable),(uint64_t)(d->alias[i].user),(uint64_t)(d->alias[i].no_execute),(uint64_t)(d->alias[i].leaf_sizes),(uint64_t)(d->mtrr[i]),(uint64_t)(d->pat[i]),(uint64_t)(d->effective[i]));
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<8;i++)console_printf("intel-memory-context: v=1 n=%u i=%u raw=%u\n",
  (uint64_t)(n),(uint64_t)(i),(uint64_t)(v->p.context[n][i]));
 console_printf("intel-memory-binding: v=1 owner=%u provider=%u lifetime=%u vtd_source=%u ggtt_source=%u root=%u epoch=%u sequence=%u hash=%u unit=%u prepare=%u gt=%u hda=%u outer=%u mmio=%u mmio_bytes=%u va=%u\n",
  (uint64_t)(v->binding.owner && v->binding.owner==gt_bootstrap_native.g && g->binding.owner==v->binding.owner),
  (uint64_t)(v->binding.provider==&gt_memory && g->binding.provider==&gt_memory),
  (uint64_t)(v->binding.lifetime==&gt_bootstrap_native.borrow && g->binding.lifetime==v->binding.lifetime),
  (uint64_t)(memory_vtd_binding(&v->binding) && memory_vtd_binding(&gt_memory.vs.binding)),
  (uint64_t)(memory_ggtt_binding(&g->binding) && memory_ggtt_binding(&gt_memory.gs.binding)),
  v->binding.root,v->binding.epoch,v->binding.sequence,v->binding.hash,v->binding.unit,
  v->binding.prepare_begin,v->binding.gt_begin,v->binding.hda_begin,v->binding.outer_begin,
  g->binding.mmio,g->binding.mmio_bytes,GT_MEMORY_VA);
 const struct x86_cache_snapshot *c=&gt_bootstrap_native.cache;
 console_printf("intel-memory-cache: v=1 sample=0 status=%u begin_us=%u end_us=%u reads=%u count=%u fixed=%u signature=%u features=%u bits=%u cr0=%u pat=%u cap=%u def=%u vendor0=%u vendor1=%u vendor2=%u\n",
  (uint64_t)c->status,c->begin_us,c->end_us,(uint64_t)c->reads,(uint64_t)c->count,(uint64_t)c->fixed_count,
  (uint64_t)c->cpu.signature,(uint64_t)c->cpu.features,(uint64_t)c->cpu.physical_bits,c->cpu.cr0,c->pat,c->cap,c->def_type,
  (uint64_t)c->cpu.vendor[0],(uint64_t)c->cpu.vendor[1],(uint64_t)c->cpu.vendor[2]);
 for(unsigned i=0;i<c->count && i<16;i++)console_printf("intel-memory-variable: v=1 sample=0 index=%u base=%u mask=%u\n",(uint64_t)i,c->base[i],c->mask[i]);
 for(unsigned i=0;i<c->fixed_count && i<11;i++)console_printf("intel-memory-fixed: v=1 sample=0 index=%u raw=%u\n",(uint64_t)i,c->fixed[i]);
 static uint8_t raw[1024];static char encoded[1369];
 for(unsigned i=0;i<256;i++)for(unsigned b=0;b<4;b++)raw[i*4+b]=(uint8_t)(v->p.observed[i]>>(b*8));
 entry_requester_base64(raw,sizeof(raw),encoded);
 console_printf("intel-memory-polls: v=1 b=%s\n",encoded);
 completion_report_named(&gt_memory.post,"intel-memory-post");
}
