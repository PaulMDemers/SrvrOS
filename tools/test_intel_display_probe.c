#include <assert.h>
#include <stdarg.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <srvros/intel_display_capture.h>
static bool intel_display_test_read(uint32_t,uint32_t *);
#define SRVROS_INTEL_DISPLAY_HOST_TEST
#include "../kernel/src/drivers/intel_display_probe.c"

static unsigned config_reads,mmio_reads,map_calls,unmap_calls;
static uint64_t ticks,current,pa[INTEL_CAPTURE_PAGES+1];
static bool firmware_ok,clock_ok,alias,malformed,reporting,backing_mode,translation_gap;
static struct pci_device devices[3];
static struct acpi_table_info dsdt;
static struct limine_memmap_entry ram,aperture,stolen_region;
static struct limine_memmap_entry *entries[]={&ram,&aperture,&stolen_region};
static struct limine_memmap_response memory;
volatile struct limine_memmap_request memmap_request;
static struct limine_framebuffer frame;
static const uint64_t fb_virtual=0xffff8000b0000000ull;
uint64_t pci_device_count(void){return 3;}
const struct pci_device *pci_device_at(uint64_t i){return i<3?&devices[i]:NULL;}
uint32_t pci_read_config32(const struct pci_device *d,uint8_t off) {
    config_reads++;assert(config_reads<1000);
    if(d==&devices[2]){switch(off){case 0:return 0x16048086;case 0x50:return 0x140;case 0xb0:return 0x90000001;case 0xb4:return 0x8fe00001;case 0xbc:return 0x92000001;default:return 0;}}
    if(d==&devices[1])return off==0?0x9cc38086:0;
    switch(off) {
    case 0:return 0x16268086;case 4:return 0x00100006;case 8:return 0x03000009;
    case 0x10:return 0xc0000004;case 0x18:return 0xb000000c;case 0x2c:return 0x011b106b;
    case 0x50:return 0x140;case 0x5c:return 0x90000001;
    case 0x34:return 0x40;case 0x40:return malformed?0x00034001:0x00030001;
    default:return 0;
    }
}
uint64_t acpi_table_count(void){return 1;}
const struct acpi_table_info *acpi_table_at(uint64_t i){return i?NULL:&dsdt;}
void *pmm_phys_to_virt(uint64_t p){return (void *)(uintptr_t)p;}
bool laptop_firmware_match(const uint8_t *p,size_t n){assert((uintptr_t)p==0x100000 && n==35034);return firmware_ok;}
uint64_t vmm_kernel_address_space(void){return 0x1000;}
uint64_t vmm_current_address_space(void){return current;}
bool vmm_virt_to_phys(uint64_t v,uint64_t *p){assert(v==fb_virtual);*p=0xb0000000;return true;}
static unsigned index_of(uint64_t v){if(v==BACKING_VA)return INTEL_CAPTURE_PAGES;assert(v>=CAPTURE_VA && v<CAPTURE_VA+4096*INTEL_CAPTURE_PAGES && !(v&4095));return (unsigned)((v-CAPTURE_VA)/4096);}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t v,uint64_t *p){assert(space==0x1000);if(backing_mode && v>=fb_virtual && v<fb_virtual+6144*900){if(translation_gap && v==fb_virtual+4096)return false;*p=0xb0000000+v-fb_virtual;return true;}unsigned n=index_of(v);*p=pa[n];return pa[n]!=0;}
bool vmm_map_page_in_space(uint64_t space,uint64_t v,uint64_t p,uint64_t flags) {
    assert(space==0x1000 && flags==(VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE));unsigned n=index_of(v);
    assert(!pa[n]);if(n==INTEL_CAPTURE_PAGES)assert(p>=0xc0000000 && p<0xc1000000);else assert(p==0xc0000000ull+intel_capture_pages[n]);pa[n]=p;map_calls++;return true;
}
bool vmm_unmap_page_in_space(uint64_t space,uint64_t v,uint64_t *p){assert(space==0x1000);unsigned n=index_of(v);assert(pa[n]);*p=pa[n];pa[n]=0;unmap_calls++;return true;}
bool timer_reference_ready(void){return clock_ok;}
uint64_t timer_monotonic_us(void){ticks+=100;return ticks;}
void console_write(const char *s){if(reporting)fputs(s,stdout);}
/* Kernel %u/%x arguments are uint64_t; adapt those conversions for host libc. */
void console_printf(const char *format,...) {
    if(!reporting)return;
    char translated[2048];size_t n=0;
    for(size_t i=0;format[i];i++) {
        if(format[i]=='%' && (format[i+1]=='u' || format[i+1]=='x')) {
            translated[n++]='%';
            if(format[i+1]=='x')translated[n++]='#';
            const char *suffix=format[i+1]=='u'?PRIu64:PRIx64;
            while(*suffix)translated[n++]=*suffix++;
            i++;
        } else translated[n++]=format[i];
    }
    translated[n]=0;va_list ap;va_start(ap,format);vprintf(translated,ap);va_end(ap);
}
static bool intel_display_test_read(uint32_t off,uint32_t *value) {
    if(backing_mode && (off>=0x100000 || !intel_capture_offset_allowed(off))) {
        *value=0;
        if(off>=0x100000 && off<0x100100){unsigned i=(off-0x100000)/8;uint64_t f=i?0:((uint64_t)(5554176-4096+47)<<32)|1;*value=(uint32_t)(f>>((off&4)?32:0));}
        else if(off>=0x800000 && off<0x803000){unsigned i=(off-0x800000)/8;*value=(uint32_t)((0x90000001ull+4096ull*i)>>((off&4)?32:0));}
        else assert(0);
        mmio_reads++;return true;
    }
    assert(intel_capture_offset_allowed(off));mmio_reads++;*value=0;
    if(off>=0x45400 && off<=0x4540c)*value=0xc0000000;
    if(off==0x70180)*value=backing_mode?0xd8000400:0x98000000;
    if(off==0x70188)*value=6144;
    if(off==0x6001c)*value=(1439u<<16)|899;
    if(off==0x70000)*value=(uint32_t)ticks%926;
    if(off==0x70040)*value=(uint32_t)(ticks/16667);
    return true;
}
static void reset(void) {
    config_reads=mmio_reads=map_calls=unmap_calls=0;ticks=100000;current=0x1000;
    firmware_ok=clock_ok=true;alias=malformed=reporting=backing_mode=translation_gap=false;backing_attempted=false;backing=(struct intel_backing_result){0};
    memset(pa,0,sizeof(pa));result=(struct intel_capture_result){0};attempted=false;
    devices[0]=(struct pci_device){.device=2,.class_code=3};devices[1]=(struct pci_device){.device=31,.class_code=6,.subclass=1};
    dsdt=(struct acpi_table_info){.physical_address=0x100000,.length=35034};memcpy(dsdt.signature,"DSDT",4);
    ram=(struct limine_memmap_entry){.base=0x100000,.length=0x70000000,.type=LIMINE_MEMMAP_USABLE};
    devices[2]=(struct pci_device){.device=0,.class_code=6};
    aperture=(struct limine_memmap_entry){.base=0xb0000000,.length=0x10000000,.type=LIMINE_MEMMAP_FRAMEBUFFER};
    stolen_region=(struct limine_memmap_entry){.base=0x90000000,.length=0x2000000,.type=LIMINE_MEMMAP_RESERVED};
    memory=(struct limine_memmap_response){.entry_count=1,.entries=entries};memmap_request.response=&memory;
    frame=(struct limine_framebuffer){.address=(void *)(uintptr_t)fb_virtual,.width=1440,.height=900,.pitch=6144,.bpp=32,
        .memory_model=LIMINE_FRAMEBUFFER_RGB,.red_mask_size=8,.red_mask_shift=16,.green_mask_size=8,.green_mask_shift=8,.blue_mask_size=8};
}
int main(int argc,char **argv) {
    reset();backing_mode=argc==2 && !strcmp(argv[1],"--backing-report");reporting=argc==2 && (!strcmp(argv[1],"--report") || backing_mode);
    if(backing_mode)memory.entry_count=3;
    intel_display_probe_boot(&frame);
    assert(result.status==INTEL_CAPTURE_OBSERVED && map_calls==9 && unmap_calls==9 && mmio_reads==116);
    assert(intel_display_probe_report());unsigned old_reads=mmio_reads;intel_display_probe_boot(&frame);assert(mmio_reads==old_reads);
    if(backing_mode){intel_display_backing_probe_boot(&frame);assert(backing.status==1 && backing.count==1356 && backing.sample[0].present==1356 && backing.fence_matches==1 && !backing.retained && map_calls==unmap_calls);assert(intel_display_backing_probe_report());unsigned calls=mmio_reads;intel_display_backing_probe_boot(&frame);assert(calls==mmio_reads);return 0;}
    if(reporting)return 0;
    reset();firmware_ok=false;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_IDENTITY && !map_calls && !mmio_reads);
    reset();ram.base=0xc0000000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_ACCESS && !map_calls && !mmio_reads);
    reset();malformed=true;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_ACCESS && !map_calls);
    reset();clock_ok=false;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_CLOCK && !config_reads && !map_calls);
    reset();current=0x2000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_CLOCK && !map_calls);
    reset();pa[3]=0xc0062000;intel_display_probe_boot(&frame);assert(result.status==INTEL_CAPTURE_MAP && map_calls==3 && unmap_calls==3 && !mmio_reads && pa[3]==0xc0062000);
    reset();backing_mode=true;memory.entry_count=2;intel_display_probe_boot(&frame);translation_gap=true;intel_display_backing_probe_boot(&frame);assert(backing.status==2 && map_calls==9 && unmap_calls==9);
    reset();backing_mode=true;memory.entry_count=2;intel_display_probe_boot(&frame);pa[INTEL_CAPTURE_PAGES]=0xc0100000;intel_display_backing_probe_boot(&frame);assert(backing.status!=1 && map_calls==9 && pa[INTEL_CAPTURE_PAGES]);
    puts("intel-display-probe: PASS actual adapter, read-only temporary mappings, existing-map/RAM alias refusal, firmware/PM/clock gates and single attempt");
}
