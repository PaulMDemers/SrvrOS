/* MacBookAir7,2 diagnostic binding. No startup callers in the normal kernel. */
#include "battery_platform.h"
#include "battery_native_policy.h"
#include <srvros/acpi.h>
#include <srvros/acpi_io_rule.h>
#include <srvros/console.h>
#include <srvros/laptop_probe.h>
#include <srvros/pci.h>
#include <srvros/pmm.h>
#include <srvros/vmm.h>
#include <uacpi/acpi.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
static struct battery_platform *platform;
static struct limine_memmap_response *map;
static uint64_t igdm;
#define GNVS BATTERY_GNVS
#define PNVS BATTERY_PNVS
#define HPTC_VA 0xffffc001f3200000ull
static bool contains(uint64_t base,uint64_t size,uint64_t a,uint64_t n) {
    return n&&a>=base&&a-base<=size&&n<=size-(a-base);
}
static bool bytes(const void *a,const char *b,size_t n) {
    const uint8_t *p=a;for(size_t i=0;i<n;i++)if(p[i]!=(uint8_t)b[i])return false;return true;
}
static bool nvs(uint64_t a,size_t n) {
    if(!map)return false;
    for(uint64_t i=0;i<map->entry_count;i++) {
        const struct limine_memmap_entry *e=map->entries[i];
        if(e->type==LIMINE_MEMMAP_ACPI_NVS&&contains(e->base,e->length,a,n))return true;
    }
    return false;
}
static const uint8_t *table(const char *name,size_t *n) {
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(bytes(t->signature,name,4)){*n=t->length;return pmm_phys_to_virt(t->physical_address);}
    }
    *n=0;return NULL;
}
static const struct pci_device *device(unsigned dev,unsigned fn,unsigned id) {
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *p=pci_device_at(i);
        if(p->bus==0&&p->device==dev&&p->function==fn&&p->vendor_id==0x8086&&p->device_id==id)return p;
    }
    return NULL;
}
static uint8_t ec_status(void *p){return inb(((struct battery_platform *)p)->ec.command);}
static uint8_t ec_read(void *p){return inb(((struct battery_platform *)p)->ec.data);}
static void ec_command(void *p,uint8_t v){outb(((struct battery_platform *)p)->ec.command,v);}
static void ec_write(void *p,uint8_t v){outb(((struct battery_platform *)p)->ec.data,v);}
static uint64_t fingerprint(const uint8_t *p,size_t n) {
    uint64_t h=14695981039346656037ull;for(size_t i=0;i<n;i++)h=(h^p[i])*1099511628211ull;return h;
}
static bool aml_tables_match(void) {
    /* Exact captured SSDT set, including addresses used by its AML. */
    static const uint64_t hashes[]={
        0x7b1010f6c77d5eb9ull,
        0x3855ae741a19efe5ull,
        0x9a34aad14986eaf5ull,
        0xf7634c379cd72fe5ull,
        0xc010202e36fbc93dull,
        0xec2802855c9ba1f7ull,
        0x196a92da329719e3ull,
        0x2d3515d0b8520937ull,
    };
    unsigned seen=0;
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(!bytes(t->signature,"SSDT",4))continue;
        if(t->length<36||t->length>65536)return false;
        uint64_t h=fingerprint(pmm_phys_to_virt(t->physical_address),t->length);
        unsigned j=0;while(j<8&&hashes[j]!=h)j++;
        if(j==8||(seen&(1u<<j)))return false;seen|=1u<<j;
    }
    return seen==255;
}
bool battery_platform_open(struct battery_platform *p,void *rsdp,struct limine_memmap_response *m) {
    const struct pci_device *gpu=device(2,0,0x1626),*lpc=device(31,0,0x9cc3);
    if(!p||!rsdp||!gpu||!lpc||!device(3,0,0x160c)||!device(31,3,0x9ca2))return false;
    size_t dn,en,fn;const uint8_t *d=table("DSDT",&dn),*e=table("ECDT",&en),*f=table("FACP",&fn);
    if(!laptop_firmware_match(d,dn)||!aml_tables_match()||
       acpi_ec_resources_parse(&p->ec,e,en,f,fn)||acpi_event_resources_parse(&p->events.resources,f,fn))return false;
    struct laptop_resources exact;
    if(!laptop_resources_parse(&exact,e,en,f,fn)||p->ec.sci!=9||p->events.resources.gpe[1].bytes||
       p->events.resources.pm1_status!=0x1800||p->events.resources.pm1_enable!=0x1802||
       p->events.resources.pm2_control!=0x1850||p->events.resources.timer!=0x1808)return false;
    map=m;
    if(!nvs(GNVS,0x282)||!nvs(PNVS,53))return false;
    igdm=*(const uint32_t *)pmm_phys_to_virt(GNVS+0x130);
    if(!igdm||(igdm&3)||igdm!=pci_read_config32(gpu,0xfc)||!nvs(igdm,8192))return false;
    const uint8_t *ig=pmm_phys_to_virt(igdm);
    if(!bytes(ig,"IntelGraphicsMem",16)||*(const uint32_t *)(ig+16)!=8||
       *(const uint32_t *)(ig+20)!=0x02000000||!(*(const uint32_t *)(ig+88)&1))return false;
    const struct acpi_fadt *fa=(const void *)f;
    p->facs=fa->x_firmware_ctrl?fa->x_firmware_ctrl:fa->firmware_ctrl;
    if(!nvs(p->facs,64))return false;
    const struct acpi_facs *facs=pmm_phys_to_virt(p->facs);
    if(!bytes(facs->signature,"FACS",4)||facs->length!=64||(facs->global_lock&3))return false;
    if(pci_read_config32(lpc,0xf0)!=0xfed1c001)return false;
    if(!vmm_map_page(HPTC_VA,0xfed1f000,VMM_PAGE_NO_EXECUTE|VMM_PAGE_CACHE_DISABLE))return false;
    if(!(*(volatile uint32_t *)(HPTC_VA+0x404)&0x80))return false;
    p->rsdp=(uintptr_t)rsdp-pmm_hhdm_offset();p->original_control=inw(p->ec.pm1_control);
    p->events.fixed_enable_mask=1u<<5;
    /* No enabled firmware event user may be silently displaced. Initialization
     * owns the disabled domain and deliberately clears pending status. */
    if(inw(p->events.resources.pm1_enable))return false;
    for(unsigned i=0;i<p->events.resources.gpe[0].bytes;i++)
        if(inb(p->events.resources.gpe[0].enable+i))return false;
    if(inb(p->ec.command)&3)return false; /* No stale output or in-flight command. */
    p->io=(struct ec_io){p,ec_status,ec_read,ec_command,ec_write};platform=p;
    console_printf("battery-native: matched Air7,2 EC=%x/%x SCI=%u GPE=%x IGDM=%x mode=%u; status domain initially disabled\n",
        (uint64_t)p->ec.data,(uint64_t)p->ec.command,(uint64_t)p->ec.sci,(uint64_t)p->ec.gpe,igdm,(uint64_t)(p->original_control&1));
    return true;
}
void *battery_platform_map(uint64_t a,size_t n) {
    if(!platform)return NULL;
    if(contains(platform->rsdp,36,a,n)||contains(platform->facs,64,a,n)||
       contains(GNVS,0x282,a,n)||contains(PNVS,53,a,n)||contains(igdm,8192,a,n))return pmm_phys_to_virt(a);
    /* Mapping is an opaque base: only the HPTC DWORD can actually be read. */
    if(contains(0xfed1c000,0x4000,a,n))return (void *)(HPTC_VA-0x3000+(a-0xfed1c000));
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(contains(t->physical_address,t->length,a,n))return pmm_phys_to_virt(a);
    }
    return NULL;
}
bool battery_platform_memory(void *ptr,unsigned bits,bool write,uint64_t *v) {
    if(!platform||!v||!(bits==8||bits==16||bits==32||bits==64))return false;
    uint64_t a=(uintptr_t)ptr-pmm_hhdm_offset();
    bool allowed=battery_native_memory_allowed(igdm,platform->facs,a,bits,write,*v)||
        (!write&&(uintptr_t)ptr==HPTC_VA+0x404&&bits==32);
    if(!allowed)return false;
#define MEM(B,T) if(bits==B){if(write)*(volatile T *)ptr=*v;else *v=*(volatile T *)ptr;}
    MEM(8,uint8_t) else MEM(16,uint16_t) else MEM(32,uint32_t) else MEM(64,uint64_t)
#undef MEM
    return true;
}
bool battery_platform_io_map(uint64_t a,size_t n) {return platform&&a==0x800&&n==0x400;}
bool battery_platform_io(uint64_t a,unsigned bits,bool write,uint32_t *v) {
    if(!platform||bits!=8||(a!=0xa74&&a!=0xb90))return false;
    struct acpi_io_rule rule={a,8,a==0xa74?4:8,a==0xa74?4:8};
    uint8_t before=inb(a);
    if(write){if(!acpi_io_rule_write(&rule,a,bits,before,*v))return false;outb(a,*v);}else *v=before;
    return true;
}
bool battery_platform_pci_open(uacpi_pci_address a,void **out) {
    if(!platform||a.segment||a.bus)return false;
    const struct pci_device *p=NULL;
    if(a.device==31&&a.function==0)p=device(31,0,0x9cc3);
    if(a.device==31&&a.function==3)p=device(31,3,0x9ca2);
    if(a.device==3&&a.function==0)p=device(3,0,0x160c);
    if(!p)return false;*out=(void *)p;return true;
}
bool battery_platform_pci_read(void *h,size_t o,unsigned bits,uint32_t *v) {
    const struct pci_device *p=h;if(!platform||!p)return false;
    if(bits==8&&((p==device(3,0,0x160c)&&o<=1)||
       (p==device(31,0,0x9cc3)&&((o>=0x60&&o<=0x63)||(o>=0x68&&o<=0x6b))))) {*v=pci_read_config8(p,o);return true;}
    if(bits==32&&p==device(31,3,0x9ca2)&&o==0x20){*v=pci_read_config32(p,o);return true;}
    return false;
}
