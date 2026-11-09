/* Dedicated BCS-only reset image. No display/HDA/GGTT/translation writer. */
#include <srvros/intel_bcs_reset.h>
struct bcs_trial {
 struct intel_bcs_reset_result reset;
 struct intel_completion_result post;
 struct intel_gt_borrow preservation;
 bool blocked_close;
 uint32_t planes[2][3][9],audio[2][10];
 unsigned samples,plane_reads;
 uint64_t begin,end;
 bool preserved,complete;
};
static uint64_t bcs_now(void *ctx) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 (void)ctx;return intel_display_test_bcs_now();
#else
 return now(ctx);
#endif
}
static bool bcs_domain(void *ctx,uint32_t *out) {
 struct wake_adapter *a=ctx;
 if(!out || !a->mapped[9] || !engine_guard(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_bcs_read(0x941c,out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+9*4096+0x41c);return true;
#endif
}
static enum intel_gt_write bcs_domain_write(void *ctx,uint32_t value) {
 struct wake_adapter *a=ctx;
 if(value!=8 || !a->mapped[9] || !engine_guard(a))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_bcs_write(0x941c,value);
#else
 __asm__ volatile("mfence":::"memory");
 *(volatile uint32_t *)(WAKE_VA+9*4096+0x41c)=value;
 __asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
#if defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL) && !defined(SRVROS_INTEL_BCS_SCALAR_TEST)
static bool bcs_snapshot(void *ctx,uint32_t out[8],uint32_t *control,uint32_t *domain) {
 struct wake_adapter *a=ctx;
 static const uint32_t regs[8]={0x22030,0x22034,0x22038,0x2203c,0x22234,0x22238,0x2209c,0x2229c};
 if(!out || !control || !domain || !a->mapped[7] || !a->mapped[9] || !engine_guard(a))return false;
 for(unsigned i=0;i<8;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_reset_ready_read(regs[i],&out[i]))return false;
#else
  out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+(regs[i]&4095));
#endif
 }
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_reset_ready_read(0x220d0,control) && intel_display_test_bcs_read(0x941c,domain);
#else
 *control=*(volatile const uint32_t *)(WAKE_VA+7*4096+0xd0);
 *domain=*(volatile const uint32_t *)(WAKE_VA+9*4096+0x41c);return true;
#endif
}
#endif
static bool bcs_planes(struct wake_adapter *a,struct intel_gt_access *g,struct bcs_trial *t,unsigned n) {
 static const uint32_t offsets[9]={0x180,0x19c,0x1ac,0x080,0x084,0x0ac,0x280,0x29c,0x2ac};
 if(n>1 || n!=t->samples || !intel_gt_access_check(g))return false;
 if(!n)t->begin=timer_monotonic_us();
 for(unsigned p=0;p<3;p++) {
  if(!a->mapped[10+p] || !engine_guard(a))return false;
  for(unsigned i=0;i<9;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_bcs_read(0x70000+p*4096+offsets[i],&t->planes[n][p][i]))return false;
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
static void bcs_run(struct wake_adapter *a,struct intel_gt_access *g,const struct intel_completion_result *pre,struct bcs_trial *t,bool policy) {
 if(!bcs_planes(a,g,t,0))return;
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 (void)intel_bcs_reset_after_shutdown(
#else
 (void)intel_bcs_reset(
#endif
&t->reset,g,pre,policy,(struct intel_bcs_reset_io){a,engine_guard,reset_ready_control,bcs_domain,reset_ready_group,reset_ready_store,bcs_domain_write,bcs_now,wake_pause,
#if defined(SRVROS_INTEL_BCS_SHUTDOWN_TRIAL) && !defined(SRVROS_INTEL_BCS_SCALAR_TEST)
 bcs_snapshot
#else
 NULL
#endif
 });
 if(t->reset.uncertain || t->reset.borrow.active){a->uncertain=true;return;}
 if(!t->reset.complete && !t->reset.reset_acks)return;
 if(!intel_gt_access_borrow(g,&t->preservation)){a->uncertain=true;return;}
 unsigned stores=g->writes;
 t->blocked_close=!intel_gt_access_close(g) && g->writes==stores && g->state==GT_ACCESS_AWAKE && g->pinned;
 if(!t->blocked_close){a->uncertain=true;return;}
 bool post=intel_completion_observe(&t->post,g,(struct intel_completion_io){a,engine_guard,completion_group,completion_context,now});
 bool planes=post && bcs_planes(a,g,t,1);
 t->preserved=post && planes && pre->engine_epoch==t->reset.epoch_before && t->post.engine_epoch==t->reset.epoch_after;
 for(unsigned n=0;n<2;n++)for(unsigned e=0;e<5;e++)for(unsigned i=0;i<8;i++)if(t->post.words[n][e][i]!=pre->words[n][e][i])t->preserved=false;
 for(unsigned n=0;n<2;n++)for(unsigned i=0;i<6;i++)if(t->post.context[n][i]!=pre->context[n][i])t->preserved=false;
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)if(t->planes[0][p][i]!=t->planes[1][p][i])t->preserved=false;
 for(unsigned i=0;i<10;i++)if(t->audio[0][i]!=t->audio[1][i])t->preserved=false;
 bool verified=t->preserved && t->samples==2 && t->plane_reads==54;
 if(verified && !intel_gt_access_return(&t->preservation))verified=false;
 t->complete=verified && t->reset.complete;
 /* Even a clean reset cannot close out unverified readers or engine state. */
 if(!verified)a->uncertain=true;
}
static void bcs_report(const struct bcs_trial *t,const struct intel_gt_access *g) {
 const struct intel_bcs_reset_result *r=&t->reset;
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 console_printf("intel-bcs-clock: v=1 why=%u stage=%u at=%u prev=%u n=%u\n",(uint64_t)r->clock_cause,(uint64_t)r->clock_stage,r->clock_sample,r->clock_previous,(uint64_t)r->snapshots);
#endif
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
 const struct intel_blitter_result *b=&r->shutdown;
 for(unsigned n=0;n<b->samples;n++) {
  const uint32_t *w=b->words[n];
  console_printf("intel-bcs-shutdown-row: v=2 s=%u d=%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
 }
 console_printf("intel-bcs-shutdown-summary: v=1 samples=%u reads=%u attempts=%u writes=%u reason=%u begin=%u end=%u disabled=%u entry_disabled=%u uncertain=%u owner_begin=%u epoch=%u\n",(uint64_t)b->samples,(uint64_t)b->reads,(uint64_t)b->attempts,(uint64_t)b->writes,(uint64_t)b->reason,b->begin_us,b->end_us,(uint64_t)b->disabled,(uint64_t)b->entry_disabled,(uint64_t)b->uncertain,g->begin,(uint64_t)r->epoch_before);
#endif

 completion_report_named(&t->post,"intel-bcs-post");
 for(unsigned n=0;n<r->samples;n++) {
  const uint32_t *w=r->words[n];
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
  console_printf("intel-bcs-row: v=2 s=%u d=%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#else
  console_printf("intel-bcs-row: v=1 sample=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#endif
 }
 for(unsigned n=0;n<r->controls_read;n++)console_printf("intel-bcs-control: v=1 phase=%u value=%u\n",(uint64_t)n,(uint64_t)r->controls[n]);
 for(unsigned n=0;n<r->domains_read;n++)console_printf("intel-bcs-domain: v=1 phase=%u value=%u\n",(uint64_t)n,(uint64_t)r->domains[n]);
 for(unsigned n=0;n<t->samples;n++) {
  for(unsigned p=0;p<3;p++) {
   const uint32_t *w=t->planes[n][p];
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
   console_printf("intel-bcs-plane: v=2 s=%u p=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
#else
   console_printf("intel-bcs-plane: v=1 sample=%u pipe=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u w8=%u\n",(uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
#endif
  }
  const uint32_t *w=t->audio[n];
#ifdef SRVROS_INTEL_BCS_SHUTDOWN_TRIAL
  console_printf("intel-bcs-audio: v=2 s=%u d=%u,%u,%u,%u,%u,%u,%u,%u,%u,%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8],(uint64_t)w[9]);
#else
  console_printf("intel-bcs-audio: v=1 sample=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u w8=%u w9=%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8],(uint64_t)w[9]);
#endif
 }
 console_printf("intel-bcs-summary: v=1 admitted=%u ready=%u cancelled=%u samples=%u controls=%u domains=%u reads=%u polls=%u attempts=%u writes=%u reset_attempts=%u reset_writes=%u reset_acks=%u reason=%u begin_us=%u end_us=%u settle_begin=%u settle_end=%u blocked_close=%u borrowed=%u returned=%u uncertain=%u core_complete=%u epoch_before=%u epoch_after=%u plane_samples=%u plane_reads=%u preserve_begin=%u preserve_end=%u preserved=%u pin=%u pin_returned=%u pin_blocked=%u owner_begin=%u owner_end=%u complete=%u obligations=255 posted_drain=0 prefetch_retired=0 pipeline_flushed=0 tlb_writes=0 cache_ownership=0 reader_retirement=0 managed_pages=0\n",(uint64_t)r->admitted,(uint64_t)r->ready,(uint64_t)r->cancelled,(uint64_t)r->samples,(uint64_t)r->controls_read,(uint64_t)r->domains_read,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reset_attempts,(uint64_t)r->reset_writes,(uint64_t)r->reset_acks,(uint64_t)r->reason,r->begin_us,r->end_us,r->settle_begin_us,r->settle_end_us,(uint64_t)r->blocked_close,(uint64_t)(r->borrow.parent!=0),(uint64_t)(r->borrow.parent && !r->borrow.active),(uint64_t)r->uncertain,(uint64_t)r->complete,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)t->samples,(uint64_t)t->plane_reads,t->begin,t->end,(uint64_t)t->preserved,(uint64_t)(t->preservation.parent!=0),(uint64_t)(t->preservation.parent && !t->preservation.active),(uint64_t)t->blocked_close,g->begin,g->last,(uint64_t)(t->complete && g->state==GT_ACCESS_CLOSED));
}
