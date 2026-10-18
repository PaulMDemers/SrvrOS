#include <srvros/hda_core.h>

#define GCAP 0x00
#define GCTL 0x08
#define STATESTS 0x0e
#define INTCTL 0x20
#define CORBCTL 0x4c
#define RIRBCTL 0x5c
#define ICOI 0x60
#define ICII 0x64
#define ICIS 0x68
#define POLL_LIMIT 2000000u
#define TIMEOUT_US 100000u
static uint32_t rd(struct hda_core *h, unsigned r, unsigned n) { return h->io.read(h->io.context,r,n); }
static void wr(struct hda_core *h,unsigned r,unsigned n,uint32_t v) { h->io.write(h->io.context,r,n,v); }
static bool wait_bits(struct hda_core *h,unsigned r,unsigned n,uint32_t mask,uint32_t value) {
    uint64_t start=h->io.now_us(h->io.context);
    for(unsigned i=0;i<POLL_LIMIT;i++) {
        if((rd(h,r,n)&mask)==value) return true;
        uint64_t now=h->io.now_us(h->io.context);
        if(now<start || now-start>=TIMEOUT_US) return false;
        h->io.relax(h->io.context);
    }
    return false;
}
bool hda_core_delay(struct hda_core *h,unsigned us) {
    if(!h || !h->io.now_us || !h->io.relax || us>10000) return false;
    uint64_t start=h->io.now_us(h->io.context);
    for(unsigned i=0;i<POLL_LIMIT;i++) {
        uint64_t now=h->io.now_us(h->io.context);
        if(now<start) return false;
        if(now-start>=us) return true;
        h->io.relax(h->io.context);
    }
    return false;
}
static bool fail(struct hda_core *h,enum hda_error error) { h->error=error; h->ready=false; return false; }
static uint32_t stream_control(struct hda_core *h) {
    return rd(h,h->stream,2)|(rd(h,h->stream+2,1)<<16);
}
static void set_stream_control(struct hda_core *h,uint32_t value) {
    wr(h,h->stream+2,1,value>>16); wr(h,h->stream,2,value&65535);
}
bool hda_core_reset(struct hda_core *h,struct hda_io io) {
    if(!h) return false;
    *h=(struct hda_core){.io=io};
    if(!io.read||!io.write||!io.now_us||!io.relax) return fail(h,HDA_BAD_ARGUMENT);
    uint32_t cap=rd(h,GCAP,2);
    h->inputs=(cap>>8)&15; h->outputs=(cap>>12)&15; h->dma64=(cap&1)!=0;
    if(!h->outputs || h->inputs+h->outputs>30) return fail(h,HDA_BAD_ARGUMENT);
    h->stream=0x80+0x20*h->inputs;
    wr(h,INTCTL,4,0); wr(h,CORBCTL,1,0); wr(h,RIRBCTL,1,0);
    /* Reset the link before allocating or exposing our own DMA buffers. */
    wr(h,GCTL,4,0);
    if(!wait_bits(h,GCTL,4,1,0)||!hda_core_delay(h,100)) return fail(h,HDA_RESET_TIMEOUT);
    wr(h,GCTL,4,1);
    if(!wait_bits(h,GCTL,4,1,1)||!hda_core_delay(h,1000)) return fail(h,HDA_RESET_TIMEOUT);
    h->codecs=rd(h,STATESTS,2)&0x7fff;
    h->ready=true; return true;
}
bool hda_core_verb(struct hda_core *h,unsigned codec,unsigned node,
    unsigned verb,unsigned payload,uint32_t *response) {
    if(!h||!h->ready) return false;
    if(codec>14||node>127||verb>0xfff||payload>65535||!response ||
       (((verb&0x700)==0x700) ? payload>255 : (verb&0xff)!=0)) return false;
    if(!wait_bits(h,ICIS,2,1,0)) return fail(h,HDA_COMMAND_TIMEOUT);
    wr(h,ICIS,2,2);
    uint32_t command=(codec<<28)|(node<<20)|(verb<<8)|payload;
    h->last_command=command; h->commands++;
    wr(h,ICOI,4,command); wr(h,ICIS,2,1);
    if(!wait_bits(h,ICIS,2,3,2)) return fail(h,HDA_COMMAND_TIMEOUT);
    uint32_t status=rd(h,ICIS,2);
    if((status&4) && (((status>>4)&15)!=codec || (status&8)))
        return fail(h,HDA_RESPONSE_MISMATCH);
    h->last_response=rd(h,ICII,4); *response=h->last_response;
    wr(h,ICIS,2,2); return true;
}
bool hda_core_stop(struct hda_core *h) {
    if(!h||!h->io.read||!h->stream) return false;
    set_stream_control(h,stream_control(h)&~2u);
    if(!wait_bits(h,h->stream,2,2,0)) return fail(h,HDA_STREAM_TIMEOUT);
    h->playing=false; return true;
}
bool hda_core_start(struct hda_core *h,uint64_t bdl,uint32_t bytes,unsigned entries) {
    if(!h||!h->ready||!bdl||(bdl&127)||!bytes||(bytes&127)||entries<2||entries>256||
       (!h->dma64 && bdl>0xffffffffu)) return false;
    if(!hda_core_stop(h)) return false;
    set_stream_control(h,1);
    if(!wait_bits(h,h->stream,2,1,1)) return fail(h,HDA_STREAM_TIMEOUT);
    set_stream_control(h,0);
    if(!wait_bits(h,h->stream,2,1,0)) return fail(h,HDA_STREAM_TIMEOUT);
    wr(h,h->stream+3,1,0x1c);
    wr(h,h->stream+8,4,bytes); wr(h,h->stream+12,2,entries-1);
    wr(h,h->stream+18,2,0x11); /* 48 kHz, signed 16-bit, stereo. */
    wr(h,h->stream+24,4,(uint32_t)bdl); wr(h,h->stream+28,4,(uint32_t)(bdl>>32));
    /* Tag 1, output direction (for dedicated output descriptors), no IRQs. */
    set_stream_control(h,(1u<<20)|2);
    if(!wait_bits(h,h->stream,2,2,2)) return fail(h,HDA_STREAM_TIMEOUT);
    h->playing=true; h->last_position=0; return true;
}
bool hda_core_position(struct hda_core *h,uint32_t *position) {
    if(!h||!h->ready||!position) return false;
    if(rd(h,h->stream+3,1)&0x18) {
        (void)hda_core_stop(h); return fail(h,HDA_STREAM_ERROR);
    }
    h->last_position=rd(h,h->stream+4,4); *position=h->last_position; return true;
}
const char *hda_error_name(enum hda_error error) {
    static const char *names[]={"none","bad-argument","reset-timeout","command-timeout",
        "response-mismatch","stream-timeout","stream-error","unsupported-codec",
        "dma-memory","worker-unavailable","mmio-mapping","invalid-bar","pci-power"};
    return (unsigned)error<sizeof(names)/sizeof(names[0])?names[error]:"unknown";
}
