#include <assert.h>
#include <stdio.h>
#include <string.h>
#define AHCI_POLL_LIMIT 1000
#define AHCI_SERVICE_LIMIT 4
#define SRVROS_AHCI_HOST_TEST
#include "../kernel/src/drivers/ahci.c"
static struct hba_port model;
static struct hba_command_header header;
static struct hba_command_table table;
static uint8_t buffer[4096];
static uint64_t now, current_space;
static unsigned polls, behavior;
static bool services_enabled, in_service, io_active;
static unsigned yields, profile_depth;
static struct ahci_disk *active_disk;
void latency_enter(enum latency_site site) { assert(site==LATENCY_AHCI); profile_depth++; }
void latency_leave(enum latency_site site) { assert(site==LATENCY_AHCI && profile_depth); profile_depth--; }
bool scheduler_io_begin(void) {
    if (!services_enabled || in_service) return false;
    assert(!io_active); io_active=true; return true;
}
bool scheduler_io_active(void) { return io_active; }
void scheduler_io_end(void) { assert(io_active && !in_service); io_active=false; }
bool scheduler_io_yield(void) {
    assert(io_active && current_space==2 && profile_depth==0 && active_disk->busy);
    yields++;
    in_service=true;
    uint8_t saved[4096]; memcpy(saved,buffer,sizeof(saved));
    struct block_device dev={.private_data=active_disk};
    uint8_t byte=0xff;
    assert(!ahci_write(&dev,0,&byte,1)); /* Never overwrite an in-flight bounce buffer. */
    assert(!memcmp(saved,buffer,sizeof(saved)));
    struct ahci_disk another=*active_disk; another.busy=false;
    dev.private_data=&another;
    assert(!ahci_read(&dev,0,&byte,1)); /* Cross-device storage rejected in service scope. */
    in_service=false;
    if (behavior==5 || (behavior==10 && yields==6)) model.ci=0;
    if (behavior==6) model.tfd=0;
    return true;
}

void console_printf(const char *fmt, ...) { (void)fmt; }
void console_write(const char *s) { (void)s; }
uint64_t vmm_current_address_space(void) { return current_space; }
uint64_t vmm_kernel_address_space(void) { return 1; }
void vmm_switch_address_space(uint64_t s) { current_space=s; }
bool vmm_virt_to_phys(uint64_t v, uint64_t *p) { *p=v; return true; }
bool vmm_map_page(uint64_t v, uint64_t p, uint64_t f) { (void)v;(void)p;(void)f; return true; }
uint64_t timer_monotonic_us(void) {
    if (behavior != 4) now += behavior >= 5 ? 1000 : 100000;
    polls++;
    if(model.ci) {
        if(behavior==0 || behavior==6 || behavior==8) model.ci=0;
        if(behavior==2) model.is=HBA_PORT_IS_TFES;
        if(behavior==3) { model.is=HBA_PORT_IS_TFES; model.ci=0; }
    }
    return now;
}
static struct ahci_disk setup(unsigned mode) {
    memset(&model,0,sizeof(model)); memset(&header,0,sizeof(header)); memset(&table,0,sizeof(table));
    assert(profile_depth==0);
    services_enabled=in_service=io_active=false; yields=0; last_service_us=0; commands_issued=0;
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
    disk=setup(5); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>=1 && !disk.busy && !io_active && current_space==2 && profile_depth==0);
    disk=setup(6); services_enabled=true; active_disk=&disk; model.tfd=0x80;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>=1 && !disk.busy && current_space==2);
    disk=setup(7); services_enabled=true; active_disk=&disk;
    assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>0 && disk.failed && !disk.busy && !io_active && profile_depth==0 && current_space==2);
    disk=setup(5); services_enabled=true; active_disk=&disk;
    memset(buffer,0x5a,sizeof(buffer));
    dev.private_data=&disk; uint8_t value=0x31;
    assert(ahci_write(&dev,10,&value,1));
    assert(buffer[9]==0x5a && buffer[10]==value && buffer[11]==0x5a && yields>=2 && !disk.busy);
    disk=setup(4); services_enabled=true; active_disk=&disk;
    assert(!issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==AHCI_SERVICE_LIMIT && polls<AHCI_POLL_LIMIT && disk.failed && !disk.busy && !io_active);
    disk=setup(10); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields>AHCI_SERVICE_LIMIT && !disk.failed); /* Healthy time keeps its full deadline. */
    disk=setup(8); services_enabled=true; active_disk=&disk;
    assert(issue_command(&disk,ATA_CMD_READ_DMA_EXT,0,1,false));
    assert(yields==1 && current_space==2); /* Fast completion still services a long sequence. */
    uint8_t identify[512]={0}; identify[167]=0x60; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE_EXT);
    identify[167]=0x50; assert(identify_flush_command(identify)==ATA_CMD_FLUSH_CACHE);
    identify[167]=0x10; assert(identify_flush_command(identify)==0);
    puts("ahci-host: PASS (DMA/flush descriptors, stuck busy/CI, errors, quarantine, address-space restore, ownership, service windows)");
}
