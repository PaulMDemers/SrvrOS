/* Exact-image BCS8 reset inside the original maintained preparation borrow.
 * Full backing/endpoint entry and exit bracket this synchronous operation;
 * every target access still admits current aliases/requesters/shared clock. */
static bool ehr_guard(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
 return intel_engine_prepare_reset_live(&p->trial->reset) && eprepare_guard(p);
}
static uint64_t ehr_now(void *ctx) {return eprepare_now(ctx);}
static bool ehr_access(struct engine_prepare_adapter *p,unsigned slot) {
 const struct intel_engine_held_reset_result *r=&p->trial->reset;
 if(!ehr_guard(p) || !completion_mapping(p->a,slot) || !engine_requesters(p->a))return false;
 uint64_t at=eprepare_now(p);
 return at && at>=r->last_us && at>=r->origin_us && at-r->origin_us<=INTEL_ENGINE_PREPARE_TOTAL_US;
}
static bool ehr_begin(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_held_reset_boundary(false);
#endif
 return ehr_guard(p) && eprepare_enter(p);
}
static bool ehr_end(void *ctx) {
 struct engine_prepare_adapter *p=ctx;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_held_reset_boundary(true);
#endif
 eprepare_exit(p);
 return !p->failed && !p->a->uncertain;
}
static bool ehr_snapshot(void *ctx,uint32_t out[8],uint32_t *domain,unsigned *count) {
 struct engine_prepare_adapter *p=ctx;
 static const uint32_t regs[9]={0x22030,0x22034,0x22038,0x2203c,0x22234,0x22238,0x2209c,0x2229c,0x941c};
 if(!out || !domain || !count || *count || p->reset_snapshot_reads>=18 || p->reset_snapshot_reads%9)return false;
 for(unsigned i=0;i<9;i++) {
  unsigned slot=i==8?9:7;
  if(!ehr_access(p,slot))return false;
  uint32_t value;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_held_reset_snapshot_read(i,regs[i],&value))return false;
#else
  value=*(volatile const uint32_t *)(WAKE_VA+slot*4096+(regs[i]&4095));
#endif
  if(i==8)*domain=value;else out[i]=value;
  ++*count;++p->reset_snapshot_reads;
  if(value==UINT32_MAX)return false;
 }
 return true;
}
static bool ehr_domain(void *ctx,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;
 if(!out || !ehr_access(p,9))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_held_reset_domain(out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+9*4096+0x41c);return true;
#endif
}
static enum intel_gt_write ehr_transition(void *ctx,uint32_t value,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;const struct intel_engine_held_reset_result *r=&p->trial->reset;
 if(!out || value!=INTEL_BCS_RESET_DOMAIN || r->stage!=2 || !r->epoch_advanced ||
    r->epoch_before==UINT32_MAX || r->epoch_after!=r->epoch_before+1 ||
    p->g->engine_epoch!=r->epoch_after || p->reset_pulses>=2 ||
    r->attempts!=p->reset_pulses+1 || !ehr_access(p,9))return GT_WRITE_NONE;
 /* No callback or target load is between this final same-origin clock and
  * the sole DWORD store. Native code never writes another GDRST domain. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_held_reset_admission();
#endif
 uint64_t at=eprepare_now(p);
 if(!at || at<r->last_us || at<r->origin_us || at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return GT_WRITE_NONE;
 ++p->reset_pulses;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_held_reset_transition(value,out);
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+9*4096+0x41c);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");
 if(!engine_requesters(p->a))return GT_WRITE_UNCERTAIN;
 *out=*reg;return GT_WRITE_DONE;
#endif
}
static void ehr_report(const struct intel_engine_held_reset_result *r) {
 for(unsigned n=0;n<2;n++) {
  console_printf("intel-held-reset-row: v=1 s=%u n=%u d=",(uint64_t)n,(uint64_t)r->counts[n]);
  if(!r->counts[n])console_write("-");
  for(unsigned i=0;i<r->counts[n] && i<9;i++)console_printf("%s%u",i?",":"",(uint64_t)(i==8?r->domains[n?3:0]:r->words[n][i]));
  console_write("\n");
 }
 for(unsigned n=0;n<r->writes && n<2;n++)console_printf("intel-held-reset-ack: v=1 n=%u d=%u,%u,%u begin=%u end=%u\n",(uint64_t)n,(uint64_t)r->ack[n],(uint64_t)r->final[n],(uint64_t)r->phase_polls[n],r->phase_begin_us[n],r->phase_end_us[n]);
 console_printf("intel-held-reset-summary: v=1 origin=%u begin=%u end=%u epoch_before=%u epoch_after=%u attempts=%u writes=%u reads=%u polls=%u snapshots=%u snapshot_attempts=%u n0=%u n1=%u acks=%u advanced=%u attempted=%u entered=%u active=%u provider_enter=%u provider_exit=%u uncertain=%u stage=%u settle_begin=%u settle_end=%u clock_at=%u clock_prev=%u reason=%u complete=%u obligations=255 pte_writes=0 managed_pages=0 hardware_ownership=0\n",r->origin_us,r->begin_us,r->end_us,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->snapshots,(uint64_t)r->snapshot_attempts,(uint64_t)r->counts[0],(uint64_t)r->counts[1],(uint64_t)r->acks,(uint64_t)r->epoch_advanced,(uint64_t)r->attempted,(uint64_t)r->entered,(uint64_t)r->active,(uint64_t)r->provider_enter,(uint64_t)r->provider_exit,(uint64_t)r->uncertain,(uint64_t)r->stage,r->settle_begin_us,r->settle_end_us,r->clock_at,r->clock_prev,(uint64_t)r->reason,(uint64_t)r->complete);
}
