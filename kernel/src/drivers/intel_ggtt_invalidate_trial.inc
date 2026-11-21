/* Separate combined diagnostic only; never compiled into the old v36 image. */
#include <srvros/intel_ggtt_invalidate.h>
#define GGTT_NATIVE_VA (BACKING_VA+0x51000)
static struct {
 const void *object;
 const struct intel_source_engines *parent;
 const struct intel_ggtt_invalidate_source *registered_child;
 struct intel_ggtt_invalidate core;
 struct intel_ggtt_invalidate_source source;
 struct vmm_alias_observation alias;
 uint64_t root,mmio,begin,end,failed_clock,previous_clock,clock_stage;
 unsigned reason,maps,retired,cache_checks,alias_checks,mtrr,pat,effective,parents;
 bool mapped,complete,retained;
} ggtt_native;
static void ggtt_prepare_storage(void) {
 volatile unsigned char *p=(volatile unsigned char *)&ggtt_native;
 for(unsigned i=0;i<sizeof(ggtt_native);i++)p[i]=0;
}
static bool ggtt_fail(unsigned why) {
 ggtt_native.reason|=why;ggtt_native.retained=true;
 nonzero_native.failed=true;nonzero_native.a->uncertain=true;return false;
}
static bool ggtt_identity(void) {
 return ggtt_native.object==&ggtt_native && ggtt_native.parent==&source_engines_native.result &&
 !ggtt_native.reason && !ggtt_native.retained && ggtt_native.root==vtd_native.root &&
 ggtt_native.mmio==result.mmio_base && vtd_native.complete && !vtd_native.mapped &&
 !vtd_native.registered_child && vtd_native.maps==1 && vtd_native.retired==1 &&
 vtd_completed_record() && vtd_clock_identity();
}
static bool ggtt_parent(void) {
 ++ggtt_native.parents;
 return ggtt_identity() && source_engines_guard(&nonzero_native);
}
static bool ggtt_registration(void) {
 const struct intel_ggtt_invalidate_source *s=&ggtt_native.source;
 const struct intel_ggtt_invalidate_binding *b=&s->binding;
 const struct intel_source_engines *p=&source_engines_native.result;
 if(!ggtt_native.maps)return !s->object && !ggtt_native.core.object && !ggtt_native.registered_child;
 return s->object==s && s->consumer==&ggtt_native.core && ggtt_native.core.object==&ggtt_native.core &&
 ggtt_native.registered_child==s && b->owner==p && b->provider==&ggtt_native && b->lifetime==&joint.scope &&
 b->root==ggtt_native.root && b->epoch==p->epoch && b->sequence==1 && b->mmio==ggtt_native.mmio &&
 b->mmio_bytes==0x1000000 && b->vendor==0x8086 && b->device==0x1626 &&
 b->prepare_begin==p->origin && b->gt_begin==p->parent_begin &&
 b->hda_begin==joint.scope.hda.begin_us && b->outer_begin==joint.scope.outer_origin_us;
}
static uint64_t ggtt_now(void *ctx) {
 if(ctx!=&ggtt_native || !ggtt_identity() || !ggtt_registration())return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_ggtt_boundary();
#endif
 uint64_t previous=source_engines_native.result.last,at=source_engines_now(&nonzero_native);
 unsigned failed=!ggtt_identity() || !ggtt_registration()?1:
  !intel_source_engines_deadline(&source_engines_native.result,at)?2:0;
 if(failed){ggtt_native.failed_clock=at;ggtt_native.previous_clock=previous;ggtt_native.clock_stage=failed;return 0;}
 return at;
}
static bool ggtt_policy(void) {
 struct x86_cache_snapshot cache;
 if(!ggtt_native.mmio || ggtt_native.mmio>UINT64_MAX-0x1000000 ||
 !backing_range_reserved(ggtt_native.mmio,0x1000000))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_ggtt_cache(&cache);
#else
 bool ok=intel_cache_probe_context(&cache);
#endif
 if(!ok || !x86_cache_equal(&cache,&source_audit_native.cache_context[1]) ||
 !x86_cache_page(&cache,ggtt_native.mmio+0x101000,2,&ggtt_native.mtrr,&ggtt_native.pat,&ggtt_native.effective) || ggtt_native.effective)return false;
 ++ggtt_native.cache_checks;return true;
}
static bool ggtt_alias(void) {
 uint64_t pa;
 if(!ggtt_native.mapped || !vmm_virt_to_phys_in_space(ggtt_native.root,GGTT_NATIVE_VA,&pa) || pa!=ggtt_native.mmio+0x101000)return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_ggtt_alias(ggtt_native.root,GGTT_NATIVE_VA,pa,&ggtt_native.alias);
#else
 bool ok=vmm_diagnostic_kernel_alias(ggtt_native.root,GGTT_NATIVE_VA,pa,1,source_audit_native.cache_context[1].cpu.physical_bits,&ggtt_native.alias,NULL);
#endif
 const struct vmm_alias_observation *a=&ggtt_native.alias;
 if(!ok || a->pages!=1 || a->leaves!=1 || a->pat_index!=2 || a->writable!=1 || a->user || a->no_execute!=1 || a->leaf_sizes!=1)return false;
 ++ggtt_native.alias_checks;return true;
}
static bool ggtt_guard_checks(void *ctx,const struct intel_ggtt_invalidate_source *s,bool hardware) {
 return ctx==&ggtt_native && s==&ggtt_native.source && ggtt_native.registered_child==s &&
 (hardware?ggtt_parent():ggtt_identity()) && ggtt_registration() && ggtt_alias() && ggtt_policy() &&
 ggtt_native.maps==1 && !ggtt_native.retired && !ggtt_native.complete;
}
static bool ggtt_guard(void *ctx,const struct intel_ggtt_invalidate_source *s) {
 return ggtt_guard_checks(ctx,s,true);
}
static bool ggtt_load(void *ctx,uint32_t off,uint32_t *out) {
 const struct intel_ggtt_invalidate *r=&ggtt_native.core;
 if(!out || off!=0x101008 || !r->busy || r->complete || r->retained || r->reason ||
 !(r->p.state==GGTT_INVALIDATE_OPENING?r->p.reads==0 && r->p.writes==0:
 r->p.state==GGTT_INVALIDATE_POSTING && r->p.reads==1 && r->p.writes==1 && r->p.attempts==1 && !r->p.uncertain) ||
 !ggtt_guard_checks(ctx,&ggtt_native.source,false) || !ggtt_now(ctx) || !ggtt_alias() || !ggtt_registration() || !ggtt_parent())return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_ggtt_read(off,out);
#else
 __asm__ volatile("mfence":::"memory");*out=*(volatile const uint32_t *)(GGTT_NATIVE_VA+8);__asm__ volatile("mfence":::"memory");return true;
#endif
}
static enum intel_gt_write ggtt_store(void *ctx,uint32_t off,uint32_t value) {
 const struct intel_ggtt_invalidate *r=&ggtt_native.core;
 if(off!=0x101008 || value!=1 || !r->busy || r->complete || r->retained || r->reason ||
 r->p.state!=GGTT_INVALIDATE_WRITING || r->p.reads!=1 || r->p.attempts!=1 || r->p.writes || r->p.uncertain ||
 !ggtt_guard_checks(ctx,&ggtt_native.source,false) || !ggtt_now(ctx) || !ggtt_alias() || !ggtt_registration() || !ggtt_parent())return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_ggtt_write(off,value);
#else
 __asm__ volatile("mfence":::"memory");*(volatile uint32_t *)(GGTT_NATIVE_VA+8)=value;__asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
static bool ggtt_completed_record(void) {
 const struct intel_ggtt_invalidate *r=&ggtt_native.core;const struct intel_ggtt_invalidate_source *s=&ggtt_native.source;
 const struct intel_source_engines *p=&source_engines_native.result;const struct intel_ggtt_invalidate_progress *v=&r->p;
 if(r->object!=r || r->source!=s || s->object!=s || s->consumer!=r || !r->complete || r->busy || r->retained || r->reason ||
 r->clock_at || r->clock_previous || r->clock_reason || v->state!=GGTT_INVALIDATE_COMPLETE || v->uncertain ||
 v->reads!=2 || v->guards!=4 || v->attempts!=1 || v->writes!=1 || v->initial || v->posted ||
 !v->begin || r->end!=v->last || r->end<=v->begin)return false;
 const struct intel_ggtt_invalidate_io *io=&r->io,*oi=&r->original_io;
 if(io->ctx!=&ggtt_native || io->guard!=ggtt_guard || io->read!=ggtt_load || io->write!=ggtt_store || io->now!=ggtt_now ||
 oi->ctx!=io->ctx || oi->guard!=io->guard || oi->read!=io->read || oi->write!=io->write || oi->now!=io->now)return false;
 for(unsigned i=0;i<2;i++) {
  const struct intel_ggtt_invalidate_binding *b=i?&s->binding:&r->binding;
  if(b->owner!=p || b->provider!=&ggtt_native || b->lifetime!=&joint.scope || b->root!=ggtt_native.root ||
  b->epoch!=p->epoch || b->sequence!=1 || b->mmio!=ggtt_native.mmio || b->mmio_bytes!=0x1000000 ||
  b->vendor!=0x8086 || b->device!=0x1626 || b->prepare_begin!=p->origin || b->gt_begin!=p->parent_begin ||
  b->hda_begin!=joint.scope.hda.begin_us || b->outer_begin!=joint.scope.outer_origin_us)return false;
 }
 const struct vmm_alias_observation *a=&ggtt_native.alias;
 return ggtt_native.cache_checks==v->guards+v->reads+v->writes+3 && ggtt_native.alias_checks==v->guards+2*(v->reads+v->writes)+2 &&
 a->pages==1 && a->leaves==1 && a->pat_index==2 && a->writable==1 && !a->user && a->no_execute==1 && a->leaf_sizes==1 &&
 !ggtt_native.effective && r->end>=p->origin && r->end-p->origin<=INTEL_DIAGNOSTIC_PREPARE_US &&
 r->end-p->parent_begin<=300000 && r->end-joint.scope.hda.begin_us<=400000 && r->end-joint.scope.outer_origin_us<=500000;
}
static bool ggtt_run(void) {
 struct intel_source_engines *p=&source_engines_native.result;uint64_t physical;
 if(ggtt_native.object)return false;
 ggtt_native.object=&ggtt_native;ggtt_native.parent=p;ggtt_native.root=vtd_native.root;ggtt_native.mmio=result.mmio_base;
 if(!ggtt_parent())return ggtt_fail(1);
 ggtt_native.begin=ggtt_now(&ggtt_native);
 if(!ggtt_native.begin || !ggtt_policy() || !ggtt_parent() || vmm_virt_to_phys_in_space(ggtt_native.root,GGTT_NATIVE_VA,&physical))return ggtt_fail(2);
 if(!vmm_map_page_in_space(ggtt_native.root,GGTT_NATIVE_VA,ggtt_native.mmio+0x101000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_WRITABLE)) {
  if(vmm_virt_to_phys_in_space(ggtt_native.root,GGTT_NATIVE_VA,&physical))ggtt_native.mapped=true;
  return ggtt_fail(4);
 }
 ggtt_native.mapped=true;++ggtt_native.maps;
 struct intel_ggtt_invalidate_source *s=&ggtt_native.source;s->object=s;
 s->binding=(struct intel_ggtt_invalidate_binding){.owner=p,.provider=&ggtt_native,.lifetime=&joint.scope,
 .root=ggtt_native.root,.epoch=p->epoch,.sequence=1,.mmio=ggtt_native.mmio,.mmio_bytes=0x1000000,
 .prepare_begin=p->origin,.gt_begin=p->parent_begin,.hda_begin=joint.scope.hda.begin_us,.outer_begin=joint.scope.outer_origin_us,.vendor=0x8086,.device=0x1626};
 ggtt_native.registered_child=s;
 if(!intel_ggtt_invalidate_run(&ggtt_native.core,s,(struct intel_ggtt_invalidate_io){&ggtt_native,ggtt_guard,ggtt_load,ggtt_store,ggtt_now}))return ggtt_fail(8);
 if(!ggtt_guard(&ggtt_native,s) || !ggtt_now(&ggtt_native) || !ggtt_alias() || !ggtt_registration() || !ggtt_parent() ||
 !vmm_unmap_page_in_space(ggtt_native.root,GGTT_NATIVE_VA,&physical) || physical!=ggtt_native.mmio+0x101000)return ggtt_fail(16);
 ggtt_native.mapped=false;++ggtt_native.retired;
 if(vmm_virt_to_phys_in_space(ggtt_native.root,GGTT_NATIVE_VA,&physical) || !ggtt_parent() || !ggtt_policy())return ggtt_fail(32);
 ggtt_native.end=ggtt_now(&ggtt_native);
 if(!ggtt_native.end || ggtt_native.end<ggtt_native.core.end || vmm_virt_to_phys_in_space(ggtt_native.root,GGTT_NATIVE_VA,&physical) || !ggtt_parent() || !ggtt_completed_record())return ggtt_fail(64);
 ggtt_native.registered_child=NULL;ggtt_native.complete=true;return true;
}
static bool ggtt_returned(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_ggtt_return();
#endif
 const struct intel_source_engines *p=&source_engines_native.result;
 return ggtt_native.object==&ggtt_native && ggtt_native.parent==p && ggtt_native.complete && !ggtt_native.reason &&
 !ggtt_native.retained && !ggtt_native.mapped && !ggtt_native.registered_child && ggtt_native.maps==1 && ggtt_native.retired==1 &&
 !ggtt_native.failed_clock && !ggtt_native.previous_clock && !ggtt_native.clock_stage &&
 ggtt_completed_record() && vtd_returned() && vtd_native.end<=ggtt_native.begin && ggtt_native.begin<=ggtt_native.core.p.begin &&
 ggtt_native.core.end<=ggtt_native.end && ggtt_native.end<=p->child_end;
}
static void ggtt_report(void) {
 const struct intel_ggtt_invalidate *r=&ggtt_native.core;const struct intel_ggtt_invalidate_binding *b=&r->binding;
 const struct vmm_alias_observation *a=&ggtt_native.alias;
 const uint64_t fields[]={r->p.state,r->reason,ggtt_native.reason,
 (uint64_t)r->complete|((uint64_t)r->busy<<1)|((uint64_t)r->retained<<2)|((uint64_t)(r->object==r)<<3)|
 ((uint64_t)(r->source==&ggtt_native.source && ggtt_native.source.object==&ggtt_native.source)<<4)|((uint64_t)(ggtt_native.source.consumer==r)<<5),
 ggtt_returned(),b->epoch,source_engines_native.result.kind,source_engines_native.result.post_mode,b->root,
 b->prepare_begin,b->gt_begin,b->hda_begin,b->outer_begin,r->p.begin,r->p.last,r->end,r->clock_at,r->clock_previous,r->clock_reason,
 r->p.reads,r->p.guards,r->p.attempts,r->p.writes,r->p.initial,r->p.posted,r->p.uncertain,ggtt_native.begin,ggtt_native.end,
 b->mmio,b->mmio_bytes,GGTT_NATIVE_VA,ggtt_native.maps,ggtt_native.retired,ggtt_native.registered_child!=NULL,ggtt_native.retained,
 ggtt_native.cache_checks,ggtt_native.alias_checks,a->pages,a->leaves,a->pat_index,a->writable,a->user,a->no_execute,a->leaf_sizes,
 ggtt_native.mtrr,ggtt_native.pat,ggtt_native.effective,b->sequence,INTEL_DIAGNOSTIC_PREPARE_US,ggtt_native.mapped,
 ggtt_native.failed_clock,ggtt_native.previous_clock,ggtt_native.clock_stage,ggtt_native.parents};
 static uint8_t raw[sizeof(fields)];static char data[(sizeof(fields)+2)/3*4+1];unsigned n=0;
 _Static_assert(sizeof(fields)==432,"GGTT receipt size");
 for(unsigned i=0;i<sizeof(fields)/sizeof(fields[0]);i++)source_audit_u64(raw,&n,fields[i]);
 entry_requester_base64(raw,n,data);console_printf("intel-ggtt-invalidate: v=1 b=%s\n",data);
}
