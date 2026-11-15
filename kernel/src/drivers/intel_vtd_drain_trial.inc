/* Distinct v36 only. A registered child of the ORIGINAL v35 held source;
 * one cooperative-UEFI graphics-unit command, no GPU range/PTE authority. */
#include <srvros/intel_vtd_drain.h>
#define VTD_NATIVE_VA (BACKING_VA+0x50000)
static struct {
 const void *object;
 const struct intel_source_engines *parent;
 const struct intel_vtd_drain_source *registered_child;
 struct intel_vtd_drain core;
 struct intel_vtd_drain_source source;
 struct vmm_alias_observation alias;
 uint64_t begin,end,root;
 unsigned reason,maps,retired,cache_checks,alias_checks,resource,mtrr,pat,effective;
 struct {uint64_t calls,parents,failed,previous,stage,first,last,max_gap,max_phase,phase[7];} cost;
 bool mapped,complete,retained;
} vtd_native;
static void vtd_prepare_storage(void) {
 volatile unsigned char *p=(volatile unsigned char *)&vtd_native;
 for(unsigned i=0;i<sizeof(vtd_native);i++)p[i]=0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_vtd_prepare();
#endif
}
static bool vtd_fail(unsigned why) {
 vtd_native.reason|=why;vtd_native.retained=true;
 nonzero_native.failed=true;nonzero_native.a->uncertain=true;return false;
}
static bool vtd_parent_identity(void) {
 const struct intel_source_engines *p=&source_engines_native.result;
 const struct intel_source_audit_native *a=&source_audit_native;
 return vtd_native.object==&vtd_native && vtd_native.parent==p && !vtd_native.reason && !vtd_native.retained &&
  p->state==ISE_HELD && p->child_entered && !p->child_complete && p->samples==2 &&
  !nonzero_native.failed && nonzero_native.a && nonzero_native.a==joint.wake &&
  p->requester==&source_requester_native.result && p->transfer==&source_transfer_native &&
  p->source==&nonzero_native.source && p->provider==&nonzero_native &&
  source_engines_native.issued==5 && intel_source_engines_live(p) &&
  a->object==a && a->parent==p && a->provider==&nonzero_native && a->state==2 && a->complete && !a->registered && !a->reason &&
  a->epoch==p->epoch && a->kind==p->kind && a->mode==p->post_mode && a->origin==p->origin && a->parent_begin==p->parent_begin &&
  a->root==vtd_native.root && a->samples==2 && a->pci_reads==18 && a->plane_reads==54 && a->cache_checks==2 &&
  a->range_checks==4 && a->alias_checks==4 && a->inventory_count==2 && !a->inventory_reason &&
  vtd_native.root==nonzero_native.a->fw.gt.a.common.cr3 && wake_irq_off();
}
static bool vtd_parent(void) {
 ++vtd_native.cost.parents;
 return vtd_parent_identity() && source_engines_guard(&nonzero_native);
}
/* Clocks advance software state only. Keep the complete original software,
 * CPU-root, alias and HDA borrow graph; do not issue controller/PCI samples.
 * Every guard and the final MMIO/map/unmap/return admissions still take the
 * original full vtd_parent()/source_engines_guard hardware path. */
static bool vtd_clock_identity(void) {
 struct nonzero_adapter *p=&nonzero_native;uint64_t physical;
 const struct intel_source_requester *r=&source_requester_native.result;
 if(!vtd_parent_identity() || source_engines_native.uncertain || source_requester_native.uncertain ||
  source_requester_native.issued!=1 || !p->source_batch_entered || p->source_batch_checked ||
  p->source_reads!=6 || p->source.count!=6 || !p->source.stable ||
  r->transfer!=&source_transfer_native || r->buffers!=&buffers_native.binding ||
  r->source!=&p->source || r->provider!=p || r->input!=&p->source_input ||
  !intel_source_requester_live(r) || !buffers_guard(p) ||
  p->g!=joint.scope.gpu || !p->g || p->r.provider!=p || p->r.entry!=&ring_native.result ||
  p->r.source!=&ring_native.result.post || p->r.parent!=p->g || p->g->io.context!=p->a ||
  !intel_nonzero_reset_live(&p->r) || p->a->uncertain || p->a->fw.gt.a.mapped ||
  joint.scope.state!=IHG_ACTIVE || joint.scope.busy || joint.scope.hda.busy ||
  !intel_hda_gpu_live(&joint.scope) || !joint_owner_guard(&joint) ||
  !display_access_held(&p->a->fw.gt.token) || !intel_boot_check(INTEL_BOOT_OBSERVER,p->a->fw.gt.a.common.gpu) ||
  vmm_current_address_space()!=vtd_native.root || vmm_virt_to_phys_in_space(vtd_native.root,BACKING_VA,&physical))return false;
 for(unsigned i=0;i<WAKE_MAPS;i++)if(!completion_mapping(p->a,i))return false;
 return true;
}
static bool vtd_policy(void) {
 struct x86_cache_snapshot cache;
 /* A declared-gap result already performed the complete fresh DMAR check
  * inside dma_resource(). Reserved-region admission still needs that check.
  * No external provider or persistent "validated" token is reused here. */
 if(!dma_resource() || (!dma_resource_declared && !dma_table_current()))return false;
 vtd_native.resource=dma_resource_source;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_vtd_cache(&cache);
#else
 bool ok=intel_cache_probe_context(&cache);
#endif
 if(!ok || !x86_cache_equal(&cache,&source_audit_native.cache_context[1]) ||
  !x86_cache_page(&cache,INTEL_VTD_DRAIN_UNIT,2,&vtd_native.mtrr,&vtd_native.pat,&vtd_native.effective) || vtd_native.effective)return false;
 ++vtd_native.cache_checks;return true;
}
static bool vtd_alias(void) {
 uint64_t pa;
 if(!vtd_native.mapped || !vmm_virt_to_phys_in_space(vtd_native.root,VTD_NATIVE_VA,&pa) || pa!=INTEL_VTD_DRAIN_UNIT)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_vtd_alias(vtd_native.root,VTD_NATIVE_VA,pa,&vtd_native.alias);
#else
 bool ok=vmm_diagnostic_kernel_alias(vtd_native.root,VTD_NATIVE_VA,pa,1,source_audit_native.cache_context[1].cpu.physical_bits,&vtd_native.alias,NULL);
#endif
 const struct vmm_alias_observation *a=&vtd_native.alias;
 if(!ok || a->pages!=1 || a->leaves!=1 || a->pat_index!=2 || a->writable!=1 || a->user || a->no_execute!=1 || a->leaf_sizes!=1)return false;
 ++vtd_native.alias_checks;return true;
}
static bool vtd_registration(void) {
 const struct intel_vtd_drain_source *s=&vtd_native.source;const struct intel_source_engines *p=&source_engines_native.result;
 if(!vtd_native.maps)return !s->object && !vtd_native.core.object && !vtd_native.registered_child;
 const struct intel_vtd_drain_binding *b=&s->binding;
 return s->object==s && s->consumer==&vtd_native.core && vtd_native.core.object==&vtd_native.core && vtd_native.registered_child==s &&
  b->owner==p && b->provider==&vtd_native && b->lifetime==&joint.scope && b->root==vtd_native.root && b->epoch==p->epoch && b->sequence==1 &&
  b->hash==INTEL_DMA_DMAR_HASH && b->unit==INTEL_VTD_DRAIN_UNIT && b->vendor==0x8086 && b->device==0x1626 &&
  b->prepare_begin==p->origin && b->gt_begin==p->parent_begin && b->hda_begin==joint.scope.hda.begin_us && b->outer_begin==joint.scope.outer_origin_us;
}
static uint64_t vtd_now(void *ctx) {
 /* No hardware operation is authorized by a clock callback. */
 if(ctx!=&vtd_native || !vtd_clock_identity() || !vtd_registration())return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_vtd_boundary();
#endif
 uint64_t at=source_engines_now(&nonzero_native);
 ++vtd_native.cost.calls;
 if(at && (!vtd_native.cost.last || at>=vtd_native.cost.last)) {
  if(!vtd_native.cost.first)vtd_native.cost.first=at;
  if(vtd_native.cost.last) {
   uint64_t delta=at-vtd_native.cost.last;unsigned phase=(unsigned)vtd_native.core.p.state;
   if(phase>6)phase=6;
   vtd_native.cost.phase[phase]+=delta;
   if(delta>vtd_native.cost.max_gap){vtd_native.cost.max_gap=delta;vtd_native.cost.max_phase=phase;}
  }
 }
 uint64_t previous=vtd_native.cost.last;vtd_native.cost.last=at;
 unsigned failed=!vtd_clock_identity() || !vtd_registration()?1:
  !intel_source_engines_deadline(&source_engines_native.result,at)?2:0;
 if(failed){vtd_native.cost.failed=at;vtd_native.cost.previous=previous;vtd_native.cost.stage=failed;return 0;}
 return at;
}
static bool vtd_guard_checks(void *ctx,const struct intel_vtd_drain_source *s,bool hardware) {
 const struct intel_source_engines *p=&source_engines_native.result;
 if(ctx!=&vtd_native || s!=&vtd_native.source || vtd_native.registered_child!=s || s->consumer!=&vtd_native.core ||
  !(hardware?vtd_parent():vtd_clock_identity()) || !vtd_registration() || !vtd_alias() || !vtd_policy())return false;
 const struct intel_vtd_drain_binding *b=&s->binding;
 return b->owner==p && b->provider==&vtd_native && b->lifetime==&joint.scope && b->epoch==p->epoch && b->sequence==1 &&
  b->root==vtd_native.root && b->hash==INTEL_DMA_DMAR_HASH && b->unit==INTEL_VTD_DRAIN_UNIT &&
  b->prepare_begin==p->origin && b->gt_begin==p->parent_begin && b->hda_begin==joint.scope.hda.begin_us &&
  b->outer_begin==joint.scope.outer_origin_us && b->vendor==0x8086 && b->device==0x1626 &&
  vtd_native.maps==1 && !vtd_native.retired && !vtd_native.complete;
}
static bool vtd_guard(void *ctx,const struct intel_vtd_drain_source *s) {
 return vtd_guard_checks(ctx,s,true);
}
static bool vtd_load(void *ctx,uint32_t off,uint32_t *out) {
 /* The core's guards retain full hardware admission. This provider also
  * checks every original identity/cache/alias predicate, then performs its
  * own fresh full hardware admission AFTER the last clock, before the load.
  * No hardware witness is cached or carried across that clock callback. */
 if(!out || !vtd_guard_checks(ctx,&vtd_native.source,false) ||
  !(off==0 || off==8 || off==12 || off==16 || off==20 || off==28 || off==32 || off==36 || off==40 || off==44 || off==52 || off==0x508 || off==0x50c))return false;
 if(!vtd_now(ctx) || !vtd_alias() || !vtd_registration() || !vtd_parent())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_vtd_read(off,out);
#else
 __asm__ volatile("mfence":::"memory");*out=*(volatile const uint32_t *)(VTD_NATIVE_VA+off);__asm__ volatile("mfence":::"memory");return true;
#endif
}
static enum intel_gt_write vtd_store(void *ctx,uint32_t off,uint32_t value) {
 const struct intel_vtd_drain *r=&vtd_native.core;
 if(off!=0x50c || value!=INTEL_VTD_DRAIN_REQUEST || r->object!=r || !r->busy || r->reason || r->retained || r->complete ||
  r->p.state!=VTD_DRAIN_REQUESTING || r->p.attempts!=1 || r->p.writes || r->p.uncertain ||
  !vtd_guard_checks(ctx,&vtd_native.source,false) || !vtd_now(ctx) || !vtd_alias() || !vtd_registration() || !vtd_parent())return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_vtd_write(off,value);
#else
 __asm__ volatile("mfence":::"memory");*(volatile uint32_t *)(VTD_NATIVE_VA+off)=value;__asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
static void vtd_pause(void *ctx) {if(ctx==&vtd_native && vtd_parent())nonzero_pause(&nonzero_native);}
static bool vtd_run(void) {
 struct intel_source_engines *p=&source_engines_native.result;uint64_t physical;
 if(vtd_native.object)return false;
 vtd_native.object=&vtd_native;vtd_native.parent=p;vtd_native.root=nonzero_native.a->fw.gt.a.common.cr3;
 if(!vtd_parent())return vtd_fail(1);
 vtd_native.begin=vtd_now(&vtd_native);
 if(!vtd_native.begin || !vtd_policy() || !vtd_parent() || vmm_virt_to_phys_in_space(vtd_native.root,VTD_NATIVE_VA,&physical))return vtd_fail(2);
 if(!vmm_map_page_in_space(vtd_native.root,VTD_NATIVE_VA,INTEL_VTD_DRAIN_UNIT,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_WRITABLE)) {
  if(vmm_virt_to_phys_in_space(vtd_native.root,VTD_NATIVE_VA,&physical))vtd_native.mapped=true;
  return vtd_fail(4);
 }
 vtd_native.mapped=true;++vtd_native.maps;
 struct intel_vtd_drain_source *s=&vtd_native.source;s->object=s;
 s->binding=(struct intel_vtd_drain_binding){.owner=p,.provider=&vtd_native,.lifetime=&joint.scope,
  .root=vtd_native.root,.epoch=p->epoch,.sequence=1,.hash=INTEL_DMA_DMAR_HASH,.unit=INTEL_VTD_DRAIN_UNIT,
  .prepare_begin=p->origin,.gt_begin=p->parent_begin,.hda_begin=joint.scope.hda.begin_us,.outer_begin=joint.scope.outer_origin_us,.vendor=0x8086,.device=0x1626};
 vtd_native.registered_child=s;
 if(!intel_vtd_drain_run(&vtd_native.core,s,(struct intel_vtd_drain_io){&vtd_native,vtd_guard,vtd_load,vtd_store,vtd_now,vtd_pause}))return vtd_fail(8);
 if(!vtd_guard(&vtd_native,s) || !vtd_now(&vtd_native) || !vtd_alias() || !vtd_registration() || !vtd_parent() ||
  !vmm_unmap_page_in_space(vtd_native.root,VTD_NATIVE_VA,&physical) || physical!=INTEL_VTD_DRAIN_UNIT)return vtd_fail(16);
 vtd_native.mapped=false;++vtd_native.retired;
 if(vmm_virt_to_phys_in_space(vtd_native.root,VTD_NATIVE_VA,&physical) || !vtd_parent() || !vtd_policy())return vtd_fail(32);
 vtd_native.end=vtd_now(&vtd_native);if(!vtd_native.end || vtd_native.end<vtd_native.core.end || vmm_virt_to_phys_in_space(vtd_native.root,VTD_NATIVE_VA,&physical) || !vtd_parent())return vtd_fail(64);
 vtd_native.registered_child=NULL;vtd_native.complete=true;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_vtd_after();
#endif
 return true;
}
static bool vtd_completed_record(void) {
 const struct intel_vtd_drain *r=&vtd_native.core;const struct intel_vtd_drain_source *s=&vtd_native.source;
 const struct intel_vtd_drain_binding *b=&r->binding,*q=&s->binding;const struct intel_vtd_drain_progress *v=&r->p;
 const struct intel_source_engines *p=&source_engines_native.result;
 if(r->object!=r || r->source!=s || s->object!=s || s->consumer!=r || !r->complete || r->busy || r->retained || r->reason ||
  r->clock_at || r->clock_previous || r->clock_reason || v->state!=VTD_DRAIN_COMPLETE || v->uncertain ||
  !v->begin || r->end!=v->last || r->end<=v->begin || v->attempts!=1 || v->writes!=1 || !v->polls || v->polls>256 ||
  v->reads!=43+7*v->polls || v->guards!=45+8*v->polls || v->final!=0x1203000000000000ull ||
  (v->initial!=0x0200000000000000ull && v->initial!=0x1203000000000000ull))return false;
 const struct intel_vtd_drain_io *io=&r->io,*oi=&r->original_io;
 if(io->ctx!=&vtd_native || io->guard!=vtd_guard || io->read!=vtd_load || io->write!=vtd_store || io->now!=vtd_now || io->pause!=vtd_pause ||
  oi->ctx!=io->ctx || oi->guard!=io->guard || oi->read!=io->read || oi->write!=io->write || oi->now!=io->now || oi->pause!=io->pause)return false;
 for(unsigned i=0;i<2;i++) {
  const struct intel_vtd_drain_binding *x=i?q:b;
  if(x->owner!=p || x->provider!=&vtd_native || x->lifetime!=&joint.scope || x->epoch!=p->epoch || x->sequence!=1 ||
   x->root!=vtd_native.root || x->hash!=INTEL_DMA_DMAR_HASH || x->unit!=INTEL_VTD_DRAIN_UNIT || x->vendor!=0x8086 || x->device!=0x1626 ||
   x->prepare_begin!=p->origin || x->gt_begin!=p->parent_begin || x->hda_begin!=joint.scope.hda.begin_us || x->outer_begin!=joint.scope.outer_origin_us)return false;
 }
 const uint64_t expected[8]={0,0x10,INTEL_VTD_DRAIN_CAP,INTEL_VTD_DRAIN_ECAP,0,0x0800000000000000ull,0,0};
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<8;j++)if(v->context[i][j]!=expected[j])return false;
 for(unsigned i=0;i<256;i++) {
  uint32_t w=v->observed[i];
  if(i>=v->polls?w!=0:i+1==v->polls?w!=0x12030000:(w&~0x06000000u)!=0x90030000)return false;
 }
 return vtd_native.cache_checks==v->guards+v->reads+4 && vtd_native.alias_checks==v->guards+2*v->reads+4 &&
  vtd_native.alias.pages==1 && vtd_native.alias.leaves==1 && vtd_native.alias.pat_index==2 && vtd_native.alias.writable==1 &&
  !vtd_native.alias.user && vtd_native.alias.no_execute==1 && vtd_native.alias.leaf_sizes==1 && !vtd_native.effective &&
  r->end>=b->prepare_begin && r->end-b->prepare_begin<=100000 && r->end-b->gt_begin<=300000 && r->end-b->hda_begin<=400000 && r->end-b->outer_begin<=500000;
}
static bool vtd_returned(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_vtd_return();
#endif
 const struct intel_source_engines *p=&source_engines_native.result;
 const struct intel_vtd_drain *r=&vtd_native.core;
 return vtd_native.object==&vtd_native && vtd_native.parent==p && vtd_native.complete && !vtd_native.reason &&
 !vtd_native.retained && !vtd_native.mapped && !vtd_native.registered_child && vtd_native.maps==1 && vtd_native.retired==1 &&
 vtd_completed_record() && vtd_native.root==source_audit_native.root && (vtd_native.resource==1 || vtd_native.resource==2) &&
 p->complete && p->child_complete && !p->registered && p->state==ISE_CLOSED &&
 p->child_begin<=source_audit_native.begin && source_audit_native.end<=vtd_native.begin &&
 vtd_native.begin<=r->p.begin && r->p.begin<r->end && r->end<=vtd_native.end && vtd_native.end<=p->child_end;
}
static void vtd_report(void) {
 const struct intel_vtd_drain *r=&vtd_native.core;const struct intel_vtd_drain_binding *b=&r->binding;
 const struct vmm_alias_observation *a=&vtd_native.alias;
 const uint64_t words[]={r->p.state,r->reason,vtd_native.reason,b->epoch,source_engines_native.result.kind,source_engines_native.result.post_mode,
  b->root,b->prepare_begin,b->gt_begin,b->hda_begin,b->outer_begin,r->p.begin,r->p.last,r->end,r->clock_at,r->clock_previous,r->clock_reason,
  r->p.reads,r->p.guards,r->p.attempts,r->p.writes,r->p.polls,r->p.uncertain,r->p.initial,r->p.final,vtd_native.maps,vtd_native.retired,
  vtd_native.registered_child!=NULL,vtd_native.retained,
  (uint64_t)r->complete|((uint64_t)r->busy<<1)|((uint64_t)r->retained<<2)|((uint64_t)(r->object==r)<<3)|
  ((uint64_t)(r->source==&vtd_native.source && vtd_native.source.object==&vtd_native.source)<<4)|((uint64_t)(vtd_native.source.consumer==r)<<5),vtd_returned(),vtd_native.begin,vtd_native.end,VTD_NATIVE_VA,
  vtd_native.cache_checks,vtd_native.alias_checks,vtd_native.resource,b->hash,b->unit,b->sequence,
  a->pages,a->leaves,a->pat_index,a->writable,a->user,a->no_execute,a->leaf_sizes,vtd_native.mtrr,vtd_native.pat,vtd_native.effective};
 static uint8_t raw[1680];static char encoded[2241];unsigned k=0;
 _Static_assert(sizeof(words)==400,"VT-d wire header");
 for(unsigned i=0;i<50;i++)source_audit_u64(raw,&k,words[i]);
 for(unsigned i=0;i<2;i++)for(unsigned j=0;j<8;j++)source_audit_u64(raw,&k,r->p.context[i][j]);
 for(unsigned i=0;i<256;i++)source_audit_u32(raw,&k,r->p.observed[i]);
 const uint64_t timing[]={vtd_native.cost.calls,vtd_native.cost.parents,vtd_native.cost.failed,vtd_native.cost.previous,vtd_native.cost.stage,
  vtd_native.cost.first,vtd_native.cost.last,vtd_native.cost.max_gap,vtd_native.cost.max_phase};
 for(unsigned i=0;i<9;i++)source_audit_u64(raw,&k,timing[i]);
 for(unsigned i=0;i<7;i++)source_audit_u64(raw,&k,vtd_native.cost.phase[i]);
 entry_requester_base64(raw,k,encoded);console_printf("intel-vtd-drain: v=2 b=%s\n",encoded);
 /* Independent exact topology bytes; an absent preflight reports zeros. */
 entry_requester_base64(dma_table_bytes,136,encoded);console_printf("intel-vtd-table: v=1 b=%s\n",encoded);
 console_printf("intel-vtd: state=%u reason=%u native=%u reads=%u polls=%u writes=%u maps=%u retired=%u retained=%u returned=%u\n",
  r->p.state,(uint64_t)r->reason,(uint64_t)vtd_native.reason,r->p.reads,r->p.polls,r->p.writes,(uint64_t)vtd_native.maps,
  (uint64_t)vtd_native.retired,(uint64_t)vtd_native.retained,(uint64_t)vtd_returned());
}
