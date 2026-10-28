#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_X86_64_IO_H
#define SRVROS_PCI_HOST_TEST
static uint8_t config[256];static unsigned selected,width,writes,delays;static int stuck;
static uint32_t get(unsigned off,unsigned n) { uint32_t v=0;memcpy(&v,config+off,n);return v; }
static void put(unsigned off,unsigned n,uint32_t v) { memcpy(config+off,&v,n); }
static void outl(uint16_t port,uint32_t value) {
    if(port==0xcf8) { selected=value&252;return; }
    assert(port==0xcfc);width=4;writes++;
    if(selected==4) value=(value&65535)|((get(6,2)&~(value>>16))<<16);
    put(selected,4,value);
}
static uint32_t inl(uint16_t port) { assert(port==0xcfc);return get(selected,4); }
static void outw(uint16_t port,uint16_t value) {
    assert(port==0xcfc||port==0xcfe);unsigned offset=selected+port-0xcfc;width=2;writes++;
    if(offset==0x54) {
        uint16_t old=get(offset,2);value=(value&0x7fff)|(old&0x8000&~value);
        if(stuck) value|=3;
    }
    put(offset,2,value);
}
static void outb(uint16_t port,uint8_t value) {
    assert(port>=0xcfc&&port<=0xcff);width=1;writes++;put(selected+port-0xcfc,1,value);
}
#include "../kernel/src/arch/x86_64/pci.c"
static struct srv_acpi_mcfg_allocation mcfg;static unsigned mcfg_count;
uint64_t acpi_mcfg_allocation_count(void) { return mcfg_count; }
const struct srv_acpi_mcfg_allocation *acpi_mcfg_allocation_at(uint64_t i) { return i<mcfg_count?&mcfg:NULL; }
bool timer_delay_us(uint64_t us) { assert(us==10000);delays++;return true; }
int main(void) {
    struct pci_device d={0};
    put(4,4,0xf9000000);pci_write_config16(&d,4,6);assert(width==2&&get(4,2)==6&&get(6,2)==0xf900);
    pci_write_config16(&d,0x52,0xbeef);assert(width==2&&get(0x52,2)==0xbeef);
    pci_write_config8(&d,0x53,0x12);assert(width==1&&get(0x52,2)==0x12ef&&pci_read_config8(&d,0x53)==0x12);
    memset(config,0,sizeof(config));assert(pci_power_d0(&d));
    put(6,2,0x10);put(0x34,1,0x40);put(0x40,2,0x5005);put(0x50,2,1);put(0x52,2,3);put(0x54,2,0x8103);
    unsigned before=writes;assert(pci_power_d0(&d)&&writes==before+1&&delays==1);
    assert(get(0x54,2)==0x8100&&width==2); /* PME state and enable survived. */
    assert(pci_power_d0(&d)&&writes==before+1); /* Already D0, no write. */
    put(0x54,2,3);stuck=1;assert(!pci_power_d0(&d));stuck=0;
    put(0x40,2,0x4005);before=writes;assert(!pci_power_d0(&d)&&writes==before); /* Cycle. */
    put(0x40,2,0x4105);assert(!pci_power_d0(&d)); /* Misaligned link. */
    put(0x40,2,0x2005);assert(!pci_power_d0(&d)); /* Header alias. */
    put(0x40,2,0xfc05);put(0xfc,2,1);assert(!pci_power_d0(&d)); /* Truncated PM. */
    put(0x40,2,0x5005);put(0x52,2,4);assert(!pci_power_d0(&d)); /* Unknown version. */
    uint32_t value=99;before=writes;
    assert(!pci_read_extended_config32(&d,0x100,&value) && value==99 && writes==before);
    static uint32_t ext[1024];ext[0x100/4]=0x12345678;ext[0xffc/4]=0xabcdef01;
    ecam_available=true;mcfg_count=1;mcfg.present=true;ecam_virtual_base[0]=(uintptr_t)ext;
    assert(pci_read_extended_config32(&d,0x100,&value) && value==0x12345678);
    assert(pci_read_extended_config32(&d,0xffc,&value) && value==0xabcdef01);
    assert(!pci_read_extended_config32(&d,0xfc,&value));assert(!pci_read_extended_config32(&d,0x101,&value));assert(!pci_read_extended_config32(&d,0x1000,&value));
    mcfg.pci_segment=1;assert(!pci_read_extended_config32(&d,0x100,&value));mcfg.pci_segment=0;
    d.bus=1;assert(!pci_read_extended_config32(&d,0x100,&value));d.bus=0;
    ecam_virtual_base[1]=(uintptr_t)ext;mcfg_count=2;assert(!pci_read_extended_config32(&d,0x100,&value));
    assert(writes==before);
    puts("pci: narrow writes preserve adjacent W1C state; D0 transition, stuck power and malformed capability lists passed");
}
