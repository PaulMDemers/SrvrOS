#include <assert.h>
#include <stdio.h>
#include <string.h>
#define AHCI_POLL_LIMIT 1000
#define SRVROS_AHCI_HOST_TEST
#include "../kernel/src/drivers/ahci.c"
static struct hba_port model;
static struct hba_command_header header;
static struct hba_command_table table;
static uint8_t buffer[4096];
static uint64_t now, current_space;
static unsigned polls, behavior;
void console_printf(const char *fmt, ...) { (void)fmt; }
void console_write(const char *s) { (void)s; }
uint64_t vmm_current_address_space(void) { return current_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t s) { current_space=s; }
bool vmm_virt_to_phys(uint64_t v, uint64_t *p) { *p=v; return true; }
bool vmm_map_page(uint64_t v, uint64_t p, uint64_t f) { (void)v;(void)p;(void)f; return true; }
uint64_t timer_monotonic_us(void) {
    if (behavior != 4) now+=100000;
    polls++;
    if(model.ci) {
        if(behavior==0) model.ci=0;
        if(behavior==2) model.is=HBA_PORT_IS_TFES;
        if(behavior==3) { model.is=HBA_PORT_IS_TFES; model.ci=0; }
    }
    return now;
}
static struct ahci_disk setup(unsigned mode) {
    memset(&model,0,sizeof(model)); memset(&header,0,sizeof(header)); memset(&table,0,sizeof(table));
    hba_physical=0x1000; now=0; polls=0; behavior=mode; current_space=2;
    return (struct ahci_disk){.port=&model,.command_list=&header,.command_table=&table,
        .io_buffer=buffer,.sectors=8,.flush_command=ATA_CMD_FLUSH_CACHE_EXT};
}
int main(void) {
    struct ahci_disk disk=setup(0);
    assert(issue_command(&disk,ATA_CMD_IDENTIFY,0,0,false));
    assert(header.prdt_length==1 && (table.prdt[0].dbc_i&0x3fffff)==511 && current_space==2);
    assert(issue_command(&disk,ATA_CMD_WRITE_DMA_EXT,0x123456789aull,1,true));
    assert(header.flags&(1<<6)); assert(table.cfis[4]==0x9a && table.cfis[9]==0x12);
    struct block_device dev={.private_data=&disk}; assert(ahci_flush(&dev));
    assert(header.prdt_length==0 && table.cfis[2]==ATA_CMD_FLUSH_CACHE_EXT);
    for(unsigned mode=1;mode<=3;mode++) {
        disk=setup(mode); assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
        assert(disk.failed && current_space==2 && polls<30);
        unsigned old=polls; assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false)); assert(old==polls);
    }
    disk=setup(0); model.tfd=0x80; assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(disk.failed && model.ci==0 && polls<30);
    disk=setup(4); assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(disk.failed && polls<=1002);
    uint8_t identify[512]={0}; identify[167]=0x60; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE_EXT);
    identify[167]=0x50; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE);
    identify[167]=0x10; assert(identify_flush_command(identify)==0);
    puts("ahci-host: PASS (DMA/flush descriptors, stuck busy/CI, errors, quarantine, address-space restore)");
}
