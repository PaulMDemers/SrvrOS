/* Separate image only. PCI Command halfword, never Status or AF control. */
static struct {
 struct intel_gpu_requester_hold result;
 struct engine_prepare_adapter *p;
 unsigned issued,pci_reads,admit_reads;
 uint32_t admission[2];
 uint16_t expected;
 bool uncertain;
} gpu_hold_native;
static bool gpu_hold_identity_command(uint32_t observed,uint32_t original) {
 const struct intel_gpu_requester_hold *r=&gpu_hold_native.result;
 const struct engine_prepare_adapter *p=gpu_hold_native.p;
 return
#ifdef SRVROS_INTEL_ENTRY_REQUESTER
  entry_requester_source(p) &&
#endif
  original==7 && observed==3 && gpu_hold_native.expected==3 &&
  gpu_hold_native.issued==1 && !gpu_hold_native.uncertain &&
  p && r->claim==&p->trial->claim && r->borrow.claim==r->claim &&
  intel_gpu_requester_hold_live(r) && intel_hda_gpu_live(&joint.scope) &&
  joint.scope.claim==r->claim && joint.scope.gpu==p->g &&
  wake_irq_off() && vmm_current_address_space()==p->a->fw.gt.a.common.cr3;
}
static bool gpu_hold_native_live(struct engine_prepare_adapter *p) {
 return
#ifdef SRVROS_INTEL_ENTRY_REQUESTER
  entry_requester_source(p) &&
#endif
  p && p==gpu_hold_native.p && !gpu_hold_native.uncertain && !p->failed && !p->a->uncertain &&
  !trial_mapping_fault && wake_irq_off() && intel_gpu_requester_hold_live(&gpu_hold_native.result) &&
  gpu_hold_native.result.claim==&p->trial->claim && joint.scope.claim==&p->trial->claim &&
  joint.scope.gpu==p->g && intel_hda_gpu_live(&joint.scope) &&
  intel_boot_check(INTEL_BOOT_OBSERVER,p->a->fw.gt.a.common.gpu) &&
  vmm_current_address_space()==p->a->fw.gt.a.common.cr3 &&
  completion_mapping(p->a,0) && completion_mapping(p->a,1) && completion_mapping(p->a,2) &&
  completion_mapping(p->a,13) && engine_requesters(p->a);
}
static bool gpu_hold_issue_time(struct engine_prepare_adapter *p) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_gpu_hold_admission();
#endif
 if(!gpu_hold_native_live(p))return false;
 uint64_t at=eprepare_now(p);
 const struct intel_gpu_requester_hold *r=&gpu_hold_native.result;
 return intel_gpu_requester_hold_deadline(&gpu_hold_native.result,at) && erehold_time(p,at) &&
  (r->state!=IGH_INHIBITED || (at>=r->drain_begin_us && at-r->drain_begin_us<=INTEL_GPU_HOLD_PHASE_US));
}
static bool gpu_hold_read(struct engine_prepare_adapter *p,unsigned offset,uint32_t *out) {
 if(!out || !gpu_hold_issue_time(p))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_gpu_hold_read(offset,out);
#else
 *out=pci_read_config32(p->a->fw.gt.a.common.gpu,(uint8_t)offset);bool ok=true;
#endif
 if(ok)++gpu_hold_native.pci_reads;
 return ok;
}
static bool gpu_hold_sample(void *ctx,uint32_t out[9],unsigned *count) {
 struct engine_prepare_adapter *p=ctx;
 if(!out || !count || !gpu_hold_native_live(p))return false;
 *count=0;
 for(unsigned i=0;i<INTEL_PCI_REQUESTER_WORDS;i++) {
  if(!intel_pci_requester_prefix(out,*count) || !gpu_hold_read(p,intel_pci_requester_offsets[i],&out[i]))return false;
  ++*count;
  if(!intel_pci_requester_prefix(out,*count))return false;
 }
 return true;
}
static bool gpu_hold_pending(void *ctx,uint32_t out[2],unsigned *count) {
 struct engine_prepare_adapter *p=ctx;
 if(!out || !count || !gpu_hold_native_live(p))return false;
 *count=0;
 if(!gpu_hold_read(p,4,&out[0]))return false;
 ++*count;
 if(out[0]!=gpu_hold_native.result.words[1][1] || !gpu_hold_read(p,0xa8,&out[1]))return false;
 ++*count;return true;
}
static enum intel_gt_write gpu_hold_command16(void *ctx,uint16_t value) {
 struct engine_prepare_adapter *p=ctx;const struct intel_gpu_requester_hold *r=&gpu_hold_native.result;
 unsigned n=gpu_hold_native.issued;
 if(!gpu_hold_native_live(p) || n>=2 || r->attempts!=n+1 || r->writes!=n ||
    !((n==0 && value==3 && r->state==IGH_INHIBITING && gpu_hold_native.expected==7) ||
      (n==1 && value==7 && r->state==IGH_RESTORING && gpu_hold_native.expected==3)))return GT_WRITE_NONE;
 if(!gpu_hold_read(p,4,&gpu_hold_native.admission[n]))return GT_WRITE_NONE;
 ++gpu_hold_native.admit_reads;
 if(gpu_hold_native.admission[n]!=r->words[n][1] || !gpu_hold_issue_time(p))return GT_WRITE_NONE;
 /* The immediate sample above is the final admission before the exact store.
  * The core checks time/source again and owns subsequent fresh raw readback. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write result=intel_display_test_gpu_hold_command16(p->a->fw.gt.a.common.gpu,4,value);
#else
 pci_write_config16(p->a->fw.gt.a.common.gpu,4,value);
 enum intel_gt_write result=GT_WRITE_DONE;
#endif
 if(result==GT_WRITE_DONE){++gpu_hold_native.issued;gpu_hold_native.expected=value;}
 if(result==GT_WRITE_UNCERTAIN)gpu_hold_native.uncertain=true;
 return result;
}
static bool gpu_hold_run(struct engine_prepare_adapter *p) {
 if(gpu_hold_native.p)return false;
 gpu_hold_native.p=p;gpu_hold_native.expected=7;
 bool ok=intel_gpu_requester_hold_run(&gpu_hold_native.result,&p->trial->claim,
  (struct intel_gpu_hold_io){p,gpu_hold_sample,gpu_hold_command16,gpu_hold_pending,eprepare_now,eprepare_pause});
 if(!ok){gpu_hold_native.uncertain=true;p->failed=true;p->a->uncertain=true;}
 return ok;
}
static void gpu_hold_report(void) {
 const struct intel_gpu_requester_hold *r=&gpu_hold_native.result;
 static uint8_t raw[700];static char encoded[937];unsigned n=0,k=0;
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)for(unsigned b=0;b<4;b++)raw[n++]=(uint8_t)(r->words[p][i]>>(8*b));
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<2;i++)for(unsigned b=0;b<8;b++)raw[n++]=(uint8_t)((i?r->sample_end[p]:r->sample_begin[p])>>(8*b));
 for(unsigned p=0;p<INTEL_GPU_HOLD_POLLS;p++) {
  for(unsigned i=0;i<2;i++)for(unsigned b=0;b<4;b++)raw[n++]=(uint8_t)(r->poll_words[p][i]>>(8*b));
  for(unsigned b=0;b<8;b++)raw[n++]=(uint8_t)(r->poll_us[p]>>(8*b));
  raw[n++]=(uint8_t)r->poll_counts[p];
 }
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
 for(unsigned i=0;i<n;i+=3) {
  unsigned left=n-i;uint32_t v=(uint32_t)raw[i]<<16;
  if(left>1)v|=(uint32_t)raw[i+1]<<8;
  if(left>2)v|=raw[i+2];
  encoded[k++]=abc[v>>18];encoded[k++]=abc[(v>>12)&63];encoded[k++]=left>1?abc[(v>>6)&63]:'=';encoded[k++]=left>2?abc[v&63]:'=';
 }
 encoded[k]=0;
 console_printf("intel-gpu-hold-data: v=1 b=%s\n",encoded);
#ifdef SRVROS_INTEL_ENTRY_REQUESTER
 const uint64_t values[]={1,(uint64_t)r->state,(uint64_t)r->reason,(uint64_t)r->phase,(uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->polls,(uint64_t)r->counts[0],(uint64_t)r->counts[1],(uint64_t)r->counts[2],r->origin_us,r->owner_origin_us,r->begin_us,r->last_us,r->end_us,r->clock_at,r->clock_prev,r->drain_begin_us,(uint64_t)r->restored,(uint64_t)r->returned,(uint64_t)r->complete,(uint64_t)r->borrow.active,(uint64_t)r->borrow.fault,(uint64_t)r->borrow.epoch,(uint64_t)r->borrow.sequence,(uint64_t)gpu_hold_native.issued,(uint64_t)gpu_hold_native.pci_reads,(uint64_t)gpu_hold_native.admit_reads,(uint64_t)gpu_hold_native.admission[0],(uint64_t)gpu_hold_native.admission[1],(uint64_t)gpu_hold_native.uncertain,(uint64_t)(gpu_hold_native.p?gpu_hold_native.p->trial->claim.borrows:0),(uint64_t)(gpu_hold_native.p?gpu_hold_native.p->trial->claim.returns:0),(uint64_t)(r->borrow.object==&r->borrow),(uint64_t)(gpu_hold_native.p && r->borrow.claim==&gpu_hold_native.p->trial->claim),255,0,0,0,0};
 uint8_t bytes[sizeof values];char data[((sizeof(values)+2)/3)*4+1];
 for(unsigned i=0;i<sizeof(values)/sizeof(values[0]);i++)
  for(unsigned b=0;b<8;b++)bytes[i*8+b]=(uint8_t)(values[i]>>(8*b));
 entry_requester_base64(bytes,sizeof bytes,data);
 console_printf("intel-gpu-hold-summary: v=2 b=%s\n",data);
#else
 console_printf("intel-gpu-hold-summary: v=1 state=%u reason=%u phase=%u samples=%u reads=%u attempts=%u writes=%u polls=%u n0=%u n1=%u n2=%u origin=%u owner_origin=%u begin=%u last=%u end=%u clock_at=%u clock_prev=%u drain_begin=%u restored=%u returned=%u complete=%u borrow_active=%u borrow_fault=%u borrow_epoch=%u borrow_sequence=%u issued=%u pci_reads=%u admit_reads=%u admit0=%u admit1=%u uncertain=%u claim_borrows=%u claim_returns=%u borrow_object=%u borrow_claim=%u obligations=255 assurances=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",
  (uint64_t)r->state,(uint64_t)r->reason,(uint64_t)r->phase,(uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->polls,(uint64_t)r->counts[0],(uint64_t)r->counts[1],(uint64_t)r->counts[2],r->origin_us,r->owner_origin_us,r->begin_us,r->last_us,r->end_us,r->clock_at,r->clock_prev,r->drain_begin_us,(uint64_t)r->restored,(uint64_t)r->returned,(uint64_t)r->complete,(uint64_t)r->borrow.active,(uint64_t)r->borrow.fault,(uint64_t)r->borrow.epoch,(uint64_t)r->borrow.sequence,(uint64_t)gpu_hold_native.issued,(uint64_t)gpu_hold_native.pci_reads,(uint64_t)gpu_hold_native.admit_reads,(uint64_t)gpu_hold_native.admission[0],(uint64_t)gpu_hold_native.admission[1],(uint64_t)gpu_hold_native.uncertain,(uint64_t)(gpu_hold_native.p?gpu_hold_native.p->trial->claim.borrows:0),(uint64_t)(gpu_hold_native.p?gpu_hold_native.p->trial->claim.returns:0),(uint64_t)(r->borrow.object==&r->borrow),(uint64_t)(gpu_hold_native.p && r->borrow.claim==&gpu_hold_native.p->trial->claim));
#endif
}
