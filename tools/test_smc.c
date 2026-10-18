#include <srvros/smc_transport.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "smc_model.h"
static void setup(struct smc_transport *t,struct model *m,unsigned length) {
    *m=(struct model){.length=length};for(unsigned i=0;i<length;i++)m->reply[i]=(uint8_t)(i*19+7);
    smc_transport_init(t,(struct smc_io){m,model_status,model_read,model_command,model_argument});
}
static int drive(struct smc_transport *t,uint64_t *clock) { int result=0;for(unsigned i=0;i<100 && !result;i++) { *clock+=10;result=smc_transport_poll(t,*clock); }return result; }
int main(void) {
    struct smc_transport t;struct model m;uint64_t clock=1000;
    for(unsigned length=1;length<=32;length++) {
        setup(&t,&m,length);assert(smc_transport_begin(&t,false,"F0Ac",length,clock));
        assert(!smc_transport_begin(&t,false,"FNum",1,clock));assert(drive(&t,&clock)==1);
        assert(m.commands==1 && m.args==5 && m.reads==length && !memcmp(m.key,"F0Ac",4) && !memcmp(t.result,m.reply,length));
        assert(smc_transport_poll(&t,clock)==1);
    }
    setup(&t,&m,1);m.early_data=true;assert(smc_transport_begin(&t,false,"FNum",1,clock));assert(drive(&t,&clock)==1);
    setup(&t,&m,1);assert(smc_transport_begin(&t,false,"FNum",1,clock));
    assert(!smc_transport_poll(&t,++clock));m.status=2;
    assert(!smc_transport_poll(&t,++clock) && t.phase==SMC_INPUT_OPEN && !m.args);
    m.status=4;assert(!smc_transport_poll(&t,++clock) && t.phase==SMC_INPUT_BUSY && !m.args);
    m.status=2;assert(!smc_transport_poll(&t,++clock) && t.phase==SMC_INPUT_OPEN && !m.args);
    m.status=4;assert(drive(&t,&clock)==1);
    setup(&t,&m,6);assert(smc_transport_begin(&t,true,"FNum",6,clock));assert(drive(&t,&clock)==1 && m.command==0x13);
    assert(!smc_transport_begin(&t,true,"FNum",1,clock));assert(!smc_transport_begin(&t,false,"FNum",33,clock));
    setup(&t,&m,1);m.status=4;assert(smc_transport_begin(&t,false,"FNum",1,clock));
    assert(!smc_transport_poll(&t,clock+1) && !m.commands);assert(smc_transport_poll(&t,clock+500000)==-1 && t.error==SMC_DEADLINE && !m.commands);
    assert(!smc_transport_begin(&t,false,"FNum",1,clock));
    setup(&t,&m,1);m.status=1;assert(smc_transport_begin(&t,false,"FNum",1,clock));assert(smc_transport_poll(&t,clock)==-1 && !m.reads && !m.commands);
    setup(&t,&m,2);assert(smc_transport_begin(&t,false,"FNum",1,clock));assert(drive(&t,&clock)==-1 && t.error==SMC_EXTRA_DATA && m.reads==1);
    setup(&t,&m,0);assert(smc_transport_begin(&t,false,"FNum",1,clock));assert(drive(&t,&clock)==-1 && t.error==SMC_NO_RESULT);
    setup(&t,&m,1);assert(smc_transport_begin(&t,false,"FNum",1,clock));assert(smc_transport_poll(&t,clock-1)==-1 && t.error==SMC_CLOCK);
    setup(&t,&m,1);assert(smc_transport_begin(&t,false,"FNum",1,clock));smc_transport_cancel(&t);assert(t.error==SMC_CANCELED && t.poisoned);
    setup(&t,&m,1);m.status=2;assert(smc_transport_begin(&t,false,"FNum",1,clock));for(unsigned i=0;i<4096;i++)assert(!smc_transport_poll(&t,clock));assert(smc_transport_poll(&t,clock)==-1 && t.error==SMC_POLL_LIMIT);
    uint8_t raw[]={0x65,0x94};assert(smc_fan_millirpm(raw)==6501000);raw[0]=raw[1]=255;assert(smc_fan_millirpm(raw)==16383750);
    puts("smc: PASS bounded read-only commands, distinct input handshakes, metadata, 1..32 byte reads, busy/data faults, timeout/clock/poll/cancel and unsigned fan decoding");
}
