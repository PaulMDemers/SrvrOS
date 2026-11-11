/* Only the separate joint image. The root HDA alias outlives logical census
 * borrows; its physical retirement is reported separately and precedes unpin. */
#include <srvros/intel_hda_gpu.h>
struct joint_adapter {
 struct intel_hda_gpu scope;
 struct hda_adapter h;
 struct wake_adapter *wake;
 const struct scanout_handoff_snapshot *snapshot;
 unsigned maps,retired,borrows,returns,admission;
 bool inspection,after,entry;
};
static struct joint_adapter joint;
static bool joint_terminal(void) {
 return joint.scope.pinned || joint.scope.retained || joint.h.mapped || joint.h.uncertain ||
  joint.inspection || joint.borrows!=joint.returns;
}
/* Cheap original root/CPU/alias guard. The owner separately reads and validates
 * its exact endpoint once; no full backing/cache census in each GPU callback. */
static bool joint_owner_guard(void *ctx) {
 struct joint_adapter *j=ctx;uint64_t physical;
 return j==&joint && j->wake && !j->h.uncertain && !trial_mapping_fault && wake_irq_off() &&
  display_access_held(&j->wake->fw.gt.token) &&
  intel_boot_check(INTEL_BOOT_OBSERVER,j->h.gt.a.common.gpu) &&
  vmm_current_address_space()==j->h.gt.a.common.cr3 && j->h.mapped &&
  vmm_virt_to_phys_in_space(j->h.gt.a.common.cr3,HDA_VA,&physical) && physical==INTEL_HDA_BASE;
}
static bool joint_endpoint(void *ctx,struct intel_hda_endpoint *e) {return hda_endpoint(&((struct joint_adapter *)ctx)->h,e);}
static bool joint_inspection_map(void *ctx) {
 struct joint_adapter *j=ctx;
 if(j->inspection || j->borrows>=4 || !joint_owner_guard(j))return false;
 j->inspection=true;++j->borrows;return true;
}
static bool joint_inspection_return(void *ctx) {
 struct joint_adapter *j=ctx;
 if(!j->inspection || !joint_owner_guard(j)){j->h.uncertain=true;return false;}
 j->inspection=false;++j->returns;return true;
}
static bool joint_inspection_guard(void *ctx) {struct joint_adapter *j=ctx;return joint_owner_guard(j) && hda_guard(&j->h);}
static bool joint_read(void *ctx,unsigned off,unsigned width,uint32_t *out) {
 struct joint_adapter *j=ctx;
 if(!j->inspection || !joint_owner_guard(j) || !now(j))return false;
 return hda_read(&j->h,off,width,out);
}
static bool joint_inspect(void *ctx,struct intel_hda_result *r) {
 return intel_hda_observe(r,true,(struct intel_hda_io){ctx,joint_endpoint,joint_inspection_guard,
  joint_inspection_map,joint_read,joint_inspection_return,now});
}
static bool joint_command16(void *ctx,uint16_t value) {
 struct joint_adapter *j=ctx;struct hda_adapter *h=&j->h;
 const struct intel_gt_access *g=j->scope.gpu;
 if(!g || (value==2 && (g->object || g->pinned || g->write_attempts)))return false;
 if(value==6 && (!g || g->pinned || g->borrows || g->dependents ||
    (g->object && (g->object!=g || g->state!=GT_ACCESS_CLOSED || !j->after ||
     j->wake->maps!=j->wake->retired || j->wake->uncertain || j->wake->fw.uncertain)) ||
    (j->scope.claim && (j->scope.claim->state!=IIC_CLOSED || j->scope.claim->pinned || j->scope.claim->reason))))return false;
 if(j->inspection || !joint_owner_guard(j) || !hda_basic(h) ||
    !((h->initial.command==6 && value==2) || (h->initial.command==2 && value==6)))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_joint_admission();
#endif
 if(!now(j))return false;
 /* True command halfword only; adjacent PCI Status is never acknowledged. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_hda_command16(h->device,4,value))return false;
#else
 pci_write_config16(h->device,4,value);
#endif
 h->initial.command=value;return true;
}
static bool joint_retire(void *ctx) {
 struct joint_adapter *j=ctx;const struct intel_gt_access *g=j->scope.gpu;
 if(!g || g->pinned || g->borrows || g->dependents || j->inspection || j->borrows!=j->returns ||
    !joint_owner_guard(j) || !hda_guard(&j->h) || j->h.initial.command!=6)return false;
 /* A known unissued HDA refusal may return before GT opens. Otherwise the
  * full GPU aliases/backing/firmware exit must already have completed. */
 if(g->object && (!j->after || g->state!=GT_ACCESS_CLOSED || j->wake->maps!=j->wake->retired ||
    j->wake->uncertain || j->wake->fw.uncertain))return false;
 if(!now(j) || !hda_unmap(&j->h))return false;
 ++j->retired;return true;
}
static bool joint_fast(void) {
 if(!joint.scope.object)return !joint.h.uncertain;
 return intel_hda_gpu_check(&joint.scope);
}
static bool joint_open(struct wake_adapter *w,struct intel_gt_access *g,
 const struct scanout_handoff_snapshot *snapshot,uint64_t origin,const struct intel_gt_boot_facts *f) {
 (void)hda_text;(void)hda_irq_save;(void)hda_irq_restore;
 joint.entry=true;
 joint.wake=w;joint.snapshot=snapshot;joint.h.gt.a=w->fw.gt.a;joint.h.gt.token=w->fw.gt.token;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_joint_prepare();
#endif
 /* Positive joint log admission uses the observed below-4GiB stolen vector.
  * A changed upper word remains losslessly reported but cannot start stores. */
 for(unsigned i=0;i<backing.count;i++)if(backing.pte[i]>>32){joint.admission=1;return false;}
 uint64_t count=pci_device_count();if(count>PCI_MAX_DEVICES){joint.admission=2;return false;}
 for(uint64_t i=0;i<count;i++) {
  const struct pci_device *d=pci_device_at(i);
  if(d && !d->bus && d->device==3 && !d->function){if(joint.h.device){joint.admission=3;return false;}joint.h.device=d;}
 }
 if(!hda_endpoint(&joint.h,&joint.h.initial) || !intel_hda_endpoint_valid(&joint.h.initial) ||
    joint.h.initial.command!=6){joint.admission=4;return false;}
 if(!hda_guard(&joint.h)){joint.admission=5;return false;}
 if(!hda_map(&joint.h)){joint.admission=6;return false;}
 ++joint.maps;joint_clock=&joint.scope;
 return intel_hda_gpu_open(&joint.scope,g,&w->fw.gt.token,origin,
  (struct intel_hda_owner_facts){true,f->recipe,f->efi64,f->loader,f->profile,f->serialized,f->excluded,f->prerequisites},
  (struct intel_hda_gpu_io){{&joint,joint_inspect,joint_owner_guard,joint_endpoint,joint_command16,now},&joint,joint_retire});
}
static void joint_close(bool after,bool gpu_complete) {
 joint.after=after;
 if(joint.scope.state==IHG_ACTIVE)(void)intel_hda_gpu_close(&joint.scope,gpu_complete);
 if(!joint_terminal())joint_clock=NULL;
}
#ifdef SRVROS_INTEL_RING_ENTRY
#define JOINT_HDA_PREFIX "intel-ring-entry-hda"
#define JOINT_SUMMARY_PREFIX "intel-ring-entry-owner"
#elif defined(SRVROS_INTEL_RING_TRANSITION)
#define JOINT_HDA_PREFIX "intel-ring-hda"
#define JOINT_SUMMARY_PREFIX "intel-ring-owner"
#elif defined(SRVROS_INTEL_HDA_SHUTDOWN)
#define JOINT_HDA_PREFIX "intel-hda-bcs-hda"
#define JOINT_SUMMARY_PREFIX "intel-hda-bcs-summary"
#else
#define JOINT_HDA_PREFIX "intel-joint-hda"
#define JOINT_SUMMARY_PREFIX "intel-joint-summary"
#endif
static void joint_report(void) {
 const struct intel_hda_gpu *s=&joint.scope;
 for(unsigned p=0;p<4;p++) {
  const struct intel_hda_result *r=p==2?&s->mid:&s->hda.phase[p==3?2:p];
  if(!r->object)continue;
  const struct intel_hda_endpoint *e=&r->endpoint;
  /* Fixed full buffers retain partial reads and uncompleted sample data too. */
  uint8_t bytes[416];char encoded[557];unsigned n=0,k=0;
  for(unsigned a=0;a<2;a++) {
   const struct intel_hda_sample *v=&r->sample[a];
   for(unsigned i=0;i<52;i++) {
    uint32_t word=i<11?v->global[i]:i==11?v->streams:v->stream[(i-12)/5][(i-12)%5];
    for(unsigned b=0;b<4;b++)bytes[n++]=(uint8_t)(word>>(8*b));
   }
  }
  static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  for(unsigned i=0;i<n;i+=3) {
   unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
   if(left>1)v|=(uint32_t)bytes[i+1]<<8;
   if(left>2)v|=bytes[i+2];
   encoded[k++]=abc[v>>18];encoded[k++]=abc[(v>>12)&63];encoded[k++]=left>1?abc[(v>>6)&63]:'=';encoded[k++]=left>2?abc[v&63]:'=';
  }
  encoded[k]=0;
  console_printf(JOINT_HDA_PREFIX ": v=1 p=%u e=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u,%u b=%s\n",
   (uint64_t)p,(uint64_t)e->id,(uint64_t)e->class_rev,(uint64_t)e->subsystem,(uint64_t)e->command,(uint64_t)e->flags,(uint64_t)e->pm,(uint64_t)e->state,(uint64_t)e->bar,(uint64_t)e->bar_high,(uint64_t)e->reserved,
   (uint64_t)r->maps,(uint64_t)r->retired,(uint64_t)r->retained,(uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->stable,(uint64_t)r->run_mask,(uint64_t)r->dma_mask,(uint64_t)r->reason,r->begin_us,r->last_us,r->end_us,(uint64_t)r->complete,encoded);
 }
 console_printf(JOINT_SUMMARY_PREFIX ": v=1 entry=%u admission=%u state=%u reason=%u pinned=%u retained=%u complete=%u gpu_complete=%u origin=%u begin=%u end=%u last=%u clock_at=%u clock_prev=%u clock_cause=%u checks=%u attached=%u hda_state=%u hda_reason=%u hda_epoch=%u hda_begin=%u hda_end=%u hda_phases=%u hda_checks=%u hda_attempts=%u hda_writes=%u hda_pins=%u hda_retained=%u hda_complete=%u hda_clock_at=%u hda_clock_prev=%u hda_clock_cause=%u root_maps=%u root_retired=%u borrows=%u returns=%u inspection=%u cache=%u obligations=255 assurances=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",
  (uint64_t)joint.entry,(uint64_t)joint.admission,(uint64_t)s->state,(uint64_t)s->reason,(uint64_t)s->pinned,(uint64_t)(joint_terminal()),(uint64_t)s->complete,(uint64_t)s->gpu_complete,s->outer_origin_us,s->begin_us,s->end_us,s->last_us,s->clock_at,s->clock_prev,(uint64_t)s->clock_cause,(uint64_t)s->checks,(uint64_t)s->attachments,
  (uint64_t)s->hda.state,(uint64_t)s->hda.reason,s->hda.epoch,s->hda.begin_us,s->hda.end_us,(uint64_t)s->hda.phases,(uint64_t)s->hda.checks,(uint64_t)s->hda.attempts,(uint64_t)s->hda.writes,(uint64_t)s->hda.pins,(uint64_t)s->hda.retained,(uint64_t)s->hda.complete,s->hda.clock_sample,s->hda.clock_previous,(uint64_t)s->hda.clock_cause,
  (uint64_t)joint.maps,(uint64_t)joint.retired,(uint64_t)joint.borrows,(uint64_t)joint.returns,(uint64_t)joint.inspection,(uint64_t)joint.h.cache_checks);
}
