#define SRVROS_IOAPIC_HOST_TEST
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/arch/x86_64/ioapic.c"
static uint32_t registers[256];
static struct { uint8_t reg;uint32_t value; } writes[256];
static unsigned written;
static bool ignore_unmask;
static struct acpi_ioapic_info info={.present=true,.address=0xfec00000};
static struct acpi_interrupt_source_override override;
uint32_t ioapic_test_read(uint8_t reg) { return registers[reg]; }
void ioapic_test_write(uint8_t reg,uint32_t value) {
    assert(written<256);writes[written].reg=reg;writes[written++].value=value;
    if(ignore_unmask && reg>=IOAPIC_REDTBL_BASE && !(reg&1))value|=IOAPIC_REDIR_MASKED;
    registers[reg]=value;
}
const struct acpi_ioapic_info *acpi_ioapic(void) {return &info;}
const struct acpi_interrupt_source_override *acpi_irq_override(uint8_t irq) {return override.present && override.source==irq?&override:NULL;}
bool vmm_map_page(uint64_t v,uint64_t p,uint64_t flags) {assert(v==IOAPIC_VIRTUAL_BASE && p==info.address && (flags&VMM_PAGE_CACHE_DISABLE));return true;}
void console_write(const char *text) {(void)text;}
void console_printf(const char *text,...) {(void)text;}
int main(void) {
    uint32_t low,high;assert(!ioapic_read_gsi(9,&low,&high));
    registers[IOAPIC_VER]=23<<16;assert(ioapic_init());
    for(unsigned i=0;i<24;i++)assert(registers[IOAPIC_REDTBL_BASE+2*i]==IOAPIC_REDIR_MASKED);
    written=0;assert(!ioapic_mask_gsi(9,false) && !written); /* No vector yet. */
    assert(!ioapic_prepare_gsi(24,41,true,true));assert(!ioapic_prepare_gsi(9,128,true,true));
    assert(ioapic_prepare_gsi(9,41,true,true));
    assert(written==3 && writes[0].value&IOAPIC_REDIR_MASKED && writes[1].reg==0x23 && writes[2].reg==0x22);
    assert(ioapic_read_gsi(9,&low,&high) && high==0 && low==(41|IOAPIC_REDIR_MASKED|IOAPIC_REDIR_ACTIVE_LOW|IOAPIC_REDIR_TRIGGER_LEVEL));
    assert(ioapic_mask_gsi(9,false));written=0;
    assert(!ioapic_prepare_gsi(9,42,false,false) && !written); /* Cannot steal live route. */
    assert(ioapic_mask_gsi(9,true));
    registers[0x22]|=1u<<14;written=0;assert(!ioapic_prepare_gsi(9,42,false,false) && !written);registers[0x22]&=~(1u<<14);
    ignore_unmask=true;assert(!ioapic_mask_gsi(9,false) && registers[0x22]&IOAPIC_REDIR_MASKED);ignore_unmask=false;
    assert(ioapic_prepare_gsi(9,41,false,false));assert(ioapic_read_gsi(9,&low,&high) && low==(41|IOAPIC_REDIR_MASKED));
    written=0;assert(ioapic_route_isa_irq(1,33));assert(registers[0x12]==33 && written==3 && writes[0].value&IOAPIC_REDIR_MASKED);
    override=(struct acpi_interrupt_source_override){.present=true,.source=9,.gsi=20,.flags=15};
    assert(ioapic_route_isa_irq(9,41));assert(registers[0x38]==(41|IOAPIC_REDIR_ACTIVE_LOW|IOAPIC_REDIR_TRIGGER_LEVEL));
    assert(ioapic_route_pci_irq(11,43));assert(registers[0x26]==(43|IOAPIC_REDIR_ACTIVE_LOW|IOAPIC_REDIR_TRIGGER_LEVEL));
    assert(!ioapic_route_pci_irq(16,48));
    assert(ioapic_mask_gsi(9,true));
    assert(!ioapic_restore_masked_gsi(9,41,0));
    assert(!ioapic_restore_masked_gsi(9,IOAPIC_REDIR_MASKED|(1u<<14),0));
    assert(!ioapic_restore_masked_gsi(9,IOAPIC_REDIR_MASKED,1));
    assert(ioapic_restore_masked_gsi(9,IOAPIC_REDIR_MASKED,0));
    assert(registers[0x22]==IOAPIC_REDIR_MASKED && registers[0x23]==0);
    assert(!ioapic_mask_gsi(9,false));
    info.gsi_base=32;written=0;assert(ioapic_init());assert(!ioapic_prepare_gsi(9,41,true,true));assert(ioapic_prepare_gsi(41,41,true,true));
    registers[IOAPIC_VER]=255<<16;assert(!ioapic_init());assert(!ioapic_mask_gsi(41,false));
    puts("ioapic: PASS masked prepare/order, explicit polarity/trigger, live/pending-route refusal, register bounds, readback failure and legacy routing");
}
