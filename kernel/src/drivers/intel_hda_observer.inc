#include <srvros/intel_display_hda.h>
#if defined(SRVROS_INTEL_HDA_OBSERVER) || defined(SRVROS_INTEL_HDA_OWNER)
#if defined(SRVROS_INTEL_WAKE_TRIAL) || defined(SRVROS_AP_PROBE) || defined(SRVROS_ACPI_BATTERY)
#error "HDA census is an independent read-only CPU0 image without GT writers or AML"
#endif
#include <srvros/intel_gt_boot.h>
#if defined(SRVROS_INTEL_HDA_OBSERVER) && defined(SRVROS_INTEL_HDA_OWNER)
#error "HDA observer and requester owner are distinct images"
#endif
#ifdef SRVROS_INTEL_HDA_LEASE
extern volatile struct limine_firmware_type_request intel_hda_lease_firmware_request;
#elif defined(SRVROS_INTEL_HDA_OWNER)
extern volatile struct limine_firmware_type_request intel_hda_owner_firmware_request;
#else
extern volatile struct limine_firmware_type_request intel_hda_firmware_request;
#endif
extern volatile struct limine_bootloader_info_request bootloader_info_request;
extern volatile struct limine_executable_cmdline_request executable_cmdline_request;
#define HDA_VA (BACKING_VA+0x40000)
struct hda_adapter {struct gt_observer gt;const struct pci_device *device;struct intel_hda_endpoint initial;bool mapped,uncertain;unsigned guards,cache_checks;};
static bool hda_text(const char *a,const char *b) {
 if(!a)return false;
 for(unsigned i=0;i<32;i++){if(a[i]!=b[i])return false;if(!b[i])return true;}return false;
}
static uint64_t hda_irq_save(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_hda_irq_save();
#else
 uint64_t f;__asm__ volatile("pushfq; popq %0; cli":"=r"(f)::"memory");return f;
#endif
}
static bool hda_irq_off(void) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_hda_irq_off();
#else
 uint64_t f;__asm__ volatile("pushfq; popq %0":"=r"(f));return !(f&(1ull<<9));
#endif
}
static void hda_irq_restore(uint64_t f) {
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 intel_display_test_hda_irq_restore(f);
#else
 if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static bool hda_endpoint(void *ctx,struct intel_hda_endpoint *e) {
 struct hda_adapter *h=ctx;struct package_pci_snapshot p={0};
 if(!h->device || h->device->bus || h->device->device!=3 || h->device->function)return false;
 package_pci_read(&p,config_read,(void *)h->device);
 *e=(struct intel_hda_endpoint){.id=p.id,.class_rev=pci_read_config32(h->device,8),.subsystem=pci_read_config32(h->device,0x2c),
  .command=p.command,.flags=p.flags,.pm=p.pm,.state=p.state,.bar=pci_read_config32(h->device,0x10),
  .bar_high=pci_read_config32(h->device,0x14),.reserved=backing_range_reserved(INTEL_HDA_BASE,0x4000)};
 return true;
}
static bool hda_basic(struct hda_adapter *h) {
 struct intel_hda_endpoint e={0};uint64_t physical;
 h->guards++;
 if(h->uncertain || !hda_irq_off() || !display_access_held(&h->gt.token) ||
    vmm_current_address_space()!=h->gt.a.common.cr3 || !intel_boot_check(INTEL_BOOT_OBSERVER,h->gt.a.common.gpu) ||
    !hda_endpoint(h,&e) || !intel_hda_endpoint_valid(&e) || e.command!=h->initial.command || e.flags!=h->initial.flags || e.pm!=h->initial.pm)return false;
 if(h->mapped)return vmm_virt_to_phys_in_space(h->gt.a.common.cr3,HDA_VA,&physical) && physical==INTEL_HDA_BASE;
 return !vmm_virt_to_phys_in_space(h->gt.a.common.cr3,HDA_VA,&physical);
}
static bool hda_guard(void *ctx) {
 struct hda_adapter *h=ctx;
 if(!hda_basic(h))return false;
 h->cache_checks++;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_hda_uc(INTEL_HDA_BASE);
#else
 return intel_cache_probe_hda_uc_page(INTEL_HDA_BASE);
#endif
}
static bool hda_map(void *ctx) {
 struct hda_adapter *h=ctx;uint64_t old;
 if(h->mapped || !hda_basic(h))return false;
 if(!vmm_map_page_in_space(h->gt.a.common.cr3,HDA_VA,INTEL_HDA_BASE,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
  if(vmm_virt_to_phys_in_space(h->gt.a.common.cr3,HDA_VA,&old))h->uncertain=true;
  return false;
 }
 h->mapped=true;return true;
}
static bool hda_read(void *ctx,unsigned off,unsigned width,uint32_t *out) {
 struct hda_adapter *h=ctx;
 if(!out || !h->mapped || !intel_hda_read_allowed(off,width) || !hda_basic(h))return false;
#ifdef SRVROS_INTEL_DISPLAY_HOST_TEST
 return intel_display_test_hda_read(off,width,out);
#else
 if(width==1)*out=*(volatile const uint8_t *)(HDA_VA+off);
 else if(width==2)*out=*(volatile const uint16_t *)(HDA_VA+off);
 else if(width==4)*out=*(volatile const uint32_t *)(HDA_VA+off);
 else return false;
 return true;
#endif
}
static bool hda_unmap(void *ctx) {
 struct hda_adapter *h=ctx;uint64_t physical;
 if(!h->mapped || vmm_current_address_space()!=h->gt.a.common.cr3 ||
    !vmm_virt_to_phys_in_space(h->gt.a.common.cr3,HDA_VA,&physical) || physical!=INTEL_HDA_BASE ||
    !vmm_unmap_page_in_space(h->gt.a.common.cr3,HDA_VA,&physical) || physical!=INTEL_HDA_BASE){h->uncertain=true;return false;}
 h->mapped=false;return true;
}
#endif
bool intel_display_hda_probe_report(void) {
#ifdef SRVROS_INTEL_HDA_OBSERVER
 static bool once;if(once)return false;once=true;
 const struct limine_firmware_type_response *fw=intel_hda_firmware_request.response;
 const struct limine_bootloader_info_response *loader=bootloader_info_request.response;
 const struct limine_executable_cmdline_response *cmd=executable_cmdline_request.response;
 bool efi=fw && !fw->revision && fw->firmware_type==LIMINE_FIRMWARE_TYPE_EFI64;
 bool pinned=loader && !loader->revision && hda_text(loader->name,"Limine") && hda_text(loader->version,"11.4.1");
 bool recipe=cmd && !cmd->revision && intel_gt_boot_hda_recipe(cmd->cmdline);
 bool prerequisites=gt_saved_complete && fw_saved_complete;
 struct hda_adapter h={0};struct intel_hda_result r={0};struct scanout_handoff_snapshot s={0};
 bool acquired=false,before=false,after=false,released=false;uint64_t begin=0,end=0;
 console_write("intel-hda: read-only display-HDA DMA census; no commands, wake/reset, stream stop, bus-master change or memory grant\n");
 bool prepared=efi && pinned && recipe && prerequisites && console_sync() && intel_window_handoff_snapshot(&s) && trial_adapter(&h.gt.a);
 uint64_t irq=hda_irq_save();bool serialized=hda_irq_off();
 if(prepared && serialized) {
  begin=timer_monotonic_us();
  uint64_t count=pci_device_count();if(count>PCI_MAX_DEVICES)prepared=false;
  for(uint64_t i=0;prepared && i<count;i++) {
   const struct pci_device *d=pci_device_at(i);
   if(d && !d->bus && d->device==3 && !d->function){if(h.device){prepared=false;break;}h.device=d;}
  }
  acquired=prepared && display_access_acquire(0x4844414f4253ull,&h.gt.token);
  if(acquired && display_present_handoff_idle())before=intel_window_handoff_revalidate(&s);
  if(before && hda_endpoint(&h,&h.initial))
   (void)intel_hda_observe(&r,true,(struct intel_hda_io){&h,hda_endpoint,hda_guard,hda_map,hda_read,hda_unmap,now});
  if(!h.uncertain && !h.mapped && !trial_mapping_fault && before)after=intel_window_handoff_revalidate(&s);
  end=timer_monotonic_us();
  if(acquired && !h.uncertain && !h.mapped && !trial_mapping_fault)released=display_access_release(&h.gt.token,after);
 }
 bool retained=h.uncertain || h.mapped || trial_mapping_fault || (acquired && !released);
 bool complete=r.complete && before && after && released && !retained && h.cache_checks==5 && begin && end>begin && end-begin<=500000;
 hda_irq_restore(irq);if(released)console_flush();
 const struct intel_hda_endpoint *e=&r.endpoint;
 console_printf("intel-hda-endpoint: v=1 id=%x class_rev=%x subsystem=%x command=%x flags=%x pm=%x state=%u bar=%x bar_high=%x reserved=%u\n",
  (uint64_t)e->id,(uint64_t)e->class_rev,(uint64_t)e->subsystem,(uint64_t)e->command,(uint64_t)e->flags,(uint64_t)e->pm,(uint64_t)e->state,(uint64_t)e->bar,(uint64_t)e->bar_high,(uint64_t)e->reserved);
 for(unsigned n=0;n<r.samples;n++) {
  const uint32_t *g=r.sample[n].global;
  console_printf("intel-hda-global: v=1 sample=%u gcap=%x gctl=%x intctl=%x corblo=%x corbhi=%x corbctl=%x rirblo=%x rirbhi=%x rirbctl=%x dpl=%x dpu=%x\n",
   (uint64_t)n,(uint64_t)g[0],(uint64_t)g[1],(uint64_t)g[2],(uint64_t)g[3],(uint64_t)g[4],(uint64_t)g[5],(uint64_t)g[6],(uint64_t)g[7],(uint64_t)g[8],(uint64_t)g[9],(uint64_t)g[10]);
  for(unsigned i=0;i<r.sample[n].streams;i++) {
   const uint32_t *w=r.sample[n].stream[i];
   console_printf("intel-hda-stream: v=1 sample=%u index=%u control=%x cbl=%x lvi=%x bdllo=%x bdlhi=%x\n",
    (uint64_t)n,(uint64_t)i,(uint64_t)w[0],(uint64_t)w[1],(uint64_t)w[2],(uint64_t)w[3],(uint64_t)w[4]);
  }
 }
 console_printf("intel-hda-summary: v=1 compiled=1 efi64=%u loader=%u recipe=%u prerequisites=%u serialized=%u acquired=%u before=%u after=%u released=%u retained=%u maps=%u retired=%u cache_checks=%u guards=%u samples=%u reads=%u streams=%u gcap=%x stable=%u run=%u dma=%u reason=%u core_begin_us=%u core_end_us=%u begin_us=%u end_us=%u complete=%u writes=0 wake=0 reset=0 ack=0 dma_retirement=0 managed_pages=0\n",
  (uint64_t)efi,(uint64_t)pinned,(uint64_t)recipe,(uint64_t)prerequisites,(uint64_t)serialized,(uint64_t)acquired,(uint64_t)before,(uint64_t)after,(uint64_t)released,(uint64_t)retained,(uint64_t)r.maps,(uint64_t)r.retired,(uint64_t)h.cache_checks,(uint64_t)h.guards,(uint64_t)r.samples,(uint64_t)r.reads,(uint64_t)r.sample[0].streams,(uint64_t)r.sample[0].global[0],(uint64_t)r.stable,(uint64_t)r.run_mask,(uint64_t)r.dma_mask,(uint64_t)r.reason,r.begin_us,r.end_us,begin,end,(uint64_t)complete);
 return complete;
#else
 console_write("intel-hda: REFUSED separate read-only display-HDA image required\n");return false;
#endif
}
