#include <srvros/intel_nonzero_reset.h>
#ifdef SRVROS_INTEL_NONZERO_SOURCE
#include <srvros/intel_nonzero_source.h>
#endif
struct nonzero_adapter {
 struct intel_nonzero_reset r;
 struct wake_adapter *a;
 struct intel_gt_access *g;
 unsigned issued_controls,issued_resets;
 uint64_t store_at[12],store_previous[12];
 bool failed;
#ifdef SRVROS_INTEL_NONZERO_SOURCE
 struct intel_nonzero_source source;
 struct scanout_handoff_snapshot source_input;
 unsigned source_reads;
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 bool source_batch_entered,source_batch_checked;
#endif
 uint64_t source_at[6],source_previous[6];
#endif
};
static struct nonzero_adapter nonzero_native;
static bool nonzero_guard(void *ctx) {
 struct nonzero_adapter *p=ctx;uint64_t physical;
 if(p!=&nonzero_native || p->failed || !p->a || p->a!=joint.wake || p->g!=joint.scope.gpu ||
    p->r.provider!=p || p->r.entry!=&ring_native.result || p->r.source!=&ring_native.result.post ||
    p->r.parent!=p->g || p->g->io.context!=p->a || !intel_nonzero_reset_live(&p->r) ||
    p->a->uncertain || !wake_irq_off() || !joint_fast() || p->a->fw.gt.a.mapped ||
    !display_access_held(&p->a->fw.gt.token) ||
    !intel_boot_check(INTEL_BOOT_OBSERVER,p->a->fw.gt.a.common.gpu) ||
    vmm_current_address_space()!=p->a->fw.gt.a.common.cr3 ||
    vmm_virt_to_phys_in_space(p->a->fw.gt.a.common.cr3,BACKING_VA,&physical))return false;
 for(unsigned i=0;i<WAKE_MAPS;i++)if(!completion_mapping(p->a,i))return false;
 return engine_requesters(p->a);
}
static bool nonzero_enter(struct nonzero_adapter *p) {return nonzero_guard(p) && engine_guard(p->a);}
static void nonzero_exit(struct nonzero_adapter *p) {
 if(!engine_guard(p->a)){p->failed=true;p->a->uncertain=true;}
}
static uint64_t nonzero_now(void *ctx) {
 struct nonzero_adapter *p=ctx;if(p!=&nonzero_native || p->a!=joint.wake)return 0;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 uint64_t at;if(intel_display_test_nonzero_clock(&at))return at;
#endif
 return now(p->a);
}
static void nonzero_pause(void *ctx){(void)ctx;wake_pause(NULL);}
static bool nonzero_load(struct nonzero_adapter *p,unsigned index,uint32_t *out) {
 static const unsigned offsets[6]={0x20d0,0x120d0,0x1a0d0,0x1c0d0,0x220d0,0x941c};
 if(!out || index>=6 || !nonzero_guard(p) || !completion_mapping(p->a,index<5?index+3:9))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_nonzero_read(offsets[index],out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+(index<5?index+3:9)*4096+(offsets[index]&4095));return true;
#endif
}
static bool nonzero_sample(void *ctx,uint32_t out[6],unsigned *count) {
 struct nonzero_adapter *p=ctx;if(!out || !count || *count || !nonzero_enter(p))return false;
 for(unsigned i=0;i<6;i++){if(!nonzero_load(p,i,&out[i]))return false;++*count;}
 nonzero_exit(p);return true;
}
static bool nonzero_control(void *ctx,unsigned e,uint32_t *out) {
 struct nonzero_adapter *p=ctx;if(e>=5 || !nonzero_enter(p))return false;
 bool ok=nonzero_load(p,e,out);nonzero_exit(p);return ok;
}
/* Final store clock belongs to the unchanged preparation and10ms phase. No
 * provider call or admission loop occurs between this check and the DWORD. */
static bool nonzero_store_admit(struct nonzero_adapter *p,bool reset,unsigned e,uint32_t value) {
 struct intel_nonzero_reset *r=&p->r;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_nonzero_admission(reset,e);
#endif
 if(!nonzero_enter(p) || !intel_nonzero_reset_write_ready(r,reset,e,value) ||
    p->issued_controls!=r->control_writes || p->issued_resets!=r->reset_writes || !engine_requesters(p->a))return false;
 unsigned n=reset?10+e:r->controls-1;uint64_t previous=r->last;
 uint64_t at=nonzero_now(p);p->store_at[n]=at;p->store_previous[n]=previous;
 uint64_t phase=reset?r->pulse_begin[e]:r->control_begin[n];
 if(!at || at<previous || at<r->begin || at-r->begin>100000 || at<phase || at-phase>10000 ||
    at<p->g->last || at<p->g->begin || at-p->g->begin>300000) {
  if(!(r->reason&INZ_CLOCK)){r->clock_at=at;r->clock_prev=previous;}r->reason|=INZ_CLOCK;return false;
 }
 /* No callback follows the final clock. Revalidate the original pointer and
  * origin fields with direct loads before dereferencing or issuing the store. */
 if(r->object!=r || r->provider!=p || r->parent!=p->g || r->entry!=&ring_native.result ||
    r->source!=&ring_native.result.post || r->parent_begin!=p->g->begin || !r->policy ||
    ring_native.result.parent!=p->g || ring_native.result.object!=&ring_native.result) {
  r->reason|=INZ_SOURCE;return false;
 }
#ifdef SRVROS_INTEL_STARTUP_SOURCE
 /* No callback/MMIO after the final clock: recheck the complete pure source
  * and captured route before this separate image may issue its DWORD. */
 if(!r->startup || r->entry_kind!=ring_native.result.kind ||
    !intel_nonzero_reset_write_ready(r,reset,e,value)){r->reason|=INZ_SOURCE;return false;}
#endif
 r->last=at;p->g->last=at;return true;
}
static enum intel_gt_write nonzero_request(void *ctx,unsigned e,uint32_t value,uint32_t *out) {
 struct nonzero_adapter *p=ctx;
 if(!out || e>=5 || !nonzero_store_admit(p,false,e,value))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write wrote=intel_display_test_nonzero_write(0x20d0+(e==0?0:e==1?0x10000:e==2?0x18000:e==3?0x1a000:0x20000),value,out);
 if(wrote==GT_WRITE_DONE)++p->issued_controls;
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+(e+3)*4096+0xd0);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");++p->issued_controls;
 if(!engine_requesters(p->a))return GT_WRITE_UNCERTAIN;
 *out=*reg;enum intel_gt_write wrote=GT_WRITE_DONE;
#endif
 if(wrote==GT_WRITE_DONE)nonzero_exit(p);
 return wrote;
}
static bool nonzero_bcs(void *ctx,uint32_t out[8],uint32_t *domain,unsigned *count) {
 struct nonzero_adapter *p=ctx;
 if(!out || !domain || !count || *count || !nonzero_enter(p))return false;
 for(unsigned i=0;i<8;i++) {
  if(!nonzero_guard(p))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_nonzero_read(i<6?0x22000+intel_engine_offsets[i]:i==6?0x2209c:0x2229c,&out[i]))return false;
#else
  out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+(i<6?intel_engine_offsets[i]:i==6?0x9c:0x29c));
#endif
  ++*count;
 }
 if(!nonzero_load(p,5,domain))return false;
 ++*count;nonzero_exit(p);return true;
}
static bool nonzero_domain(void *ctx,uint32_t *out) {
 struct nonzero_adapter *p=ctx;if(!nonzero_enter(p))return false;
 bool ok=nonzero_load(p,5,out);nonzero_exit(p);return ok;
}
static enum intel_gt_write nonzero_reset(void *ctx,uint32_t value,uint32_t *out) {
 struct nonzero_adapter *p=ctx;unsigned n=p->r.resets-1;
 if(!out || n>=2 || !nonzero_store_admit(p,true,n,value))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 enum intel_gt_write wrote=intel_display_test_nonzero_write(0x941c,value,out);
 if(wrote==GT_WRITE_DONE)++p->issued_resets;
#else
 volatile uint32_t *reg=(volatile uint32_t *)(WAKE_VA+9*4096+0x41c);
 __asm__ volatile("mfence":::"memory");*reg=value;__asm__ volatile("mfence":::"memory");++p->issued_resets;
 if(!engine_requesters(p->a))return GT_WRITE_UNCERTAIN;
 *out=*reg;enum intel_gt_write wrote=GT_WRITE_DONE;
#endif
 if(wrote==GT_WRITE_DONE)nonzero_exit(p);
 return wrote;
}
static bool nonzero_post_group(void *ctx,unsigned e,uint32_t out[8]) {
 struct nonzero_adapter *p=ctx;if(!nonzero_enter(p))return false;
 bool ok=completion_group_raw(p->a,e,out);nonzero_exit(p);return ok;
}
static bool nonzero_post_context(void *ctx,uint32_t out[6]) {
 struct nonzero_adapter *p=ctx;if(!nonzero_enter(p))return false;
 bool ok=completion_context_raw(p->a,out);nonzero_exit(p);return ok;
}
static bool nonzero_post_sample(void *ctx,uint32_t words[5][8],uint32_t context[6]) {
 struct nonzero_adapter *p=ctx;if(!nonzero_guard(p))return false;
 bool ok=completion_snapshot(p->a,words,context);nonzero_exit(p);return ok;
}
static bool nonzero_post(void *ctx,struct intel_completion_result *out) {
 struct nonzero_adapter *p=ctx;if(out!=&p->r.post || !nonzero_enter(p))return false;
 return intel_completion_observe(out,p->g,(struct intel_completion_io){p,nonzero_guard,nonzero_post_group,nonzero_post_context,nonzero_now,nonzero_post_sample});
}
static bool nonzero_readers(void *ctx,struct intel_ring_readers *out,unsigned *count) {
 static const unsigned offsets[9]={0x180,0x19c,0x1ac,0x80,0x84,0xac,0x280,0x29c,0x2ac};
 struct nonzero_adapter *p=ctx;
 if(out!=&p->r.readers || count!=&p->r.reader_count || *count || !nonzero_enter(p))return false;
 for(unsigned plane=0;plane<3;plane++) {
  if(!nonzero_guard(p))return false;
  for(unsigned i=0;i<9;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_nonzero_read(0x70000+plane*4096+offsets[i],&out->planes[plane][i]))return false;
#else
   out->planes[plane][i]=*(volatile const uint32_t *)(WAKE_VA+(10+plane)*4096+offsets[i]);
#endif
   ++*count;
  }
 }
 unsigned found=0;struct package_pci_snapshot a={0};uint64_t total=pci_device_count();if(total>PCI_MAX_DEVICES)return false;
 for(uint64_t i=0;i<total;i++) {const struct pci_device *d=pci_device_at(i);
  if(d && !d->bus && d->device==3 && !d->function && d->vendor_id==0x8086 && d->device_id==0x160c) {
   if(found++)return false;
   package_pci_read(&a,config_read,(void *)d);
  }
 }
 if(found!=1)return false;
 const uint32_t values[10]={a.id,a.class_code,a.command,a.flags,a.pm,a.state,a.link_cap,a.link,a.ltr_cap,a.ltr};
 for(unsigned i=0;i<10;i++){out->audio[i]=values[i];++*count;}
 nonzero_exit(p);return true;
}
#ifdef SRVROS_INTEL_NONZERO_SOURCE
static bool nonzero_source_guard(void *ctx) {
 struct nonzero_adapter *p=ctx;
 return p==&nonzero_native && p->source.reset==&p->r && p->source.input==&p->source_input &&
  p->source.provider==p && intel_nonzero_source_live(&p->source) && nonzero_guard(p);
}
static bool nonzero_source_sample(void *ctx,uint32_t out[2][3],unsigned *count) {
 struct nonzero_adapter *p=ctx;struct intel_nonzero_source *s=&p->source;
 if(out!=s->words || count!=&s->count || *count || p->source_reads ||
    !nonzero_source_guard(p) || !nonzero_enter(p))return false;
#ifdef SRVROS_INTEL_SOURCE_REQUESTER
 p->source_batch_entered=true;
#endif
 for(unsigned i=0;i<6;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  intel_display_test_nonzero_source_admission(i);
#endif
  if(!nonzero_source_guard(p) || !completion_mapping(p->a,13))return false;
  uint64_t previous=s->last,at=nonzero_now(p);p->source_at[i]=at;p->source_previous[i]=previous;
  /* Final same-origin clock, then direct immutable source loads only. */
  if(!at || at<previous || at<p->r.last || at<p->g->last || at<s->origin || at-s->origin>100000 ||
     at<s->parent_begin || at-s->parent_begin>300000) {
   if(!(s->reason&INS_CLOCK)){s->clock_at=at;s->clock_prev=previous;}s->reason|=INS_CLOCK;return false;
  }
  if(s->object!=s || s->reset!=&p->r || s->parent!=p->g || s->post!=&p->r.post ||
     s->entry!=&ring_native.result || s->input!=&p->source_input || s->provider!=p ||
     p->r.dependent!=s || p->r.registered!=s || p->r.stage!=INZ_PRESERVE_STAGE ||
     !intel_nonzero_source_live(s) || p->r.returned || p->r.complete || p->r.parent!=p->g || p->r.object!=&p->r ||
#ifdef SRVROS_INTEL_STARTUP_SOURCE
     p->r.epoch_after!=s->epoch || p->g->engine_epoch!=s->epoch ||
#else
     p->r.epoch_after!=2 || p->g->engine_epoch!=2 ||
#endif
     s->origin!=p->r.begin ||
     s->parent_begin!=p->g->begin || p->source_reads!=i || *count!=i) {s->reason|=INS_PARENT;return false;}
  s->last=p->r.last=p->g->last=at;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  bool ok=intel_display_test_nonzero_source_read(INTEL_WINDOW_PAGE+(i%3==1?0:4),&out[i/3][i%3]);
#else
  out[i/3][i%3]=*(volatile const uint32_t *)(WAKE_VA+13*4096+(i%3==1?0:4));bool ok=true;
#endif
  if(!ok)return false;
  ++p->source_reads;++*count;
 }
#ifndef SRVROS_INTEL_SOURCE_REQUESTER
 nonzero_exit(p);
#endif
 return !p->failed;
}
#ifdef SRVROS_INTEL_STARTUP_BUFFERS
#include "intel_source_buffers_trial.inc"
#endif
static bool nonzero_source_observe(void *ctx,struct intel_nonzero_reset *r) {
 struct nonzero_adapter *p=ctx;
 if(p!=&nonzero_native || r!=&p->r || !intel_window_handoff_snapshot(&p->source_input))return false;
#ifdef SRVROS_INTEL_STARTUP_BUFFERS
 return intel_startup_buffers_source_run(&p->source,r,&p->source_input,
  (struct intel_nonzero_source_io){p,nonzero_source_guard,nonzero_source_sample,nonzero_now},buffers_observe,p);
#else
 return intel_nonzero_source_run(&p->source,r,&p->source_input,
  (struct intel_nonzero_source_io){p,nonzero_source_guard,nonzero_source_sample,nonzero_now});
#endif
}
#ifdef SRVROS_INTEL_STARTUP_SOURCE
static void startup_source_report(void) {
 const struct nonzero_adapter *p=&nonzero_native;
 console_printf("intel-startup-route: v=1 startup=%u selected=%u original=%u entry_epoch=%u reset_before=%u reset_after=%u source_epoch=%u complete=%u\n",
  (uint64_t)p->r.startup,(uint64_t)p->r.entry_kind,(uint64_t)ring_native.result.kind,
  (uint64_t)ring_native.result.epoch_after,(uint64_t)p->r.epoch_before,
  (uint64_t)p->r.epoch_after,(uint64_t)p->source.epoch,(uint64_t)(p->r.complete && p->source.complete));
}
#endif
static void nonzero_source_report(void) {
 const struct nonzero_adapter *p=&nonzero_native;const struct intel_nonzero_source *s=&p->source;
 for(unsigned i=0;i<6;i++)console_printf("intel-nzsource-read: v=1 n=%u value=%u at=%u previous=%u\n",(uint64_t)i,(uint64_t)s->words[i/3][i%3],p->source_at[i],p->source_previous[i]);
 console_printf("intel-nzsource: v=1 origin=%u parent_begin=%u post_begin=%u post_end=%u post_last=%u begin=%u end=%u last=%u read_begin=%u read_end=%u return_begin=%u clock_at=%u clock_prev=%u expected=%u epoch=%u post_mode=%u count=%u native_reads=%u borrows=%u returns=%u entered=%u observed=%u registered=%u blocked=%u stable=%u returned=%u reason=%u complete=%u obligations=255 assurances=0 managed_pages=0 pte_writes=0 hardware_ownership=0\n",
 s->origin,s->parent_begin,s->post_begin,s->post_end,s->post_last,s->begin,s->end,s->last,s->read_begin,s->read_end,s->return_begin,s->clock_at,s->clock_prev,s->expected,(uint64_t)s->epoch,(uint64_t)s->post_mode,(uint64_t)s->count,(uint64_t)p->source_reads,(uint64_t)p->r.child_borrows,(uint64_t)p->r.child_returns,(uint64_t)p->r.observer_entered,(uint64_t)p->r.observer_complete,(uint64_t)s->registered,(uint64_t)s->blocked,(uint64_t)s->stable,(uint64_t)s->returned,(uint64_t)s->reason,(uint64_t)s->complete);
}
#endif
static void nonzero_run(struct wake_adapter *a,struct intel_gt_access *g,bool policy) {
 struct nonzero_adapter *p=&nonzero_native;if(p->r.object)return;
 p->a=a;p->g=g;
 const struct intel_nonzero_io io={p,nonzero_guard,nonzero_sample,nonzero_control,nonzero_request,nonzero_bcs,nonzero_domain,nonzero_reset,nonzero_post,nonzero_readers,nonzero_now,nonzero_pause};
#ifdef SRVROS_INTEL_STARTUP_SOURCE
 (void)intel_startup_reset_observe(&p->r,g,&ring_native.result,policy,io,nonzero_source_observe,p);
#elif defined(SRVROS_INTEL_NONZERO_SOURCE)
 (void)intel_nonzero_reset_observe(&p->r,g,&ring_native.result,policy,io,nonzero_source_observe,p);
#else
 (void)intel_nonzero_reset_run(&p->r,g,&ring_native.result,policy,io);
#endif
 if(p->r.uncertain || p->r.borrow.active)a->uncertain=true;
}
static void nonzero_words(const char *kind,unsigned n,const uint32_t *words,unsigned count) {
 uint8_t bytes[184]={0};char encoded[249];if(count>46)return;
 for(unsigned i=0;i<count;i++)for(unsigned b=0;b<4;b++)bytes[i*4+b]=(uint8_t)(words[i]>>(8*b));
 entry_requester_base64(bytes,count*4,encoded);
 console_printf("intel-nonzero-%s: v=1 n=%u count=%u b=%s\n",kind,(uint64_t)n,(uint64_t)count,count?encoded:"-");
}
static void nonzero_report(void) {
#ifdef SRVROS_INTEL_NONZERO_SOURCE
 nonzero_source_report();
#endif
 const struct nonzero_adapter *p=&nonzero_native;const struct intel_nonzero_reset *r=&p->r;
 for(unsigned n=0;n<2;n++)nonzero_words("admit",n,r->admission[n],r->admission_count[n]);
 for(unsigned n=0;n<5;n++)nonzero_words("control",n,r->control[n],r->control_count[n]);
 for(unsigned n=0;n<2;n++)nonzero_words("bcs",n,r->bcs[n],r->bcs_count[n]>8?8:r->bcs_count[n]);
 nonzero_words("domain",0,r->domain,4);
 for(unsigned n=0;n<10;n++)console_printf("intel-nonzero-ack: v=1 n=%u ack=%u final=%u polls=%u begin=%u end=%u store=%u previous=%u\n",(uint64_t)n,(uint64_t)r->ack[n],(uint64_t)r->final[n],(uint64_t)r->polls[n],r->control_begin[n],r->control_end[n],p->store_at[n],p->store_previous[n]);
 for(unsigned n=0;n<2;n++)console_printf("intel-nonzero-pulse: v=1 n=%u ack=%u final=%u polls=%u begin=%u end=%u store=%u previous=%u\n",(uint64_t)n,(uint64_t)r->reset_ack[n],(uint64_t)r->reset_final[n],(uint64_t)r->reset_polls[n],r->pulse_begin[n],r->pulse_end[n],p->store_at[n+10],p->store_previous[n+10]);
 for(unsigned n=0;n<2;n++) {
  uint32_t words[46];unsigned groups=r->post.groups>n*5?r->post.groups-n*5:0;if(groups>5)groups=5;
  unsigned count=groups*8+(r->post.contexts>n?6:0);
  for(unsigned i=0;i<count;i++)words[i]=i<40?r->post.words[n][i/8][i%8]:r->post.context[n][i-40];
  nonzero_words("post",n,words,count);
 }
 uint32_t readers[37];for(unsigned i=0;i<37;i++)readers[i]=i<27?r->readers.planes[i/9][i%9]:r->readers.audio[i-27];
 nonzero_words("readers",0,readers,r->reader_count);
 completion_report_summary(&r->post,"intel-nonzero-post");
 console_printf("intel-nonzero-time: v=1 source_begin=%u source_end=%u source_last=%u entry_end=%u parent_begin=%u admit_begin=%u admit_end=%u begin=%u end=%u last=%u settle_begin=%u settle_end=%u guard_begin=%u guard_end=%u return_begin=%u clock_at=%u clock_prev=%u\n",r->source_begin,r->source_end,r->source_last,r->entry_end,r->parent_begin,r->admit_begin,r->admit_end,r->begin,r->end,r->last,r->settle_begin,r->settle_end,r->guard_begin,r->guard_end,r->return_begin,r->clock_at,r->clock_prev);
 console_printf("intel-nonzero-summary: v=1 policy=%u eligible=%u blocked=%u epoch_before=%u epoch_after=%u epoch_advanced=%u admission_attempts=%u control_samples=%u bcs_samples=%u bcs0=%u bcs1=%u readers=%u controls=%u control_writes=%u control_acks=%u resets=%u reset_writes=%u reset_acks=%u control_reads=%u bcs_reads=%u checks=%u stage=%u index=%u post_mode=%u reason=%u preserved=%u returned=%u retained=%u uncertain=%u complete=%u native_controls=%u native_resets=%u obligations=255 managed_pages=0 pte_writes=0 hardware_ownership=0\n",(uint64_t)r->policy,(uint64_t)r->eligible,(uint64_t)r->blocked,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)r->epoch_advanced,(uint64_t)r->admission_attempts,(uint64_t)r->control_samples,(uint64_t)r->bcs_samples,(uint64_t)r->bcs_count[0],(uint64_t)r->bcs_count[1],(uint64_t)r->reader_count,(uint64_t)r->controls,(uint64_t)r->control_writes,(uint64_t)r->control_acks,(uint64_t)r->resets,(uint64_t)r->reset_writes,(uint64_t)r->reset_acks,(uint64_t)r->control_reads,(uint64_t)r->bcs_reads,(uint64_t)r->checks,(uint64_t)r->stage,(uint64_t)r->index,(uint64_t)r->post_mode,(uint64_t)r->reason,(uint64_t)r->preserved,(uint64_t)r->returned,(uint64_t)r->borrow.active,(uint64_t)r->uncertain,(uint64_t)r->complete,(uint64_t)p->issued_controls,(uint64_t)p->issued_resets);
}
