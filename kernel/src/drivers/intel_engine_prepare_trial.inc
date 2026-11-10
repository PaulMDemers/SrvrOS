#include <srvros/intel_engine_prepare.h>
#ifdef SRVROS_INTEL_PREPARE_RESET
#include <srvros/intel_engine_held_reset.h>
#endif
#ifdef SRVROS_INTEL_HELD_REQUESTER
#include <srvros/intel_pci_held.h>
#endif
struct engine_prepare_trial {
 struct intel_engine_prepare_result action;
 struct intel_completion_result post;
#ifdef SRVROS_INTEL_PREPARE_RESET
 struct intel_engine_held_reset_result reset;
#endif
#ifdef SRVROS_INTEL_HELD_REQUESTER
 struct intel_pci_held_result requester;
 bool held_provider_enter,held_provider_exit;
#endif
 uint32_t planes[2][3][9],audio[2][10];
 unsigned samples,plane_reads;
 uint64_t begin,end;
};
struct engine_prepare_adapter {struct wake_adapter *a;struct intel_gt_access *g;struct engine_prepare_trial *trial;bool failed;
#ifdef SRVROS_INTEL_PREPARE_RESET
 unsigned reset_snapshot_reads,reset_pulses;
#endif
};
/* Core bookkeeping does not issue target accesses. Every provider below owns
 * its full entry/exit backing validation; these fresh checks keep the core's
 * before/after parent and clock brackets live without repeating that census.
 * No successful guard is cached or carried into the next callback. */
static bool eprepare_guard(void *ctx) {
 struct engine_prepare_adapter *p=ctx;struct wake_adapter *a=p->a;
 if(p->failed || a->uncertain || !wake_irq_off() || p->g!=p->trial->action.parent ||
    a->fw.gt.a.mapped || !display_access_held(&a->fw.gt.token) ||
    !intel_boot_check(INTEL_BOOT_OBSERVER,a->fw.gt.a.common.gpu) ||
    vmm_current_address_space()!=a->fw.gt.a.common.cr3)return false;
 uint64_t physical;
 if(vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,BACKING_VA,&physical))return false;
 for(unsigned i=0;i<WAKE_MAPS;i++)if(!a->mapped[i] ||
    !vmm_virt_to_phys_in_space(a->fw.gt.a.common.cr3,WAKE_VA+4096*i,&physical) ||
    physical!=result.mmio_base+wake_pages[i])return false;
 return engine_requesters(a);
}
static bool eprepare_enter(struct engine_prepare_adapter *p) {
 return eprepare_guard(p) && engine_guard(p->a);
}
static void eprepare_exit(struct engine_prepare_adapter *p) {
 /* Keep completion prefixes: a failed full exit is delivered by the next core
  * guard, after the core accounts the completed loads/stores. Never retry it. */
 if(!engine_guard(p->a)){p->failed=true;p->a->uncertain=true;}
}
static uint64_t eprepare_now(void *ctx) {return now(((struct engine_prepare_adapter *)ctx)->a);}
static void eprepare_pause(void *ctx) {(void)ctx;wake_pause(NULL);}
static bool eprepare_sample(void *ctx,uint32_t out[6],unsigned *count) {
 struct engine_prepare_adapter *p=ctx;
 if(!eprepare_guard(p))return false;
 bool ok=takeover_sample(p->a,out,count);eprepare_exit(p);return ok;
}
static bool eprepare_control(void *ctx,unsigned e,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;struct wake_adapter *a=p->a;
 if(e>=5 || !out || !eprepare_enter(p) || !completion_mapping(a,e+3) || !engine_requesters(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 bool ok=intel_display_test_takeover_read(e,takeover_registers[e],out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+(e+3)*4096+0xd0);bool ok=true;
#endif
 eprepare_exit(p);return ok;
}
static enum intel_gt_write eprepare_transition(void *ctx,unsigned e,uint32_t value,uint32_t *out) {
 struct engine_prepare_adapter *p=ctx;struct wake_adapter *a=p->a;
 if(e>=5 || !out || (value!=INTEL_ENGINE_PREPARE_REQUEST && value!=INTEL_ENGINE_PREPARE_CANCEL) ||
    !eprepare_enter(p) || !completion_mapping(a,e+3) || !engine_requesters(a))return GT_WRITE_NONE;
 /* Full admission must finish inside the dependent deadline BEFORE issue.
  * Do not use the parent's longer deadline to authorize a late target store. */
 uint64_t at=eprepare_now(ctx);const struct intel_engine_prepare_result *r=&p->trial->action;
 if(!at || at<r->last_us || at<r->begin_us || at-r->begin_us>INTEL_ENGINE_PREPARE_TOTAL_US)return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write wrote=intel_display_test_prepare_transition(e,value,out);
 if(wrote==GT_WRITE_DONE)eprepare_exit(p);
 return wrote;
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+(e+3)*4096+0xd0);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");
 if(!engine_requesters(a))return GT_WRITE_UNCERTAIN;
 *out=*reg;eprepare_exit(p);return GT_WRITE_DONE;
#endif
}
static bool eprepare_planes(struct wake_adapter *a,struct intel_gt_access *g,struct engine_prepare_trial *t,unsigned n) {
 static const uint32_t offsets[9]={0x180,0x19c,0x1ac,0x080,0x084,0x0ac,0x280,0x29c,0x2ac};
 if(n>1 || n!=t->samples || !intel_gt_access_check(g))return false;
 if(!n)t->begin=timer_monotonic_us();
 for(unsigned p=0;p<3;p++) {
  if(!a->mapped[10+p] || !engine_guard(a))return false;
  for(unsigned i=0;i<9;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_prepare_reader(0x70000+p*4096+offsets[i],&t->planes[n][p][i]))return false;
#else
   t->planes[n][p][i]=*(volatile const uint32_t *)(WAKE_VA+(10+p)*4096+offsets[i]);
#endif
   ++t->plane_reads;if(t->planes[n][p][i]==UINT32_MAX)return false;
  }
  /* Bind both observations to the accepted inherited primary/cursor state. */
  const uint32_t *old=result.sample[1].pipe[p],*v=t->planes[n][p];
  if(v[0]!=old[2] || v[1]!=old[4] || v[2]!=old[6] || v[3]!=old[7] || v[4]!=old[8])return false;
 }
 unsigned found=0;struct package_pci_snapshot snap={0};
 uint64_t count=pci_device_count();if(count>PCI_MAX_DEVICES)return false;
 for(uint64_t i=0;i<count;i++) {
  const struct pci_device *d=pci_device_at(i);
  if(d && !d->bus && d->device==3 && !d->function && d->vendor_id==0x8086 && d->device_id==0x160c) {
   if(found++)return false;
   package_pci_read(&snap,config_read,(void *)d);
  }
 }
 if(found!=1 || snap.state!=1 || snap.id!=0x160c8086u || snap.class_code!=0x040300u)return false;
 const uint32_t words[10]={snap.id,snap.class_code,snap.command,snap.flags,snap.pm,snap.link_cap,snap.link,snap.ltr_cap,snap.ltr,snap.state};
 for(unsigned i=0;i<10;i++)t->audio[n][i]=words[i];
 ++t->samples;t->end=timer_monotonic_us();
 return t->begin && t->end>t->begin && t->end-t->begin<=GT_ACCESS_TOTAL_US && intel_gt_access_check(g);
}
/* The nested census keeps the same live core brackets as preparation itself.
 * Each provider still validates full backing/endpoint state at entry and exit;
 * failure poisons the outer adapter before the core accepts the sample. */
static bool eprepare_post_group(void *ctx,unsigned e,uint32_t out[INTEL_COMPLETION_WORDS]) {
 struct engine_prepare_adapter *p=ctx;
 if(!eprepare_enter(p))return false;
 bool ok=completion_group_raw(p->a,e,out);eprepare_exit(p);return ok;
}
static bool eprepare_post_context(void *ctx,uint32_t out[INTEL_COMPLETION_CONTEXT]) {
 struct engine_prepare_adapter *p=ctx;
 if(!eprepare_enter(p))return false;
 bool ok=completion_context_raw(p->a,out);eprepare_exit(p);return ok;
}
#ifndef SRVROS_INTEL_CENSUS_SCALAR_TEST
static bool eprepare_post_sample(void *ctx,uint32_t words[INTEL_ENGINE_COUNT][INTEL_COMPLETION_WORDS],uint32_t context[INTEL_COMPLETION_CONTEXT]) {
 struct engine_prepare_adapter *p=ctx;
 if(!eprepare_guard(p))return false;
 /* completion_snapshot supplies full entry validation plus fresh requester
  * and exact mapping checks between groups. Add its full exit here. */
 bool ok=completion_snapshot(p->a,words,context);eprepare_exit(p);return ok;
}
#define EPREPARE_POST_SAMPLE eprepare_post_sample
#else
#define EPREPARE_POST_SAMPLE NULL
#endif
static bool eprepare_preserve(void *ctx,const struct intel_completion_result *pre) {
 struct engine_prepare_adapter *p=ctx;struct engine_prepare_trial *t=p->trial;
 if(!eprepare_enter(p))return false;
 if(!intel_completion_observe(&t->post,p->g,(struct intel_completion_io){p,eprepare_guard,eprepare_post_group,eprepare_post_context,eprepare_now,EPREPARE_POST_SAMPLE}) ||
    !eprepare_planes(p->a,p->g,t,1) || t->post.engine_epoch!=
#ifdef SRVROS_INTEL_PREPARE_RESET
    t->reset.epoch_after || !t->reset.complete || t->reset.epoch_before!=pre->engine_epoch || t->reset.epoch_after!=pre->engine_epoch+1
#else
    pre->engine_epoch
#endif
    )return false;
 for(unsigned n=0;n<2;n++) {
  for(unsigned e=0;e<5;e++)for(unsigned i=0;i<8;i++)if(pre->words[n][e][i]!=t->post.words[n][e][i])return false;
  for(unsigned i=0;i<6;i++)if(pre->context[n][i]!=t->post.context[n][i])return false;
 }
 for(unsigned pidx=0;pidx<3;pidx++)for(unsigned i=0;i<9;i++)if(t->planes[0][pidx][i]!=t->planes[1][pidx][i])return false;
 for(unsigned i=0;i<10;i++)if(t->audio[0][i]!=t->audio[1][i])return false;
 bool ok=t->samples==2 && t->plane_reads==54;eprepare_exit(p);return ok;
}
#ifdef SRVROS_INTEL_HELD_REQUESTER
#include "intel_pci_held_trial.inc"
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
#include "intel_engine_held_reset_trial.inc"
#endif
static void eprepare_run(struct wake_adapter *a,struct intel_gt_access *g,const struct intel_takeover_result *pre,struct engine_prepare_trial *t,bool policy) {
 if(!eprepare_planes(a,g,t,0))return;
 struct engine_prepare_adapter p={.a=a,.g=g,.trial=t};
#ifdef SRVROS_INTEL_PREPARE_RESET
 (void)intel_engine_prepare_with_bcs_reset(&t->action,g,pre,policy,(struct intel_engine_prepare_io){&p,eprepare_guard,eprepare_sample,eprepare_control,eprepare_transition,eprepare_preserve,eprepare_now,eprepare_pause},(struct intel_engine_prepare_action){&p,eheld_action},&t->reset,(struct intel_engine_held_reset_io){&p,ehr_guard,ehr_snapshot,ehr_domain,ehr_transition,ehr_begin,ehr_end,ehr_now,eprepare_pause});
#else
#ifdef SRVROS_INTEL_HELD_REQUESTER
 (void)intel_engine_prepare_with_action(&t->action,g,pre,policy,(struct intel_engine_prepare_io){&p,eprepare_guard,eprepare_sample,eprepare_control,eprepare_transition,eprepare_preserve,eprepare_now,eprepare_pause},(struct intel_engine_prepare_action){&p,eheld_action});
#else
 (void)intel_engine_prepare(&t->action,g,pre,policy,(struct intel_engine_prepare_io){&p,eprepare_guard,eprepare_sample,eprepare_control,eprepare_transition,eprepare_preserve,eprepare_now,eprepare_pause});
#endif
#endif
 if(t->action.uncertain || t->action.borrow.active || (t->action.reason&IEP_PARENT))a->uncertain=true;
}
static void eprepare_report(const struct engine_prepare_trial *t) {
 const struct intel_engine_prepare_result *r=&t->action;
#ifdef SRVROS_INTEL_HELD_REQUESTER
 eheld_report(&t->requester);
 console_printf("intel-prepare-action: v=1 attempted=%u completed=%u active=%u begin=%u end=%u provider_enter=%u provider_exit=%u\n",(uint64_t)r->action_attempted,(uint64_t)r->action_completed,(uint64_t)r->action_active,r->action_begin_us,r->action_end_us,(uint64_t)t->held_provider_enter,(uint64_t)t->held_provider_exit);
#endif
#ifdef SRVROS_INTEL_PREPARE_RESET
 ehr_report(&t->reset);
#endif
 for(unsigned n=0;n<r->sample_attempts && n<5;n++) {
  console_printf("intel-prepare-row: v=1 s=%u n=%u d=",(uint64_t)n,(uint64_t)r->counts[n]);
  for(unsigned i=0;i<r->counts[n] && i<6;i++)console_printf("%s%u",i?",":"",(uint64_t)r->words[n][i]);
  if(!r->counts[n])console_write("-");
  console_write("\n");
 }
 for(unsigned n=0;n<r->acks && n<10;n++)console_printf("intel-prepare-ack: v=2 n=%u d=%u,%u,%u\n",(uint64_t)n,(uint64_t)r->ack[n],(uint64_t)r->final[n],(uint64_t)r->phase_polls[n]);
#ifdef SRVROS_INTEL_HELD_REQUESTER
 eheld_post_report(&t->post);
#else
 completion_report_named(&t->post,"intel-prepare-post");
#endif
 for(unsigned n=0;n<t->samples;n++) {
  for(unsigned p=0;p<3;p++) {
   const uint32_t *w=t->planes[n][p];
   console_printf("intel-prepare-plane: v=1 s=%u p=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
  }
  const uint32_t *w=t->audio[n];
  console_printf("intel-prepare-audio: v=1 s=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8],(uint64_t)w[9]);
 }
#ifdef SRVROS_INTEL_PREPARE_RESET
 console_printf("intel-prepare-summary: v=2 epoch_after=%u begin=%u end=%u epoch=%u attempts=%u writes=%u reads=%u polls=%u snapshots=%u sample_attempts=%u acks=%u ready=%u cancelled=%u blocked=%u admitted=%u held=%u preserved=%u uncertain=%u returned=%u borrowed=%u retained=%u reason=%u stage=%u index=%u clock_at=%u clock_prev=%u plane_samples=%u plane_reads=%u complete=%u obligations=255 reset_writes=%u pte_writes=0 managed_pages=0 hardware_ownership=0\n",(uint64_t)t->reset.epoch_after,r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->snapshots,(uint64_t)r->sample_attempts,(uint64_t)r->acks,(uint64_t)r->ready,(uint64_t)r->cancelled,(uint64_t)r->blocked,(uint64_t)r->admitted,(uint64_t)r->held,(uint64_t)r->preserved,(uint64_t)r->uncertain,(uint64_t)r->returned,(uint64_t)(r->borrow.parent!=NULL),(uint64_t)r->borrow.active,(uint64_t)r->reason,(uint64_t)r->stage,(uint64_t)r->index,r->clock_at,r->clock_prev,(uint64_t)t->samples,(uint64_t)t->plane_reads,(uint64_t)r->complete,(uint64_t)t->reset.writes);
#else
 console_printf("intel-prepare-summary: v=1 begin=%u end=%u epoch=%u attempts=%u writes=%u reads=%u polls=%u snapshots=%u sample_attempts=%u acks=%u ready=%u cancelled=%u blocked=%u admitted=%u held=%u preserved=%u uncertain=%u returned=%u borrowed=%u retained=%u reason=%u stage=%u index=%u clock_at=%u clock_prev=%u plane_samples=%u plane_reads=%u complete=%u obligations=255 reset_writes=0 pte_writes=0 managed_pages=0 hardware_ownership=0\n",r->begin_us,r->end_us,(uint64_t)r->epoch,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->snapshots,(uint64_t)r->sample_attempts,(uint64_t)r->acks,(uint64_t)r->ready,(uint64_t)r->cancelled,(uint64_t)r->blocked,(uint64_t)r->admitted,(uint64_t)r->held,(uint64_t)r->preserved,(uint64_t)r->uncertain,(uint64_t)r->returned,(uint64_t)(r->borrow.parent!=NULL),(uint64_t)r->borrow.active,(uint64_t)r->reason,(uint64_t)r->stage,(uint64_t)r->index,r->clock_at,r->clock_prev,(uint64_t)t->samples,(uint64_t)t->plane_reads,(uint64_t)r->complete);
#endif
}
