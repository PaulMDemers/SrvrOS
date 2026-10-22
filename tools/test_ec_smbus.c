#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/ec_smbus.h>
struct device { uint8_t memory[256],mode,address,output;unsigned step,reads,writes,busy_reads;bool obf,stall,stall_after_write; };
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
    } else { assert(d->mode==0x81);d->memory[d->address]=n;d->writes++;if(d->stall_after_write)d->stall=true; }
}
static void setup(struct ec_smbus *s,struct ec_transport *ec,struct device *d) {
    *s=(struct ec_smbus){0};*d=(struct device){.busy_reads=3};d->memory[0x24]=0x34;d->memory[0x25]=0x12;
    ec_transport_init(ec,(struct ec_io){d,status,data,command,value});
}
static int run(struct ec_smbus *s) { int r=0;for(unsigned n=0;n<22000 && !r;n++) r=ec_smbus_poll(s,100+n*100);return r; }
static int event_run(struct ec_smbus_event *s) {
    int r=0;for(unsigned n=0;n<12000 && !r;n++)r=ec_smbus_event_poll(s,100+n*100);return r;
}
static void event_tests(void) {
    struct ec_smbus word;struct ec_smbus_event event;struct ec_transport ec;struct device d;
    for(unsigned status=0;status<256;status++) {
        setup(&word,&ec,&d);event=(struct ec_smbus_event){0};
        d.memory[0x21]=status;d.memory[0x45]=0x16;d.memory[0x46]=0x34;d.memory[0x47]=0x12;
        assert(ec_smbus_event_begin(&event,&ec,0x20,100));
        assert(!ec_smbus_event_begin(&event,&ec,0x20,100));
        int result=event_run(&event);
        if(status&0x3f)assert(result==-1 && !d.writes && ec.poisoned);
        else {
            assert(result==1 && event.status==status && !ec.poisoned);
            assert(event.alarm==!!(status&64) && d.writes==!!(status&64));
            if(event.alarm)assert(event.acknowledged && event.recognized && event.data==0x1234 && event.address==11 && !d.memory[0x21]);
            else assert(!event.ack_attempted && d.memory[0x21]==status && d.reads==2);
        }
    }
    for(unsigned address=0;address<256;address++) {
        setup(&word,&ec,&d);event=(struct ec_smbus_event){0};d.memory[0x21]=0xc0;d.memory[0x45]=address;
        assert(ec_smbus_event_begin(&event,&ec,0x20,100));int result=event_run(&event);
        if(!address || address&1)assert(result==-1 && !d.writes);
        else assert(result==1 && d.writes==1 && event.recognized==((address>>1)>=9 && (address>>1)<=11));
    }
    for(unsigned phase=0;phase<8;phase++) {
        setup(&word,&ec,&d);event=(struct ec_smbus_event){0};d.memory[0x21]=64;d.memory[0x45]=0x14;
        assert(ec_smbus_event_begin(&event,&ec,0x20,100));uint64_t now=100;
        while(event.phase!=phase || !event.pending){assert(!ec_smbus_event_poll(&event,now));now+=100;}
        d.stall=true;int r=0;while(!r){now+=100;r=ec_smbus_event_poll(&event,now);}
        assert(r==-1 && event.failed && ec.poisoned && !event.acknowledged && !d.writes);
    }
    for(unsigned change=0;change<3;change++) {
        setup(&word,&ec,&d);event=(struct ec_smbus_event){0};d.memory[0x21]=64;d.memory[0x45]=0x16;
        assert(ec_smbus_event_begin(&event,&ec,0x20,100));uint64_t now=100;
        if(!change)d.memory[0x20]=0x0b;
        else {
            while(event.phase<5){assert(!ec_smbus_event_poll(&event,now));now+=100;}
            if(change==1)d.memory[0x20]=0x0b;else d.memory[0x21]=0;
        }
        int r=0;while(!r){now+=100;r=ec_smbus_event_poll(&event,now);}
        assert(r==-1 && !d.writes && ec.poisoned);
    }
    /* The device consumed the acknowledgement, but its final handshake did
     * not complete. Preserve uncertainty and prohibit retry on this transport. */
    setup(&word,&ec,&d);event=(struct ec_smbus_event){0};
    d.memory[0x21]=64;d.memory[0x45]=0x16;d.stall_after_write=true;
    assert(ec_smbus_event_begin(&event,&ec,0x20,100));
    assert(event_run(&event)==-1 && d.writes==1 && !d.memory[0x21]);
    assert(event.ack_attempted && !event.acknowledged && ec.poisoned);
    event=(struct ec_smbus_event){0};assert(!ec_smbus_event_begin(&event,&ec,0x20,100));
    setup(&word,&ec,&d);event=(struct ec_smbus_event){0};
    assert(!ec_smbus_event_begin(&event,&ec,217,100));
    assert(ec_smbus_event_begin(&event,&ec,0x20,100));assert(ec_smbus_event_poll(&event,99)==-1);
    setup(&word,&ec,&d);event=(struct ec_smbus_event){0};
    assert(ec_smbus_event_begin(&event,&ec,0x20,100));assert(ec_smbus_event_poll(&event,1000100)==-1);
    puts("ec-smbus-event: PASS 256 statuses, 256 addresses, all transfer stalls, busy/raced bank, no completion write, full alarm capture before acknowledgement, clock/deadline");
}
int main(void) {
    event_tests();
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
