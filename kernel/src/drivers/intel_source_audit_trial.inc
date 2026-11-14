/* v35 only: observations inside the original all-five held child. No grant. */
static bool source_audit_fail(unsigned reason) {
 source_audit_native.reason|=reason;source_audit_native.state=3;return false;
}
static bool source_audit_live(void) {
 const struct intel_source_audit_native *a=&source_audit_native;
 const struct intel_source_engines *r=&source_engines_native.result;
 return a->object==a && a->parent==r && a->provider==&nonzero_native && a->state==1 && a->registered && !a->complete && !a->reason &&
  a->epoch==r->epoch && a->kind==r->kind && a->mode==r->post_mode && a->origin==r->origin && a->parent_begin==r->parent_begin &&
  a->samples<3 && a->pci_reads<=18 && a->plane_reads<=54 && a->cache_checks<=2 && a->range_checks<=4 &&
  r->state==ISE_HELD && r->child_entered && !r->child_complete && r->samples==2 && r->writes==5 && r->acks==5 &&
  source_engines_native.issued==5 && intel_source_engines_live(r);
}
static bool source_audit_tick(void) {
 struct intel_source_audit_native *a=&source_audit_native;
 if(!source_audit_live())return source_audit_fail(1);
 unsigned samples=a->samples,pc=a->pci_reads,pr=a->plane_reads,cc=a->cache_checks;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at=0;bool injected=intel_display_test_source_audit_clock(&at);
 if(!injected)at=source_engines_now(&nonzero_native);
#else
 uint64_t at=source_engines_now(&nonzero_native);
#endif
 if(!source_audit_live() || a->samples!=samples || a->pci_reads!=pc || a->plane_reads!=pr || a->cache_checks!=cc)return source_audit_fail(1);
 if(!at || at<a->last || !intel_source_engines_deadline(&source_engines_native.result,at)) {
  a->clock_at=at;a->clock_prev=a->last;return source_audit_fail(2);
 }
 a->last=at;return true;
}
static bool source_audit_guard(void) {
 if(!source_audit_tick())return false;
 if(!source_engines_guard(&nonzero_native) || !source_audit_live())return source_audit_fail(4);
 return source_audit_tick();
}
static bool source_audit_run(void) {
 struct intel_source_audit_native *a=&source_audit_native;struct intel_source_engines *r=&source_engines_native.result;
 if(a->object)return false;
 a->object=a;a->parent=r;a->provider=&nonzero_native;a->epoch=r->epoch;a->kind=r->kind;a->mode=r->post_mode;
 a->origin=r->origin;a->parent_begin=r->parent_begin;a->state=1;a->registered=true;a->last=r->last;a->begin=r->last;
 static const unsigned offsets[9]={0x180,0x19c,0x1ac,0x80,0x84,0xac,0x280,0x29c,0x2ac};
 if(nonzero_native.r.reader_count!=37 || !source_audit_guard())return source_audit_fail(1);
 for(unsigned n=0;n<2;n++) {
  a->sample_begin[n]=a->last;
  for(unsigned i=0;i<9;i++) {
   if(!intel_pci_requester_prefix(a->pci[n],a->pci_count[n]) || !source_audit_guard())return source_audit_fail(8);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   bool ok=intel_display_test_source_audit_pci(i,&a->pci[n][i]);
#else
   a->pci[n][i]=pci_read_config32(nonzero_native.a->fw.gt.a.common.gpu,(uint8_t)intel_pci_requester_offsets[i]);bool ok=true;
#endif
   if(!ok)return source_audit_fail(8);
   ++a->pci_count[n];++a->pci_reads;
   if(!source_audit_tick())return false;
   if(!intel_pci_requester_prefix(a->pci[n],a->pci_count[n]) || (i<8 && a->pci[n][i]!=source_requester_native.result.words[1][i]) ||
      (i==1 && (a->pci[n][i]&65535)!=3) || (i==8 && a->pci[n][i]))return source_audit_fail(16);
  }
  for(unsigned p=0;p<3;p++) {
   /* Same non-clearing nine-DWORD group as the admitted reader provider.
    * Fresh full original-source/root/maps guard encloses each exact page;
    * each load also has original-deadline and live pure admission. */
   if(!source_audit_guard() || !completion_mapping(nonzero_native.a,10+p))return source_audit_fail(4);
   for(unsigned i=0;i<9;i++) {
    if(!source_audit_tick())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
    bool ok=intel_display_test_source_audit_plane(p,i,&a->planes[n][p][i]);
#else
    a->planes[n][p][i]=*(volatile const uint32_t *)(WAKE_VA+(10+p)*4096+offsets[i]);bool ok=true;
#endif
    if(!ok)return source_audit_fail(8);
    ++a->plane_count[n][p];++a->plane_reads;
    if(!source_audit_tick())return false;
    if(a->planes[n][p][i]==UINT32_MAX || a->planes[n][p][i]!=nonzero_native.r.readers.planes[p][i])return source_audit_fail(16);
   }
   if(!source_audit_guard())return false;
  }
  if(!source_audit_guard())return false;
  struct x86_cache_snapshot *current=&a->cache_context[n];
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool cache_ok=intel_display_test_source_audit_cache(current);
#else
  bool cache_ok=intel_cache_probe_context(current);
#endif
  a->cache_begin[n]=current->begin_us;a->cache_end[n]=current->end_us;a->cache_reads[n]=current->reads;
  if(!source_audit_guard())return false;
  if(!cache_ok || current->status!=CACHE_OBSERVED || current->count>X86_CACHE_VARIABLES || current->fixed_count>X86_CACHE_FIXED ||
     current->reads!=6+2*current->count+current->fixed_count || current->begin_us<a->sample_begin[n] ||
     current->end_us<=current->begin_us || current->end_us>a->last)return source_audit_fail(32);
  a->cache_ok[n]=true;++a->cache_checks;
  for(unsigned b=0;b<2;b++) {
   if(!source_audit_tick() || !intel_source_buffers_live(&buffers_native.binding))return source_audit_fail(64);
   struct intel_source_audit_range *q=&a->range[n][b];
   q->physical=buffers_native.binding.physical[b];
   q->pages=buffers_native.memory[b].layout.pages;q->index=0;
   if(q->physical!=buffers_native.memory[b].physical || q->pages!=1356 ||
      !x86_cache_range(current,q->physical,q->pages,q->index,&q->mtrr,&q->pat,&q->effective,&q->checks) ||
      q->mtrr!=6 || q->pat!=6 || q->effective!=6)return source_audit_fail(64);
   ++a->range_checks;
   if(!source_audit_tick())return false;
  }
  if(!source_audit_guard())return false;
  a->sample_end[n]=a->last;++a->samples;
 }
 (void)offsets;
 if(!source_audit_guard() || a->samples!=2 || a->pci_reads!=18 || a->plane_reads!=54 || a->cache_checks!=2 || a->range_checks!=4 || a->last<=a->begin)return source_audit_fail(1);
 a->end=a->last;a->state=2;a->registered=false;a->complete=true;return true;
}
/* Pure original-result gate used after the parent's final restore clock. */
static bool source_audit_returned(void) {
 const struct intel_source_audit_native *a=&source_audit_native;
 const struct intel_source_engines *r=&source_engines_native.result;
 if(a->object!=a || a->parent!=r || a->provider!=&nonzero_native || a->state!=2 || !a->complete || a->registered || a->reason || a->clock_at || a->clock_prev ||
    a->epoch!=r->epoch || a->kind!=r->kind || a->mode!=r->post_mode || a->origin!=r->origin || a->parent_begin!=r->parent_begin ||
    a->samples!=2 || a->pci_reads!=18 || a->plane_reads!=54 || a->cache_checks!=2 || a->range_checks!=4 ||
    r->object!=r || !r->complete || !r->child_complete || r->registered || r->state!=ISE_CLOSED ||
    !a->begin || a->begin<r->child_begin || a->end!=a->last || a->end<=a->begin || a->end>r->child_end)return false;
 uint64_t previous=a->begin;
 for(unsigned n=0;n<2;n++) {
  if(a->pci_count[n]!=9 || !a->cache_ok[n] || !a->cache_reads[n] || a->sample_begin[n]<previous ||
     a->cache_begin[n]<a->sample_begin[n] || a->cache_end[n]<=a->cache_begin[n] || a->cache_end[n]>a->sample_end[n] || a->sample_end[n]>a->end)return false;
  for(unsigned i=0;i<9;i++)if(i<8?a->pci[n][i]!=source_requester_native.result.words[1][i]:a->pci[n][i]!=0)return false;
  for(unsigned p=0;p<3;p++) {
   if(a->plane_count[n][p]!=9)return false;
   for(unsigned i=0;i<9;i++)if(a->planes[n][p][i]!=nonzero_native.r.readers.planes[p][i])return false;
  }
  for(unsigned b=0;b<2;b++) {
   const struct intel_source_audit_range *q=&a->range[n][b],*first=&a->range[0][b];
   if(q->physical!=buffers_native.binding.physical[b] || q->physical!=buffers_native.memory[b].physical ||
      q->pages!=1356 || q->pages!=buffers_native.memory[b].layout.pages || q->index ||
      q->mtrr!=6 || q->pat!=6 || q->effective!=6 || !q->checks || q->checks>1+2*a->cache_context[n].count ||
      q->checks!=first->checks)return false;
  }
  previous=a->sample_end[n];
 }
 for(unsigned n=0;n<2;n++)if(a->cache_context[n].status!=CACHE_OBSERVED || a->cache_context[n].reads!=a->cache_reads[n] ||
    a->cache_context[n].begin_us!=a->cache_begin[n] || a->cache_context[n].end_us!=a->cache_end[n])return false;
 return x86_cache_equal(&a->cache_context[0],&a->cache_context[1]);
}
static void source_audit_u32(uint8_t *out,unsigned *k,uint32_t v){for(unsigned i=0;i<4;i++)out[(*k)++]=(uint8_t)(v>>(8*i));}
static void source_audit_u64(uint8_t *out,unsigned *k,uint64_t v){for(unsigned i=0;i<8;i++)out[(*k)++]=(uint8_t)(v>>(8*i));}
static void source_audit_report(void) {
 const struct intel_source_audit_native *a=&source_audit_native;static uint8_t raw[528];static char encoded[705];unsigned k=0;
 for(unsigned n=0;n<2;n++) {
  for(unsigned i=0;i<9;i++)source_audit_u32(raw,&k,a->pci[n][i]);
  for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)source_audit_u32(raw,&k,a->planes[n][p][i]);
  source_audit_u64(raw,&k,a->sample_begin[n]);source_audit_u64(raw,&k,a->sample_end[n]);
  source_audit_u64(raw,&k,a->cache_begin[n]);source_audit_u64(raw,&k,a->cache_end[n]);source_audit_u32(raw,&k,a->cache_reads[n]);
  source_audit_u32(raw,&k,a->pci_count[n]);for(unsigned p=0;p<3;p++)source_audit_u32(raw,&k,a->plane_count[n][p]);source_audit_u32(raw,&k,a->cache_ok[n]);
  for(unsigned b=0;b<2;b++) {
   const struct intel_source_audit_range *q=&a->range[n][b];source_audit_u64(raw,&k,q->physical);
   source_audit_u32(raw,&k,q->pages);source_audit_u32(raw,&k,q->index);source_audit_u32(raw,&k,q->mtrr);
   source_audit_u32(raw,&k,q->pat);source_audit_u32(raw,&k,q->effective);source_audit_u32(raw,&k,q->checks);
  }
 }
 entry_requester_base64(raw,k,encoded);
 console_printf("intel-source-audit-data: v=2 b=%s\n",encoded);
 console_printf("intel-source-audit-summary: v=2 state=%u reason=%u epoch=%u kind=%u mode=%u origin=%u parent=%u begin=%u last=%u end=%u clock=%u previous=%u samples=%u pci_reads=%u plane_reads=%u cache_checks=%u range_checks=%u registered=%u complete=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
 (uint64_t)a->state,(uint64_t)a->reason,(uint64_t)a->epoch,(uint64_t)a->kind,(uint64_t)a->mode,a->origin,a->parent_begin,a->begin,a->last,a->end,a->clock_at,a->clock_prev,
 (uint64_t)a->samples,(uint64_t)a->pci_reads,(uint64_t)a->plane_reads,(uint64_t)a->cache_checks,(uint64_t)a->range_checks,(uint64_t)a->registered,(uint64_t)a->complete);
}
static void source_audit_reset_report(const struct nonzero_adapter *p) {
 const struct intel_nonzero_reset *r=&p->r;static uint8_t raw[528];static char encoded[705];unsigned k=0;
 for(unsigned n=0;n<12;n++) {
  source_audit_u32(raw,&k,n<10?r->ack[n]:r->reset_ack[n-10]);source_audit_u32(raw,&k,n<10?r->final[n]:r->reset_final[n-10]);source_audit_u32(raw,&k,n<10?r->polls[n]:r->reset_polls[n-10]);
  source_audit_u64(raw,&k,n<10?r->control_begin[n]:r->pulse_begin[n-10]);source_audit_u64(raw,&k,n<10?r->control_end[n]:r->pulse_end[n-10]);
  source_audit_u64(raw,&k,p->store_at[n]);source_audit_u64(raw,&k,p->store_previous[n]);
 }
 entry_requester_base64(raw,k,encoded);console_printf("intel-source-audit-reset: v=1 b=%s\n",encoded);
}
