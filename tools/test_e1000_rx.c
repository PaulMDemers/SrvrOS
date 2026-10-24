#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../kernel/src/drivers/e1000.c"

static bool entered, map_ok=true;
static unsigned delivered[3];
void console_write(const char *s) { (void)s; }
void console_putc(char c) { (void)c; }
void console_printf(const char *s,...) { (void)s; }
bool vmm_virt_to_phys(uint64_t address,uint64_t *physical) { *physical=address;return map_ok; }
bool vmm_map_page(uint64_t virtual_address,uint64_t physical,uint64_t flags) {
    (void)virtual_address;(void)physical;(void)flags;return map_ok;
}
void net_handle_ethernet_frame(const uint8_t *frame,uint16_t length) {
    assert(length==60 && frame[0]>=1 && frame[0]<=2);
    delivered[frame[0]]++;
    if(!entered) {
        entered=true;
        /* A woken application can poll before the outer receive callback
         * returns its DMA descriptor. It must not consume that descriptor. */
        assert(e1000_poll(2,false)==0);
    }
}
int main(void) {
    static uint32_t registers[0x6000/4];
    static struct e1000_rx_desc ring[E1000_RX_DESC_COUNT];
    static uint8_t buffers[2][60];
    static struct pci_device device;
    nic=&device;mmio=registers;mmio_physical=0x100000;rings_ready=true;rx_desc=ring;
    for(unsigned i=0;i<2;i++) {
        buffers[i][0]=i+1;rx_buffers[i]=buffers[i];ring[i].length=60;
        ring[i].status=E1000_RX_DESC_STATUS_DD|E1000_RX_DESC_STATUS_EOP;
    }
    assert(e1000_poll(2,false)==2);
    assert(delivered[1]==1 && delivered[2]==1 && rx_tail==2 && rx_packets==2);
    assert(!ring[0].status && !ring[1].status && reg_read(E1000_REG_RDT)==1);
    assert(e1000_poll(2,false)==0);
    map_ok=false;assert(e1000_poll(2,false)==0);map_ok=true;
    ring[2]=ring[0];ring[2].length=60;ring[2].status=E1000_RX_DESC_STATUS_DD;
    rx_buffers[2]=buffers[0];assert(e1000_poll(1,false)==1 && delivered[1]==2);
    puts("e1000-rx: PASS nested poll refusal, descriptor/data preservation and ownership release");
}
