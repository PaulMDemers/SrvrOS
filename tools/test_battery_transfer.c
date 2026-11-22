/* Execute the real runtime transfer wrapper and EC state machine. Only IRQ
 * instructions, scheduler identity and the clock/port model are substituted. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <inttypes.h>
#include <srvros/irq_handlers.h>
#define SRVROS_BATTERY_TRANSFER_HOST_TEST
#include "../ports/srvros/uacpi/runtime/battery_runtime.c"
uint64_t battery_transfer_test_flags=1ull<<9;
static uint64_t identity=15,now=1000;
static unsigned command_calls,data_calls,status_calls,mode;
static char report[16384];static size_t report_size;
/* Translate only the kernel formatter's typed u64/i64 specifiers for libc. */
void console_printf(const char *fmt,...) {
    char host[2048];size_t n=0;
    for(size_t i=0;fmt[i];i++) {
        if(fmt[i]=='%'&&(fmt[i+1]=='u'||fmt[i+1]=='d')) {
            const char *suffix=fmt[++i]=='u'?PRIu64:PRId64;
            host[n++]='%';while(*suffix)host[n++]=*suffix++;
        } else host[n++]=fmt[i];
        assert(n<sizeof(host)-8);
    }
    host[n]=0;va_list args;va_start(args,fmt);
    int written=vsnprintf(report+report_size,sizeof(report)-report_size,host,args);va_end(args);
    assert(written>=0&&(size_t)written<sizeof(report)-report_size);report_size+=(size_t)written;
}
bool scheduler_io_active(void){assert(!(battery_transfer_test_flags&(1ull<<9)));return true;}
bool scheduler_io_busy(void){assert(!(battery_transfer_test_flags&(1ull<<9)));return true;}
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
    first_protocol_failure=(struct battery_protocol_diagnostic){0};report_size=0;report[0]=0;
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
static void protocol_tests(void) {
    reset();runtime.transport.phase=EC_DONE;
    assert(sbs_sample_begin(&runtime.sampler,&runtime.transport,0x20,100));
    assert(!sbs_sample_poll(&runtime.sampler,100));
    /* A previously submitted read-word is complete at its byte layer but the
     * next upper-level poll arrives at the original 2s boundary. */
    runtime.sampler.transaction.phase=4;runtime.sampler.transaction.last=500;
    int result=sbs_sample_poll(&runtime.sampler,2000100);
    assert(result==-1&&runtime.sampler.error==SBS_TRANSPORT&&runtime.transport.poisoned);
    assert(runtime.transport.phase==EC_DONE&&runtime.transport.error==EC_OK);
    runtime.lease=true;now=2000200;protocol_failure(BATTERY_PROTOCOL_SAMPLE,true,result,NULL);runtime.lease=false;
    struct battery_protocol_diagnostic first=first_protocol_failure;
    assert(first.valid&&first.kind==BATTERY_PROTOCOL_SAMPLE&&first.lease&&first.io&&first.busy);
    assert(first.sample.reason==SBS_TRANSPORT&&first.bus.reason==EC_SMBUS_DEADLINE);
    assert(first.bus.ec_phase==EC_DONE&&!first.bus.ec_poisoned&&first.poisoned);
    assert(first.bus.at_us==2000100&&first.observed_us==2000200&&first.current==15&&first.expected==15);
    uint8_t value=0xa5;assert(!transfer(false,0,&value,true)&&transfer_failure.reason==BATTERY_TRANSFER_POISON);
    assert(!memcmp(&first,&first_protocol_failure,sizeof(first)));
    memset(&runtime,0,sizeof(runtime));protocol_failure(BATTERY_PROTOCOL_EVENT,false,0,NULL);
    assert(!memcmp(&first,&first_protocol_failure,sizeof(first)));
    protocol_failure_report("retired",1);transfer_failure_report();
    assert(strstr(report,"captured=1 kind=1")&&strstr(report,"entry_poison=1")&&strstr(report,"result=-1"));
    assert(!command_calls&&!data_calls&&!status_calls);
    assert(battery_transfer_test_flags==(1ull<<9));
}
int main(int argc,char **argv) {
    if(argc==2&&!strcmp(argv[1],"--protocol-clean")){reset();protocol_failure_report("retired",1);fputs(report,stdout);return 0;}
    if(argc==2&&!strcmp(argv[1],"--protocol-failure")){protocol_tests();fputs(report,stdout);return 0;}
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
    protocol_tests();
    reset();assert(!first_protocol_failure.valid); /* Same zero reset as new lifetime. */
    puts("battery transfer: PASS real wrapper admission/refusal, unchanged deadline, no I/O on refusal, failure-only evidence");
}
