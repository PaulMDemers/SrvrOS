#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_LAPTOP_PROBE_HOST_TEST
#include "../kernel/src/drivers/laptop_probe.c"
static bool match=true;
static unsigned reads;
static bool complete,refused;
static struct pci_device devices[]={
 {.vendor_id=0x8086,.device_id=0x0a26,.device=2,.class_code=3},
 {.vendor_id=0x8086,.device_id=0x9c43,.device=31,.class_code=6,.subclass=1}};
static struct acpi_table_info tables[]={
 {.signature="DSDT",.physical_address=1,.length=31777},
 {.signature="ECDT",.physical_address=2,.length=83},
 {.signature="FACP",.physical_address=3,.length=244}};
uint64_t pci_device_count(void){return 2;}
const struct pci_device *pci_device_at(uint64_t i){assert(i<2);return devices+i;}
uint64_t acpi_table_count(void){return 3;}
const struct acpi_table_info *acpi_table_at(uint64_t i){assert(i<3);return tables+i;}
void *pmm_phys_to_virt(uint64_t a){assert(a>=1&&a<=3);return (void *)(uintptr_t)a;}
bool laptop_air61_power_resources(uint16_t gpu,uint16_t pch,const uint8_t *d,size_t dn,const uint8_t *e,size_t en,const uint8_t *f,size_t fn){
 assert((uintptr_t)d==1&&dn==31777&&(uintptr_t)e==2&&en==83&&(uintptr_t)f==3&&fn==244);
 return match&&gpu==0x0a26&&pch==0x9c43;
}
static uint8_t read8(uint16_t port){
 assert(port==0x66||(port>=0x1880&&port<0x18a0));reads++;return 0;
}
static uint16_t read16(uint16_t port){assert(port==0x1800||port==0x1802||port==0x1804);reads++;return 0;}
void console_write(const char *s){if(strstr(s,"complete reads=36 writes=0"))complete=true;if(strstr(s,"REFUSED"))refused=true;}
void console_printf(const char *fmt,...){(void)fmt;}
int main(void){
 match=false;laptop_battery_preflight();assert(refused&&!reads&&!complete);
 match=true;refused=false;laptop_battery_preflight();assert(reads==36&&complete&&!refused);
 reads=0;complete=false;devices[1].bus=1;laptop_battery_preflight();assert(refused&&!reads&&!complete);
 puts("battery-preflight: PASS refused identities perform zero reads; admitted capture reads only passive ports");
}
