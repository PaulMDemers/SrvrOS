/* Dedicated read-only completion audit under the existing wake owner. */
#include <srvros/intel_completion_audit.h>
static bool completion_group_raw(void *ctx,unsigned e,uint32_t out[INTEL_COMPLETION_WORDS]) {
 struct wake_adapter *a=ctx;
 if(e>=INTEL_ENGINE_COUNT || !a->mapped[e+3] || !a->mapped[8])return false;
 for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_completion_read(e,i,i==7?intel_completion_tlb[e]:intel_engine_pages[e]+(i==6?0x2d0:intel_engine_offsets[i]),&out[i]))return false;
#else
  uint64_t va=i==7?WAKE_VA+8*4096+(intel_completion_tlb[e]&4095):WAKE_VA+(e+3)*4096+(i==6?0x2d0:intel_engine_offsets[i]);
  out[i]=*(volatile const uint32_t *)va;
#endif
 }
 return true;
}
static bool completion_group(void *ctx,unsigned e,uint32_t out[INTEL_COMPLETION_WORDS]) {
 struct wake_adapter *a=ctx;if(e>=INTEL_ENGINE_COUNT || !a->mapped[e+3] || !a->mapped[8])return false;
 return engine_guard(ctx) && completion_group_raw(ctx,e,out);
}
static bool completion_context_raw(void *ctx,uint32_t out[INTEL_COMPLETION_CONTEXT]) {
 struct wake_adapter *a=ctx;if(!a->mapped[3] || !a->mapped[7])return false;
 for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_completion_context(intel_completion_context[i],&out[i]))return false;
#else
  unsigned page=i<3?3:7;
  out[i]=*(volatile const uint32_t *)(WAKE_VA+page*4096+(intel_completion_context[i]&4095));
#endif
 }
 return true;
}
static bool completion_context(void *ctx,uint32_t out[INTEL_COMPLETION_CONTEXT]) {
 return engine_guard(ctx) && completion_context_raw(ctx,out);
}
#if defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL) && !defined(SRVROS_INTEL_CENSUS_SCALAR_TEST)
static bool completion_mapping(void *ctx,unsigned slot) {
 struct wake_adapter *a=ctx;uint64_t physical;
 return slot<WAKE_MAPS && a->mapped[slot] && vmm_current_address_space()==a->fw.gt.a.common.cr3 &&
  vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,WAKE_VA+4096*slot,&physical) && physical==result.mmio_base+wake_pages[slot];
}
/* Same46 non-clearing DWORDs/order, one fresh full provider admission. The
 * core brackets the complete sample; requesters remain fresh between groups. */
static bool completion_snapshot(void *ctx,uint32_t words[INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS],uint32_t context[INTEL_COMPLETION_CONTEXT]) {
 if(!words || !context || !engine_guard(ctx))return false;
 for(unsigned e=0;e<INTEL_ENGINE_COUNT;e++) {
  if(!completion_mapping(ctx,e+3) || !completion_mapping(ctx,8) || !engine_requesters(ctx) || !completion_group_raw(ctx,e,words[e]))return false;
  for(unsigned i=0;i<INTEL_COMPLETION_WORDS;i++)if(words[e][i]==UINT32_MAX)return false;
  if(words[e][7]&~1u)return false;
 }
 if(!completion_mapping(ctx,3) || !completion_mapping(ctx,7) || !engine_requesters(ctx) || !completion_context_raw(ctx,context))return false;
 for(unsigned i=0;i<INTEL_COMPLETION_CONTEXT;i++)if(context[i]==UINT32_MAX)return false;
 return engine_requesters(ctx);
}
#define COMPLETION_SAMPLE completion_snapshot
#else
#define COMPLETION_SAMPLE NULL
#endif
static void completion_report_named(const struct intel_completion_result *r,const char *prefix) {
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<INTEL_ENGINE_COUNT && n*5+e<r->groups;e++) {
   const uint32_t *w=r->words[n][e];
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
   console_printf("%s-row: v=2 s=%u e=%u d=%u,%u,%u,%u,%u,%u,%u,%u\n",prefix,(uint64_t)n,(uint64_t)e,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#else
   console_printf("%s-row: v=1 sample=%u engine=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u\n",prefix,(uint64_t)n,(uint64_t)e,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#endif
  }
  if(n<r->contexts) {
   const uint32_t *w=r->context[n];
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
   console_printf("%s-context: v=2 s=%u d=%u,%u,%u,%u,%u,%u\n",prefix,(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5]);
#else
   console_printf("%s-context: v=1 sample=%u c0=%u c1=%u c2=%u c3=%u c4=%u c5=%u\n",prefix,(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5]);
#endif
  }
 }
 console_printf("%s-summary: v=1 groups=%u samples=%u contexts=%u reads=%u reason=%u stable=%u enabled=%u pending=%u flip_wait=%u tlb_pending=%u parser_idle=%u begin_us=%u end_us=%u complete=%u obligations=255 posted_drain=0 prefetch_retired=0 pipeline_flushed=0 tlb_writes=0 reset_writes=0 engine_writes=0 cache_ownership=0 reader_retirement=0 managed_pages=0\n",prefix,(uint64_t)r->groups,(uint64_t)r->samples,(uint64_t)r->contexts,(uint64_t)r->reads,(uint64_t)r->reason,(uint64_t)r->stable,(uint64_t)r->enabled,(uint64_t)r->pending,(uint64_t)r->flip_wait,(uint64_t)r->tlb_pending,(uint64_t)r->parser_idle,r->begin_us,r->end_us,(uint64_t)r->complete);
}

static void completion_report(const struct intel_completion_result *r){completion_report_named(r,"intel-completion");}
