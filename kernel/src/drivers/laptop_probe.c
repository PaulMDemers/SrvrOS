#include <srvros/laptop_probe.h>
#include <srvros/backlight.h>
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
static uint32_t pwm_read(void *context,unsigned reg) {
    uintptr_t va=(uintptr_t)context;
    return *(volatile uint32_t *)(va+(reg==0?0x250:4096+(reg==1?0x250:0x254)));
}
static void pwm_write(void *context,uint32_t value) {
    *(volatile uint32_t *)((uintptr_t)context+4096+0x254)=value;
}
static bool pro_display_probe(bool brightness) {
    const struct pci_device *display=NULL;uint16_t pch=0;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p->vendor_id!=0x8086) continue;
        if(p->device_id==0x0d26 && p->class_code==3) display=p;
        if(p->device_id==0x8c4b && p->class_code==6 && p->subclass==1) pch=p->device_id;
    }
    if(!display) return false;
    size_t n;const uint8_t *d=find("DSDT",&n);
    if(!laptop_pro_display_match(display->device_id,pch,d,n)) {
        console_write("pro-backlight: identity mismatch; no register access\n");return true;
    }
    uint32_t bar=display->bar[0];uint64_t base=bar&0xfffffff0u;
    if((bar&7)==4) base|=(uint64_t)display->bar[1]<<32;
    else if(bar&7) return true;
    if(!base || base>0x000ffffffff00000ull || !(pci_read_config16(display,4)&2)) return true;
    const uint64_t va=0xffffc001f3100000ull;
    if(!vmm_map_page(va,base+0x48000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE) ||
       !vmm_map_page(va+4096,base+0xc8000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|(brightness?VMM_PAGE_WRITABLE:0))) {
        console_write("pro-backlight: mapping unavailable\n");return true;
    }
    uint32_t cpu=*(volatile uint32_t *)(va+0x250),cd=*(volatile uint32_t *)(va+0x254);
    uint32_t pchctl=*(volatile uint32_t *)(va+4096+0x250),pd=*(volatile uint32_t *)(va+4096+0x254);
    struct laptop_backlight b=laptop_backlight_decode(cpu,cd,pchctl,pd);
    console_printf("pro-backlight: read-only cpu_ctl=%x cpu_duty=%x pch_ctl=%x pch_duty=%x mode=%u duty=%u period=%u\n",
        (uint64_t)cpu,(uint64_t)cd,(uint64_t)pchctl,(uint64_t)pd,(uint64_t)b.mode,(uint64_t)b.duty,(uint64_t)b.period);
    if(brightness) {
        bool ready=backlight_bind_profile((struct backlight_io){(void *)(uintptr_t)va,pwm_read,pwm_write},BACKLIGHT_PRO11_2);
        console_printf("backlight: Pro11,2 provider=%s initial_percent=%u; panel unchanged\n",ready?"ready":"unavailable",ready?(uint64_t)backlight_percent(0):0);
    } else console_write("pro-backlight: no PWM writes, provider binding or EC access\n");
    return true;
}
static bool air61_display_probe(bool brightness) {
    const struct pci_device *display=NULL;uint16_t pch=0;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p->vendor_id!=0x8086) continue;
        if(p->device_id==0x0a26 && p->class_code==3) display=p;
        if(p->device_id==0x9c43 && p->class_code==6 && p->subclass==1) pch=p->device_id;
    }
    if(!display) return false;
    if(brightness) { console_write("air61-backlight: provider not admitted; no register access\n");return true; }
    size_t n;const uint8_t *d=find("DSDT",&n);
    if(!laptop_air61_display_match(display->device_id,pch,d,n)) {
        console_write("air61-backlight: identity mismatch; no register access\n");return true;
    }
    uint32_t bar=display->bar[0];uint64_t base=bar&0xfffffff0u;
    if((bar&7)==4) base|=(uint64_t)display->bar[1]<<32;
    else if(bar&7) return true;
    if(!base || base>0x000ffffffff00000ull || !(pci_read_config16(display,4)&2)) return true;
    const uint64_t va=0xffffc001f3100000ull;
    if(!vmm_map_page(va,base+0x48000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE) ||
       !vmm_map_page(va+4096,base+0xc8000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE)) {
        console_write("air61-backlight: mapping unavailable\n");return true;
    }
    uint32_t cpu=*(volatile uint32_t *)(va+0x250),cd=*(volatile uint32_t *)(va+0x254);
    uint32_t pchctl=*(volatile uint32_t *)(va+4096+0x250),pd=*(volatile uint32_t *)(va+4096+0x254);
    struct laptop_backlight b=laptop_backlight_decode(cpu,cd,pchctl,pd);
    console_printf("air61-backlight: read-only cpu_ctl=%x cpu_duty=%x pch_ctl=%x pch_duty=%x mode=%u duty=%u period=%u\n",
        (uint64_t)cpu,(uint64_t)cd,(uint64_t)pchctl,(uint64_t)pd,(uint64_t)b.mode,(uint64_t)b.duty,(uint64_t)b.period);
    console_write("air61-backlight: no PWM writes, provider binding or EC access\n");
    return true;
}
static void inspect(bool brightness) {
    if(air61_display_probe(brightness)) return;
    if(pro_display_probe(brightness)) return;
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
       !vmm_map_page(va+4096,base+0xc8000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE|(brightness?VMM_PAGE_WRITABLE:0))) {
        console_write("laptop-probe: backlight mapping unavailable\n");return;
    }
    uint32_t cpu=*(volatile uint32_t *)(va+0x250),cd=*(volatile uint32_t *)(va+0x254);
    uint32_t pch=*(volatile uint32_t *)(va+4096+0x250),pd=*(volatile uint32_t *)(va+4096+0x254);
    struct laptop_backlight b=laptop_backlight_decode(cpu,cd,pch,pd);
    console_printf("laptop-probe: backlight cpu_ctl=%x cpu_duty=%x pch_ctl=%x pch_duty=%x mode=%u duty=%u period=%u\n",
        (uint64_t)cpu,(uint64_t)cd,(uint64_t)pch,(uint64_t)pd,(uint64_t)b.mode,(uint64_t)b.duty,(uint64_t)b.period);
    if(brightness) {
        bool ready=backlight_bind((struct backlight_io){(void *)(uintptr_t)va,pwm_read,pwm_write});
        console_printf("backlight: provider=%s initial_percent=%u; panel unchanged\n",ready?"ready":"unavailable",ready?(uint64_t)backlight_percent(0):0);
    }
}

void laptop_probe(void) { inspect(false); }
void laptop_brightness_init(void) { inspect(true); }
