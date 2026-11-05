#include <srvros/haswell_firmware.h>
#include <srvros/haswell_cache_context.h>
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
uint64_t vmm_kernel_address_space(void){return 0x100000;}
uint64_t vmm_current_address_space(void){return mode==3?0x2000:0x100000;}
bool vmm_virt_to_phys_in_space(uint64_t space,uint64_t va,uint64_t *out){(void)space;translated++;*out=info.physical_address+va-(uint64_t)(uintptr_t)bytes;return mode!=4;}
uint64_t pmm_hhdm_offset(void){return 0xffff800000000000ull;}
uint64_t timer_monotonic_us(void){return ticks+=100;}
bool timer_reference_ready(void){return true;}
void console_write(const char *s){fputs(s,stdout);}
/* Kernel formatter consumes uint64 for every numeric argument. */
void console_printf(const char *fmt,...){va_list ap;va_start(ap,fmt);for(const char *p=fmt;*p;p++){if(*p!='%'){putchar(*p);continue;}p++;if(*p=='u')printf("%llu",(unsigned long long)va_arg(ap,uint64_t));else if(*p=='s')fputs(va_arg(ap,const char *),stdout);else assert(0);}va_end(ap);}

static unsigned msrs,ptes;
bool hcc_test_cpu(void *ctx,struct x86_cache_cpu *c){(void)ctx;*c=(struct x86_cache_cpu){.vendor={0x756e6547,0x49656e69,0x6c65746e},.signature=mode==9?0x306d4:0x40651,.features=0x11020,.physical_bits=39};return true;}
bool hcc_test_msr(void *ctx,uint32_t n,uint64_t *v){(void)ctx;msrs++;if(mode==11)return false;*v=n==0xfe?(mode==13?0x510:0x100):n==0x277?0x0007040600070406ull:n==0x2ff?0x806:0;return true;}
uint64_t hcc_test_cr3(void){return 0x100000;}
uint64_t hcc_test_cr4(void){return mode==10?1ull<<12:0;}
bool hcc_test_entry(uint64_t pa,unsigned index,uint64_t *v){
 assert(pa!=0xfed90000);ptes++;
 if(pa==0x100000){if(index==384 && mode==13){*v=0x104003;return true;}if(index==384 && mode!=7){*v=0;return true;}*v=mode==8?0xfed90003:0x101003;}
 else if(pa==0x101000)*v=0x102003;
 else if(pa==0x102000)*v=0x103003;
 else if(pa==0x103000)*v=0xfed90013;
 else if(pa==0x104000)*v=0x105003;
 else if(pa==0x105000)*v=0x106003;
 else if(pa==0x106000)*v=0;
 else return false;
 if(mode==12 && ptes>5)*v^=32;
 return true;
}
int main(int argc,char **argv){
 if(argc>1){FILE *f=fopen(argv[1],"rb");assert(f);assert(fread(bytes,1,136,f)==136 && fgetc(f)==EOF);fclose(f);}
 for(unsigned i=0;i<128;i++){entry[i]=(struct limine_memmap_entry){.base=0x100000+4096*i,.length=4096,.type=2};entries[i]=entry+i;}
 response=(struct limine_memmap_response){.entry_count=128,.entries=entries};memmap_request.response=&response;
 if(argc>2)mode=(unsigned)atoi(argv[2]);
 if(argc==1)mode=1;
 bool ok=haswell_cache_context_run();
 if(argc==2 || mode==13){assert(ok);puts("laptop-capture: completed=yes");}
 else {assert(!ok);puts("laptop-capture: completed=NO");}
 if(mode==1 || mode==9 || mode==10)assert(!msrs && !ptes);
 return 0;
}
