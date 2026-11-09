/* Execute the real runtime transfer wrapper and EC state machine. Only IRQ
 * instructions, scheduler identity and the clock/port model are substituted. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/irq_handlers.h>
#define SRVROS_BATTERY_TRANSFER_HOST_TEST
#include "../ports/srvros/uacpi/runtime/battery_runtime.c"
uint64_t battery_transfer_test_flags=1ull<<9;
static uint64_t identity=15,now=1000;
static unsigned command_calls,data_calls,status_calls,mode;
static uint8_t status(void *ctx) {
    (void)ctx;status_calls++;
    if(mode==1)return 2; /* IBF never clears: original deadline. */
    if(mode==2)return 1; /* Unexpected output before command. */
    return command_calls?1:32;
}
static uint8_t read_data(void *ctx){(void)ctx;data_calls++;return 0x10;}
static void command(void *ctx,uint8_t v){(void)ctx;assert(v==0x84);command_calls++;}
static void write_data(void *ctx,uint8_t v){(void)ctx;(void)v;assert(false);}
uint64_t scheduler_current_thread_id(void){return identity;}
/* The runtime's static SCI adapter table can be retained by a host linker.
 * No transfer-only case is allowed to use these hardware callbacks. */
bool ioapic_read_gsi(uint32_t g,uint32_t *l,uint32_t *h){(void)g;(void)l;(void)h;assert(false);return false;}
bool ioapic_mask_gsi(uint32_t g,bool m){(void)g;(void)m;assert(false);return false;}
bool lapic_vector_snapshot(uint8_t v,struct lapic_vector_state *s){(void)v;(void)s;assert(false);return false;}
uint64_t irq_handler_register_observed(uint8_t l,irq_handler_fn h,irq_completion_fn c,void *p){(void)l;(void)h;(void)c;(void)p;assert(false);return 0;}
uint64_t timer_monotonic_us(void){return now++;}
bool timer_delay_us(uint64_t n){now+=n;return true;}
void timer_wait_ticks(uint64_t n){now+=10000*n;}
static void reset(void) {
    memset(&runtime,0,sizeof(runtime));transfer_failure=(struct battery_transfer_diagnostic){0};
    battery_transfer_test_flags=1ull<<9;identity=15;now=1000;
    command_calls=data_calls=status_calls=mode=0;runtime.worker.identity=15;
    ec_transport_init(&runtime.transport,(struct ec_io){.status=status,.read_data=read_data,
        .write_command=command,.write_data=write_data});
}
static void refused(unsigned reason) {
    uint8_t value=0xa5;
    uint64_t f=battery_transfer_test_flags;
    assert(!transfer(false,0,&value,true));
    assert(value==0xa5&&transfer_failure.valid&&transfer_failure.reason==reason);
    assert(transfer_failure.query&&!transfer_failure.write&&transfer_failure.current==identity);
    assert(transfer_failure.flags==f&&battery_transfer_test_flags==f);
    assert(!command_calls&&!data_calls&&!status_calls);
}
int main(void) {
    reset();uint8_t value=0;
    assert(transfer(false,0,&value,true)&&value==0x10);
    assert(command_calls==1&&data_calls==1&&status_calls==2&&!runtime.lease&&!transfer_failure.valid);
    assert(runtime.transport.phase==EC_DONE&&runtime.transport.error==EC_OK);
    /* Exact terminal EC_DONE/EC_OK can coexist with a later entry refusal. */
    command_calls=data_calls=status_calls=0;battery_transfer_test_flags=0;
    refused(BATTERY_TRANSFER_IRQ);
    assert(transfer_failure.phase==EC_DONE&&transfer_failure.error==EC_OK);
    reset();identity=16;refused(BATTERY_TRANSFER_WORKER);
    reset();runtime.worker.identity=0;refused(BATTERY_TRANSFER_WORKER);
    reset();runtime.lease=true;refused(BATTERY_TRANSFER_LEASE);assert(runtime.lease);
    reset();runtime.transport.poisoned=true;refused(BATTERY_TRANSFER_POISON);assert(runtime.transport.poisoned);
    reset();runtime.transport.phase=EC_ADDRESS;refused(BATTERY_TRANSFER_BEGIN);
    assert(!runtime.lease&&transfer_failure.phase==EC_ADDRESS&&!transfer_failure.begun);
    reset();runtime.transport.io.status=NULL;refused(BATTERY_TRANSFER_BEGIN);
    reset();mode=1;value=0xa5;
    assert(!transfer(false,0,&value,true)&&value==0xa5&&!runtime.lease);
    assert(transfer_failure.reason==BATTERY_TRANSFER_POLL&&transfer_failure.begun&&transfer_failure.result==-1);
    assert(transfer_failure.phase==EC_FAILED&&transfer_failure.error==EC_TIMEOUT);
    assert(runtime.transport.timeout_us==100000&&now>=101000&&now<120000);
    assert(!command_calls&&!data_calls&&status_calls);
    reset();mode=2;assert(!transfer(false,0,&value,true));
    assert(transfer_failure.error==EC_UNEXPECTED_OUTPUT&&transfer_failure.failed_phase==EC_COMMAND);
    assert(!command_calls&&!data_calls&&status_calls==1&&runtime.transport.poisoned);
    puts("battery transfer: PASS real wrapper admission/refusal, unchanged deadline, no I/O on refusal, failure-only evidence");
}
