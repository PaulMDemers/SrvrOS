/* Exact request/control callbacks for the two separately compiled BCS trials. */
#include <srvros/intel_reset_ready.h>
static bool reset_ready_control(void *ctx,uint32_t *out) {
 struct wake_adapter *a=ctx;
 if(!out || !a->mapped[7] || !engine_guard(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_reset_ready_read(INTEL_BCS_RESET_CTL,out);
#else
 *out=*(volatile const uint32_t *)(WAKE_VA+7*4096+0xd0);return true;
#endif
}
static bool reset_ready_group(void *ctx,uint32_t out[8]) {
 struct wake_adapter *a=ctx;
 static const uint32_t regs[8]={0x22030,0x22034,0x22038,0x2203c,0x22234,0x22238,0x2209c,0x2229c};
 if(!out || !a->mapped[7] || !engine_guard(a))return false;
 for(unsigned i=0;i<8;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
  if(!intel_display_test_reset_ready_read(regs[i],&out[i]))return false;
#else
  out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+(regs[i]&4095));
#endif
 }
 return true;
}
static enum intel_gt_write reset_ready_store(void *ctx,uint32_t value) {
 struct wake_adapter *a=ctx;
 if((value!=INTEL_BCS_RESET_REQUEST && value!=INTEL_BCS_RESET_CANCEL) ||
    !a->mapped[7] || !engine_guard(a))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_reset_ready_write(INTEL_BCS_RESET_CTL,value);
#else
 __asm__ volatile("mfence":::"memory");
 *(volatile uint32_t *)(WAKE_VA+7*4096+0xd0)=value;
 __asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
#ifndef SRVROS_INTEL_BCS_RESET_TRIAL
static void reset_ready_report(const struct intel_reset_ready_result *r) {
 for(unsigned n=0;n<r->samples;n++) {
  const uint32_t *w=r->words[n];
  console_printf("intel-reset-ready-row: v=1 sample=%u w0=%u w1=%u w2=%u w3=%u w4=%u w5=%u w6=%u w7=%u\n",(uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
 }
 for(unsigned n=0;n<r->controls_read;n++)console_printf("intel-reset-ready-control: v=1 phase=%u value=%u\n",(uint64_t)n,(uint64_t)r->controls[n]);
 console_printf("intel-reset-ready-summary: v=1 admitted=%u ready=%u cancelled=%u samples=%u controls=%u reads=%u polls=%u attempts=%u writes=%u reason=%u begin_us=%u end_us=%u blocked_close=%u borrowed=%u returned=%u uncertain=%u complete=%u obligations=255 engine_reset=0 posted_drain=0 prefetch_retired=0 pipeline_flushed=0 tlb_writes=0 cache_ownership=0 reader_retirement=0 managed_pages=0\n",(uint64_t)r->admitted,(uint64_t)r->ready,(uint64_t)r->cancelled,(uint64_t)r->samples,(uint64_t)r->controls_read,(uint64_t)r->reads,(uint64_t)r->polls,(uint64_t)r->attempts,(uint64_t)r->writes,(uint64_t)r->reason,r->begin_us,r->end_us,(uint64_t)r->blocked_close,(uint64_t)(r->borrow.parent!=0),(uint64_t)(r->borrow.parent && !r->borrow.active),(uint64_t)r->uncertain,(uint64_t)r->complete);
}

#endif
