/* Included only by the separately compiled idle-BCS diagnostic. */
#include <srvros/intel_blitter.h>
static const uint32_t reader_offsets[9]={0x180,0x19c,0x1ac,0x080,0x084,0x0ac,0x280,0x29c,0x2ac};
struct reader_audit {
 uint32_t words[2][3][9];struct package_pci_snapshot audio[2];
 uint64_t begin,end;unsigned samples,reads,primary,cursor,sprite;
 bool stable,audio_stable,complete;
};
static bool blitter_group(void *context,uint32_t out[INTEL_BCS_WORDS]) {
 struct wake_adapter *a=context;
 if(!a->mapped[7] || !engine_guard(a))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_blitter_read(out);
#else
 for(unsigned i=0;i<6;i++)out[i]=*(volatile const uint32_t *)(WAKE_VA+7*4096+intel_engine_offsets[i]);
 out[6]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x9c);
 out[7]=*(volatile const uint32_t *)(WAKE_VA+7*4096+0x29c);return true;
#endif
}
static enum intel_gt_write blitter_store(void *context,uint32_t value) {
 struct wake_adapter *a=context;
 if(value!=INTEL_BCS_DISABLED || !a->mapped[7] || !engine_guard(a))return GT_WRITE_NONE;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_blitter_write(value);
#else
 __asm__ volatile("mfence":::"memory");
 *(volatile uint32_t *)(WAKE_VA+7*4096+0x3c)=value;
 __asm__ volatile("mfence":::"memory");return GT_WRITE_DONE;
#endif
}
static bool reader_pass(struct wake_adapter *a,struct intel_gt_access *g,struct reader_audit *r,unsigned n) {
 if(n>1 || n!=r->samples || !intel_gt_access_check(g))return false;
 if(!n)r->begin=timer_monotonic_us();
 for(unsigned p=0;p<3;p++) {
  if(!a->mapped[8+p] || !engine_guard(a))return false;
  for(unsigned i=0;i<9;i++) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
   if(!intel_display_test_reader_read(p,i,&r->words[n][p][i]))return false;
#else
   r->words[n][p][i]=*(volatile const uint32_t *)(WAKE_VA+(8+p)*4096+reader_offsets[i]);
#endif
   r->reads++;if(r->words[n][p][i]==UINT32_MAX)return false;
  }
 }
 /* Config-only coverage: do not map, wake, reset or acknowledge display HDA. */
 unsigned found=0;
 uint64_t count=pci_device_count();if(count>PCI_MAX_DEVICES)return false;
 for(uint64_t i=0;i<count;i++) {
  const struct pci_device *d=pci_device_at(i);
  if(d && !d->bus && d->device==3 && !d->function && d->vendor_id==0x8086 && d->device_id==0x160c) {
   if(found++)return false;
   package_pci_read(&r->audio[n],config_read,(void *)d);
  }
 }
 r->samples++;r->end=timer_monotonic_us();
 return intel_gt_access_check(g) && r->begin && r->end>r->begin && r->end-r->begin<=150000;
}
static void reader_finish(struct reader_audit *r) {
 if(r->samples!=2 || r->reads!=54)return;
 r->stable=true;r->audio_stable=true;
 for(unsigned p=0;p<3;p++)for(unsigned i=0;i<9;i++)if(r->words[0][p][i]!=r->words[1][p][i])r->stable=false;
 for(unsigned n=0;n<2;n++)for(unsigned p=0;p<3;p++) {
  if(r->words[n][p][0]&(1u<<31))r->primary|=1u<<p;
  if(r->words[n][p][3]&63u)r->cursor|=1u<<p;
  if(r->words[n][p][6]&(1u<<31))r->sprite|=1u<<p;
 }
 const struct package_pci_snapshot *a=&r->audio[0],*b=&r->audio[1];
 if(a->id!=b->id || a->class_code!=b->class_code || a->command!=b->command || a->flags!=b->flags || a->pm!=b->pm || a->state!=b->state)r->audio_stable=false;
 r->complete=r->stable && r->audio_stable && r->begin && r->end>r->begin && r->end-r->begin<=150000;
}
static void blitter_report(const struct intel_blitter_result *b,const struct reader_audit *r) {
 for(unsigned n=0;n<b->samples;n++) {
  const uint32_t *w=b->words[n];
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
  console_printf("intel-blitter-row: v=2 sample=%u tail=%u head=%u start=%u ctl=%u execlo=%u exechi=%u mode=%u bltmode=%u\n",
   (uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#else
  console_printf("intel-blitter-row: v=1 sample=%u tail=%x head=%x start=%x ctl=%x execlo=%x exechi=%x mode=%x bltmode=%x\n",
   (uint64_t)n,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7]);
#endif
 }
 console_printf("intel-blitter-summary: v=2 contract=1 admitted=%u samples=%u reads=%u attempts=%u writes=%u reason=%u begin_us=%u end_us=%u disabled=%u uncertain=%u entry_disabled=%u engine_reset=0 ring_stop=0 restore=0 reader_retirement=0 managed_pages=0\n",
  (uint64_t)b->admitted,(uint64_t)b->samples,(uint64_t)b->reads,(uint64_t)b->attempts,(uint64_t)b->writes,(uint64_t)b->reason,b->begin_us,b->end_us,(uint64_t)b->disabled,(uint64_t)b->uncertain,(uint64_t)b->entry_disabled);
 for(unsigned n=0;n<r->samples;n++) {
  for(unsigned p=0;p<3;p++) {
   const uint32_t *w=r->words[n][p];
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
   console_printf("intel-reader-row: v=2 sample=%u pipe=%u primary_ctl=%u primary_surf=%u primary_live=%u cursor_ctl=%u cursor_base=%u cursor_live=%u sprite_ctl=%u sprite_surf=%u sprite_live=%u\n",
    (uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
#else
   console_printf("intel-reader-row: v=1 sample=%u pipe=%u primary_ctl=%x primary_surf=%x primary_live=%x cursor_ctl=%x cursor_base=%x cursor_live=%x sprite_ctl=%x sprite_surf=%x sprite_live=%x\n",
    (uint64_t)n,(uint64_t)p,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4],(uint64_t)w[5],(uint64_t)w[6],(uint64_t)w[7],(uint64_t)w[8]);
#endif
  }
  const struct package_pci_snapshot *s=&r->audio[n];
#ifdef SRVROS_INTEL_SHUTDOWN_TRIAL
  console_printf("intel-reader-audio: v=2 sample=%u id=%u class=%u command=%u flags=%u pm=%u state=%u mmio_reads=0 power_writes=0 dma_retired=0\n",
   (uint64_t)n,(uint64_t)s->id,(uint64_t)s->class_code,(uint64_t)s->command,(uint64_t)s->flags,(uint64_t)s->pm,(uint64_t)s->state);
#else
  console_printf("intel-reader-audio: v=1 sample=%u id=%x class=%x command=%x flags=%x pm=%x state=%u mmio_reads=0 power_writes=0 dma_retired=0\n",
   (uint64_t)n,(uint64_t)s->id,(uint64_t)s->class_code,(uint64_t)s->command,(uint64_t)s->flags,(uint64_t)s->pm,(uint64_t)s->state);
#endif
 }
 console_printf("intel-reader-summary: v=1 samples=%u reads=%u stable=%u audio_stable=%u primary=%u cursor=%u sprite=%u begin_us=%u end_us=%u complete=%u display_writes=0 audio_mmio=0 reader_retirement=0 managed_pages=0\n",
  (uint64_t)r->samples,(uint64_t)r->reads,(uint64_t)r->stable,(uint64_t)r->audio_stable,(uint64_t)r->primary,(uint64_t)r->cursor,(uint64_t)r->sprite,r->begin,r->end,(uint64_t)r->complete);
}
