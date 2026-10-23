#include <assert.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_SMC_HOST_TEST
#include "../kernel/src/drivers/smc.c"
#include "smc_model.h"
static uint64_t clock_us=1000000;
static bool chipset=true,firmware=true,spawn_ok=true,delay_ok=true;
static struct pci_device devices[]={ {.vendor_id=0x8086,.device_id=0x1626,.class_code=3}, {.vendor_id=0x8086,.device_id=0x9cc3,.class_code=6} };
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
bool laptop_pro_display_match(uint16_t gpu,uint16_t pch,const uint8_t *data,size_t size) {
    assert(data==(uint8_t*)&table && size==35034);return firmware && gpu==0x0d26 && pch==0x8c4b;
}
bool laptop_air61_display_match(uint16_t gpu,uint16_t pch,const uint8_t *data,size_t size) {
    assert(data==(uint8_t*)&table && size==35034);return firmware && gpu==0x0a26 && pch==0x9c43;
}
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
static void init(void) { initialized=false;stage=0;snapshot=(struct srv_smc_status){.version=SRV_SMC_VERSION};smc_init(); }
int main(void) {
    chipset=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    chipset=true;firmware=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    firmware=true;spawn_ok=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE && !working);
    spawn_ok=true;init();assert(snapshot.state==SRV_SMC_STARTING && working);
    uint8_t count_info[]={1,'u','i','8',' ',0x80},count[]={1},fan_info[]={2,'f','p','e','2',0x80},fan[]={0x65,0x94};
    reply(count_info,6);assert(stage==1 && m.command==0x13);
    reply(count,1);assert(stage==2 && snapshot.fan_count==1 && m.command==0x10);
    reply(fan_info,6);assert(stage==4);
    reply(fan,2);assert(stage==4 && snapshot.state==SRV_SMC_ACTIVE && snapshot.fan_millirpm==6501000 && snapshot.samples==1 && snapshot.transactions==4);
    assert(!ready(0));clock_us+=1000000;assert(ready(0));fan[0]=fan[1]=0;reply(fan,2);assert(snapshot.fan_millirpm==0 && snapshot.samples==2 && snapshot.flags==SRV_SMC_FAN_VALID);
    uint64_t saved=snapshot.sampled_us;
    transport.phase=SMC_IDLE;m=(struct model){.status=4};step();clock_us+=500000;step();
    assert(!working && snapshot.state==SRV_SMC_FAULT && !snapshot.flags && snapshot.samples==2 && snapshot.sampled_us==saved && snapshot.error==SMC_DEADLINE && !m.commands);
    init();count_info[5]=0;reply(count_info,6);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x101 && !working);
    init();count_info[5]=0x80;reply(count_info,6);count[0]=2;reply(count,1);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x102);
    init();m=(struct model){.length=6};transport.io=(struct smc_io){&m,model_status,model_read,model_command,model_argument};
    delay_ok=false;step();assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==SMC_CANCELED && !working && m.commands==1 && !m.args);
    delay_ok=true;devices[0].device_id=0x0d26;devices[1].device_id=0x8c4b;
    firmware=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE && !expected_fans);
    firmware=true;devices[1].device_id=0x9cc3;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    devices[1].device_id=0x8c4b;init();assert(expected_fans==2 && snapshot.state==SRV_SMC_STARTING);
    count[0]=1;reply(count_info,6);reply(count,1);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x102);
    init();count[0]=2;reply(count_info,6);reply(count,1);reply(fan_info,6);
    assert(stage==3 && !snapshot.samples && !memcmp(m.key,"F0Ac",4));
    reply(fan_info,6);assert(stage==4 && !memcmp(m.key,"F1Ac",4));
    fan[0]=0x23;fan[1]=0xa0;reply(fan,2);
    assert(stage==5 && !snapshot.samples && !snapshot.flags && !memcmp(m.key,"F0Ac",4));
    fan[0]=0x21;fan[1]=0x20;reply(fan,2);
    assert(stage==4 && snapshot.samples==1 && snapshot.fan_count==2 && snapshot.fan_millirpm==2280000 && snapshot.fan1_millirpm==2120000);
    assert(!memcmp(m.key,"F1Ac",4) && snapshot.flags==SRV_SMC_FAN_VALID);
    fan[0]=fan[1]=0;reply(fan,2);assert(snapshot.samples==1 && snapshot.fan_millirpm==2280000);
    reply(fan,2);assert(snapshot.samples==2 && !snapshot.fan_millirpm && !snapshot.fan1_millirpm && snapshot.flags==SRV_SMC_FAN_VALID);
    reply(fan,2);assert(stage==5);transport.phase=SMC_IDLE;m=(struct model){.status=4};step();clock_us+=500000;step();
    assert(snapshot.state==SRV_SMC_FAULT && !snapshot.flags && snapshot.samples==2);
    init();reply(count_info,6);reply(count,1);reply(fan_info,6);
    fan_info[0]=1;reply(fan_info,6);assert(snapshot.state==SRV_SMC_FAULT && !snapshot.samples && !snapshot.flags);
    devices[0].device_id=0x0a26;devices[1].device_id=0x9c43;
    firmware=false;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE && !expected_fans);
    firmware=true;devices[1].device_id=0x9cc3;init();assert(snapshot.state==SRV_SMC_UNAVAILABLE);
    devices[1].device_id=0x9c43;init();assert(expected_fans==1 && snapshot.state==SRV_SMC_STARTING);
    fan_info[0]=2;count[0]=1;reply(count_info,6);reply(count,1);reply(fan_info,6);
    assert(stage==4 && !memcmp(m.key,"F0Ac",4));
    fan[0]=0x12;fan[1]=0xc8;reply(fan,2);
    assert(snapshot.state==SRV_SMC_ACTIVE && snapshot.fan_count==1 && snapshot.fan_millirpm==1202000);
    assert(!snapshot.fan1_millirpm && snapshot.samples==1 && m.command==0x10 && !memcmp(m.key,"F0Ac",4));
    struct srv_smc_status cached;unsigned commands=m.commands;smc_status(&cached);
    assert(cached.samples==1 && m.commands==commands);
    clock_us+=1000000;reply(fan,2);assert(snapshot.samples==2 && !memcmp(m.key,"F0Ac",4));
    init();count[0]=2;reply(count_info,6);reply(count,1);assert(snapshot.state==SRV_SMC_FAULT && snapshot.error==0x102);
    puts("smc-service: PASS exact-target gate, worker failure, metadata/count checks, cached fan samples, zero RPM and fault invalidation without control writes");
}
