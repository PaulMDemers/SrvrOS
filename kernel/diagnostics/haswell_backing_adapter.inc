/* Isolated Air6,1 diagnostic only. Never maps pixels/stolen RAM.
 * The backing window is supervisor/RO/NX/cache-disabled. The forcewake trial
 * has a separate write alias for the request register. Neither is shared
 * through the global mapping registry. Uncertain retirement lasts until reset. */
#include <srvros/haswell_backing.h>
#include <srvros/haswell_firmware.h>
#include <srvros/intel_hsw_engine_hold.h>
#include <srvros/intel_hsw_ggtt.h>
#define HWB_VA 0xffffc001f3240000ull
static const struct pci_device *hsw_host;
static struct hwb_result hsw_result;
static uint64_t hsw_window_pa;
#define HSW_FBC_PAGE 0x43000u
#define HSW_FBC_BASE 0x43200u
#define HSW_FBC_CTL 0x43208u
#define HSW_FBC_STATUS 0x43210u
#define HSW_ACCESS_SLOTS 16u
#define HSW_WAKE_ACK_PAGE 0x130000u
#define HSW_WAKE_ACK 0x130044u
static uint32_t hsw_candidate_window;
static uint32_t hsw_survey_window, hsw_survey_window_end;
static bool hsw_access_window, hsw_wake_ack_window;
static bool hsw_forcewake_backing;
static bool hsw_window_live,hsw_window_poison,hsw_firmware,hsw_cpu_control,hsw_linux_fences;
static struct {
 enum { HSW_HOLD_OFF, HSW_HOLD_ACTIVE, HSW_HOLD_RETIRED, HSW_HOLD_RETAINED } state;
 const struct pci_device *igd, *hda;
 uint32_t first, end;
 uint64_t begin, denied[2], finish;
 unsigned refusals;
} hsw_requester_hold;
static bool hsw_safe_range(uint64_t base,uint64_t size,bool reserved) {
 const struct limine_memmap_response *m=memmap_request.response;
 if(!m || !m->entries || !m->entry_count || m->entry_count>4096 || !size || base>UINT64_MAX-size)return false;
 unsigned overlaps=0;bool covered=false;
 for(unsigned i=0;i<m->entry_count;i++) {
  const struct limine_memmap_entry *e=m->entries[i];
  if(!e || !e->length || e->base>UINT64_MAX-e->length)return false;
  uint64_t end=e->base+e->length;
  if(e->base<base+size && base<end) {
   overlaps++;
   if(e->type!=LIMINE_MEMMAP_RESERVED && e->type!=LIMINE_MEMMAP_RESERVED_MAPPED)return false;
   if(e->type==LIMINE_MEMMAP_RESERVED && e->base<=base && base+size<=end)covered=true;
  }
 }
 return !reserved || (overlaps==1 && covered);
}
static bool hsw_config_valid(void) {
 uint32_t pch;
 return gpu && hsw_host && !hsw_host->bus && !hsw_host->device && !hsw_host->function &&
  hsw_host->class_code==6 && !hsw_host->subclass &&
  pci_read_config32(hsw_host,0)==0x0a048086 &&
  pci_read_config32(gpu,0)==0x0a268086 && pci_read_config32(gpu,8)==0x03000009 &&
  pci_read_config32(gpu,0x2c)==0x011a106b && !pci_read_config32(gpu,0x14) &&
  (pci_read_config32(gpu,0x10)&0x3fffff)==4 && !pci_read_config32(gpu,0x1c) &&
  (pci_read_config32(gpu,0x18)&0xfffffff)==12 &&
  (pci_read_config32(gpu,0x18)&0xf0000000) && dependency_pch_valid(&pch) && graphics_access_valid();
}
static bool hsw_endpoint(void *ctx,struct hwb_endpoint *e) {
 (void)ctx;
#ifdef SRVROS_PLATFORM_HOST_TEST
 extern void platform_test_hsw_endpoint_start(void);platform_test_hsw_endpoint_start();
#endif
 if(!hsw_host || !hsw_firmware || !mapped || !resource_mapped || !hsw_config_valid())return false;
 *e=(struct hwb_endpoint){0};uint64_t *v=e->v;
 v[HW_GPU]=pci_read_config32(gpu,0);v[HW_REV]=pci_read_config32(gpu,8);v[HW_SUB]=pci_read_config32(gpu,0x2c);
 v[HW_MMIO_LO]=pci_read_config32(gpu,0x10);v[HW_MMIO_HI]=pci_read_config32(gpu,0x14);
 v[HW_AP_LO]=pci_read_config32(gpu,0x18);v[HW_AP_HI]=pci_read_config32(gpu,0x1c);
 struct package_pci_snapshot pm;package_pci_read(&pm,conventional_read,(void *)gpu);
 if(pm.state!=1 || !(pm.flags&1))return false;
 v[HW_CMD]=pm.command;v[HW_PM]=pm.pm;
 v[HW_HOST]=pci_read_config32(hsw_host,0);v[HW_GGC]=pci_read_config32(hsw_host,0x50);
 v[HW_BDSM]=pci_read_config32(hsw_host,0xb0);v[HW_BGSM]=pci_read_config32(hsw_host,0xb4);v[HW_TOLUD]=pci_read_config32(hsw_host,0xbc);
 v[HW_GPU_GGC]=pci_read_config32(gpu,0x50);v[HW_GPU_BDSM]=pci_read_config32(gpu,0x5c);
 uint32_t pchid;if(!dependency_pch_valid(&pchid))return false;v[HW_PCH]=pchid;
 if(!hsw_safe_range(gpu_base,0x400000,false))return false;
 v[HW_MMIO_SAFE]=1;v[HW_FIRMWARE]=1;
 for(unsigned i=0;i<4;i++){v[HW_WELL0+i]=well_read(0x400+4*i);if(v[HW_WELL0+i]==UINT32_MAX || !(v[HW_WELL0+i]&(1u<<30)))return false;}
 v[HW_DISPARB]=well_read(0);if(v[HW_DISPARB]==UINT32_MAX)return false;
 struct backing_capture f={0};if(!backing_descriptor(&f))return false;
 for(unsigned i=0;i<9;i++){v[HW_CONTROL+i]=resource_read(backing_offsets[i]);if(v[HW_CONTROL+i]==UINT32_MAX)return false;}
 if(hsw_forcewake_backing && v[HW_SURF])return false;
 for(unsigned i=0;i<7;i++)v[HW_FB_COUNT+i]=f.fb[i];
 for(unsigned i=0;i<3;i++)v[HW_MEM_BASE+i]=f.entry[i];
 v[HW_PAGES]=f.pages;v[HW_ENTRIES]=f.entries;v[HW_BPP]=f.bpp;
 for(unsigned i=0;i<7;i++)v[HW_COLOR]|=(uint64_t)f.color[i]<<(8*i);
 v[HW_CLASS]=f.classification;v[HW_CONTIG]=f.contiguous;
 uint64_t stolen=v[HW_BDSM]&0xfff00000,bytes=((v[HW_GGC]>>3)&31)<<25;
 v[HW_STOLEN_RESERVED]=hsw_safe_range(stolen,bytes,true);
 for(unsigned i=0;i<4;i++)if(v[HW_WELL0+i]!=well_read(0x400+4*i))return false;
 return hsw_config_valid();
}
static bool hsw_map(void *ctx,uint32_t off) {
 (void)ctx;uint64_t existing,kernel=vmm_kernel_address_space();
 if(!hsw_result.count || hsw_result.count>HWB_PTES)return false;
 uint64_t first=(0x200000+(hsw_result.offset/4096)*4)&~4095ull;
 uint64_t last=(0x200000+(hsw_result.offset/4096+hsw_result.count)*4-1)&~4095ull;
 if(hsw_window_live || hsw_window_poison || !kernel || vmm_current_address_space()!=kernel ||
    (hsw_requester_hold.state!=HSW_HOLD_OFF &&
     (hsw_requester_hold.state!=HSW_HOLD_ACTIVE || off!=hsw_requester_hold.first ||
      hsw_survey_window!=off || hsw_survey_window_end!=hsw_requester_hold.end)) ||
    (off&4095) || (hsw_wake_ack_window ? off!=HSW_WAKE_ACK_PAGE :
      hsw_survey_window ? off!=hsw_survey_window :
      hsw_candidate_window ? off!=(hsw_candidate_window&~4095u) &&
        !(hsw_access_window && off==HSW_FBC_PAGE) :
        off!=0x100000 && !(hsw_cpu_control && off==0x101000) && (off<first || off>last || off>=0x400000)) ||
    !hsw_config_valid() || !hsw_safe_range(gpu_base,0x400000,false) ||
    vmm_virt_to_phys_in_space(kernel,HWB_VA,&existing))return false;
 hsw_window_pa=gpu_base+off;
 if(!vmm_map_page_in_space(kernel,HWB_VA,hsw_window_pa,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
  /* A failed mapper may have published a partial entry. Never retry/reuse it. */
  hsw_window_poison=true;return false;
 }
 hsw_window_live=true;return true;
}
static bool hsw_unmap(void *ctx) {
 (void)ctx;uint64_t pa=0,existing,kernel=vmm_kernel_address_space();
 if(!hsw_window_live || hsw_window_poison || vmm_current_address_space()!=kernel ||
    !vmm_unmap_page_in_space(kernel,HWB_VA,&pa) || pa!=hsw_window_pa ||
    vmm_virt_to_phys_in_space(kernel,HWB_VA,&existing)) {hsw_window_poison=true;return false;}
 hsw_window_live=false;return true;
}
static bool hsw_candidate_read_allowed(uint32_t off) {
 if(off==hsw_candidate_window)return true;
 return hsw_access_window && ((off>HSW_CANDIDATE_MMIO &&
         off<HSW_CANDIDATE_MMIO+HSW_ACCESS_SLOTS*4) ||
         off==HSW_FBC_BASE || off==HSW_FBC_CTL || off==HSW_FBC_STATUS);
}

static bool hsw_survey_protected(uint32_t off) {
 return off==HSW_CANDIDATE_MMIO || off==HSW_BOOT_SLOT_MMIO;
}

static bool hsw_read(void *ctx,uint32_t off,uint32_t *v) {
 (void)ctx;
 if(!hsw_window_live || hsw_window_poison || vmm_current_address_space()!=vmm_kernel_address_space() ||
    gpu_base+(off&~4095u)!=hsw_window_pa || (off&3) ||
    (hsw_wake_ack_window && off!=HSW_WAKE_ACK) ||
    (hsw_forcewake_backing && hsw_survey_protected(off)) ||
    (hsw_requester_hold.state!=HSW_HOLD_OFF &&
     (hsw_requester_hold.state!=HSW_HOLD_ACTIVE || off<hsw_requester_hold.first || off>=hsw_requester_hold.end)) ||
    (hsw_survey_window && (off>=hsw_survey_window_end || hsw_survey_protected(off))) ||
    (hsw_candidate_window && !hsw_candidate_read_allowed(off)) ||
    (hsw_window_pa==gpu_base+0x101000 && (!hsw_cpu_control || off!=0x101000)) ||
    (off>=0x100080 && off<0x101000 && (!hsw_linux_fences || off>=0x100100)))return false;
#ifdef SRVROS_PLATFORM_HOST_TEST
 extern uint32_t platform_test_hsw_read(unsigned);*v=platform_test_hsw_read(off);
#else
 *v=*(volatile const uint32_t *)(HWB_VA+(off&4095));
#endif
 return true;
}
static uint64_t hsw_now(void *ctx) {(void)ctx;return timer_monotonic_us();}
static void hsw_progress(void *ctx,unsigned phase) {(void)ctx;console_printf("hsw-backing-progress: pass=%u/2 full-vector%s%s; no input required\n",(uint64_t)phase+1,hsw_cpu_control?" + raw CPU TILECTL":"",hsw_linux_fences?" + Linux-model32 fences":"");}
static void hsw_b64(const uint8_t *bytes,unsigned n,char *data) {
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";unsigned j=0;
 for(unsigned i=0;i<n;i+=3) {
  unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;
  if(left>1)v|=(uint32_t)bytes[i+1]<<8;
  if(left>2)v|=bytes[i+2];
  data[j++]=abc[v>>18];data[j++]=abc[(v>>12)&63];data[j++]=left>1?abc[(v>>6)&63]:'=';data[j++]=left>2?abc[v&63]:'=';
 }
 data[j]=0;
}
static void hsw_report(void) {
 const struct hwb_result *r=&hsw_result;
 console_printf("hsw-backing: v=%u status=%u count=%u cap=1536 fences=%u upper=%s swizzle=partial maps=%u retained=%u offset=%u bytes=%u table=%u stolen=%u,%u limit_us=100000 writes=0 ownership=0\n",
 (uint64_t)r->version,(uint64_t)r->status,(uint64_t)r->count,(uint64_t)r->fences,r->version==3?"linux-model-32":"unknown",(uint64_t)r->maps,(uint64_t)r->retained,r->offset,r->bytes,r->table_bytes,r->stolen_base,r->stolen_end);
 static uint8_t bytes[HWB_PTES*4];static char data[HWB_PTES*4/3*4+5];
 for(unsigned phase=0;phase<2;phase++) {
  const struct hwb_sample *s=&r->sample[phase];unsigned n=0;
  for(unsigned i=0;i<HWB_WORDS;i++)hda_put(bytes,&n,s->endpoint.v[i],8);
  for(unsigned i=0;i<r->fences;i++)hda_put(bytes,&n,s->fence[i],8);
  hda_put(bytes,&n,s->begin,8);hda_put(bytes,&n,s->end,8);hda_put(bytes,&n,s->hash,8);
  hda_put(bytes,&n,s->reads,4);hda_put(bytes,&n,s->count,4);hda_put(bytes,&n,s->present,4);hda_put(bytes,&n,s->stolen,4);
  if(r->version>=2)hda_put(bytes,&n,s->tilectl,4);
  hsw_b64(bytes,n,data);console_printf("hsw-backing-meta: v=%u phase=%u data=%s\n",(uint64_t)r->version,(uint64_t)phase,data);
  n=0;for(unsigned i=0;i<s->count;i++)hda_put(bytes,&n,s->pte[i],4);
  hsw_b64(bytes,n,data);console_printf("hsw-backing-ptes: v=%u phase=%u data=%s\n",(uint64_t)r->version,(uint64_t)phase,data);
 }
}
static bool hsw_capture_bound(void) {
 struct hwb_io io={.linux_fences=hsw_linux_fences,.cpu_control=hsw_cpu_control,.endpoint=hsw_endpoint,.map=hsw_map,.unmap=hsw_unmap,.read=hsw_read,.now=hsw_now,.progress=hsw_progress};
 bool ok=hwb_capture(&hsw_result,&io);
 if(hsw_window_poison){hsw_result.retained=1;hsw_result.status=6;ok=false;}
 hsw_report();return ok;
}
#include "haswell_candidate.inc"
#include "haswell_survey.inc"
#include "haswell_engine_prereqs.inc"
#include "haswell_forcewake.inc"

static bool hsw_range_run(enum hsw_range_trial range, bool backing_captured) {
 switch(range) {
 case HSW_RANGE_SURVEY: {
  bool observed=backing_captured && hsw_survey_run();
  hsw_survey_report();
  return observed;
 }
 case HSW_RANGE_SWEEP: {
  bool observed=backing_captured && hsw_sweep_run();
  hsw_sweep_report();
  return observed;
 }
 case HSW_RANGE_OWNERSHIP: {
  bool observed=backing_captured && hsw_ownership_run();
  hsw_ownership_report();
  return observed;
 }
 case HSW_RANGE_REQUESTER_HOLD: {
  bool observed=backing_captured && hsw_requester_hold_run();
  hsw_ownership_report();
  hsw_requester_hold_report();
  return observed;
 }
 case HSW_RANGE_ENGINE_PREREQS: {
  bool observed=backing_captured && hsw_engine_prereqs_run();
  hsw_engine_prereqs_report();
  return observed;
 }
 case HSW_RANGE_FORCEWAKE:
 case HSW_RANGE_ENGINE_DRAIN:
 case HSW_RANGE_EXCLUSION_AUDIT:
 case HSW_RANGE_HANDOFF_AUDIT:
 case HSW_RANGE_RESET_FREE_AUDIT:
 case HSW_RANGE_ENGINE_HOLD: {
  hsw_reset_free_audit=range==HSW_RANGE_RESET_FREE_AUDIT || range==HSW_RANGE_ENGINE_HOLD;
  hsw_handoff_audit=hsw_reset_free_audit || range==HSW_RANGE_HANDOFF_AUDIT;
  hsw_exclusion_audit=hsw_handoff_audit || range==HSW_RANGE_EXCLUSION_AUDIT;
  bool sample_engines=range!=HSW_RANGE_FORCEWAKE;
  bool verified=backing_captured && hsw_forcewake_run(sample_engines);
  hsw_forcewake_report();
  if(sample_engines)hsw_drain_report();
  if(range==HSW_RANGE_ENGINE_HOLD) {
   struct hsw_engine_hold hold={0};
   /* No native firmware exclusion and completion-destination provider. */
   if(verified)(void)intel_hsw_engine_hold(&hold,NULL,0);
   console_printf("hsw-engine-hold: v=1 state=%s reason=%s polls=%u attempted=%u completion_us=%u authority=none\n",
                  hold.state==HSW_ENGINE_HOLD_REFUSED?"refused":"unavailable",
                  hold.reason?hold.reason:"baseline", (uint64_t)hold.polls,
                  (uint64_t)hold.attempted,hold.completed_at);
   return verified && hold.state==HSW_ENGINE_HOLD_REFUSED && !hold.attempted;
  }
  return verified;
 }
 case HSW_RANGE_NONE:
  return false;
 }
 return false;
}

static void hsw_backing_refusal(bool candidate_slot, bool access, bool boot_slot, enum hsw_range_trial range) {
 hsw_report();
 if(candidate_slot)hsw_candidate_report(boot_slot);
 if(access)hsw_access_report(false);
 (void)hsw_range_run(range,false);
}

static bool hsw_backing_run(bool cpu_control,bool linux_fences,bool candidate_slot,bool access,
                            bool boot_slot,enum hsw_range_trial range) {
 hsw_cpu_control=cpu_control;hsw_linux_fences=linux_fences;
 hsw_forcewake_backing=range==HSW_RANGE_FORCEWAKE || range==HSW_RANGE_ENGINE_DRAIN ||
                       range==HSW_RANGE_EXCLUSION_AUDIT || range==HSW_RANGE_HANDOFF_AUDIT ||
                       range==HSW_RANGE_REQUESTER_HOLD || range==HSW_RANGE_RESET_FREE_AUDIT ||
                       range==HSW_RANGE_ENGINE_HOLD;
 hsw_result=(struct hwb_result){.status=2,.version=linux_fences?3:cpu_control?2:1,.fences=linux_fences?32:HWB_FENCES};
 console_write("hsw-backing-progress: exact Air6,1 read-only identity/config preflight\n");
 const struct pci_device *candidate=NULL;
 for(uint64_t i=0;i<pci_device_count();i++) {
  const struct pci_device *p=pci_device_at(i);
  if(!p->bus && !p->device && !p->function && p->vendor_id==0x8086 && p->device_id==0x0a04)hsw_host=p;
  if(!p->bus && p->device==31 && !p->function && p->vendor_id==0x8086 && p->device_id==0x9c43)pch_device=p;
  if(!p->bus && p->device==2 && !p->function && p->vendor_id==0x8086 && p->device_id==0x0a26)candidate=p;
 }
 uint64_t base;
 if(!candidate || !hsw_host || !timer_reference_ready() ||
    !package_graphics_bar(pci_read_config32(candidate,0x10),pci_read_config32(candidate,0x14),pci_read_config16(candidate,4),&base) ||
    !hsw_safe_range(base,0x400000,false)) {hsw_backing_refusal(candidate_slot,access,boot_slot,range);return false;}
 gpu=candidate;gpu_base=base;
 if(!hsw_config_valid()){hsw_backing_refusal(candidate_slot,access,boot_slot,range);return false;}
 mapped=graphics_bind();hsw_firmware=mapped;
 if(!mapped){hsw_backing_refusal(candidate_slot,access,boot_slot,range);return false;}
 bool captured=hsw_capture_bound();
 if(range!=HSW_RANGE_NONE)return hsw_range_run(range,captured);
 if(!candidate_slot)return captured;
 bool observed=captured && (!access || hsw_live.slot_probe || haswell_firmware_run()) &&
               hsw_candidate_observe_access(access,boot_slot);
 hsw_candidate_report(boot_slot);
 if(access)hsw_access_report(observed);
 return observed;
}

bool platform_haswell_backing_run(bool cpu_control,bool linux_fences,bool candidate_slot,bool access,
                                 bool boot_slot,enum hsw_range_trial range) {
 if((boot_slot && (!candidate_slot || access)) ||
    (range!=HSW_RANGE_NONE && (candidate_slot || access || boot_slot || !cpu_control || !linux_fences)))return false;
 return hsw_backing_run(cpu_control,linux_fences,candidate_slot,access,boot_slot,range);
}

bool platform_haswell_live_slot(bool sampler_saved) {
 bool ok=false;
 if(sampler_saved && hsw_live.requester_rows==2) {
  hsw_live.slot_probe=true;
  ok=hsw_backing_run(true,true,true,true,false,HSW_RANGE_NONE) && hsw_live_match();
  hsw_live.slot_probe=false;
 }
 hsw_live_report(ok);
 return ok;
}
