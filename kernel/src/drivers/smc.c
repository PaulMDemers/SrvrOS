#include <srvros/smc.h>
#include <srvros/smc_transport.h>
#include <srvros/laptop_probe.h>
#include <srvros/acpi.h>
#include <srvros/pmm.h>
#include <srvros/pci.h>
#include <srvros/timer.h>
#include <srvros/scheduler.h>
#include <srvros/console.h>
#include <stddef.h>
static struct srv_smc_status snapshot={.version=SRV_SMC_VERSION};
static struct smc_transport transport;
static struct scheduler_wait_queue wake;
static uint64_t due;
static unsigned stage;
static bool initialized,working;
static uint64_t lock(void) {
#ifdef SRVROS_SMC_HOST_TEST
    return 0;
#else
    uint64_t f;__asm__ volatile("pushfq;popq %0;cli":"=r"(f)::"memory");return f;
#endif
}
static void unlock(uint64_t f) {
#ifdef SRVROS_SMC_HOST_TEST
    (void)f;
#else
    if(f&(1ull<<9))__asm__ volatile("sti":::"memory");
#endif
}
static uint8_t read_port(uint16_t port) { uint8_t b;__asm__ volatile("inb %1,%0":"=a"(b):"Nd"(port));return b; }
static void write_port(uint16_t port,uint8_t b) { __asm__ volatile("outb %0,%1"::"a"(b),"Nd"(port)); }
static uint8_t status(void *ctx) { (void)ctx;return read_port(0x304); }
static uint8_t data(void *ctx) { (void)ctx;return read_port(0x300); }
static void command(void *ctx,uint8_t b) { (void)ctx;write_port(0x304,b); }
static void argument(void *ctx,uint8_t b) { (void)ctx;write_port(0x300,b); }
static bool target(void) {
    bool display=false,lpc=false,firmware=false;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *d=pci_device_at(i);
        if(d->vendor_id==0x8086 && d->device_id==0x1626 && d->class_code==3)display=true;
        if(d->vendor_id==0x8086 && d->device_id==0x9cc3 && d->class_code==6)lpc=true;
    }
    if(!display || !lpc)return false;
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t->signature[0]=='D' && t->signature[1]=='S' && t->signature[2]=='D' && t->signature[3]=='T')
            firmware=laptop_firmware_match(pmm_phys_to_virt(t->physical_address),t->length);
    }
    /* Exact table identity includes APP0001 _CRS: 0x300..0x31f. No MMIO,
     * IRQ6, control-key writes or guessed resources are used. */
    return firmware;
}
static bool metadata(const uint8_t *p,unsigned length,const char type[4]) {
    if(p[0]!=length || !(p[5]&0x80))return false;
    for(unsigned i=0;i<4;i++)if(p[i+1]!=(uint8_t)type[i])return false;
    return true;
}
static bool ready(void *arg) { (void)arg;return working && timer_ticks()>=due; }
static void fault(unsigned error) {
    uint64_t f=lock();snapshot.state=SRV_SMC_FAULT;snapshot.flags=0;snapshot.error=error;snapshot.failures++;
    snapshot.phase=transport.failed_phase;snapshot.last_status=transport.status;working=false;unlock(f);
    smc_print_status();
}
static void step(void) {
    if(transport.phase==SMC_IDLE || transport.phase==SMC_DONE) {
        const char *key=stage<2?"FNum":"F0Ac";
        bool info=stage==0 || stage==2;
        if(!smc_transport_begin(&transport,info,key,info?6:stage==1?1:2,timer_monotonic_us())) { fault(0x100);return; }
    }
    int result=0;
    /* Bounded short service slice, with interrupts enabled by the scheduler. */
    for(unsigned i=0;i<32 && !result;i++) {
        result=smc_transport_poll(&transport,timer_monotonic_us());
        if(!result && !timer_delay_us(8)) { smc_transport_cancel(&transport);result=-1; }
    }
    due=timer_ticks()+1;
    if(result<0) { fault(transport.error);return; }
    if(!result)return;
    uint64_t f=lock();snapshot.transactions++;snapshot.last_status=transport.status;unlock(f);
    if((stage==0 && !metadata(transport.result,1,"ui8 ")) ||
       (stage==2 && !metadata(transport.result,2,"fpe2"))) { fault(0x101);return; }
    if(stage==1) {
        if(transport.result[0]!=1) { fault(0x102);return; }
        f=lock();snapshot.fan_count=1;unlock(f);
    }
    if(stage<3)stage++;
    else {
        f=lock();snapshot.fan_millirpm=smc_fan_millirpm(transport.result);snapshot.sampled_us=timer_monotonic_us();
        snapshot.samples++;snapshot.flags=SRV_SMC_FAN_VALID;snapshot.state=SRV_SMC_ACTIVE;unlock(f);
        due=timer_ticks()+100;
    }
}
static void worker(void *arg) { (void)arg;for(;;) {
    if(!working) { scheduler_wait(&wake,ready,0);continue; }
    scheduler_wait_timeout(&wake,ready,0,due);if(ready(0))step();
} }
void smc_init(void) {
    if(initialized)return;
    initialized=true;snapshot.state=SRV_SMC_UNAVAILABLE;
    if(!timer_reference_ready() || !target()) { smc_print_status();return; }
    smc_transport_init(&transport,(struct smc_io){0,status,data,command,argument});
    snapshot.state=SRV_SMC_STARTING;working=true;due=timer_ticks();
    if(!scheduler_spawn_io_service("smc",worker,0,ready)) { working=false;snapshot.state=SRV_SMC_UNAVAILABLE;snapshot.error=0x103; }
    smc_print_status();
}
void smc_status(struct srv_smc_status *out) { if(out) { uint64_t f=lock();*out=snapshot;unlock(f); } }
void smc_print_status(void) {
    struct srv_smc_status s;smc_status(&s);
    console_printf("smc: state=%u flags=%x fans=%u fan_millirpm=%u sample_us=%u samples=%u transactions=%u failures=%u error=%u phase=%u status=%x; fan control disabled, native fan reading NOT_TESTED\n",
        (uint64_t)s.state,(uint64_t)s.flags,(uint64_t)s.fan_count,(uint64_t)s.fan_millirpm,s.sampled_us,s.samples,s.transactions,s.failures,(uint64_t)s.error,(uint64_t)s.phase,(uint64_t)s.last_status);
    if(s.state==SRV_SMC_FAULT) {
        /* A failed worker is quarantined, so this retained reply is stable.
         * Repeat it at desktop exit in case the original failure scrolled out
         * of the bounded boot log. Only FNum/F0Ac are queried by this service. */
        uint64_t bytes=0;char key[5]={0};
        for(unsigned i=0;i<4;i++)key[i]=transport.key[i]>=32&&transport.key[i]<=126?transport.key[i]:'?';
        for(unsigned i=0;i<transport.received && i<6;i++)bytes|=(uint64_t)transport.result[i]<<(i*8);
        console_printf("smc: failed-key=%s stage=%u command=%x requested=%u received=%u first_bytes_le=%x\n",
            key,(uint64_t)stage,(uint64_t)transport.command,(uint64_t)transport.length,(uint64_t)transport.received,bytes);
    }
}
