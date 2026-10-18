#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SMC_HOST_TEST
#include "../kernel/src/drivers/smc.c"
#include "smc_model.h"
static uint64_t clock_us=1000000;
static bool chipset=true,firmware=true,spawn_ok=true,delay_ok=true;
static const struct pci_device devices[]={ {.vendor_id=0x8086,.device_id=0x1626,.class_code=3}, {.vendor_id=0x8086,.device_id=0x9cc3,.class_code=6} };
static struct acpi_table_info table={.signature="DSDT",.physical_address=0x1000,.length=35034};
uint64_t timer_ticks(void) { return clock_us/10000; }
uint64_t timer_monotonic_us(void) { return clock_us; }
bool timer_reference_ready(void) { return true; }
bool timer_delay_us(uint64_t us) { assert(us==8);clock_us+=us;return delay_ok; }
uint64_t pci_device_count(void) { return chipset?2:0; }
const struct pci_device *pci_device_at(uint64_t i) { assert(i<2);return &devices[i]; }
uint64_t acpi_table_count(void) { return 1; }
const struct acpi_table_info *acpi_table_at(uint64_t i) { assert(!i);return &table; }
void *pmm_phys_to_virt(uint64_t physical) { assert(physical==0x1000);return &table; }
bool laptop_firmware_match(const uint8_t *data,size_t size) { assert(data==(uint8_t*)&table && size==35034);return firmware; }
bool scheduler_spawn_io_service(const char *name,scheduler_thread_fn fn,void *arg,scheduler_wait_condition_fn eligible) { assert(!strcmp(name,"smc") && fn==worker && !arg && eligible==ready);return spawn_ok; }
bool scheduler_wait(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg) { (void)q;(void)fn;(void)arg;assert(0);return false; }
bool scheduler_wait_timeout(struct scheduler_wait_queue *q,scheduler_wait_condition_fn fn,void *arg,uint64_t deadline) { (void)q;(void)fn;(void)arg;(void)deadline;assert(0);return false; }
void console_printf(const char *fmt,...) { (void)fmt; }
static struct model m;
static void reply(const uint8_t *data,unsigned length) {
    m=(struct model){.length=length};memcpy(m.reply,data,length);
    transport.io=(struct smc_io){&m,model_status,model_read,model_command,model_argument};
    step();assert(m.commands==1 && m.args==5 && m.reads==length);
}
static void init(void) { initialized=false;stage=0;snapshot=(struct srv_smc_status){.version=1};smc_init(); }
int main(void) {
    chipset=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    chipset=true;firmware=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    firmware=true;spawn_ok=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE && !working);
    spawn_ok=true;init();assert(snapshot.state==SRV_SMC_STARTING && working);
    uint8_t count_info[]={1,'u','i','8',' ',0x80},count[]={1},fan_info[]={2,'f','p','e','2',0x80},fan[]={0x65,0x94};
    reply(count_info,6);assert(stage==1 && m.command==0x13);
    reply(count,1);assert(stage==2 && snapshot.fan_count==1 && m.command==0x10);
    reply(fan_info,6);assert(stage==3);
    reply(fan,2);assert(stage==3 && snapshot.state==SRV_SMC_ACTIVE && snapshot.fan_millirpm==6501000 && snapshot.samples==1 && snapshot.transactions==4);
    assert(!ready(0));clock_us+=1000000;assert(ready(0));fan[0]=fan[1]=0;reply(fan,2);assert(snapshot.fan_millirpm==0 && snapshot.samples==2 && snapshot.flags==SRV_SMC_FAN_VALID);
    uint64_t saved=snapshot.sampled_us;
    transport.phase=SMC_IDLE;m=(struct model){.status=4};step();clock_us+=500000;step();
    assert(!working && snapshot.state==SRV_SMC_FAULT && !snapshot.flags && snapshot.samples==2 && snapshot.sampled_us==saved && snapshot.error==SMC_DEADLINE && !m.commands);
    init();count_info[5]=0;reply(count_info,6);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x101 && !working);
    init();count_info[5]=0x80;reply(count_info,6);count[0]=2;reply(count,1);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x102);
    init();m=(struct model){.length=6};transport.io=(struct smc_io){&m,model_status,model_read,model_command,model_argument};
    delay_ok=false;step();assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==SMC_CANCELED && !working && m.commands==1 && !m.args);
    puts("smc-service: PASS exact-target gate, worker failure, metadata/count checks, cached fan samples, zero RPM and fault invalidation without control writes");
}
