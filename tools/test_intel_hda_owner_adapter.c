#include <stdint.h>
#include <stdbool.h>
static uint64_t intel_display_test_hda_irq_save(void);
static bool intel_display_test_hda_irq_off(void);
static void intel_display_test_hda_irq_restore(uint64_t);
static bool intel_display_test_hda_uc(uint64_t);
static bool intel_display_test_hda_read(unsigned,unsigned,uint32_t *);
static uint32_t intel_display_test_hda_config(unsigned);
static bool intel_display_test_hda_map(void);
static bool intel_display_test_hda_retire(void);
struct pci_device;
static bool intel_display_test_hda_command16(const struct pci_device *,unsigned,uint16_t);
#define main existing_observer_fixture_main
#include "test_intel_display_probe.c"
#undef main
#ifdef SRVROS_INTEL_HDA_LEASE
#define HDA_TEST_MODE "srvros.intel-hda=lease-trial"
#define HDA_TEST_LABEL "intel-hda-lease"
#define HDA_TEST_FW intel_hda_lease_firmware_request
#else
#define HDA_TEST_MODE "srvros.intel-hda=owner-trial"
#define HDA_TEST_LABEL "intel-hda-owner"
#define HDA_TEST_FW intel_hda_owner_firmware_request
#endif
static const char *hda_case;
static unsigned hda_reads,hda_cache_calls,hda_stores;
static uint16_t hda_command=6;
static bool hc(const char *s){return !strcmp(hda_case,s);}
static struct limine_firmware_type_response hda_firmware={.firmware_type=2};
static struct limine_bootloader_info_response hda_loader={.name="Limine",.version="11.4.1"};
static struct limine_executable_cmdline_response hda_cmd={.cmdline="srvros.storage=ram srvros.workspace-volume=37d8c5d0 srvros.desktop-release=0123456789abcdef srvros.capture=intel-display srvros.intel-backing=read-only srvros.intel-cache=read-only srvros.intel-window=read-only srvros.intel-handoff=qualify srvros.intel-gt-preflight=read-only srvros.intel-firmware=read-only " HDA_TEST_MODE " srvros.intel-hda-policy=cooperative-efi-v1 srvros.capture-failure=reboot"};
volatile struct limine_firmware_type_request HDA_TEST_FW={.response=&hda_firmware};
volatile struct limine_bootloader_info_request bootloader_info_request={.response=&hda_loader};
volatile struct limine_executable_cmdline_request executable_cmdline_request={.response=&hda_cmd};
static uint64_t intel_display_test_hda_irq_save(void){wake_irq=true;return 512;}
static bool intel_display_test_hda_irq_off(void){return wake_irq && !hc("irq");}
static void intel_display_test_hda_irq_restore(uint64_t f){assert(f==512);wake_irq=false;}
static bool intel_display_test_hda_uc(uint64_t p){assert(p==INTEL_HDA_BASE && wake_irq && !display_access_allowed());hda_cache_calls++;return !(hc("store-cache") && hda_stores) && !(hc("held-cache") && hda_reads>=116) && !hc("cache") && !(hc("late-cache") && hda_cache_calls==3);}
static bool intel_display_test_hda_map(void){return !hc("map");}
static bool intel_display_test_hda_retire(void){return !hc("retire") && !(hc("late-retire") && hda_stores);}
static uint32_t intel_display_test_hda_config(unsigned off){switch(off){case 0:return hc("id")?0x160b8086:0x160c8086;case 4:return hc("decode")?0x00100004:(0x00100000|hda_command);case 8:return hc("revision")?0x04030008:0x04030009;case 0x2c:return hc("subsystem")?0x012b106b:0x011b106b;case 0x10:return hc("bar")?0xc1620004:0xc1610004;case 0x14:return hc("high")?1:0;case 0x34:return 0x40;case 0x40:return 0x00030001;case 0x44:return hc("d3") || (hc("store-power") && hda_stores) || (hc("power-change") && hda_reads>=5)?3:0;default:return 0;}}
static bool intel_display_test_hda_read(unsigned off,unsigned width,uint32_t *v){assert(wake_irq && !display_access_allowed() && intel_hda_read_allowed(off,width));uint64_t physical;assert(vmm_virt_to_phys_in_space(0x1000,HDA_VA,&physical) && physical==INTEL_HDA_BASE);hda_reads++;*v=0;if(off==0)*v=hc("over")?0x9001:hc("max")?0x4401:0x3001;if(off==8)*v=1;if(off>=0x80 && off<0xe0 && (off-0x80)%32==2)*v=4;if(((hc("late-active") && hda_stores) || (hc("final-active") && hda_stores==2)) && off==0x80)*v=2;if(hc("held-active") && hda_reads>116 && off==0x80)*v=2;
 if(hc("held-cr3") && hda_reads==120)current=0x2000;
 if(hc("active")){if(off==0x80 || off==0x4c || off==0x5c)*v=2;if(off==0x70)*v=0x12345001;}if(hc("changed") && hda_reads>23 && off==0x98)*v=4096;if(hc("float"))*v=(1ull<<(8*width))-1;if(hc("slow"))ticks+=200001;if(hc("cr3") && hda_reads==5)current=0x2000;if(hc("stolen") && hda_reads==5)pa[INTEL_CAPTURE_PAGES+12]=0xdead000;return !hc("read") && !(hc("held-read") && hda_reads>116);}
static bool intel_display_test_hda_command16(const struct pci_device *device,unsigned offset,uint16_t value) {
 assert(device==&devices[3] && offset==4 && wake_irq && !display_access_allowed());
 assert((hda_stores==0 && value==2 && hda_command==6) || (hda_stores==1 && value==6 && hda_command==2));
 hda_stores++;
 if(hc("store-none") || (hc("restore-none") && hda_stores==2))return false;
 if(!hc("readback") && !(hc("restore-readback") && hda_stores==2))hda_command=value;
 if(hc("store-cr3"))current=0x2000;
 if(hc("store-alias"))pa[INTEL_CAPTURE_PAGES+12]=0xdead000;
 if(hc("store-parent"))trial_mapping_fault=true;
 if(hc("store-uncertain"))return false;
 if(hc("write-slow"))ticks+=400001;
 return true;
}
int main(int argc,char **argv){hda_case=argc>1?argv[1]:"normal";reset();backing_mode=cache_mode=window_mode=gt_mode=fw_mode=reporting=true;ram.length=0x10000000;firmware_region=(struct limine_memmap_entry){.base=0x12345000,.length=8192,.type=LIMINE_MEMMAP_ACPI_NVS};stolen_region.base=0x8c000000;stolen_region.length=0x4000000;memory.entry_count=4;devices[3]=(struct pci_device){.device=3,.vendor_id=0x8086,.device_id=0x160c,.class_code=4,.subclass=3};intel_display_probe_boot(&frame);assert(intel_display_probe_report());intel_display_backing_probe_boot(&frame);assert(intel_display_backing_probe_report());assert(intel_ggtt_window_probe_report());assert(intel_gt_preflight_probe_report());assert(intel_gt_firmware_probe_report());assert(display_access_allowed());
 if(hc("loader"))hda_loader.version="bad";
 if(hc("efi"))hda_firmware.firmware_type=1;
 if(hc("recipe"))hda_cmd.cmdline="srvros.intel-hda=read-only";
 if(hc("alias"))pa[INTEL_CAPTURE_PAGES+12]=0xdead000;
 struct limine_memmap_entry hda_ram={.base=INTEL_HDA_BASE,.length=4096,.type=LIMINE_MEMMAP_USABLE};
 struct limine_memmap_entry *with_hda_ram[]={&ram,&aperture,&stolen_region,&firmware_region,&hda_ram};
 if(hc("ram")){memory.entry_count=5;memory.entries=with_hda_ram;}
 console_write(HDA_TEST_LABEL ": preflight checkpoint saved\n");
 bool ok=intel_display_hda_owner_report();bool expected=hc("normal");assert(ok==expected);assert(!wake_irq && !wake_writes);
 bool retained=hc("map") || hc("retire") || hc("cr3") || hc("stolen") || hc("late-active") || hc("late-retire") || hc("store-none") || hc("store-uncertain") || hc("readback") || hc("restore-none") || hc("write-slow") || hc("store-cache") || hc("store-power") || hc("store-cr3") || hc("store-alias") || hc("store-parent") || hc("restore-readback") || hc("final-active") || hc("held-cache") || hc("held-active") || hc("held-cr3") || hc("held-read");
 assert(display_access_allowed()!=retained);assert(intel_display_hda_owner_retained()==retained);
 if(expected)assert(hda_stores==2 && hda_command==6 && hda_reads==
#ifdef SRVROS_INTEL_HDA_LEASE
 232
#else
 174
#endif
 && !pa[INTEL_CAPTURE_PAGES+12]);
 if(!retained && !expected)assert(!hda_stores);
 if(hc("late-active") || hc("late-retire") || hc("store-none") || hc("store-uncertain") || hc("readback") || hc("write-slow") || hc("store-cache") || hc("store-power") || hc("store-cr3") || hc("store-alias") || hc("store-parent"))assert(hda_stores==1);
 if(hc("held-cache") || hc("held-active") || hc("held-cr3") || hc("held-read"))assert(hda_stores==1);
 if(hc("restore-readback"))assert(hda_stores==2 && hda_command==2);
 if(hc("final-active"))assert(hda_stores==2 && hda_command==6);
 if(hc("store-alias"))assert(pa[INTEL_CAPTURE_PAGES+12]==0xdead000);
 if(hc("restore-none"))assert(hda_stores==2 && hda_command==2);
 if(hc("power-change"))assert(hda_reads==5);
 if(hc("loader") || hc("efi") || hc("recipe") || hc("irq") || hc("id") || hc("revision") || hc("subsystem") || hc("bar") || hc("high") || hc("d3") || hc("decode") || hc("cache"))assert(!hda_reads);
 if(hc("alias"))assert(pa[INTEL_CAPTURE_PAGES+12]==0xdead000 && !hda_reads);
 unsigned reads=hda_reads,stores=hda_stores;assert(!intel_display_hda_owner_report());assert(reads==hda_reads && stores==hda_stores);return 0;}
