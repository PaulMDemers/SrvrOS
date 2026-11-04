/* Existing isolated Air6,1 diagnostic only. Never maps pixels/stolen RAM.
 * New window is supervisor/RO/NX/cache-disabled, kernel-space only, outside
 * the global mapping registry. Uncertain retirement is retained until reset. */
#include <srvros/haswell_backing.h>
#define HWB_VA 0xffffc001f3240000ull
static const struct pci_device *hsw_host;
static struct hwb_result hsw_result;
static uint64_t hsw_window_pa;
static bool hsw_window_live,hsw_window_poison,hsw_firmware,hsw_cpu_control,hsw_linux_fences;
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
    (off&4095) || (off!=0x100000 && !(hsw_cpu_control && off==0x101000) && (off<first || off>last || off>=0x400000)) ||
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
static bool hsw_read(void *ctx,uint32_t off,uint32_t *v) {
 (void)ctx;
 if(!hsw_window_live || hsw_window_poison || vmm_current_address_space()!=vmm_kernel_address_space() ||
    gpu_base+(off&~4095u)!=hsw_window_pa || (off&3) ||
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
bool platform_haswell_backing_run(bool cpu_control,bool linux_fences) {
 hsw_cpu_control=cpu_control;hsw_linux_fences=linux_fences;
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
    !hsw_safe_range(base,0x400000,false)) {hsw_report();return false;}
 gpu=candidate;gpu_base=base;
 if(!hsw_config_valid()){hsw_report();return false;}
 mapped=graphics_bind();hsw_firmware=mapped;
 if(!mapped){hsw_report();return false;}
 return hsw_capture_bound();
}
