/* Same-borrow PCI observation inside the maintained five-engine prepare action.
 * Only the enclosing existing prepare protocol writes engine controls. */
struct held_requester_adapter {struct engine_prepare_adapter *p;unsigned next;};
static bool eheld_guard(void *ctx) {return eprepare_guard(((struct held_requester_adapter *)ctx)->p);}
static uint64_t eheld_now(void *ctx) {return eprepare_now(((struct held_requester_adapter *)ctx)->p);}
static bool eheld_read(void *ctx,uint32_t offset,uint32_t *out) {
 struct held_requester_adapter *h=ctx;struct engine_prepare_adapter *p=h->p;
 struct wake_adapter *a=p->a;const struct intel_pci_held_result *r=&p->trial->requester;
 if(!out || h->next>=2*INTEL_PCI_REQUESTER_WORDS ||
    offset!=intel_pci_requester_offsets[h->next%INTEL_PCI_REQUESTER_WORDS] ||
    !eprepare_guard(p) || !completion_mapping(a,0) || !completion_mapping(a,1) ||
    !completion_mapping(a,2) || !engine_requesters(a))return false;
 /* Provider admission must not consume the remaining shared100ms allowance
  * before the load. Core checks both sides again and retains its first failure. */
 uint64_t at=eprepare_now(p);
 if(!at || at<r->last_us || at<r->origin_us ||
    at-r->origin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_held_read(offset,out);
#else
 *out=pci_read_config32(a->fw.gt.a.common.gpu,(uint8_t)offset);bool ok=true;
#endif
 if(ok)++h->next;
 return ok;
}
static bool eheld_action(void *ctx,const struct intel_engine_prepare_result *r) {
 struct engine_prepare_adapter *p=ctx;
 if(r!=&p->trial->action || !intel_engine_prepare_action_live(r))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_held_boundary(false);
#endif
 if(!eprepare_enter(p))return false;
 p->trial->held_provider_enter=true;
 struct held_requester_adapter h={.p=p};
 bool captured=intel_pci_held_observe(&p->trial->requester,r,
  (struct intel_pci_held_io){&h,eheld_guard,eheld_read,eheld_now});
 /* A synchronous full census brackets the entire action, never cached across
  * callbacks. Per-load guards remain fresh without36 full backing censuses. */
 #ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_held_boundary(true);
#endif
 eprepare_exit(p);
 p->trial->held_provider_exit=!p->failed && !p->a->uncertain;
 if(p->failed || p->a->uncertain) {
  p->trial->requester.reason|=IPH_GUARD;p->trial->requester.complete=false;
  return false;
 }
 return captured;
}
static void eheld_report(const struct intel_pci_held_result *r) {
 for(unsigned n=0;n<2;n++) {
  console_printf("intel-held-requester-row: v=1 s=%u n=%u d=",(uint64_t)n,(uint64_t)r->counts[n]);
  if(!r->counts[n])console_write("-");
  for(unsigned i=0;i<r->counts[n] && i<INTEL_PCI_REQUESTER_WORDS;i++)console_printf("%s%u",i?",":"",(uint64_t)r->words[n][i]);
  console_write("\n");
 }
 console_printf("intel-held-requester-summary: v=1 origin=%u begin=%u end=%u epoch=%u attempts=%u reads=%u samples=%u profiles=%u n0=%u n1=%u stable=%u bus_master=%u pending=%u flr=%u reason=%u clock_at=%u clock_prev=%u complete=%u obligations=255 config_writes=0 reset_writes=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",r->origin_us,r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)r->attempts,(uint64_t)r->reads,(uint64_t)r->samples,(uint64_t)r->profiles,(uint64_t)r->counts[0],(uint64_t)r->counts[1],(uint64_t)r->stable,(uint64_t)r->bus_master,(uint64_t)r->pending,(uint64_t)r->flr,(uint64_t)r->reason,r->clock_at,r->clock_prev,(uint64_t)r->complete);
}

/* Lossless fixed-width raw encoding keeps both full46-DWORD preservation
 * vectors inside the existing log tail budget, including its spare allowance.
 * Other images retain their existing row format byte for byte. */
static void eheld_post_report(const struct intel_completion_result *r) {
 static const char hex[]="0123456789abcdef";
 for(unsigned n=0;n<2;n++) {
  unsigned groups=r->groups>n*5?r->groups-n*5:0;
  if(groups>5)groups=5;
  unsigned count=groups*INTEL_COMPLETION_WORDS+(n<r->contexts?INTEL_COMPLETION_CONTEXT:0);
  if(!count)continue;
  char raw[(INTEL_ENGINE_COUNT*INTEL_COMPLETION_WORDS+INTEL_COMPLETION_CONTEXT)*8+1];
  for(unsigned i=0;i<count;i++) {
   uint32_t value=i<groups*INTEL_COMPLETION_WORDS?
    r->words[n][i/INTEL_COMPLETION_WORDS][i%INTEL_COMPLETION_WORDS]:r->context[n][i-groups*INTEL_COMPLETION_WORDS];
   for(unsigned j=0;j<8;j++)raw[i*8+j]=hex[(value>>(28-j*4))&15];
  }
  raw[count*8]=0;
  console_printf("intel-prepare-post-vector: v=3 s=%u n=%u h=%s\n",(uint64_t)n,(uint64_t)count,raw);
 }
 completion_report_summary(r,"intel-prepare-post");
}
