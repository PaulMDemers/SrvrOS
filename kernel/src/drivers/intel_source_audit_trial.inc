/* v35 held observations and isolated private CPU preparation. No GPU grant. */
#include "x86_private_clean.inc"
static struct private_clean_result source_clean;
static uint64_t source_clean_root;
static struct x86_cache_snapshot source_clean_context;
static bool source_clean_ready;
static bool source_clean_retired=true;
static bool source_clean_bound(bool);
#include "intel_alias_ram.inc"
static struct intel_alias_ram_catalog source_audit_ram;
static bool source_audit_ram_prepare(void) {
 bool ok=intel_alias_ram_prepare(&source_audit_ram,memmap_request.response);
 source_audit_native.inventory_reason=ok?0:VMM_ALIAS_RAM;
 return ok;
}
static bool source_audit_fail(unsigned reason) {
 source_audit_native.reason|=reason;source_audit_native.state=3;return false;
}
static bool source_audit_live(void) {
 const struct intel_source_audit_native *a=&source_audit_native;
 const struct intel_source_engines *r=&source_engines_native.result;
 return a->object==a && a->parent==r && a->provider==&nonzero_native && a->state==1 && a->registered && !a->complete && !a->reason &&
  a->epoch==r->epoch && a->kind==r->kind && a->mode==r->post_mode && a->origin==r->origin && a->parent_begin==r->parent_begin &&
  source_clean_bound(false) && source_audit_ram.ready && source_audit_ram.count && source_audit_ram.count<=VMM_ALIAS_RAM_LIMIT && a->inventory_count<=2 && !a->inventory_reason &&
  a->samples<3 && a->pci_reads<=18 && a->plane_reads<=54 && a->cache_checks<=2 && a->range_checks<=4 && a->alias_checks<=4 && a->root==nonzero_native.a->fw.gt.a.common.cr3 &&
  r->state==ISE_HELD && r->child_entered && !r->child_complete && r->samples==2 && r->writes==5 && r->acks==5 &&
  source_engines_native.issued==5 && intel_source_engines_live(r);
}
static bool source_audit_tick(void) {
 struct intel_source_audit_native *a=&source_audit_native;
 if(!source_audit_live())return source_audit_fail(1);
 unsigned samples=a->samples,pc=a->pci_reads,pr=a->plane_reads,cc=a->cache_checks,rc=a->range_checks,ac=a->alias_checks,ic=a->inventory_count;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at=0;bool injected=intel_display_test_source_audit_clock(&at);
 if(!injected)at=source_engines_now(&nonzero_native);
#else
 uint64_t at=source_engines_now(&nonzero_native);
#endif
 if(!source_audit_live() || a->samples!=samples || a->pci_reads!=pc || a->plane_reads!=pr || a->cache_checks!=cc || a->range_checks!=rc || a->alias_checks!=ac || a->inventory_count!=ic)return source_audit_fail(1);
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
 a->root=nonzero_native.a->fw.gt.a.common.cr3;
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
   q->pages=buffers_native.memory[b].layout.pages;
   q->virtual_base=(uint64_t)pmm_phys_to_virt(q->physical);
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   bool alias_ok=intel_display_test_source_audit_alias(a->root,q->virtual_base,q->physical,q->pages,current->cpu.physical_bits,&q->alias);
#else
   bool alias_ok=vmm_diagnostic_kernel_alias(a->root,q->virtual_base,q->physical,q->pages,current->cpu.physical_bits,&q->alias,NULL);
#endif
   if(!alias_ok || q->alias.pages!=q->pages || !q->alias.leaves || q->alias.leaves>q->pages ||
      !q->alias.leaf_sizes || (q->alias.leaf_sizes&~7u) || q->alias.pat_index>7 ||
      q->alias.writable!=1 || q->alias.user || q->alias.no_execute>1)return source_audit_fail(128);
   ++a->alias_checks;q->index=q->alias.pat_index;
   if(!source_audit_tick())return false;
   if(q->physical!=buffers_native.memory[b].physical || q->pages!=1356 ||
      !x86_cache_range(current,q->physical,q->pages,q->index,&q->mtrr,&q->pat,&q->effective,&q->checks) ||
      q->mtrr!=6 || q->pat!=6 || q->effective!=6)return source_audit_fail(64);
   ++a->range_checks;
   if(!source_audit_tick())return false;
  }
  struct vmm_alias_span expected[2];
  for(unsigned b=0;b<2;b++)expected[b]=(struct vmm_alias_span){a->range[n][b].virtual_base,a->range[n][b].physical,a->range[n][b].pages};
  struct vmm_alias_detail detail={0};
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool unique=intel_display_test_source_audit_inventory(a->root,expected,current->cpu.physical_bits,source_audit_ram.ranges,source_audit_ram.count,&a->inventory[n],&detail);
#else
  bool unique=vmm_diagnostic_unique_kernel_aliases(a->root,expected,2,current->cpu.physical_bits,source_audit_ram.ranges,source_audit_ram.count,&a->inventory[n],&detail);
#endif
  if(!unique){a->inventory_reason=detail.reason;return source_audit_fail(128);}
  const struct vmm_alias_inventory *v=&a->inventory[n];
  if(!v->tables || v->tables>VMM_ALIAS_TABLE_LIMIT || v->entries!=v->tables*512 || !v->leaves || v->leaves>v->entries)return source_audit_fail(128);
  for(unsigned b=0;b<2;b++)if(v->pages[b]!=expected[b].pages || v->matches[b]!=a->range[n][b].alias.leaves)return source_audit_fail(128);
  ++a->inventory_count;
  if(!source_audit_guard())return false;
  a->sample_end[n]=a->last;++a->samples;
 }
 (void)offsets;
 if(!source_audit_guard() || a->samples!=2 || a->pci_reads!=18 || a->plane_reads!=54 || a->cache_checks!=2 || a->range_checks!=4 || a->alias_checks!=4 || a->inventory_count!=2 || a->inventory_reason || a->last<=a->begin)return source_audit_fail(1);
 a->end=a->last;a->state=2;a->registered=false;a->complete=true;return true;
}
/* Pure original-result gate used after the parent's final restore clock. */
static bool source_audit_returned(void) {
 const struct intel_source_audit_native *a=&source_audit_native;
 const struct intel_source_engines *r=&source_engines_native.result;
 if(!source_clean_bound(true) || a->object!=a || a->parent!=r || a->provider!=&nonzero_native || a->state!=2 || !a->complete || a->registered || a->reason || a->clock_at || a->clock_prev ||
    a->epoch!=r->epoch || a->kind!=r->kind || a->mode!=r->post_mode || a->origin!=r->origin || a->parent_begin!=r->parent_begin ||
    a->samples!=2 || a->pci_reads!=18 || a->plane_reads!=54 || a->cache_checks!=2 || a->range_checks!=4 || a->alias_checks!=4 || a->inventory_count!=2 || a->inventory_reason ||
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
      q->pages!=1356 || q->pages!=buffers_native.memory[b].layout.pages ||
      a->root!=nonzero_native.a->fw.gt.a.common.cr3 || q->virtual_base!=(uint64_t)pmm_phys_to_virt(q->physical) ||
      q->virtual_base!=first->virtual_base || q->alias.pages!=q->pages || q->index!=q->alias.pat_index || q->index>7 ||
      !q->alias.leaves || q->alias.leaves>q->pages || q->alias.leaves!=first->alias.leaves ||
      q->alias.writable!=1 || q->alias.user || q->alias.no_execute>1 || q->alias.no_execute!=first->alias.no_execute ||
      !q->alias.leaf_sizes || (q->alias.leaf_sizes&~7u) || q->alias.leaf_sizes!=first->alias.leaf_sizes || q->index!=first->index ||
      q->mtrr!=6 || q->pat!=6 || q->effective!=6 || !q->checks || q->checks>1+2*a->cache_context[n].count ||
      q->checks!=first->checks)return false;
  }
  const struct vmm_alias_inventory *v=&a->inventory[n],*first=&a->inventory[0];
  if(!v->tables || v->tables>VMM_ALIAS_TABLE_LIMIT || v->entries!=512*v->tables || !v->leaves || v->leaves>v->entries ||
     v->tables!=first->tables || v->entries!=first->entries || v->leaves!=first->leaves)return false;
  for(unsigned b=0;b<2;b++)if(v->pages[b]!=a->range[n][b].pages || v->matches[b]!=a->range[n][b].alias.leaves)return false;
  previous=a->sample_end[n];
 }
 for(unsigned n=0;n<2;n++)if(a->cache_context[n].status!=CACHE_OBSERVED || a->cache_context[n].reads!=a->cache_reads[n] ||
    a->cache_context[n].begin_us!=a->cache_begin[n] || a->cache_context[n].end_us!=a->cache_end[n])return false;
 return x86_cache_equal(&a->cache_context[0],&a->cache_context[1]);
}
static void source_audit_u32(uint8_t *out,unsigned *k,uint32_t v){for(unsigned i=0;i<4;i++)out[(*k)++]=(uint8_t)(v>>(8*i));}
static void source_audit_u64(uint8_t *out,unsigned *k,uint64_t v){for(unsigned i=0;i<8;i++)out[(*k)++]=(uint8_t)(v>>(8*i));}
static void source_clean_report(void);
static void source_audit_report(void) {
 const struct intel_source_audit_native *a=&source_audit_native;static uint8_t raw[600];static char encoded[801];unsigned k=0;
 source_audit_u64(raw,&k,a->root);
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
   source_audit_u64(raw,&k,q->virtual_base);source_audit_u32(raw,&k,q->alias.leaves);
   source_audit_u32(raw,&k,q->alias.writable | (q->alias.user<<1) | (q->alias.no_execute<<2) | (q->alias.leaf_sizes<<3));
  }
 }
 entry_requester_base64(raw,k,encoded);
 console_printf("intel-source-audit-data: v=3 b=%s\n",encoded);
 console_printf("intel-source-audit-summary: v=3 state=%u reason=%u epoch=%u kind=%u mode=%u origin=%u parent=%u begin=%u last=%u end=%u clock=%u previous=%u samples=%u pci_reads=%u plane_reads=%u cache_checks=%u range_checks=%u alias_checks=%u registered=%u complete=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
 (uint64_t)a->state,(uint64_t)a->reason,(uint64_t)a->epoch,(uint64_t)a->kind,(uint64_t)a->mode,a->origin,a->parent_begin,a->begin,a->last,a->end,a->clock_at,a->clock_prev,
 (uint64_t)a->samples,(uint64_t)a->pci_reads,(uint64_t)a->plane_reads,(uint64_t)a->cache_checks,(uint64_t)a->range_checks,(uint64_t)a->alias_checks,(uint64_t)a->registered,(uint64_t)a->complete);
 admission_report();
 admission_cost_report(nonzero_native.r.begin,nonzero_native.r.end);
 source_clean_report();
 static uint8_t inv[60];static char inv64[81];unsigned pos=4;
 inv[0]=(uint8_t)a->inventory_count;inv[1]=(uint8_t)a->inventory_reason;inv[2]=inv[3]=0;
 for(unsigned n=0;n<2;n++){
  const struct vmm_alias_inventory *v=&a->inventory[n];
  source_audit_u32(inv,&pos,v->tables);source_audit_u32(inv,&pos,v->entries);source_audit_u32(inv,&pos,v->leaves);
  for(unsigned b=0;b<2;b++)source_audit_u32(inv,&pos,v->matches[b]);
  for(unsigned b=0;b<2;b++)source_audit_u32(inv,&pos,v->pages[b]);
 }
 entry_requester_base64(inv,60,inv64);console_printf("intel-alias-census: v=1 b=%s\n",inv64);
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

/* Private held RAM only. This occurs before the original outer wake clocks. */
static bool source_clean_hold(void *ctx) {
 if(ctx!=&source_clean || !wake_irq_off() || vmm_current_address_space()!=source_clean_root || !source_clean_root || buffers_native.prepared!=2)return false;
 for(unsigned i=0;i<2;i++) {
  const struct scanout_memory_hold *h=&buffers_native.hold[i];const struct scanout_memory *m=&buffers_native.memory[i];
  if(!scanout_memory_hold_live(h) || h->lifetime!=&buffers_native || h->memory.object!=m || h->dependent || h->registered ||
     h->physical!=m->physical || h->layout.pages!=1356 || m->layout.pages!=1356 || m->cpu_serial || m->state!=MEM_RAM ||
     m->owner!=0x42554646455253ull || m->space || m->range.object || m->publication.object)return false;
 }
 return true;
}
static uint64_t source_clean_now(void *ctx) {
 (void)ctx;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_private_clean_now();
#else
 return timer_monotonic_us();
#endif
}
static bool source_clean_line(void *ctx,uint64_t va) {
 if(ctx!=&source_clean)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_private_clean_line(va);
#else
 __asm__ volatile("clflush (%0)"::"r"(va):"memory");return true;
#endif
}
/* CPU0 current-root target refresh, not a GPU/IOMMU invalidation. Revalidate
 * the exact owned page before issuing the non-addressed-cache operation. */
static bool source_clean_page(void *ctx,uint64_t va) {
 if(!source_clean_hold(ctx) || (va&4095))return false;
 for(unsigned i=0;i<2;i++) {
  const struct private_clean_span *s=&source_clean.span[i];
  if(va<s->virtual_base || va-s->virtual_base>=(uint64_t)s->pages*4096)continue;
  uint64_t pa=s->physical+va-s->virtual_base;struct vmm_alias_observation a={0};
  if(!pmm_frame_has_tag(pa,PMM_FRAME_DMA))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_private_clean_page(source_clean_root,va,pa,source_clean_context.cpu.physical_bits,&a);
#else
  bool ok=vmm_diagnostic_kernel_alias(source_clean_root,va,pa,1,source_clean_context.cpu.physical_bits,&a,NULL);
#endif
  if(!ok || a.pages!=1 || a.leaves!=1 || a.writable!=1 || a.user || a.pat_index>7)return false;
  /* Full-span admission requires WB; re-evaluate the fresh actual index rather
   * than assuming the native PAT's default index. */
  unsigned mtrr,pat,effective;
  if(!x86_cache_page(&source_clean_context,pa,a.pat_index,&mtrr,&pat,&effective) || mtrr!=6 || pat!=6 || effective!=6)return false;
#ifndef SRVROS_INTEL_DISPLAY_HOST_TEST
  __asm__ volatile("invlpg (%0)"::"r"(va):"memory");
#endif
  return source_clean_hold(ctx);
 }
 return false;
}
static bool source_clean_fence(void *ctx) {
 if(ctx!=&source_clean)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_private_clean_fence();
#else
 __asm__ volatile("mfence":::"memory");return true;
#endif
}
static void source_clean_cpu(uint32_t *signature,uint32_t *ebx,uint32_t *edx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_private_clean_cpu(signature,ebx,edx);
#else
 uint32_t a,b,c,d;__asm__ volatile("cpuid":"=a"(a),"=b"(b),"=c"(c),"=d"(d):"a"(1),"c"(0):"memory");*signature=a;*ebx=b;*edx=d;
#endif
}
static bool source_clean_cache(struct x86_cache_snapshot *c) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_private_clean_cache(c);
#else
 return intel_cache_probe_context(c);
#endif
}
static bool source_clean_admit(struct private_clean_span span[2]) {
 if(!source_clean_hold(&source_clean) || !source_clean_cache(&source_clean_context) || source_clean_context.cpu.signature!=0x306d4)return false;
 struct vmm_alias_span aliases[2];
 for(unsigned i=0;i<2;i++) {
  uint64_t pa=buffers_native.memory[i].physical,va=(uint64_t)pmm_phys_to_virt(pa);uint32_t pages=buffers_native.memory[i].layout.pages;
  struct vmm_alias_observation a={0};unsigned mtrr,pat,effective,checks;
  if(!buffers_validate(&buffers_native,pa,pages))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_private_clean_alias(source_clean_root,va,pa,pages,source_clean_context.cpu.physical_bits,&a);
#else
  bool ok=vmm_diagnostic_kernel_alias(source_clean_root,va,pa,pages,source_clean_context.cpu.physical_bits,&a,NULL);
#endif
  if(!ok || a.pages!=pages || !a.leaves || !a.leaf_sizes || a.writable!=1 || a.user || a.pat_index>7 ||
     !x86_cache_range(&source_clean_context,pa,pages,a.pat_index,&mtrr,&pat,&effective,&checks) || mtrr!=6 || pat!=6 || effective!=6)return false;
  aliases[i]=(struct vmm_alias_span){va,pa,pages};span[i]=(struct private_clean_span){va,pa,pages};
 }
 struct vmm_alias_inventory v={0};struct vmm_alias_detail d={0};
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool unique=intel_display_test_private_clean_inventory(source_clean_root,aliases,source_clean_context.cpu.physical_bits,source_audit_ram.ranges,source_audit_ram.count,&v,&d);
#else
 bool unique=vmm_diagnostic_unique_kernel_aliases(source_clean_root,aliases,2,source_clean_context.cpu.physical_bits,source_audit_ram.ranges,source_audit_ram.count,&v,&d);
#endif
 return unique && v.pages[0]==1356 && v.pages[1]==1356 && source_clean_hold(&source_clean);
}
static bool source_clean_prepare(void) {
 if(source_clean.object)return false;
 uint64_t flags=wake_irq_save();source_clean_root=vmm_current_address_space();
 uint64_t begin=source_clean_now(&source_clean);struct private_clean_span span[2];uint32_t signature,ebx,edx;
 source_clean_cpu(&signature,&ebx,&edx);
 bool ok=source_audit_ram.ready && source_clean_admit(span) && source_clean_context.cpu.features==edx && private_clean_run(&source_clean,span,signature,ebx,edx,begin,
  (struct private_clean_io){&source_clean,source_clean_hold,source_clean_line,source_clean_fence,source_clean_page,source_clean_now});
 uint32_t a,b,d;source_clean_cpu(&a,&b,&d);
 static struct x86_cache_snapshot closing;
 /* Closing context is checked even after a partial failure. Completion alone
  * cannot release private holds after their source/cache identity was lost. */
 bool known=source_clean_cache(&closing) && x86_cache_equal(&source_clean_context,&closing) &&
  a==signature && b==ebx && d==edx && source_clean_hold(&source_clean);
 uint64_t end=source_clean_now(&source_clean);
 bool held=source_clean_hold(&source_clean);
 source_clean_retired=!source_clean.attempted || (source_clean.drained && known && held);
 bool clock_ok=end>=source_clean.last && end>=begin && end-begin<=100000;
 if(!clock_ok && !source_clean.reason){source_clean.reason=3;source_clean.end=end;}
 ok=ok && known && held && source_clean.object==&source_clean && source_clean.context==&source_clean &&
  source_clean.complete && !source_clean.busy && !source_clean.reason && source_clean.drained &&
  source_clean.completed==2 && source_clean.fences==4 && source_clean.begin==begin && source_clean.end==source_clean.last &&
  source_clean.signature==signature && source_clean.ebx==ebx && source_clean.edx==edx &&
  clock_ok;
 for(unsigned i=0;ok && i<2;i++)ok=source_clean.span[i].physical==span[i].physical && source_clean.span[i].virtual_base==span[i].virtual_base &&
  source_clean.span[i].pages==span[i].pages && source_clean.lines[i]==span[i].pages*64 && source_clean.invalidations[i]==span[i].pages;
 if(ok)source_clean.end=source_clean.last=end;
 source_clean_ready=ok;wake_irq_restore(flags);return ok;
}
static bool source_clean_bound(bool returned) {
 const struct private_clean_result *c=&source_clean;
 if(!source_clean_ready || !source_clean_retired || !c->drained || !c->attempted || c->object!=c || c->context!=c || !c->complete || c->busy || c->reason || c->completed!=2 || c->fences!=4 ||
    !c->begin || c->end!=c->last || c->end<=c->begin || c->end-c->begin>100000 || c->signature!=0x306d4 || !(c->edx&(1u<<19)) || ((c->ebx>>8)&255)!=8 ||
    c->edx!=source_clean_context.cpu.features || source_clean_root!=source_audit_native.root || c->end>=source_audit_native.parent_begin)return false;
 for(unsigned i=0;i<2;i++) {
  const struct private_clean_span *s=&c->span[i];
  if(s->physical!=buffers_native.memory[i].physical || s->physical!=buffers_native.binding.physical[i] || s->pages!=1356 || s->pages!=buffers_native.memory[i].layout.pages ||
     s->virtual_base!=(uint64_t)pmm_phys_to_virt(s->physical) || c->lines[i]!=s->pages*64 || c->invalidations[i]!=s->pages)return false;
 }
 return !returned || c->end<source_audit_native.begin;
}
static void source_clean_report(void) {
 static uint8_t raw[72];static char encoded[97];unsigned k=4;const struct private_clean_result *c=&source_clean;
 raw[0]=(uint8_t)c->completed;raw[1]=(uint8_t)(c->reason?c->reason:source_clean_ready?0:6);raw[2]=(uint8_t)c->fences;raw[3]=64;
 source_audit_u64(raw,&k,c->begin);source_audit_u64(raw,&k,c->end);source_audit_u32(raw,&k,c->signature);source_audit_u32(raw,&k,c->ebx);source_audit_u32(raw,&k,c->edx);
 for(unsigned i=0;i<2;i++){source_audit_u64(raw,&k,c->span[i].physical);source_audit_u32(raw,&k,c->span[i].pages);source_audit_u32(raw,&k,c->lines[i]);}
 for(unsigned i=0;i<2;i++)source_audit_u32(raw,&k,c->invalidations[i]);
 entry_requester_base64(raw,72,encoded);console_printf("intel-private-clean: v=2 b=%s\n",encoded);
}
