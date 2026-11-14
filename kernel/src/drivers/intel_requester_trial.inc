/* PCI-only requester observation. No function/BME/reset/PTE writer. */
#include <srvros/intel_pci_requester.h>
static bool requester_sample(void *ctx,uint32_t out[INTEL_PCI_REQUESTER_WORDS],unsigned *count) {
 struct wake_adapter *a=ctx;
 if(!out || !count || !engine_guard(a))return false;
 *count=0;
 for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS;i++) {
  if(!completion_mapping(a,0) || !completion_mapping(a,1) || !completion_mapping(a,2) || !engine_requesters(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_requester_read(i,intel_pci_requester_offsets[i],&out[i]))return false;
#else
  out[i]=pci_read_config32(a->fw.gt.a.common.gpu,(uint8_t)intel_pci_requester_offsets[i]);
#endif
  ++*count;
  if(!intel_pci_requester_prefix(out,*count))return false;
 }
 /* Preserve the read prefix. A lost full exit poisons native ownership rather
  * than accepting a sample or freeing an alias through uncertainty. */
 if(!engine_guard(a)){a->uncertain=true;return false;}
 return true;
}
static void requester_report(const struct intel_pci_requester_result *r) {
 for(unsigned n=0;n<r->attempts && n<2;n++) {
  console_printf("intel-requester-row: v=1 s=%u n=%u d=",(uint64_t)n,(uint64_t)r->counts[n]);
  if(!r->counts[n])console_write("-");
  for(unsigned i=0;i<r->counts[n] && i<INTEL_PCI_REQUESTER_WORDS;i++)console_printf("%s%u",i?",":"",(uint64_t)r->words[n][i]);
  console_write("\n");
 }
 console_printf("intel-requester-summary: v=1 attempts=%u samples=%u profiles=%u reads=%u begin=%u end=%u epoch=%u blocked=%u borrowed=%u returned=%u retained=%u stable=%u bus_master=%u pending=%u flr=%u reason=%u clock_at=%u clock_prev=%u complete=%u obligations=255 config_writes=0 reset_writes=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",(uint64_t)r->attempts,(uint64_t)r->samples,(uint64_t)r->profiles,(uint64_t)r->reads,r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)r->blocked,(uint64_t)(r->borrow.parent!=NULL),(uint64_t)r->returned,(uint64_t)r->borrow.active,(uint64_t)r->stable,(uint64_t)r->bus_master,(uint64_t)r->pending,(uint64_t)r->flr,(uint64_t)r->reason,r->clock_at,r->clock_prev,(uint64_t)r->complete);
}
