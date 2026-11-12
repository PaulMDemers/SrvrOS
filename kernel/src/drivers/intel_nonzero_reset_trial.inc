#include <srvros/intel_nonzero_reset.h>
struct nonzero_adapter {
 struct intel_nonzero_reset r;
 struct wake_adapter *a;
 struct intel_gt_access *g;
 unsigned issued_controls,issued_resets;
 uint64_t store_at[12],store_previous[12];
 bool failed;
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
static void nonzero_run(struct wake_adapter *a,struct intel_gt_access *g,bool policy) {
 struct nonzero_adapter *p=&nonzero_native;if(p->r.object)return;
 p->a=a;p->g=g;
 (void)intel_nonzero_reset_run(&p->r,g,&ring_native.result,policy,(struct intel_nonzero_io){p,nonzero_guard,nonzero_sample,nonzero_control,nonzero_request,nonzero_bcs,nonzero_domain,nonzero_reset,nonzero_post,nonzero_readers,nonzero_now,nonzero_pause});
 if(p->r.uncertain || p->r.borrow.active)a->uncertain=true;
}
static void nonzero_words(const char *kind,unsigned n,const uint32_t *words,unsigned count) {
 uint8_t bytes[184]={0};char encoded[249];if(count>46)return;
 for(unsigned i=0;i<count;i++)for(unsigned b=0;b<4;b++)bytes[i*4+b]=(uint8_t)(words[i]>>(8*b));
 entry_requester_base64(bytes,count*4,encoded);
 console_printf("intel-nonzero-%s: v=1 n=%u count=%u b=%s\n",kind,(uint64_t)n,(uint64_t)count,count?encoded:"-");
}
static void nonzero_report(void) {
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
 console_printf("intel-nonzero-time: v=1 source_begin=%u source_end=%u source_last=%u entry_end=%u admit_begin=%u admit_end=%u begin=%u end=%u last=%u settle_begin=%u settle_end=%u guard_begin=%u guard_end=%u return_begin=%u clock_at=%u clock_prev=%u\n",r->source_begin,r->source_end,r->source_last,r->entry_end,r->admit_begin,r->admit_end,r->begin,r->end,r->last,r->settle_begin,r->settle_end,r->guard_begin,r->guard_end,r->return_begin,r->clock_at,r->clock_prev);
 console_printf("intel-nonzero-summary: v=1 policy=%u eligible=%u blocked=%u epoch_before=%u epoch_after=%u epoch_advanced=%u admission_attempts=%u control_samples=%u bcs_samples=%u bcs0=%u bcs1=%u readers=%u controls=%u control_writes=%u control_acks=%u resets=%u reset_writes=%u reset_acks=%u control_reads=%u bcs_reads=%u checks=%u stage=%u index=%u post_mode=%u reason=%u preserved=%u returned=%u retained=%u uncertain=%u complete=%u native_controls=%u native_resets=%u obligations=255 managed_pages=0 pte_writes=0 hardware_ownership=0\n",(uint64_t)r->policy,(uint64_t)r->eligible,(uint64_t)r->blocked,(uint64_t)r->epoch_before,(uint64_t)r->epoch_after,(uint64_t)r->epoch_advanced,(uint64_t)r->admission_attempts,(uint64_t)r->control_samples,(uint64_t)r->bcs_samples,(uint64_t)r->bcs_count[0],(uint64_t)r->bcs_count[1],(uint64_t)r->reader_count,(uint64_t)r->controls,(uint64_t)r->control_writes,(uint64_t)r->control_acks,(uint64_t)r->resets,(uint64_t)r->reset_writes,(uint64_t)r->reset_acks,(uint64_t)r->control_reads,(uint64_t)r->bcs_reads,(uint64_t)r->checks,(uint64_t)r->stage,(uint64_t)r->index,(uint64_t)r->post_mode,(uint64_t)r->reason,(uint64_t)r->preserved,(uint64_t)r->returned,(uint64_t)r->borrow.active,(uint64_t)r->uncertain,(uint64_t)r->complete,(uint64_t)p->issued_controls,(uint64_t)p->issued_resets);
}
