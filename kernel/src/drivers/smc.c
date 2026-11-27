#include <srvros/smc.h>
#include <srvros/smc_transport.h>
#include <srvros/laptop_probe.h>
#include <srvros/acpi.h>
#include <srvros/battery_cache.h>
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
static unsigned stage,expected_fans;
static uint32_t pending_fan0;
static bool initialized,working,stopping,battery_mode,pro81;
enum battery_key { AC_BEGIN, PRESENT, CURRENT, VOLTAGE, REMAINING, FULL, SOC, AC_END };
static const struct { char name[5],type[5]; uint8_t size; } battery_keys[]={
    {"ACIN","flag",1},{"BBIN","flag",1},{"B0AC","si16",2},{"B0AV","ui16",2},
    {"B0RM","ui16",2},{"B0FC","ui16",2},{"BRSC","ui16",2},{"ACIN","flag",1},
};
static struct battery_cache battery;
static bool battery_metadata;
static unsigned battery_key;
static uint16_t battery_words[AC_END+1];
static uint64_t battery_began,battery_generation;
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
    uint16_t gpu=0,pch=0;
    for(uint64_t i=0;i<pci_device_count();i++) {
        const struct pci_device *d=pci_device_at(i);
        if(d->vendor_id==0x8086 && d->class_code==3)gpu=d->device_id;
        if(d->vendor_id==0x8086 && d->class_code==6 &&
           (d->device_id==0x9cc3 || d->device_id==0x8c4b || d->device_id==0x9c43 || d->device_id==0x1c49))pch=d->device_id;
    }
    expected_fans=0;pro81=false;
    for(uint64_t i=0;i<acpi_table_count();i++) {
        const struct acpi_table_info *t=acpi_table_at(i);
        if(t->signature[0]!='D' || t->signature[1]!='S' || t->signature[2]!='D' || t->signature[3]!='T')continue;
        const uint8_t *data=pmm_phys_to_virt(t->physical_address);
        if(gpu==0x1626 && pch==0x9cc3 && laptop_firmware_match(data,t->length))expected_fans=1;
        else if(laptop_air61_display_match(gpu,pch,data,t->length))expected_fans=1;
        else if(laptop_pro_display_match(gpu,pch,data,t->length))expected_fans=2;
        else if(laptop_pro81_firmware_match(gpu,pch,data,t->length)) { expected_fans=1;pro81=true; }
    }
    /* The independently captured DSDTs include APP0001 I/O 0x300..0x31f. This service
     * uses only metadata/read commands for FNum/F0Ac/F1Ac; no control keys,
     * MMIO or IRQ6 ownership. Air6,1 resources are recorded in its telemetry note. */
    return expected_fans!=0;
}
static bool metadata(const uint8_t *p,unsigned length,const char type[4]) {
    if(p[0]!=length || !(p[5]&0x80))return false;
    for(unsigned i=0;i<4;i++)if(p[i+1]!=(uint8_t)type[i])return false;
    return true;
}
static bool ready(void *arg) { (void)arg;return working && timer_ticks()>=due; }
static bool transaction_boundary(void) { return transport.phase==SMC_IDLE || transport.phase==SMC_DONE; }
static void fault(unsigned error) {
    if(battery_mode) {
        uint64_t f=lock();
        battery.value.state=BATTERY_SERVICE_FAULTED;
        battery.value.error=BATTERY_SERVICE_CONTAINMENT;
        working=false;unlock(f);
        console_printf("smc-battery: fault=%u key=%s metadata=%u phase=%u status=%x\n",
            (uint64_t)error,battery_keys[battery_key].name,(uint64_t)battery_metadata,
            (uint64_t)transport.failed_phase,(uint64_t)transport.status);
        return;
    }
    uint64_t f=lock();snapshot.state=SRV_SMC_FAULT;snapshot.flags=0;snapshot.error=error;snapshot.failures++;
    snapshot.phase=transport.failed_phase;snapshot.last_status=transport.status;working=false;unlock(f);
    smc_print_status();
}
static void battery_reply(void) {
    if(battery_metadata) {
        if(!metadata(transport.result,battery_keys[battery_key].size,battery_keys[battery_key].type)) {
            fault(0x101);return;
        }
        if(++battery_key==AC_END) { battery_metadata=false;battery_key=AC_BEGIN; }
        return;
    }
    uint16_t *words=battery_words;
    words[battery_key]=transport.result[0];
    if(battery_keys[battery_key].size==2)
        words[battery_key]=(uint16_t)(words[battery_key]<<8)|transport.result[1];
    if(battery_key++!=AC_END)return;
    battery_key=AC_BEGIN;
    uint64_t now=timer_monotonic_us();
    bool valid=words[PRESENT]==1 && words[AC_BEGIN]<=1 && words[AC_END]<=1 &&
        words[VOLTAGE]>=6000 && words[VOLTAGE]<=16000 && words[CURRENT]!=0x8000 &&
        words[FULL] && words[FULL]!=UINT16_MAX && words[REMAINING]<=words[FULL] && words[SOC]<=100;
    struct sbs_snapshot sample={.generation=++battery_generation,.started_us=battery_began,.completed_us=now};
    if(valid) {
        sample.valid=SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CURRENT|SBS_VALID_CAPACITY;
        sample.relative_soc=words[SOC];
        sample.capacity_unit=SBS_CAPACITY_UAH;
        sample.voltage_uv=(uint64_t)words[VOLTAGE]*1000;
        sample.current_ua=(int64_t)(int16_t)words[CURRENT]*1000;
        sample.remaining_capacity=(uint64_t)words[REMAINING]*1000;
        sample.full_capacity=(uint64_t)words[FULL]*1000;
    }
    uint64_t f=lock();
    battery.value.state=BATTERY_SERVICE_ACTIVE;
    battery_cache_publish(&battery,&sample,words[AC_BEGIN]!=0,words[AC_BEGIN]==words[AC_END],battery_began,now);
    unlock(f);
    due=timer_ticks()+150;
}
static void step(void) {
    if(transaction_boundary()) {
        if(stopping) { working=false;return; }
        const char *key=battery_mode?battery_keys[battery_key].name:stage<2?"FNum":(stage==3 || stage==5)?"F1Ac":"F0Ac";
        bool info=battery_mode?battery_metadata:stage==0 || stage==2 || stage==3;
        unsigned size=info?6:battery_mode?battery_keys[battery_key].size:stage==1?1:2;
        uint64_t now=timer_monotonic_us();
        if(battery_mode && !info && battery_key==AC_BEGIN)battery_began=now;
        if(!smc_transport_begin(&transport,info,key,size,now)) { fault(0x100);return; }
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
    if(battery_mode) { battery_reply();return; }
    uint64_t f=lock();snapshot.transactions++;snapshot.last_status=transport.status;unlock(f);
    if((stage==0 && !metadata(transport.result,1,"ui8 ")) ||
       ((stage==2 || stage==3) && !metadata(transport.result,2,"fpe2"))) { fault(0x101);return; }
    if(stage==1) {
        if(transport.result[0]!=expected_fans) { fault(0x102);return; }
        f=lock();snapshot.fan_count=expected_fans;unlock(f);
    }
    if(stage<2)stage++;
    else if(stage==2)stage=expected_fans==2?3:4;
    else if(stage==3)stage=4;
    else if(stage==4 && expected_fans==2) {
        pending_fan0=smc_fan_millirpm(transport.result);stage=5;
    } else {
        f=lock();
        snapshot.fan_millirpm=expected_fans==2?pending_fan0:smc_fan_millirpm(transport.result);
        snapshot.fan1_millirpm=expected_fans==2?smc_fan_millirpm(transport.result):0;
        snapshot.sampled_us=timer_monotonic_us();snapshot.samples++;
        snapshot.flags=SRV_SMC_FAN_VALID;snapshot.state=SRV_SMC_ACTIVE;unlock(f);
        stage=4;due=timer_ticks()+100;
    }
}
static void worker(void *arg) { (void)arg;for(;;) {
    if(!working) { scheduler_wait(&wake,ready,0);continue; }
    scheduler_wait_timeout(&wake,ready,0,due);if(ready(0))step();
} }
static void smc_start(bool read_battery) {
    if(initialized)return;
    initialized=true;battery_mode=read_battery;
    bool admitted=timer_reference_ready() && target();
    if(battery_mode) {
        battery_cache_begin(&battery,1);battery_metadata=true;
        if(!admitted || !pro81) { battery.value.state=BATTERY_SERVICE_UNAVAILABLE;return; }
    } else {
        snapshot.state=SRV_SMC_UNAVAILABLE;
        if(!admitted) { smc_print_status();return; }
        snapshot.state=SRV_SMC_STARTING;
    }
    smc_transport_init(&transport,(struct smc_io){0,status,data,command,argument});
    working=true;due=timer_ticks();
    if(!scheduler_spawn_io_service("smc",worker,0,ready)) {
        working=false;
        if(battery_mode) { battery.value.state=BATTERY_SERVICE_UNAVAILABLE;battery.value.error=BATTERY_SERVICE_SPAWN; }
        else { snapshot.state=SRV_SMC_UNAVAILABLE;snapshot.error=0x103; }
    }
    if(!battery_mode)smc_print_status();
}
void smc_init(void) { smc_start(false); }
void smc_battery_init(void) { smc_start(true); }
void smc_battery_status(struct battery_service_status *out) {
    if(out) { uint64_t f=lock();battery_cache_read(&battery,timer_monotonic_us(),out);unlock(f); }
}
void smc_status(struct srv_smc_status *out) { if(out) { uint64_t f=lock();*out=snapshot;unlock(f); } }
bool smc_quiesce(void) {
    uint64_t f=lock();
    stopping=true;
    if(transaction_boundary()) working=false;
    if(working) { due=timer_ticks();scheduler_wake_all(&wake); }
    unlock(f);
    uint64_t start=timer_monotonic_us();
    while(working && timer_monotonic_us()-start<600000) scheduler_yield();
    return !working && transaction_boundary();
}
void smc_print_status(void) {
    struct srv_smc_status s;smc_status(&s);
    console_printf("smc: state=%u flags=%x fans=%u fan_millirpm=%u sample_us=%u samples=%u transactions=%u failures=%u error=%u phase=%u status=%x; fan control disabled, native fan reading NOT_TESTED\n",
        (uint64_t)s.state,(uint64_t)s.flags,(uint64_t)s.fan_count,(uint64_t)s.fan_millirpm,s.sampled_us,s.samples,s.transactions,s.failures,(uint64_t)s.error,(uint64_t)s.phase,(uint64_t)s.last_status);
    if(s.fan_count==2)console_printf("smc-fans: fan0_millirpm=%u fan1_millirpm=%u flags=%u sample_us=%u samples=%u\n",
        (uint64_t)s.fan_millirpm,(uint64_t)s.fan1_millirpm,(uint64_t)s.flags,s.sampled_us,s.samples);
    if(s.state==SRV_SMC_FAULT) {
        /* A failed worker is quarantined, so this retained reply is stable.
         * Repeat it at desktop exit in case the original failure scrolled out
         * of the bounded boot log. Only FNum/F0Ac/F1Ac are queried by this service. */
        uint64_t bytes=0;char key[5]={0};
        for(unsigned i=0;i<4;i++)key[i]=transport.key[i]>=32&&transport.key[i]<=126?transport.key[i]:'?';
        for(unsigned i=0;i<transport.received && i<6;i++)bytes|=(uint64_t)transport.result[i]<<(i*8);
        console_printf("smc: failed-key=%s stage=%u command=%x requested=%u received=%u first_bytes_le=%x\n",
            key,(uint64_t)stage,(uint64_t)transport.command,(uint64_t)transport.length,(uint64_t)transport.received,bytes);
    }
}
