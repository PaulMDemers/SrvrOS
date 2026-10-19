#include <srvros/ec_transport.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model { unsigned reads,writes,status_reads;uint8_t status,output,bytes[8]; };
static uint8_t status(void *v) { struct model *m=v;m->status_reads++;return m->status; }
static uint8_t data(void *v) { struct model *m=v;m->reads++;m->status&=~1u;return m->output; }
static void write_byte(void *v,uint8_t b) { struct model *m=v;assert(m->writes<8);m->bytes[m->writes++]=b;m->status|=2; }
static void reset(struct ec_transport *t,struct model *m) {
    *m=(struct model){0};ec_transport_init(t,(struct ec_io){m,status,data,write_byte,write_byte});
}
static void read_start(struct ec_transport *t,struct model *m) {
    assert(ec_transport_begin(t,false,0x60,0,100,1000));
    assert(!ec_transport_poll(t,101) && m->bytes[0]==0x80);
    assert(!ec_transport_begin(t,true,0,0,102,100));
    assert(!ec_transport_poll(t,102) && m->writes==1);
    m->status=0;assert(!ec_transport_poll(t,103) && m->bytes[1]==0x60);
}
int main(void) {
    struct ec_transport t;struct model m;reset(&t,&m);
    assert(!ec_transport_begin(&t,false,0,0,0,0) && !ec_transport_begin(&t,false,0,0,0,1000001));
    read_start(&t,&m);assert(!ec_transport_poll(&t,104));m.status=0;
    assert(!ec_transport_poll(&t,105));m.output=0xa5;m.status=1|32;
    assert(ec_transport_poll(&t,106)==1 && t.result==0xa5 && m.reads==1 && m.writes==2);
    assert(ec_transport_poll(&t,107)==1 && m.reads==1);
    assert(ec_transport_begin(&t,true,0x20,9,110,1000));m.status=0;
    assert(!ec_transport_poll(&t,111));m.status=0;assert(!ec_transport_poll(&t,112));
    m.status=0;assert(!ec_transport_poll(&t,113));assert(!ec_transport_poll(&t,114));
    m.status=0;assert(ec_transport_poll(&t,115)==1 && !memcmp(m.bytes+2,(uint8_t[]){0x81,0x20,9},3));
    for(unsigned phase=EC_COMMAND;phase<=EC_WRITE_RESULT;phase++) {
        reset(&t,&m);assert(ec_transport_begin(&t,phase==EC_VALUE || phase==EC_WRITE_RESULT,0,0,10,20));
        t.phase=phase;unsigned before=m.status_reads;
        assert(ec_transport_poll(&t,30)==-1 && t.error==EC_TIMEOUT && t.failed_phase==phase && t.poisoned);
        assert(m.status_reads==before && !ec_transport_begin(&t,false,0,0,31,20));
    }
    reset(&t,&m);read_start(&t,&m);assert(ec_transport_poll(&t,102)==-1 && t.error==EC_CLOCK && !m.reads);
    reset(&t,&m);m.status=1;assert(ec_transport_begin(&t,false,0,0,0,100));
    assert(ec_transport_poll(&t,1)==-1 && t.error==EC_UNEXPECTED_OUTPUT && !m.reads && !m.writes);
    reset(&t,&m);assert(ec_transport_begin(&t,false,0,0,0,100));m.status=2;
    for(unsigned n=0;n<4096;n++) assert(!ec_transport_poll(&t,1));
    assert(ec_transport_poll(&t,1)==-1 && t.error==EC_POLL_LIMIT && !m.writes);
    reset(&t,&m);read_start(&t,&m);ec_transport_cancel(&t);
    assert(t.error==EC_CANCELED && t.poisoned && ec_transport_poll(&t,105)==-1 && !m.reads);
    /* SCI query uses one command and one reply, never an address write. A
     * cleared SCI_EVT after submission does not cancel the pending reply. */
    reset(&t,&m);m.status=32|2;assert(ec_transport_begin_query(&t,100,1000));
    assert(!ec_transport_poll(&t,101) && !m.writes);
    assert(!ec_transport_begin_query(&t,102,1000));
    m.status=32;assert(!ec_transport_poll(&t,102) && m.writes==1 && m.bytes[0]==0x84);
    assert(t.phase==EC_READ_RESULT && !ec_transport_begin(&t,false,9,0,103,100));
    m.status=0;assert(!ec_transport_poll(&t,103) && !m.reads);
    m.status=1;m.output=0x10;assert(ec_transport_poll(&t,104)==1 && t.result==0x10);
    assert(m.writes==1 && m.reads==1 && ec_transport_poll(&t,105)==1 && m.reads==1);
    /* Starting an ordinary transaction resets query mode. */
    assert(ec_transport_begin(&t,false,0x60,0,106,1000) && !t.query);
    assert(!ec_transport_poll(&t,107) && m.bytes[1]==0x80);m.status=0;
    assert(!ec_transport_poll(&t,108) && m.bytes[2]==0x60);
    for(unsigned code=0;code<256;code++) {
        reset(&t,&m);m.status=32;assert(ec_transport_begin_query(&t,0,100));
        assert(!ec_transport_poll(&t,1));m.status=1;m.output=(uint8_t)code;
        assert(ec_transport_poll(&t,2)==1 && t.result==code && m.writes==1 && m.reads==1);
    }
    reset(&t,&m);assert(ec_transport_begin_query(&t,0,100));
    assert(ec_transport_poll(&t,1)==1 && !t.result && !m.writes && !m.reads);
    reset(&t,&m);m.status=1|32;assert(ec_transport_begin_query(&t,0,100));
    assert(ec_transport_poll(&t,1)==-1 && t.error==EC_UNEXPECTED_OUTPUT && !m.reads && !m.writes);
    assert(!ec_transport_begin_query(&t,2,100));
    reset(&t,&m);m.status=32;assert(ec_transport_begin_query(&t,0,100));
    assert(!ec_transport_poll(&t,1));m.status=0;
    assert(ec_transport_poll(&t,100)==-1 && t.error==EC_TIMEOUT && !m.reads && m.writes==1);
    reset(&t,&m);m.status=32;assert(ec_transport_begin_query(&t,0,100));
    assert(!ec_transport_poll(&t,1));ec_transport_cancel(&t);m.status=1;m.output=0x22;
    assert(ec_transport_poll(&t,2)==-1 && t.error==EC_CANCELED && !m.reads);
    assert(!ec_transport_begin_query(NULL,0,100));
    puts("ec-transport: PASS delayed read/write/query handshakes, all event codes, spurious/cleared events, overlapping-request exclusion, phase deadlines, clock rollback/stall, stale output refusal, poison and cancellation");
}
