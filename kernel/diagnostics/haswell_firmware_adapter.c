/* Firmware copies only. No GPU/VT-d register, control or mapping operation. */
#include <srvros/haswell_firmware.h>
#include <srvros/acpi.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
extern volatile struct limine_memmap_request memmap_request;
static const uint8_t pin[32]={0xaa,0xfa,0x82,0x6c,0xe5,0x0c,0x10,0x38,0x8f,0x2c,0x41,0x98,0xde,0x9a,0x42,0x9a,0xb5,0x1a,0x55,0x73,0xad,0x1c,0x5d,0x28,0x56,0xea,0x32,0xed,0x07,0x8b,0x63,0x07};
static bool identity(void *ctx,uint32_t out[5]) {
 (void)ctx;const struct pci_device *g=NULL,*h=NULL,*p=NULL;
 for(uint64_t i=0;i<pci_device_count();i++) {
  const struct pci_device *d=pci_device_at(i);if(!d || d->bus || d->function)continue;
  if(d->device==2){if(g)return false;g=d;}
  if(d->device==0){if(h)return false;h=d;}
  if(d->device==31){if(p)return false;p=d;}
 }
 if(!g || !h || !p)return false;
 out[0]=pci_read_config32(g,0);out[1]=pci_read_config32(g,8);out[2]=pci_read_config32(g,0x2c);
 out[3]=pci_read_config32(h,0);out[4]=pci_read_config32(p,0);return true;
}
static bool catalog(void *ctx,struct hfw_entry *out,unsigned *count) {
 (void)ctx;const struct limine_memmap_response *m=memmap_request.response;
 if(!m || !m->entries || !m->entry_count || m->entry_count>HFW_MAP_CAP)return false;
 *count=(unsigned)m->entry_count;
 for(unsigned i=0;i<*count;i++){const struct limine_memmap_entry *e=m->entries[i];if(!e)return false;out[i]=(struct hfw_entry){e->base,e->length,e->type};}
 return true;
}
static bool table(void *ctx,const struct hfw_entry *entries,unsigned count,uint8_t *out,uint64_t *pa) {
 (void)ctx;const struct acpi_table_info *found=NULL;
 if(acpi_table_count()>128)return false;
 for(uint64_t i=0;i<acpi_table_count();i++) {
  const struct acpi_table_info *t=acpi_table_at(i);if(!t)return false;
  if(t->signature[0]=='D' && t->signature[1]=='M' && t->signature[2]=='A' && t->signature[3]=='R') {if(found)return false;found=t;}
 }
 if(!found || found->length!=HFW_DMAR_BYTES || found->revision!=1 || !hfw_table_covered(entries,count,found->physical_address))return false;
 uint64_t kernel=vmm_kernel_address_space();
 if(!kernel || vmm_current_address_space()!=kernel)return false;
 const volatile uint8_t *data=pmm_phys_to_virt(found->physical_address);if(!data)return false;
 for(unsigned i=0;i<HFW_DMAR_BYTES;i++) {
  uint64_t physical;
  if(!vmm_virt_to_phys_in_space(kernel,(uint64_t)(uintptr_t)(data+i),&physical) || physical!=found->physical_address+i)return false;
  out[i]=data[i];
 }
 *pa=found->physical_address;return true;
}
static uint64_t now(void *ctx){(void)ctx;return timer_monotonic_us();}
static void progress(void *ctx,unsigned phase){(void)ctx;console_printf("hsw-firmware-progress: pass=%u/2 full DMAR/catalog; no input required\n",(uint64_t)phase+1);}
static void b64(const uint8_t *bytes,unsigned n,char *out) {
 static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";unsigned j=0;
 for(unsigned i=0;i<n;i+=3){unsigned left=n-i;uint32_t v=(uint32_t)bytes[i]<<16;if(left>1)v|=(uint32_t)bytes[i+1]<<8;if(left>2)v|=bytes[i+2];out[j++]=abc[v>>18];out[j++]=abc[(v>>12)&63];out[j++]=left>1?abc[(v>>6)&63]:'=';out[j++]=left>2?abc[v&63]:'=';}
 out[j]=0;
}
static void put(uint8_t *out,unsigned *n,uint64_t v,unsigned bytes){for(unsigned i=0;i<bytes;i++)out[(*n)++]=(uint8_t)(v>>(8*i));}
void hfw_report_named(const struct hfw_result *r,const char *prefix) {
 console_printf("%s: v=1 status=%u count=%u cap=128 dmar_bytes=136 limit_us=100000 mmio=0 writes=0 ownership=0\n",prefix,(uint64_t)r->status,(uint64_t)(r->status==1?r->sample[0].count:0));
 if(r->status!=1)return;
 static uint8_t bytes[HFW_MAP_CAP*24];static char data[HFW_MAP_CAP*32+1];
 for(unsigned phase=0;phase<2;phase++) {
  const struct hfw_sample *s=&r->sample[phase];unsigned n=0;
  for(unsigned i=0;i<5;i++)put(bytes,&n,s->identity[i],4);
  put(bytes,&n,s->count,4);put(bytes,&n,s->table_pa,8);put(bytes,&n,s->begin,8);put(bytes,&n,s->end,8);
  b64(bytes,n,data);console_printf("%s-meta: v=1 phase=%u data=%s\n",prefix,(uint64_t)phase,data);
  b64(s->dmar,HFW_DMAR_BYTES,data);console_printf("%s-dmar: v=1 phase=%u data=%s\n",prefix,(uint64_t)phase,data);
  n=0;for(unsigned i=0;i<s->count;i++){put(bytes,&n,s->entries[i].base,8);put(bytes,&n,s->entries[i].length,8);put(bytes,&n,s->entries[i].type,8);}
  b64(bytes,n,data);console_printf("%s-map: v=1 phase=%u data=%s\n",prefix,(uint64_t)phase,data);
 }
}
void hfw_report(const struct hfw_result *r){hfw_report_named(r,"hsw-firmware");}
bool haswell_firmware_run(void) {
 static struct hfw_result result;
 console_write("hsw-firmware-progress: exact Air6,1 firmware-only preflight; no VT-d register access\n");
 const struct hfw_io io={.identity=identity,.catalog=catalog,.table=table,.now=now,.progress=progress};
 bool ok=timer_reference_ready() && hfw_capture(&result,&io,pin);
 if(!timer_reference_ready())result=(struct hfw_result){.status=6};
 hfw_report(&result);return ok;
}

#include "haswell_cache_context.inc"
