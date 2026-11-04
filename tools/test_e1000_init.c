#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t registers[0x20000/4];
#define E1000_MMIO_VIRTUAL ((uint64_t)(uintptr_t)registers)
#include "../kernel/src/drivers/e1000.c"
static struct pci_device pci={.vendor_id=E1000_VENDOR_INTEL,.device_id=E1000_DEVICE_82540EM,.bar={0x100000}};
static unsigned allocations,maps,writes,discoveries,registrations;
static bool fail_map,fail_alloc,no_device;
static uint8_t pages[101][4096];
uint64_t pci_device_count(void) { discoveries++;return no_device?0:1; }
const struct pci_device *pci_device_at(uint64_t i) { assert(!i);return &pci; }
uint16_t pci_read_config16(const struct pci_device *d,uint8_t offset) { assert(d==&pci && offset==4);return 0; }
void pci_write_config16(const struct pci_device *d,uint8_t offset,uint16_t value) { assert(d==&pci && offset==4 && value==6);writes++; }
bool vmm_map_page(uint64_t v,uint64_t p,uint64_t f) { (void)v;(void)p;(void)f;maps++;return !fail_map; }
bool vmm_virt_to_phys(uint64_t v,uint64_t *p) { *p=v;return true; }
uint64_t pmm_alloc_frame_tagged(enum pmm_frame_tag tag) {
    assert(tag==PMM_FRAME_DMA);allocations++;assert(allocations<=100);
    return fail_alloc && allocations==3?0:allocations*4096;
}
void *pmm_phys_to_virt(uint64_t p) { assert(p/4096<=100);return pages[p/4096]; }
bool net_device_register(const struct net_device_ops *ops) { assert(ops->send && ops->poll);registrations++;return true; }
void net_notify_rx(void) {}
void net_handle_ethernet_frame(const uint8_t *p,uint16_t n) { (void)p;(void)n; }
void console_write(const char *p) { (void)p; }
void console_putc(char c) { (void)c; }
void console_printf(const char *p,...) { (void)p; }
int main(int argc,char **argv) {
    fail_map=argc>1 && argv[1][0]=='m';fail_alloc=argc>1 && argv[1][0]=='a';
    no_device=argc>1 && argv[1][0]=='n';
    bool success=e1000_init();assert(success==!(fail_map || fail_alloc || no_device));
    fail_map=fail_alloc=no_device=false;
    if(success) { rx_tail=7;tx_tail=9;rx_desc[7].status=1;reg_write(E1000_REG_RDT,11); }
    unsigned a=allocations,m=maps,w=writes,d=discoveries,r=registrations;
    static uint32_t saved_regs[sizeof(registers)/4];memcpy(saved_regs,registers,sizeof(registers));
    static uint8_t saved_pages[sizeof(pages)];memcpy(saved_pages,pages,sizeof(pages));
    assert(e1000_init()==success);
    assert(allocations==a && maps==m && writes==w && discoveries==d && registrations==r);
    assert(!memcmp(saved_regs,registers,sizeof(registers)) && !memcmp(saved_pages,pages,sizeof(pages)));
    if(success)assert(rx_tail==7 && tx_tail==9 && rx_desc[7].status==1);
    puts("e1000-init: PASS repeated success/failure preserves DMA, indices, MMIO, PCI and provider state");
}
