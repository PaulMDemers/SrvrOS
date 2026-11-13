/* Included only by the separate v31 adapter. All private allocation, full
 * initialization/checking and allocator validation occur outside GPU budgets. */
#include <srvros/intel_source_buffers.h>
#include <srvros/scanout_transfer.h>
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
 return ctx==&nonzero_native && nonzero_native.source.provider==ctx &&
  nonzero_native.source.input==&nonzero_native.source_input &&
  intel_nonzero_source_live(&nonzero_native.source);
}
static bool buffers_forbidden(void *ctx,const struct scanout_transfer_manifest *m,enum scanout_transfer_phase phase) {
 (void)ctx;(void)m;(void)phase;buffers_native.callbacks++;return false;
}
static uint64_t buffers_forbidden_clock(void *ctx){(void)ctx;buffers_native.callbacks++;return 0;}
static bool buffers_observe(void *ctx,struct intel_nonzero_source *s) {
 struct nonzero_adapter *p=ctx;
 if(p!=&nonzero_native || s!=&p->source || buffers_native.prepared!=2)return false;
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
 const struct intel_source_buffers *b=&buffers_native.binding;
 for(unsigned i=0;i<2;i++) {
  const struct scanout_memory *m=&buffers_native.memory[i];const struct scanout_memory_hold *h=&buffers_native.hold[i];
  console_printf("intel-source-buffer: v=1 n=%u physical=%u epoch=%u serial=%u bytes=%u pages=%u state=%u error=%u release_attempts=%u releases=%u active=%u fault=%u replacement=%u original=%u index=%u offset=%u\n",
   (uint64_t)i,m->physical,m->epoch,h->serial,(uint64_t)m->layout.pages*4096,(uint64_t)m->layout.pages,
   (uint64_t)m->state,(uint64_t)m->error,(uint64_t)m->release_attempts,(uint64_t)m->releases,
   (uint64_t)h->active,(uint64_t)h->fault,b->page[i].replacement,b->page[i].original,(uint64_t)b->page[i].index,(uint64_t)b->page[i].mmio_offset);
 }
 console_printf("intel-source-buffers: v=1 epoch=%u kind=%u post_mode=%u expected=%u origin=%u parent_begin=%u begin=%u end=%u last=%u clock_at=%u clock_prev=%u registered=%u blocked=%u returned=%u complete=%u reason=%u borrows=%u returns=%u entered=%u observed=%u prepared=%u patterns=%u words=%u allocated=%u released=%u dma_before=%u dma_after=%u grant_refused=%u callbacks=%u retained=%u success=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
  (uint64_t)b->epoch,(uint64_t)b->kind,(uint64_t)b->post_mode,b->expected,b->origin,b->parent_begin,b->begin,b->end,b->last,b->clock_at,b->clock_prev,
  (uint64_t)b->registered,(uint64_t)b->blocked,(uint64_t)b->returned,(uint64_t)b->complete,(uint64_t)b->reason,
  (uint64_t)nonzero_native.source.child_borrows,(uint64_t)nonzero_native.source.child_returns,(uint64_t)nonzero_native.source.observer_entered,(uint64_t)nonzero_native.source.observer_complete,
  (uint64_t)buffers_native.prepared,(uint64_t)buffers_native.patterns,buffers_native.words,buffers_native.allocated,buffers_native.released,buffers_native.dma_before,buffers_native.dma_after,
  (uint64_t)buffers_native.grant_refused,(uint64_t)buffers_native.callbacks,(uint64_t)buffers_native.retained,(uint64_t)complete);
}
