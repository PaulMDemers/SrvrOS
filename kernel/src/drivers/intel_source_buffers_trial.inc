/* Included only by the separate v31 adapter. All private allocation, full
 * initialization/checking and allocator validation occur outside GPU budgets. */
#include <srvros/intel_source_buffers.h>
#include <srvros/scanout_transfer.h>
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
#include <srvros/intel_source_transfer.h>
static struct intel_source_transfer source_transfer_native;
#endif
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
#include <srvros/intel_source_requester.h>
static struct {
 struct intel_source_requester result;
 unsigned issued,reads,admit_reads;
 uint32_t admission[2];
 uint64_t store_at[2],store_previous[2];
 bool uncertain;
} source_requester_native;
#endif
#ifdef SRVROS_INTEL_SOURCE_ENGINES
#include <srvros/intel_source_engines.h>
static struct {
 struct intel_source_engines result;
 unsigned issued,reads,admit_reads;
 uint32_t admission[10];
 uint64_t store_at[10],store_previous[10];
 bool uncertain;
} source_engines_native;
#endif
#ifdef SRVROS_INTEL_SOURCE_AUDIT
static struct intel_source_audit_native {
 const void *object,*provider;
 const struct intel_source_engines *parent;
 uint32_t epoch,kind,mode,state,reason,pci[2][9],planes[2][3][9];
 uint64_t origin,parent_begin,begin,last,end,clock_at,clock_prev;
 uint64_t sample_begin[2],sample_end[2],cache_begin[2],cache_end[2];
 struct x86_cache_snapshot cache_context[2];
 unsigned samples,pci_reads,plane_reads,cache_checks,pci_count[2],plane_count[2][3],cache_reads[2];
 bool cache_ok[2],registered,complete;
} source_audit_native;
static bool source_audit_returned(void);
#endif
static struct {
 struct scanout_memory memory[2];
 struct scanout_memory_pool pool;
 struct scanout_memory_token token[2];
 struct scanout_memory_hold hold[2];
 struct intel_source_buffers binding;
 struct scanout_transfer refused;
 struct scanout_transfer_pool transfer_pool;
 struct scanout_space_pool space_pool;
 uint64_t dma_before,dma_after,allocated,released,words,begin,end;
 unsigned prepared,patterns,callbacks;
 bool grant_refused,retained;
} buffers_native;
static bool buffers_validate(void *ctx,uint64_t pa,uint32_t pages) {
 if(ctx!=&buffers_native || !intel_scanout_trial_ram(pa,pages))return false;
 for(unsigned i=0;i<pages;i++) {
  uint64_t physical=pa+(uint64_t)i*4096,actual=0;
  if(!pmm_frame_has_tag(physical,PMM_FRAME_DMA))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_buffer_translation(physical,&actual))return false;
#else
  if(!vmm_virt_to_phys((uint64_t)pmm_phys_to_virt(physical),&actual))return false;
#endif
  if(actual!=physical)return false;
 }
 return true;
}
static uint64_t buffers_allocate(void *ctx,uint32_t pages) {
 if(ctx!=&buffers_native)return 0;
 uint64_t pa=pmm_alloc_frames_tagged(pages,PMM_FRAME_DMA);if(pa)buffers_native.allocated+=pages;return pa;
}
static bool buffers_release(void *ctx,uint64_t pa,uint32_t pages) {
 if(!buffers_validate(ctx,pa,pages))return false;
 pmm_free_frame_range(pa,pages);buffers_native.released+=pages;return true;
}
static bool buffers_pattern(unsigned n,bool initialize) {
 struct scanout_memory *m=&buffers_native.memory[n];
 if(!buffers_validate(&buffers_native,m->physical,m->layout.pages))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 volatile uint32_t *ram=intel_display_test_buffer_pointer(m->physical);
#else
 volatile uint32_t *ram=pmm_phys_to_virt(m->physical);
#endif
 if(!ram)return false;
 uint64_t words=(uint64_t)m->layout.pages*1024;
 uint32_t seed=n?0xabcdef01u:0x55667788u;
 for(uint64_t i=0;i<words;i++) {
  if(initialize)ram[i]=(uint32_t)i^seed;
  if(ram[i]!=((uint32_t)i^seed))return false;
 }
 buffers_native.words+=words;buffers_native.patterns++;return true;
}
static bool buffers_prepare(void) {
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
 /* Touch the entire owned static transfer storage before the excluded native
  * lifetime/census. First-touch CPU A/D updates and cold zeroing are not work
  * to introduce inside the original 100ms preparation interval. */
 volatile unsigned char *storage=(volatile unsigned char *)&source_transfer_native;
 for(unsigned i=0;i<sizeof(source_transfer_native);i++)storage[i]=0;
#endif
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 volatile unsigned char *requester_storage=(volatile unsigned char *)&source_requester_native;
 for(unsigned i=0;i<sizeof(source_requester_native);i++)requester_storage[i]=0;
#endif
#ifdef SRVROS_INTEL_SOURCE_ENGINES
 volatile unsigned char *engine_storage=(volatile unsigned char *)&source_engines_native;
 for(unsigned i=0;i<sizeof(source_engines_native);i++)engine_storage[i]=0;
#endif
#ifdef SRVROS_INTEL_SOURCE_AUDIT
 volatile unsigned char *audit_storage=(volatile unsigned char *)&source_audit_native;
 for(unsigned i=0;i<sizeof(source_audit_native);i++)audit_storage[i]=0;
#endif
 buffers_native.dma_before=pmm_tagged_frames(PMM_FRAME_DMA);buffers_native.begin=timer_monotonic_us();
 struct scanout_profile p={0};struct scanout_memory_layout l={0};
 if(!intel_scanout_trial_profile(&p) || p.width!=1440 || p.height!=900 || p.pitch!=6144 ||
    !scanout_memory_layout(p.width,p.height,p.pitch,true,&l))return false;
 for(unsigned i=0;i<2;i++) {
  if(!scanout_memory_create(&buffers_native.memory[i],&buffers_native.pool,0x42554646455253ull,&l,0x100000000ull,
      (struct scanout_memory_io){&buffers_native,buffers_allocate,buffers_validate,buffers_release},&buffers_native.token[i]))return false;
  struct scanout_memory_cpu cpu;
  if(!scanout_memory_cpu_begin(&buffers_native.memory[i],&buffers_native.token[i],&cpu) || !buffers_pattern(i,true) ||
     !scanout_memory_cpu_end(&buffers_native.memory[i],&cpu) ||
     !scanout_memory_hold_begin(&buffers_native.memory[i],&buffers_native.token[i],&buffers_native.hold[i]))return false;
  buffers_native.hold[i].lifetime=&buffers_native;
  ++buffers_native.prepared;
 }
 return true;
}
static uint64_t buffers_now(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_buffer_clock(&at))return at;
#endif
 return nonzero_now(ctx);
}
static bool buffers_guard(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_buffer_admission();
#endif
 for(unsigned i=0;i<2;i++)if(buffers_native.binding.hold[i]!=&buffers_native.hold[i] ||
    buffers_native.hold[i].memory.object!=&buffers_native.memory[i] ||
    buffers_native.hold[i].lifetime!=&buffers_native || buffers_native.memory[i].owner!=0x42554646455253ull)return false;
 return ctx==&nonzero_native && buffers_native.binding.source==&nonzero_native.source && nonzero_native.source.provider==ctx &&
  nonzero_native.source.input==&nonzero_native.source_input &&
  intel_nonzero_source_live(&nonzero_native.source);
}
#if !defined(SRVROS_INTEL_SOURCE_TRANSFER) || !defined(SRVROS_INTEL_DISPLAY_HOST_TEST)
static bool buffers_forbidden(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
 (void)ctx;(void)m;(void)phase;buffers_native.callbacks++;return false;
}
#endif
#ifndef SRVROS_INTEL_SOURCE_TRANSFER
static uint64_t buffers_forbidden_clock(void *ctx){(void)ctx;buffers_native.callbacks++;return 0;}
#endif
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
static uint64_t transfer_now(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_transfer_clock(&at))return at;
#endif
 return buffers_now(ctx);
}
static bool transfer_guard(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_transfer_admission())return false;
#endif
 return buffers_guard(ctx);
}
static bool transfer_provider(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_transfer_provider(ctx,m,phase);
#else
 return buffers_forbidden(ctx,m,phase);
#endif
}
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
#include "intel_source_requester_trial.inc"
#endif
static bool transfer_visit(void *ctx,struct intel_source_buffers *b) {
 struct nonzero_adapter *p=ctx;
 if(p!=&nonzero_native || b!=&buffers_native.binding)return false;
 struct intel_source_transfer_evidence evidence={0,255};
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_transfer_evidence(&evidence);
#endif
 if(!intel_source_transfer_open(&source_transfer_native,b,&p->a->fw.gt.token,evidence,
    (struct intel_source_transfer_io){p,transfer_guard,transfer_now,transfer_provider}))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(source_transfer_native.opened)intel_display_test_transfer_active(&source_transfer_native);
#endif
 buffers_native.grant_refused=source_transfer_native.refused && !source_transfer_native.opened;
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 if(!source_requester_run(p))return false;
 /* Source loads and requester control share one original full hardware
  * bracket. Each I/O retains its source/map/requester and final-clock checks. */
 nonzero_exit(p);if(p->failed || p->a->uncertain)return false;
 p->source_batch_checked=true;
#endif
 return intel_source_transfer_check(&source_transfer_native) &&
  intel_source_transfer_close(&source_transfer_native);
}
#endif
static bool buffers_observe(void *ctx,struct intel_nonzero_source *s) {
 struct nonzero_adapter *p=ctx;
 if(p!=&nonzero_native || s!=&p->source || buffers_native.prepared!=2)return false;
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
 bool ok=intel_source_buffers_transfer_run(&buffers_native.binding,s,&buffers_native.hold[0],&buffers_native.hold[1],
     (struct intel_source_buffers_io){p,buffers_guard,buffers_now},transfer_visit,p);
 return ok;
#else
 bool ok=intel_source_buffers_run(&buffers_native.binding,s,&buffers_native.hold[0],&buffers_native.hold[1],
     (struct intel_source_buffers_io){p,buffers_guard,buffers_now});
 if(!ok)return false;
 struct scanout_transfer_manifest m={s->input->mmio,s->input->aperture,s->input->aperture_bytes,
  s->input->fence_begin,s->input->fence_end,s->epoch,s->input->first,1,0,255,&buffers_native.binding.expected};
 struct scanout_transfer_token token={0};
 buffers_native.grant_refused=!scanout_transfer_open(&buffers_native.refused,&buffers_native.transfer_pool,
  &buffers_native.space_pool,&p->a->fw.gt.token,&m,
  (struct scanout_transfer_io){&buffers_native,buffers_forbidden,buffers_forbidden_clock},&token) &&
  !buffers_native.refused.object && !buffers_native.transfer_pool.epoch && !buffers_native.space_pool.epoch &&
  !token.object && !buffers_native.callbacks;
 return buffers_native.grant_refused;
#endif
}
static bool buffers_finish(bool outer_safe) {
 bool ok=outer_safe;
 /* Source uncertainty never drops RAM holds, even though no GPU mapping was
  * attempted. The retained original owner is the recovery boundary. */
 for(unsigned i=0;i<2;i++) {
  struct scanout_memory *m=&buffers_native.memory[i];
  if(!m->physical)continue;
  if(!outer_safe || m->cpu_serial || m->state!=MEM_RAM) {ok=false;continue;}
  if(buffers_native.hold[i].active) {
   if(buffers_native.hold[i].lifetime!=&buffers_native){ok=false;continue;}
   buffers_native.hold[i].lifetime=0;
   if(!scanout_memory_hold_end(&buffers_native.hold[i])){ok=false;continue;}
  }
  struct scanout_memory_cpu cpu;
  if(!scanout_memory_cpu_begin(m,&buffers_native.token[i],&cpu) || !buffers_pattern(i,false) ||
     !scanout_memory_cpu_end(m,&cpu)) {ok=false;continue;}
  if(!scanout_memory_close(m,&buffers_native.token[i]))ok=false;
 }
 buffers_native.dma_after=pmm_tagged_frames(PMM_FRAME_DMA);buffers_native.end=timer_monotonic_us();
 for(unsigned i=0;i<2;i++)if(buffers_native.memory[i].physical && buffers_native.memory[i].state!=MEM_RELEASED)buffers_native.retained=true;
 return ok && !buffers_native.retained && buffers_native.allocated==buffers_native.released &&
  buffers_native.dma_before==buffers_native.dma_after;
}
static void buffers_report(bool complete) {
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 source_requester_report();
#endif
 const struct intel_source_buffers *b=&buffers_native.binding;
 for(unsigned i=0;i<2;i++) {
  const struct scanout_memory *m=&buffers_native.memory[i];const struct scanout_memory_hold *h=&buffers_native.hold[i];
  console_printf("intel-source-buffer: v=1 n=%u physical=%u epoch=%u owner=%u serial=%u bytes=%u pages=%u state=%u error=%u release_attempts=%u releases=%u active=%u fault=%u replacement=%u original=%u index=%u offset=%u\n",
   (uint64_t)i,m->physical,m->epoch,m->owner,h->serial,(uint64_t)m->layout.pages*4096,(uint64_t)m->layout.pages,
   (uint64_t)m->state,(uint64_t)m->error,(uint64_t)m->release_attempts,(uint64_t)m->releases,
   (uint64_t)h->active,(uint64_t)h->fault,b->page[i].replacement,b->page[i].original,(uint64_t)b->page[i].index,(uint64_t)b->page[i].mmio_offset);
 }
 console_printf("intel-source-buffers: v=1 epoch=%u kind=%u post_mode=%u expected=%u origin=%u parent_begin=%u begin=%u end=%u last=%u clock_at=%u clock_prev=%u registered=%u blocked=%u returned=%u complete=%u reason=%u borrows=%u returns=%u entered=%u observed=%u prepared=%u patterns=%u words=%u allocated=%u released=%u dma_before=%u dma_after=%u grant_refused=%u callbacks=%u retained=%u success=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
  (uint64_t)b->epoch,(uint64_t)b->kind,(uint64_t)b->post_mode,b->expected,b->origin,b->parent_begin,b->begin,b->end,b->last,b->clock_at,b->clock_prev,
  (uint64_t)b->registered,(uint64_t)b->blocked,(uint64_t)b->returned,(uint64_t)b->complete,(uint64_t)b->reason,
  (uint64_t)nonzero_native.source.child_borrows,(uint64_t)nonzero_native.source.child_returns,(uint64_t)nonzero_native.source.observer_entered,(uint64_t)nonzero_native.source.observer_complete,
  (uint64_t)buffers_native.prepared,(uint64_t)buffers_native.patterns,buffers_native.words,buffers_native.allocated,buffers_native.released,buffers_native.dma_before,buffers_native.dma_after,
  (uint64_t)buffers_native.grant_refused,(uint64_t)buffers_native.callbacks,(uint64_t)buffers_native.retained,(uint64_t)complete);
#ifdef SRVROS_INTEL_SOURCE_TRANSFER
 const struct intel_source_transfer *t=&source_transfer_native;
 console_printf("intel-source-transfer: v=1 state=%u reason=%u epoch=%u kind=%u post_mode=%u expected=%u first=%u physical0=%u physical1=%u origin=%u parent_begin=%u begin=%u end=%u last=%u clock_at=%u clock_prev=%u registered=%u borrows=%u returns=%u source_blocked=%u holds_blocked=%u refused=%u opened=%u checked=%u closed=%u callbacks=%u clocks=%u owner_state=%u owner_pinned=%u token=%u pool=%u space_pool=%u assurances=%u obligations=%u managed_pages=0 pte_writes=0 hardware_ownership=0\n",
  (uint64_t)t->state,(uint64_t)t->reason,(uint64_t)t->epoch,(uint64_t)t->kind,(uint64_t)t->post_mode,t->expected,
  (uint64_t)(t->input?t->input->first:0),t->physical[0],t->physical[1],t->origin,t->parent_begin,t->begin,t->end,t->last,t->clock_at,t->clock_prev,
  (uint64_t)t->registered,(uint64_t)t->borrows,(uint64_t)t->returns,(uint64_t)t->source_blocked,(uint64_t)t->holds_blocked,(uint64_t)t->refused,
  (uint64_t)t->opened,(uint64_t)t->checked,(uint64_t)t->closed,(uint64_t)t->callbacks,(uint64_t)t->clocks,(uint64_t)t->owner.state,
  (uint64_t)t->owner.pinned,(uint64_t)(t->token.object!=0),t->pool.epoch,t->space_pool.epoch,(uint64_t)t->assurances,(uint64_t)t->obligations);
#endif
#ifdef SRVROS_INTEL_SOURCE_ENGINES
 source_engines_report();
#endif
}
