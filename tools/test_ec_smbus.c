#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/ec_smbus.h>
struct device { uint8_t memory[256],mode,address,output;unsigned step,reads,writes,busy_reads;bool obf,stall; };
static uint8_t status(void *v) { struct device *d=v;return d->stall?2:d->obf?1:0; }
static uint8_t data(void *v) { struct device *d=v;assert(d->obf);d->obf=false;d->reads++;return d->output; }
static void command(void *v,uint8_t n) { struct device *d=v;assert(n==0x80 || n==0x81);d->mode=n;d->step=0; }
static void value(void *v,uint8_t n) {
    struct device *d=v;
    if(!d->step++) {
        d->address=n;
        if(d->mode==0x80) {
            if(n==0x20 && d->memory[n]==9 && d->busy_reads && !--d->busy_reads) d->memory[n]=0;
            d->output=d->memory[n];d->obf=true;
        }
    } else { assert(d->mode==0x81);d->memory[d->address]=n;d->writes++; }
}
static void setup(struct ec_smbus *s,struct ec_transport *ec,struct device *d) {
    *s=(struct ec_smbus){0};*d=(struct device){.busy_reads=3};d->memory[0x24]=0x34;d->memory[0x25]=0x12;
    ec_transport_init(ec,(struct ec_io){d,status,data,command,value});
}
static int run(struct ec_smbus *s) { int r=0;for(unsigned n=0;n<22000 && !r;n++) r=ec_smbus_poll(s,100+n*100);return r; }
int main(void) {
    struct ec_smbus s;struct ec_transport ec;struct device d;
    setup(&s,&ec,&d);assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,0x0d,100));
    assert(!ec_smbus_read_word(&s,&ec,0x20,0x0b,0x16,100));
    assert(run(&s)==1 && s.result==0x1234 && d.memory[0x22]==0x16 && d.memory[0x23]==0x0d && d.writes==3);
    setup(&s,&ec,&d);d.memory[0x20]=9;assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,13,100));assert(run(&s)==-1 && !d.writes && ec.poisoned);
    setup(&s,&ec,&d);d.busy_reads=0;assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,13,100));assert(run(&s)==-1 && ec.poisoned);
    setup(&s,&ec,&d);d.memory[0x21]=0x18;assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,13,100));assert(run(&s)==-1 && s.status==0x18);
    setup(&s,&ec,&d);d.stall=true;assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,13,100));assert(run(&s)==-1 && ec.poisoned);
    setup(&s,&ec,&d);assert(!ec_smbus_read_word(&s,&ec,251,0x0b,13,100));assert(ec_smbus_read_word(&s,&ec,0x20,0x0b,13,100));assert(ec_smbus_poll(&s,99)==-1);
    puts("ec-smbus: PASS read-word bytes, delayed completion, overlap refusal, busy-bank refusal, status error, EC/bus timeout, rollback and quarantine");
}
