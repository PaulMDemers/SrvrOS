#include <srvros/haswell_firmware.h>
#include <srvros/acpi.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <srvros/timer.h>
#include <srvros/console.h>
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>
static uint8_t bytes[136];static uint64_t ticks;static unsigned mode,translated,table_lookups;
static struct pci_device devices[3]={{.device=2},{.device=0},{.device=31}};
static struct acpi_table_info info={.signature="DMAR",.physical_address=0x100100,.length=136,.revision=1};
static struct limine_memmap_entry entry[128];static struct limine_memmap_entry *entries[128];
static struct limine_memmap_response response;
volatile struct limine_memmap_request memmap_request;
uint64_t pci_device_count(void){return 3;}
const struct pci_device *pci_device_at(uint64_t i){return i<3?devices+i:NULL;}
uint32_t pci_read_config32(const struct pci_device *d,uint8_t off){if(mode==1)return 0;return d->device==2?(off==0?0x0a268086:off==8?0x03000009:0x011a106b):d->device==0?0x0a048086:0x9c438086;}
uint64_t acpi_table_count(void){table_lookups++;return mode==2?2:1;}
const struct acpi_table_info *acpi_table_at(uint64_t i){(void)i;return &info;}
void *pmm_phys_to_virt(uint64_t pa){return pa==info.physical_address?bytes:NULL;}
uint64_t vmm_kernel_address_space(void){return 0x1000;}
uint64_t vmm_current_address_space(void){return mode==3?0x2000:0x1000;}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t va,uint64_t *out){(void)space;translated++;*out=info.physical_address+va-(uint64_t)(uintptr_t)bytes;return mode!=4;}
uint64_t pmm_hhdm_offset(void){return 0xffff800000000000ull;}
uint64_t timer_monotonic_us(void){return ticks+=100;}
bool timer_reference_ready(void){return true;}
void console_write(const char *s){fputs(s,stdout);}
/* Kernel formatter consumes uint64 for every numeric argument. */
void console_printf(const char *fmt,...){va_list ap;va_start(ap,fmt);for(const char *p=fmt;*p;p++){if(*p!='%'){putchar(*p);continue;}p++;if(*p=='u')printf("%llu",(unsigned long long)va_arg(ap,uint64_t));else if(*p=='s')fputs(va_arg(ap,const char *),stdout);else assert(0);}va_end(ap);}
int main(int argc,char **argv){
 bool native=argc>1;if(native){FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(bytes,1,136,f)==136 && fgetc(f)==EOF);fclose(f);}
 for(unsigned i=0;i<128;i++){entry[i]=(struct limine_memmap_entry){.base=0x100000+4096*i,.length=4096,.type=2};entries[i]=entry+i;}
 response=(struct limine_memmap_response){.entry_count=128,.entries=entries};memmap_request.response=&response;
 if(native){assert(haswell_firmware_run());puts("laptop-capture: completed=yes");return 0;}
 mode=1;assert(!haswell_firmware_run() && !translated && !table_lookups);
 mode=2;assert(!haswell_firmware_run() && !translated);
 mode=3;assert(!haswell_firmware_run() && !translated);
 mode=4;assert(!haswell_firmware_run() && translated==1);
 mode=0;response.entry_count=129;assert(!haswell_firmware_run());
 puts("haswell-firmware adapter refusal PASS");return 0;
}

bool vmm_diagnostic_sole_kernel_root(uint64_t expected){(void)expected;return false;}
