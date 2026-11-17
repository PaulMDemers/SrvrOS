/* Execute the actual native read-only adapter against a modeled PCI/VMM bus.
 * No assertion below represents native device acceptance. */
#include <stdbool.h>
#include <stdint.h>
#include <assert.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
static bool intel_stolen_test_alias(uint64_t,uint64_t,uint64_t);
static bool intel_stolen_test_read(uint32_t,uint32_t *);
#define SRVROS_INTEL_STOLEN_CAPTURE
#define SRVROS_INTEL_STOLEN_HOST_TEST
#ifndef SRVROS_STOLEN_ADAPTER_FILE
#define SRVROS_STOLEN_ADAPTER_FILE "../kernel/src/drivers/intel_stolen_probe.c"
#endif
#include SRVROS_STOLEN_ADAPTER_FILE

enum fault {PROFILE=1,SNAPSHOT,REVALIDATE,CLEAN,SYNC,ACQUIRE,HELD,ROOT,CLAIM,PCI,CACHE,ALIAS,MAP,UNMAP,READ,CLOCK,RELEASE,FAULT_COUNT};
static struct {
 unsigned calls[FAULT_COUNT],which,at,cases,reads,maps,unmaps;
 uint64_t physical,clock;
 bool mapped,held,report;
 unsigned mode;
} model;
static struct pci_device devices[2]={{.device=0},{.device=2}};
static struct limine_memmap_entry entry;
static struct limine_memmap_entry *entries[]={&entry};
static struct limine_memmap_response memory;
volatile struct limine_memmap_request memmap_request;
static const char *command="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-stolen=read-only srvros.capture-failure=reboot";
static bool fault(unsigned which) {return ++model.calls[which]==model.at && model.which==which;}
static void reset(void) {
 unsigned cases=model.cases;memset(&model,0,sizeof(model));model.cases=cases;model.clock=1000;
 memset(&stolen,0,sizeof(stolen));
 entry=(struct limine_memmap_entry){.base=0xc0000000,.length=0x1000000,.type=LIMINE_MEMMAP_RESERVED};
 memory=(struct limine_memmap_response){.entry_count=1,.entries=entries};memmap_request.response=&memory;
}
bool timer_reference_ready(void) {return true;}
uint64_t timer_monotonic_us(void) {if(fault(CLOCK)) {
 if(model.mode==1)return 0;
 if(model.mode==2)return 1000;
 model.clock+=STOLEN_CENSUS_US+1;return model.clock;
 }return ++model.clock;}
bool intel_scanout_trial_profile(struct scanout_profile *p) {
 if(fault(PROFILE))return false;
 *p=(struct scanout_profile){.width=1440,.height=900,.pitch=6144};if(model.mode==10)p->width++;return true;
}
bool intel_window_handoff_snapshot(struct scanout_handoff_snapshot *s) {
 if(fault(SNAPSHOT))return false;
 *s=(struct scanout_handoff_snapshot){.mmio=0xc0000000,.aperture=0xb0000000,.aperture_bytes=0x10000000,.fence_end=0x2200000,.first=8704,.pages=16};
 for(unsigned i=0;i<16;i++)s->original[i]=0x8e201001ull+4096ull*i;
 if(model.mode==11)s->original[5]^=4096;
 if(model.mode==12)s->assurances=31;
 if(model.mode==13)s->pages=15;
 if(model.mode==14)s->fence_end-=4096;
 if(model.mode==15)s->aperture_bytes/=2;
 if(model.mode==16)s->mmio+=4096;
 return true;
}
bool intel_window_handoff_revalidate(const struct scanout_handoff_snapshot *s) {assert(s==&stolen.source && model.held && !model.mapped);return !fault(REVALIDATE);}
bool intel_window_handoff_clean(void) {return !fault(CLEAN);}
bool console_sync(void) {return !fault(SYNC);}
bool display_access_acquire(uint64_t owner,struct display_access_token *t) {
 assert(!model.held && owner);if(fault(ACQUIRE))return false;
 *t=(struct display_access_token){1,owner};model.held=true;return true;
}
bool display_access_held(const struct display_access_token *t) {assert(t==&stolen.exclusion);return model.held && !fault(HELD);}
bool display_access_release(const struct display_access_token *t,bool clean) {assert(t==&stolen.exclusion && clean && !model.mapped);if(fault(RELEASE))return false;model.held=false;return true;}
uint64_t vmm_current_address_space(void) {return fault(ROOT)?0x2000:0x1000;}
uint64_t vmm_kernel_address_space(void) {return 0x1000;}
uint64_t pci_device_count(void) {return 2;}
const struct pci_device *pci_device_at(uint64_t i) {return i<2?&devices[i]:NULL;}
bool intel_boot_check(unsigned client,const struct pci_device *gpu) {assert(client==INTEL_BOOT_OBSERVER && gpu==&devices[1]);return !fault(CLAIM);}
uint32_t pci_read_config32(const struct pci_device *p,uint8_t offset) {
 if(fault(PCI))return offset==4?0:UINT32_MAX;
 bool gpu=p==&devices[1];assert(gpu || p==&devices[0]);
 switch(offset) {
 case 0:return gpu?0x16268086:0x16048086;
 case 4:return 7;
 case 0x10:return 0xc0000004;
 case 0x14:return 0;
 case 0x2c:return 0x011b106b;
 case 0x50:return 0x2c1;
 case 0x5c:case 0xb0:return 0x8c000001;
 case 0xb4:return 0x8b800001;
 case 0xbc:return 0x90000001;
 default:assert(false);return 0;
 }
}
bool intel_cache_probe_context(struct x86_cache_snapshot *s) {
 if(fault(CACHE))return false;
 *s=(struct x86_cache_snapshot){.cpu={.vendor={0x756e6547,0x49656e69,0x6c65746e},.features=0x11000,.physical_bits=36},.pat=0x0007040600070406ull,.def_type=0x800,.status=CACHE_OBSERVED};
 if(model.mode==3 && model.calls[CACHE]>1)++s->cpu.signature;
 if(model.mode==27)s->pat|=7ull<<24;
 if(model.mode==28)s->def_type=0x806;
 if(model.mode==29)s->pat|=4ull<<24;
 return true;
}
bool vmm_map_page_in_space(uint64_t root,uint64_t va,uint64_t physical,uint64_t flags) {
 assert(root==0x1000 && va==STOLEN_VA && !model.mapped && model.held);
 assert(flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|VMM_PAGE_WRITE_THROUGH));
 assert(physical>=0xc0000000 && physical<0xc1000000);
 bool fail=fault(MAP);
 if(fail && model.mode!=4)return false;
 model.mapped=true;model.physical=physical+(model.mode==7?4096:0);++model.maps;return !fail;
}
bool vmm_virt_to_phys_in_space(uint64_t root,uint64_t va,uint64_t *physical) {
 assert(root==0x1000 && va==STOLEN_VA);if(!model.mapped)return false;
 *physical=model.physical;return true;
}
bool vmm_unmap_page_in_space(uint64_t root,uint64_t va,uint64_t *physical) {
 assert(root==0x1000 && va==STOLEN_VA && model.mapped);
 if(fault(UNMAP))return false;
 *physical=model.physical+(model.mode==8?4096:0);model.mapped=false;++model.unmaps;return true;
}
static bool intel_stolen_test_alias(uint64_t root,uint64_t va,uint64_t physical) {
 assert(root==0x1000 && va==STOLEN_VA && physical==model.physical && model.mapped);
 return !fault(ALIAS);
}
static bool intel_stolen_test_read(uint32_t offset,uint32_t *out) {
 assert(model.mapped && model.physical==0xc0000000ull+(offset&~4095u));
 if(fault(READ))return false;
 ++model.reads;
 switch(offset) {
 case 0x1082c0:*out=0x8ff00007;return true;
 case 0x43208:*out=0;return true;
 case 0x6f800:*out=0x00100001;return true;
 case 0x6f840:*out=0;return true;
 default: {
  assert(offset>=0x800000 && offset<0x817000 && !(offset&3));
  uint64_t pte=0x8c001001ull+((offset-0x800000)/8)*4096ull;
  if(model.mode==5 && (offset-0x800000)/8==9000)pte^=4096;
  *out=(offset&4)?(uint32_t)(pte>>32):(uint32_t)pte;
  if(model.mode==6 && model.reads==7)*out^=1;
  return true;
 }
 }
}
void console_write(const char *s) {if(model.report)fputs(s,stdout);}
void console_printf(const char *fmt,...) {
 if(!model.report)return;
 va_list args;va_start(args,fmt);
 for(const char *p=fmt;*p;p++) {
  if(*p!='%'){putchar(*p);continue;}
  ++p;uint64_t value=va_arg(args,uint64_t);
  assert(*p=='u' || *p=='x');if(*p=='u')printf("%" PRIu64,value);else printf("0x%016" PRIx64,value);
 }
 va_end(args);
}
static void positive(void) {
 reset();assert(intel_stolen_probe_report(command));assert(stolen.complete && !intel_stolen_probe_retained());
 assert(!model.mapped && !model.held && model.maps==52 && model.unmaps==52);
 assert(model.reads==70664 && stolen.layout.checked==23552 && stolen.meta_reads==8);
 assert(model.calls[REVALIDATE]==4);
 for(unsigned n=0;n<2;n++)for(unsigned r=0;r<3;r++)assert(stolen.count[n][r]==stolen.plan.range[r].pages);
 assert(!intel_stolen_probe_report(command));++model.cases;
}
int main(int argc,char **argv) {
 if(argc==2 && !strcmp(argv[1],"--unsupported-report")) {reset();model.which=PROFILE;model.at=1;model.report=true;return intel_stolen_probe_report(command)?1:0;}
 if(argc==2 && !strcmp(argv[1],"--refusal-report")) {reset();model.mode=27;model.report=true;return intel_stolen_probe_report(command)?1:0;}
 if(argc==2 && !strcmp(argv[1],"--report")) {reset();model.report=true;return intel_stolen_probe_report(command)?0:1;}
 positive();
 unsigned counts[FAULT_COUNT];memcpy(counts,model.calls,sizeof(counts));
 for(unsigned which=1;which<FAULT_COUNT;which++) {
  assert(counts[which]);unsigned stops[]={1,2,3,counts[which]/2,counts[which]};
  for(unsigned k=0;k<5;k++) {
   if(!stops[k] || stops[k]>counts[which] || (which==CLOCK && stops[k]==1))continue;
   reset();model.which=which;model.at=stops[k];
   bool ok=intel_stolen_probe_report(command);assert(stolen.fail);if(ok)fprintf(stderr,"undetected which=%u at=%u calls=%u\n",which,stops[k],model.calls[which]);assert(!ok);if(stolen.complete)assert(intel_stolen_probe_retained());
   if(model.mapped || model.held)assert(intel_stolen_probe_retained());
   else assert(!intel_stolen_probe_retained());
   assert(model.unmaps<=model.maps);++model.cases;
  }
 }
 for(unsigned mode=1;mode<=2;mode++)for(unsigned at=1;at<375;at+=7) {
  reset();model.mode=mode;model.which=CLOCK;model.at=at;
  if(intel_stolen_probe_report(command))assert(model.calls[CLOCK]<at || (mode==2 && at==1));
  else assert(!stolen.complete);
  ++model.cases;
 }
 for(unsigned mode=3;mode<=5;mode++) {
  reset();model.mode=mode;if(mode==4){model.which=MAP;model.at=2;}
  assert(!intel_stolen_probe_report(command));
  if(mode==4)assert(model.mapped && intel_stolen_probe_retained() && !stolen.released);
  else assert(!model.mapped && !model.held);
  ++model.cases;
 }
 for(unsigned mode=6;mode<=16;mode++) {
  if(mode==9)continue;
  reset();model.mode=mode;assert(!intel_stolen_probe_report(command));
  if(mode==7 || mode==8){assert(intel_stolen_probe_retained() && model.held);assert(stolen.fail==(mode==7?16:22));}
  else assert(!model.held && !model.mapped && !intel_stolen_probe_retained());
  if(mode>=12)assert(!model.maps);
  ++model.cases;
 }
 for(unsigned mode=0;mode<4;mode++) {
  reset();if(mode==0)entry.type=LIMINE_MEMMAP_USABLE;
  if(mode==1)entry.length=0;
  if(mode==2)memmap_request.response=NULL;
  if(mode==3){entry.base=UINT64_MAX-1;entry.length=4096;}
  assert(!intel_stolen_probe_report(command));assert(!model.maps && !model.held);++model.cases;
 }
 /* Captured PAT2=UC-minus is not silently accepted as PAT UC0. These
  * failures are after original-source admission and before any new alias. */
 for(unsigned mode=27;mode<=29;mode++) {
  reset();model.mode=mode;assert(!intel_stolen_probe_report(command));
  assert(stolen.fail==14 && stolen.fail_offset==0x1082c0);
  assert(!model.maps && !model.held && !intel_stolen_probe_retained());
  assert((mode==27 && stolen.pat==7) || (mode==28 && stolen.mtrr==6) || (mode==29 && stolen.pat==4));
  ++model.cases;
 }
 const char *bad[]={"",NULL,"srvros.intel-stolen=read-only", "srvros.storage=ram srvros.storage=ram"};
 for(unsigned i=0;i<sizeof(bad)/sizeof(bad[0]);i++) {reset();assert(!intel_stolen_probe_report(bad[i]) && !model.maps && !model.held);++model.cases;}
 char changed[512];assert(strlen(command)+80<sizeof(changed));
 const char *extra[]={" extra=1"," srvros.intel-stolen=read-only"," srvros.intel-wake=trial"};
 for(unsigned i=0;i<3;i++){reset();snprintf(changed,sizeof(changed),"%s%s",command,extra[i]);assert(!intel_stolen_probe_report(changed) && !model.maps);++model.cases;}
 printf("stolen-adapter PASS cases=%u actual-adapter=1 hardware=0 writes=0 ownership=0\n",model.cases);return 0;
}
