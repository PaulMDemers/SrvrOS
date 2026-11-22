#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
static uint32_t registers[0x20000/4];
#define SRVROS_E1000_PROBE_HOST_TEST
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
static bool selected=true;static unsigned retires;static uint64_t probe_irq=512;
uint64_t e1000_probe_test_irq_save(void){uint64_t f=probe_irq;probe_irq=0;return f;}
void e1000_probe_test_irq_restore(uint64_t f){probe_irq=f;}
bool net_device_retire(const struct net_device_ops *id){assert(!probe_irq && id && !id->context && id->send==device_send);retires++;selected=false;return true;}
bool net_device_selected_is(const struct net_device_ops *id){assert(id && !id->context && id->send==device_send);return selected;}
int main(int argc,char **argv) {
    struct e1000_probe_info absent={0};assert(!e1000_probe_sample(NULL) && !e1000_probe_sample(&absent) && !e1000_probe_retire_provider() && !retires && probe_irq==512);
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
    if(success){
        struct e1000_probe_info sample={0};unsigned saved_maps=maps,saved_writes=writes,saved_allocs=allocations;
        assert(e1000_probe_sample(&sample) && sample.selected && sample.rx_ready && !sample.tx_pending && !sample.rx_dispatching && sample.rx_ring==(uint64_t)(uintptr_t)rx_desc && sample.tx_ring==(uint64_t)(uintptr_t)tx_desc);
        selected=false;assert(e1000_probe_sample(&sample) && !sample.selected);selected=true;
        tx_desc[0].status=0;assert(e1000_probe_sample(&sample) && sample.tx_pending==1);tx_desc[0].status=1;
        rx_poll_active=true;assert(e1000_probe_sample(&sample) && sample.rx_dispatching);rx_poll_active=false;
        rx_desc[7].status=0;assert(e1000_probe_sample(&sample) && !sample.rx_ready);rx_desc[7].status=1;
        assert(maps==saved_maps && writes==saved_writes && allocations==saved_allocs);
        assert(!memcmp(saved_regs,registers,sizeof(registers)) && !memcmp(saved_pages,pages,sizeof(pages)));
        selected=false;assert(!e1000_probe_retire_provider() && !probe_provider_closed && !retires);selected=true;
        tx_desc[0].status=0;assert(!e1000_probe_retire_provider() && !probe_provider_closed && !retires);tx_desc[0].status=1;
        rx_poll_active=true;assert(!e1000_probe_retire_provider() && !probe_provider_closed && !retires);rx_poll_active=false;
        assert(e1000_probe_retire_provider() && probe_provider_closed && !selected && retires==1 && probe_irq==512);
        assert(e1000_probe_sample(&sample) && !sample.selected && sample.rx_ready && !sample.rx_dispatching && !sample.tx_pending);
        uint8_t frame[64]={0};assert(!e1000_send_frame(frame,sizeof frame) && !e1000_poll(16,false) && !device_rx_wake_ready(NULL));
        assert(!e1000_probe_retire_provider() && retires==1 && e1000_init());
        assert(maps==saved_maps && writes==saved_writes && allocations==saved_allocs);
        assert(!memcmp(saved_regs,registers,sizeof(registers)) && !memcmp(saved_pages,pages,sizeof(pages)));
    }
    puts("e1000-probe-sample: PASS read-only selected/ring/TX-DD/RX-ready/dispatch observation,missing-init/refusal terminal registry/direct publish seal with retained rawRX and unchanged DMA/MMIO/PCI/allocation state");
}
