#include <srvros/laptop_probe.h>
#include <srvros/acpi.h>
#include <srvros/console.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
static const uint8_t *find(const char *sig,size_t *size) {
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t->signature[0]==sig[0] && t->signature[1]==sig[1] && t->signature[2]==sig[2] && t->signature[3]==sig[3]) {
            *size=t->length;return pmm_phys_to_virt(t->physical_address);
        }
    }
    *size=0;return NULL;
}
static uint8_t read8(uint16_t port) { uint8_t n;__asm__ volatile("inb %1,%0":"=a"(n):"Nd"(port));return n; }
static uint16_t read16(uint16_t port) { uint16_t n;__asm__ volatile("inw %1,%0":"=a"(n):"Nd"(port));return n; }
void laptop_probe(void) {
    const struct pci_device *display=NULL;bool chipset=false;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p->vendor_id==0x8086 && p->device_id==0x1626 && p->class_code==3) display=p;
        if(p->vendor_id==0x8086 && p->device_id==0x9cc3 && p->class_code==6) chipset=true;
    }
    if(!display || !chipset) { console_write("laptop-probe: unsupported chipset; no register access\n");return; }
    size_t dn,en,fn;const uint8_t *dsdt=find("DSDT",&dn),*ecdt=find("ECDT",&en),*fadt=find("FACP",&fn);
    struct laptop_resources r;
    if(!laptop_firmware_match(dsdt,dn) || !laptop_resources_parse(&r,ecdt,en,fadt,fn)) {
        console_write("laptop-probe: firmware/resource identity mismatch; no register access\n");return;
    }
    uint16_t pm=read16(r.pm1_control);uint8_t ec=read8(r.status),gpe=read8(r.gpe_enable);
    console_printf("laptop-probe: read-only firmware=MacBookAir7,2 pm1_cnt=%x sci_enabled=%u ec_status=%x ec_gpe_enabled=%u\n",
        (uint64_t)pm,(uint64_t)(pm&1),(uint64_t)ec,(uint64_t)((gpe&r.gpe_mask)!=0));
    console_write("laptop-probe: no EC data/command access, ACPI mode changes, queries or PWM writes\n");
    uint32_t bar=display->bar[0];uint64_t base=bar&0xfffffff0u;
    if((bar&7)==4) base|=(uint64_t)display->bar[1]<<32;
    else if(bar&7) return;
    if(!base || base>0x000ffffffff00000ull || !(pci_read_config16(display,4)&2)) return;
    const uint64_t va=0xffffc001f3100000ull;
    if(!vmm_map_page(va,base+0x48000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE) ||
       !vmm_map_page(va+4096,base+0xc8000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
        console_write("laptop-probe: backlight mapping unavailable\n");return;
    }
    uint32_t cpu=*(volatile uint32_t *)(va+0x250),cd=*(volatile uint32_t *)(va+0x254);
    uint32_t pch=*(volatile uint32_t *)(va+4096+0x250),pd=*(volatile uint32_t *)(va+4096+0x254);
    struct laptop_backlight b=laptop_backlight_decode(cpu,cd,pch,pd);
    console_printf("laptop-probe: backlight cpu_ctl=%x cpu_duty=%x pch_ctl=%x pch_duty=%x mode=%u duty=%u period=%u\n",
        (uint64_t)cpu,(uint64_t)cd,(uint64_t)pch,(uint64_t)pd,(uint64_t)b.mode,(uint64_t)b.duty,(uint64_t)b.period);
}
