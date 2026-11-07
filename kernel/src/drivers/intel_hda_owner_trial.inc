#include <srvros/intel_hda_owner.h>
#ifdef SRVROS_INTEL_HDA_OWNER
#ifdef SRVROS_INTEL_HDA_LEASE
#define HDA_LABEL "intel-hda-lease"
#define HDA_WORD "%u"
#else
#define HDA_LABEL "intel-hda-owner"
#define HDA_WORD "%x"
#endif
static bool hda_owner_retained;
struct hda_owner_adapter {struct hda_adapter h;struct scanout_handoff_snapshot snapshot;};
static bool hda_owner_guard(void *ctx) {
 struct hda_owner_adapter *a=ctx;
 return !a->h.mapped && !a->h.uncertain && !trial_mapping_fault && hda_guard(&a->h) && intel_window_handoff_revalidate(&a->snapshot);
}
static bool hda_owner_endpoint(void *ctx,struct intel_hda_endpoint *e) {return hda_endpoint(&((struct hda_owner_adapter *)ctx)->h,e);}
static bool hda_owner_inspect(void *ctx,struct intel_hda_result *r) {
 struct hda_owner_adapter *a=ctx;struct hda_adapter *h=&a->h;
 return hda_basic(h) && intel_hda_observe(r,true,(struct intel_hda_io){h,hda_endpoint,hda_guard,hda_map,hda_read,hda_unmap,now});
}
static bool hda_owner_command16(void *ctx,uint16_t command) {
 struct hda_adapter *h=&((struct hda_owner_adapter *)ctx)->h;
 if(!hda_basic(h) || h->mapped || !((h->initial.command==6 && command==2) || (h->initial.command==2 && command==6)))return false;
 /* A true halfword access: no adjacent W1C PCI Status acknowledgement. */
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 if(!intel_display_test_hda_command16(h->device,4,command))return false;
#else
 pci_write_config16(h->device,4,command);
#endif
 h->initial.command=command;return true;
}
static void hda_owner_rows(const struct intel_hda_owner_result *r,const struct intel_hda_result *mid) {
#ifdef SRVROS_INTEL_HDA_LEASE
 const unsigned count=4;
#else
 const unsigned count=3;(void)mid;
#endif
 for(unsigned phase=0;phase<count;phase++) {
#ifdef SRVROS_INTEL_HDA_LEASE
  const struct intel_hda_result *s=phase==2?mid:&r->phase[phase==3?2:phase];
#else
  const struct intel_hda_result *s=&r->phase[phase];
#endif
  if(!s->object)continue;
  const struct intel_hda_endpoint *e=&s->endpoint;
  console_printf(HDA_LABEL "-phase: v=1 phase=%u command=%x id=%x class_rev=%x subsystem=%x flags=%x pm=%x state=%u bar=%x bar_high=%x reserved=%u maps=%u retired=%u retained=%u samples=%u reads=%u stable=%u run=%u dma=%u reason=%u begin_us=%u end_us=%u complete=%u\n",
   (uint64_t)phase,(uint64_t)e->command,(uint64_t)e->id,(uint64_t)e->class_rev,(uint64_t)e->subsystem,(uint64_t)e->flags,(uint64_t)e->pm,(uint64_t)e->state,(uint64_t)e->bar,(uint64_t)e->bar_high,(uint64_t)e->reserved,(uint64_t)s->maps,(uint64_t)s->retired,(uint64_t)s->retained,(uint64_t)s->samples,(uint64_t)s->reads,(uint64_t)s->stable,(uint64_t)s->run_mask,(uint64_t)s->dma_mask,(uint64_t)s->reason,s->begin_us,s->end_us,(uint64_t)s->complete);
  for(unsigned n=0;n<s->samples;n++) {
   const struct intel_hda_sample *p=&s->sample[n];const uint32_t *g=p->global;
   console_printf(HDA_LABEL "-global: v=1 phase=%u sample=%u streams=%u g0=" HDA_WORD " g1=" HDA_WORD " g2=" HDA_WORD " g3=" HDA_WORD " g4=" HDA_WORD " g5=" HDA_WORD " g6=" HDA_WORD " g7=" HDA_WORD " g8=" HDA_WORD " g9=" HDA_WORD " g10=" HDA_WORD "\n",
    (uint64_t)phase,(uint64_t)n,(uint64_t)p->streams,(uint64_t)g[0],(uint64_t)g[1],(uint64_t)g[2],(uint64_t)g[3],(uint64_t)g[4],(uint64_t)g[5],(uint64_t)g[6],(uint64_t)g[7],(uint64_t)g[8],(uint64_t)g[9],(uint64_t)g[10]);
   for(unsigned i=0;i<p->streams;i++) {
    const uint32_t *w=p->stream[i];
    console_printf(HDA_LABEL "-stream: v=1 phase=%u sample=%u index=%u w0=" HDA_WORD " w1=" HDA_WORD " w2=" HDA_WORD " w3=" HDA_WORD " w4=" HDA_WORD "\n",(uint64_t)phase,(uint64_t)n,(uint64_t)i,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4]);
   }
  }
 }
}
#endif
bool intel_display_hda_owner_report(void) {
#ifdef SRVROS_INTEL_HDA_OWNER
 static bool once;if(once)return false;once=true;
#ifdef SRVROS_INTEL_HDA_LEASE
 const struct limine_firmware_type_response *fw=intel_hda_lease_firmware_request.response;
#else
 const struct limine_firmware_type_response *fw=intel_hda_owner_firmware_request.response;
#endif
 const struct limine_bootloader_info_response *loader=bootloader_info_request.response;
 const struct limine_executable_cmdline_response *cmd=executable_cmdline_request.response;
 bool efi=fw && !fw->revision && fw->firmware_type==LIMINE_FIRMWARE_TYPE_EFI64;
 bool loader_ok=loader && !loader->revision && hda_text(loader->name,"Limine") && hda_text(loader->version,"11.4.1");
#ifdef SRVROS_INTEL_HDA_LEASE
 bool recipe=cmd && !cmd->revision && intel_gt_boot_hda_lease_recipe(cmd->cmdline);
#else
 bool recipe=cmd && !cmd->revision && intel_gt_boot_hda_owner_recipe(cmd->cmdline);
#endif
 bool prerequisites=gt_saved_complete && fw_saved_complete;
 struct hda_owner_adapter a={0};struct intel_hda_owner_result r={0};struct intel_hda_result mid={0};
#ifdef SRVROS_INTEL_HDA_LEASE
 bool pinned=false,blocked=false,checked=false,unpinned=false,live=false;
#endif
 bool acquired=false,before=false,after=false,released=false,profile=false;uint64_t begin=0,end=0;
 console_write(HDA_LABEL ": cooperative EFI submission policy v1; command16 BME inhibit/restore only, no outstanding-DMA or memory grant\n");
 bool prepared=efi && loader_ok && recipe && prerequisites && console_sync() && intel_window_handoff_snapshot(&a.snapshot) && trial_adapter(&a.h.gt.a);
 uint64_t irq=hda_irq_save();bool serialized=hda_irq_off();
 if(prepared && serialized) {
  begin=timer_monotonic_us();uint64_t count=pci_device_count();if(count>PCI_MAX_DEVICES)prepared=false;
  for(uint64_t i=0;prepared && i<count;i++) {
   const struct pci_device *d=pci_device_at(i);
   if(d && !d->bus && d->device==3 && !d->function){if(a.h.device){prepared=false;break;}a.h.device=d;}
  }
  acquired=prepared && display_access_acquire(0x4844414f574eull,&a.h.gt.token);
  if(acquired && display_present_handoff_idle())before=intel_window_handoff_revalidate(&a.snapshot);
  profile=before && hda_endpoint(&a.h,&a.h.initial) && intel_hda_endpoint_valid(&a.h.initial) && a.h.initial.command==6;
  if(profile) {
   struct intel_hda_owner_facts facts={true,recipe,efi,loader_ok,profile,serialized,acquired,prerequisites};
   struct intel_hda_owner_io io={&a,hda_owner_inspect,hda_owner_guard,hda_owner_endpoint,hda_owner_command16,now};
#ifdef SRVROS_INTEL_HDA_LEASE
   struct intel_hda_owner_token token={0};struct intel_hda_owner_borrow borrow={0};
   bool opened=intel_hda_owner_open(&r,facts,io,&token);
   if(opened) {
    pinned=intel_hda_owner_pin(&r,&token,&borrow);
    blocked=pinned && !intel_hda_owner_close(&r,&token) && r.pins==1 && r.blocked_closes==1 && r.attempts==1;
    checked=blocked && intel_hda_owner_check(&r,&borrow,&mid);
    live=checked && intel_hda_owner_live(&r,&token);
    unpinned=live && intel_hda_owner_unpin(&r,&borrow);
    bool closed=unpinned && intel_hda_owner_close(&r,&token);
    if(!closed)r.retained=true;
   }
#else
   (void)intel_hda_owner_trial(&r,facts,io);
#endif
  }
  bool clean_core=!r.retained && r.state!=INTEL_HDA_OWNER_LIVE && !r.pins;
  if(clean_core && !a.h.uncertain && !a.h.mapped && !trial_mapping_fault && before)after=intel_window_handoff_revalidate(&a.snapshot);
  end=timer_monotonic_us();
  if(acquired && clean_core && !a.h.uncertain && !a.h.mapped && !trial_mapping_fault)released=display_access_release(&a.h.gt.token,after);
 }
 bool retained=r.retained || r.state==INTEL_HDA_OWNER_LIVE || r.pins || a.h.uncertain || a.h.mapped || trial_mapping_fault || (acquired && !released);
 hda_owner_retained=retained;
 bool complete=r.complete && before && after && released && !retained && begin && end>begin && end-begin<=500000;
 hda_irq_restore(irq);if(released)console_flush();
 hda_owner_rows(&r,&mid);
#ifdef SRVROS_INTEL_HDA_LEASE
 console_printf(HDA_LABEL "-lifetime: v=1 epoch=%u pins=%u checks=%u blocked_closes=%u pin=%u blocked=%u checked=%u unpin=%u live=%u\n",r.epoch,(uint64_t)r.pins,(uint64_t)r.checks,(uint64_t)r.blocked_closes,(uint64_t)pinned,(uint64_t)blocked,(uint64_t)checked,(uint64_t)unpinned,(uint64_t)live);
#endif
 console_printf(HDA_LABEL "-summary: v=1 compiled=1 policy=1 efi64=%u loader=%u recipe=%u prerequisites=%u profile=%u serialized=%u acquired=%u before=%u after=%u released=%u retained=%u admitted=%u phases=%u attempts=%u writes=%u inhibited=%u restored=%u reason=%u guards=%u cache_checks=%u core_begin_us=%u core_end_us=%u begin_us=%u end_us=%u complete=%u mmio_writes=0 status_writes=0 wake=0 reset=0 ack=0 dma_retirement=0 managed_pages=0\n",
  (uint64_t)efi,(uint64_t)loader_ok,(uint64_t)recipe,(uint64_t)prerequisites,(uint64_t)profile,(uint64_t)serialized,(uint64_t)acquired,(uint64_t)before,(uint64_t)after,(uint64_t)released,(uint64_t)retained,(uint64_t)r.admitted,(uint64_t)(r.phases+r.checks),(uint64_t)r.attempts,(uint64_t)r.writes,(uint64_t)r.inhibited,(uint64_t)r.restored,(uint64_t)r.reason,(uint64_t)a.h.guards,(uint64_t)a.h.cache_checks,r.begin_us,r.end_us,begin,end,(uint64_t)complete);
 return complete;
#else
 console_write("intel-hda-owner: REFUSED separate cooperative requester image required\n");return false;
#endif
}

bool intel_display_hda_owner_retained(void) {
#ifdef SRVROS_INTEL_HDA_OWNER
 return hda_owner_retained;
#else
 return false;
#endif
}
