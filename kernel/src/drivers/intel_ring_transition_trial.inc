#include <srvros/intel_ring_transition.h>
#ifdef SRVROS_INTEL_RING_ENTRY
#define RING_PREFIX "intel-ring-entry"
#else
#define RING_PREFIX "intel-ring"
#endif
struct ring_adapter {struct intel_ring_transition result;struct wake_adapter *a;struct intel_gt_access *g;const struct intel_completion_result *c;unsigned store_checks,store_fault;};
static struct ring_adapter ring_native;
static bool ring_guard(void *ctx) {
 struct ring_adapter *e=ctx;
 return e==&ring_native && e->a==joint.wake && e->g==joint.scope.gpu && e->g &&
  e->result.object==&e->result && e->result.parent==e->g && e->result.source==e->c &&
  e->g->io.context==e->a && engine_guard(e->a);
}
static bool ring_read(void *ctx,uint32_t out[INTEL_BCS_WORDS]) {
 struct ring_adapter *e=ctx;if(!out || !ring_guard(e) || !e->a->mapped[7])return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_ring_read(out);
#else
 for(unsigned i=0;i<6;i++)out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+intel_engine_offsets[i]);
 out[6]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x9c);
 out[7]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x29c);return true;
#endif
}
static enum intel_gt_write ring_write(void *ctx,uint32_t value) {
 struct ring_adapter *e=ctx;struct intel_ring_transition *r=&e->result;struct intel_gt_access *g=e->g;
 if(value!=INTEL_BCS_DISABLED || !ring_guard(e) || !e->a->mapped[7] || e->store_checks ||
    r->borrow.object!=&r->borrow || !r->borrow.active || r->borrow.parent!=g || g->borrows!=1 || g->dependents!=&r->borrow ||
    !r->epoch_advanced || r->epoch_after!=g->engine_epoch || r->epoch_after!=r->epoch_before+1 || r->store_calls!=1 ||
    r->bcs.object!=&r->bcs || !r->bcs.admitted || r->bcs.samples!=2 || r->bcs.attempts!=1 || r->bcs.writes)return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_ring_admission(e);
#endif
 /* Recheck identity after the host fault seam and immediately before the final
  * clock. No callback, parser, or ownership construction follows that clock. */
 if(!ring_guard(e) || r->parent!=g || r->source!=e->c || !intel_ring_transition_store_ready(r))return GT_WRITE_NONE;
 e->store_checks=1;r->store_previous=r->bcs.last_us;r->store_at=now(e->a);uint64_t at=r->store_at;
 if(!at || at<r->last || at<r->bcs.last_us || at<r->bcs.begin_us || at-r->bcs.begin_us>100000 ||
    at<r->source_end || at-r->source_end>100000 || at<r->begin || at-r->begin>150000 ||
    at<g->last || at<g->begin || at-g->begin>300000) {
  e->store_fault=1;if(!(r->reason&IRT_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}r->reason|=IRT_CLOCK;return GT_WRITE_NONE;
 }
 r->last=at;r->bcs.last_us=at;g->last=at;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_ring_write(value);
#else
 __asm__ volatile("mfence":::"memory");
 *(volatile uint32_t *)(WAKE_VA+7*4096+0x3c)=value;
 __asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
static uint64_t ring_post_now(void *ctx) {
 struct ring_adapter *e=&ring_native;struct intel_ring_transition *r=&e->result;
 if(ctx!=e->a)return 0;
 uint64_t at=now(ctx);
 if(!at || at<r->last || at<r->begin || at-r->begin>150000 ||
    (r->post.begin_us && (at<r->post.last_us || at<r->post.begin_us || at-r->post.begin_us>100000))) {
  if(!(r->reason&IRT_CLOCK)){r->clock_at=at;r->clock_prev=r->last;}
  r->reason|=IRT_CLOCK;
 } else r->last=at;
 return at;
}
static bool ring_census(void *ctx,struct intel_completion_result *out) {
 struct ring_adapter *e=ctx;
 if(out!=&e->result.post || !ring_guard(e) || !e->result.bcs.disabled || (e->result.kind==IRT_KIND_ENABLED ? !e->result.epoch_advanced : e->result.kind!=IRT_KIND_DISABLED || e->result.epoch_advanced || e->result.store_calls || e->result.bcs.attempts || e->result.bcs.writes) || e->result.epoch_after!=e->g->engine_epoch)return false;
 return intel_completion_observe(out,e->g,(struct intel_completion_io){e->a,engine_guard,completion_group,completion_context,ring_post_now,COMPLETION_SAMPLE});
}
static bool ring_reader(void *ctx,unsigned n,struct intel_ring_readers *out,unsigned *count) {
 static const uint32_t offsets[9]={0x180,0x19c,0x1ac,0x080,0x084,0x0ac,0x280,0x29c,0x2ac};
 struct ring_adapter *e=ctx;
 if(n>1 || out!=&e->result.readers[n] || count!=&e->result.reader_counts[n] || !ring_guard(e))return false;
 for(unsigned p=0;p<3;p++) {
  if(!e->a->mapped[10+p] || !engine_guard(e->a))return false;
  for(unsigned i=0;i<9;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_ring_reader(p,i,&out->planes[p][i]))return false;
#else
   out->planes[p][i]=*(volatile const uint32_t *)(WAKE_VA+(10+p)*4096+offsets[i]);
#endif
   ++*count;
  }
  const uint32_t *old=result.sample[1].pipe[p],*v=out->planes[p];
  if(v[0]!=old[2] || v[1]!=old[4] || v[2]!=old[6] || v[3]!=old[7] || v[4]!=old[8])return false;
 }
 (void)offsets;unsigned found=0;struct package_pci_snapshot audio={0};
 uint64_t devices=pci_device_count();if(devices>PCI_MAX_DEVICES)return false;
 for(uint64_t i=0;i<devices;i++) {
  const struct pci_device *d=pci_device_at(i);
  if(d && !d->bus && d->device==3 && !d->function && d->vendor_id==0x8086 && d->device_id==0x160c) {
   if(found++)return false;
   package_pci_read(&audio,config_read,(void *)d);
  }
 }
 if(found!=1)return false;
 uint32_t values[10]={audio.id,audio.class_code,audio.command,audio.flags,audio.pm,audio.state,audio.link_cap,audio.link,audio.ltr_cap,audio.ltr};
 for(unsigned i=0;i<10;i++){out->audio[i]=values[i];++*count;}
 return ring_guard(e);
}
static uint64_t ring_now(void *ctx) {
 struct ring_adapter *e=ctx;
 if(e!=&ring_native || e->a!=joint.wake)return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_ring_core_clock(&at))return at;
#endif
 return now(e->a);
}
static bool ring_run(struct wake_adapter *a,struct intel_gt_access *g,const struct intel_completion_result *c,bool policy) {
 if(ring_native.result.object)return false;
 ring_native.a=a;ring_native.g=g;ring_native.c=c;
 bool ok=
#ifdef SRVROS_INTEL_RING_ENTRY
 intel_ring_entry_run(
#else
 intel_ring_transition_run(
#endif
&ring_native.result,g,c,policy,(struct intel_ring_io){&ring_native,ring_guard,ring_read,ring_write,ring_census,ring_reader,ring_now});
 if(ring_native.result.uncertain || ring_native.result.borrow.active)a->uncertain=true;
 return ok;
}
static void ring_report(void) {
 const struct intel_ring_transition *r=&ring_native.result;const struct intel_blitter_result *b=&r->bcs;
 for(unsigned n=0;n<r->read_attempts;n++) {
  const uint32_t *w=b->words[n];
  console_printf(RING_PREFIX "-bcs: v=1 n=%u d=%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
 }
 completion_report_named(&r->post,RING_PREFIX "-post");
 for(unsigned n=0;n<r->reader_attempts;n++) {
  for(unsigned p=0;p<3;p++) {
   const uint32_t *w=r->readers[n].planes[p];
   console_printf(RING_PREFIX "-reader: v=1 n=%u p=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
  }
  const uint32_t *a=r->readers[n].audio;
  console_printf(RING_PREFIX "-audio: v=1 n=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)a[0],(uint64_t)a[1],(uint64_t)a[2],(uint64_t)a[3],(uint64_t)a[4],(uint64_t)a[5],(uint64_t)a[6],(uint64_t)a[7],(uint64_t)a[8],(uint64_t)a[9]);
 }
 console_printf(RING_PREFIX "-summary: "
#ifdef SRVROS_INTEL_RING_ENTRY
 "v=2 "
#else
 "v=1 "
#endif
 "policy=%u borrowed=%u returned=%u blocked=%u post_bound=%u readers_bound=%u complete=%u retained=%u uncertain=%u reason=%u checks=%u read_attempts=%u reader_attempts=%u reader0=%u reader1=%u store_calls=%u store_checks=%u store_fault=%u begin=%u end=%u last=%u source_begin=%u source_end=%u source_last=%u epoch_before=%u epoch_after=%u epoch_advanced=%u parent_attempts=%u clock_at=%u clock_prev=%u store_at=%u store_previous=%u return_begin=%u post_begin=%u post_end=%u post_last=%u bcs_samples=%u bcs_reads=%u bcs_admitted=%u bcs_attempts=%u bcs_writes=%u bcs_disabled=%u bcs_uncertain=%u bcs_reason=%u bcs_begin=%u bcs_end=%u reset_writes=0 managed_pages=0"
#ifdef SRVROS_INTEL_RING_ENTRY
 " kind=%u"
#endif
 "\n",
 (uint64_t)r->policy,(uint64_t)(r->borrow.parent!=0),(uint64_t)r->returned,(uint64_t)r->blocked,(uint64_t)r->post_bound,(uint64_t)r->readers_bound,(uint64_t)r->complete,(uint64_t)r->borrow.active,(uint64_t)r->uncertain,(uint64_t)r->reason,(uint64_t)r->checks,(uint64_t)r->read_attempts,(uint64_t)r->reader_attempts,(uint64_t)r->reader_counts[0],(uint64_t)r->reader_counts[1],(uint64_t)r->store_calls,(uint64_t)ring_native.store_checks,(uint64_t)ring_native.store_fault,r->begin,r->end,r->last,r->source_begin,r->source_end,r->source_last,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)r->epoch_advanced,(uint64_t)r->parent_attempts,r->clock_at,r->clock_prev,r->store_at,r->store_previous,r->return_begin,r->post_begin,r->post_end,r->post_last,(uint64_t)b->samples,(uint64_t)b->reads,(uint64_t)b->admitted,(uint64_t)b->attempts,(uint64_t)b->writes,(uint64_t)b->disabled,(uint64_t)b->uncertain,(uint64_t)b->reason,b->begin_us,b->end_us
#ifdef SRVROS_INTEL_RING_ENTRY
 ,(uint64_t)r->kind
#endif
 );
}

#undef RING_PREFIX
