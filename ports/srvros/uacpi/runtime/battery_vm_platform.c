/* Disposable Q35 model. Never linked into the physical diagnostic image. */
#include "battery_platform.h"
#include "q35_fixture.h"
#include <srvros/acpi.h>
#include <srvros/pmm.h>
#include <srvros/console.h>
#include <uacpi/tables.h>
#include "../../../../kernel/src/arch/x86_64/io.h"
static struct battery_platform *platform;
static uint64_t dsdt;
static struct {
    uint8_t memory[256], mode,address,output,foreign_status[2],foreign_enable[2];
    uint16_t words[32];
    unsigned step,busy,fault,foreign_reads[2];
    bool obf,pending;
} model;
static bool contains(uint64_t a,uint64_t n,uint64_t p,uint64_t s){return s&&p>=a&&p-a<=n&&s<=n-(p-a);}
static bool equal(const void *a,const char *b,unsigned n){const char *p=a;for(unsigned i=0;i<n;i++)if(p[i]!=b[i])return false;return true;}
static uint8_t status(void *c){(void)c;return (model.fault==2?2:0)|(model.obf?1:0)|(model.pending?32:0);}
static uint8_t read_data(void *c){(void)c;model.obf=false;return model.output;}
static void command(void *c,uint8_t v){(void)c;model.mode=v;model.step=0;if(v==0x84){model.output=model.pending?0x20:0;model.pending=model.fault==3;model.obf=true;}}
static void write_data(void *c,uint8_t v){
    (void)c;
    if(!model.step++) {
        model.address=v;
        if(model.mode==0x80){if(v==0x20&&model.busy&&!--model.busy)model.memory[0x20]=0;model.output=model.memory[v];model.obf=true;}
        return;
    }
    model.memory[model.address]=v;
    if(model.address==0x20&&v==9&&model.memory[0x22]==0x16&&model.memory[0x23]<32){
        uint16_t word=model.words[model.memory[0x23]];model.memory[0x24]=word;model.memory[0x25]=word>>8;model.busy=2;
    }
}
static uacpi_table_installation_disposition install(struct acpi_sdt_hdr *h,uacpi_u64 *override){
    if(!equal(h->signature,"DSDT",4))return UACPI_TABLE_INSTALLATION_DISPOSITION_ALLOW;
    if(equal(h->oem_table_id,"BATTEST ",8))return UACPI_TABLE_INSTALLATION_DISPOSITION_DENY;
    *override=dsdt;return UACPI_TABLE_INSTALLATION_DISPOSITION_PHYSICAL_OVERRIDE;
}
/* Two shadow GPE bytes exercise the interpreter's W1C requests and partial
 * lease/restore failures. They are not physical graphics/alarm sources. */
static unsigned foreign_index(uint16_t port){return port==0x626||port==0x62e?1:0;}
static uint8_t foreign_read(void *ctx,uint16_t port) {
    (void)ctx;unsigned i=foreign_index(port);
    if(port>=0x628&&model.fault==10&&i==0&&++model.foreign_reads[i]==4)model.foreign_enable[i]=0x40;
    return port<0x628?model.foreign_status[i]:model.foreign_enable[i];
}
static void foreign_write(void *ctx,uint16_t port,uint8_t value) {
    (void)ctx;unsigned i=foreign_index(port);uint8_t mask=i?4:0x40;
    if(port<0x628) {
        if(value&mask)console_write("battery-vm: ERROR foreign status acknowledged\n");
        model.foreign_status[i]&=~value;
    } else {
        if(model.fault==9&&i==1&&value==0)return;
        if(model.fault==8&&i==1&&value==mask)return;
        model.foreign_enable[i]=value;
        if(value==mask)console_printf("battery-vm: foreign restored enable=%x pending=%x\n",(uint64_t)port,(uint64_t)model.foreign_status[i]);
    }
}
static void model_mode_entered(void) {
    uint16_t value=model.fault==11?1u<<8:1u<<9;
    outw(0x602,value);
    console_printf("battery-vm: injected post-mode fixed-enable=%x\n",(uint64_t)inw(0x602));
}
bool battery_platform_open(struct battery_platform *p,void *rsdp,struct limine_memmap_response *map){
    (void)map;dsdt=0;platform=NULL;model=(typeof(model)){0};
    const struct acpi_fadt *f=acpi_q35_fixture_fadt();if(!f||!p||!rsdp)return false;
    for(uint64_t i=0;i<acpi_table_count();i++){
        const struct acpi_table_info *t=acpi_table_at(i);const struct acpi_sdt_hdr *h=pmm_phys_to_virt(t->physical_address);
        if(equal(t->signature,"DSDT",4)&&equal(h->oem_table_id,"BATTEST ",8)){if(dsdt)return false;dsdt=t->physical_address;model.fault=h->oem_revision;}
    }
    if(!dsdt||model.fault>12||acpi_event_resources_parse(&p->events.resources,(const uint8_t *)f,f->hdr.length)||inw(0x602))return false;
    for(unsigned port=0x628;port<0x630;port++)if(inb(port))return false;
    p->ec=(struct acpi_ec_resources){.data=0x62,.command=0x66,.sci=9,.smi_command=0xb2,.pm1_control=0x604,
        .gpe_status=0x620,.gpe_enable=0x628,.gpe=1,.gpe_mask=2,.acpi_enable=f->acpi_enable,.acpi_disable=f->acpi_disable,.path="\\_SB.EC"};
    /* Reproduce a real legacy-to-ACPI transition on disposable Q35. Do not
     * fake original_control: retirement must restore the actual starting mode. */
    if(model.fault==11||model.fault==12) {
        outb(f->smi_cmd,f->acpi_disable);
        if(inw(0x604)&1)return false;
        p->model_mode_entered=model_mode_entered;
    }
    p->events.fixed_enable_mask=1u<<5;p->original_control=inw(0x604);
    p->rsdp=(uintptr_t)rsdp-pmm_hhdm_offset();p->facs=f->firmware_ctrl;
    p->io=(struct ec_io){NULL,status,read_data,command,write_data};
    model.memory[0x60]=3;model.pending=true;
    model.words[0x1a]=0x31;model.words[9]=12000;model.words[10]=(uint16_t)-250;
    model.words[13]=86;model.words[15]=4300;model.words[16]=5000;model.words[24]=6000;
    model.words[8]=2981;model.words[18]=120;model.words[19]=65535;model.words[22]=0xc0;
    if(uacpi_set_table_installation_handler(install))return false;
    if(model.fault>=7&&model.fault<=10) {
        model.foreign_status[0]=model.foreign_enable[0]=0x40;
        model.foreign_status[1]=model.foreign_enable[1]=4;
        p->foreign_events[0]=(struct battery_event_hold){.status=0x627,.enable=0x62f,.mask=0x40,.original=0x40};
        p->foreign_events[1]=(struct battery_event_hold){.status=0x626,.enable=0x62e,.mask=4,.original=4};
        p->foreign_io=(struct battery_event_ops){.read=foreign_read,.write=foreign_write};
    }
    platform=p;console_printf("battery-vm: model fault=%u; no physical EC\n",(uint64_t)model.fault);return true;
}
void *battery_platform_map(uint64_t a,size_t n){
    if(!platform)return NULL;
    if(contains(platform->rsdp,36,a,n)||contains(platform->facs,64,a,n))return pmm_phys_to_virt(a);
    for(uint64_t i=0;i<acpi_table_count();i++){const struct acpi_table_info *t=acpi_table_at(i);if(contains(t->physical_address,t->length,a,n))return pmm_phys_to_virt(a);}
    return NULL;
}
bool battery_platform_memory(void *p,unsigned bits,bool write,uint64_t *v){
    if(!platform||write||bits!=32||(uintptr_t)p-pmm_hhdm_offset()!=platform->facs+16)return false;
    *v=*(volatile uint32_t *)p;return true;
}
bool battery_platform_io_map(uint64_t a,size_t n){(void)a;(void)n;return false;}
bool battery_platform_io(uint64_t a,unsigned b,bool w,uint32_t *v){(void)a;(void)b;(void)w;(void)v;return false;}
bool battery_platform_pci_open(uacpi_pci_address a,void **h){(void)a;(void)h;return false;}
bool battery_platform_pci_read(void *h,size_t o,unsigned b,uint32_t *v){(void)h;(void)o;(void)b;(void)v;return false;}
