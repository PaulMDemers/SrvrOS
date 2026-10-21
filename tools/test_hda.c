#include <srvros/hda_core.h>
#include <srvros/hda_codec.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
struct model {
    uint8_t reg[4096];
    uint64_t time, reset_asserted, reset_released;
    uint32_t vendor, subsystem, powered_nodes;
    uint32_t gpio_mask, gpio_direction, gpio_data;
    uint16_t amp[32], coefficient[64];
    uint8_t pin_control[32], stream[32], coefficient_index;
    unsigned writes, mutations, gpio_writes, pauses, completed, power_delay;
    bool reset_stuck, command_stuck, stop_stuck, frozen_clock, wrong_response, bad_connection;
    bool bad_bass, bad_pin, bad_revision;
    bool stale_cleared, run_tagged, no_pcm, no_stereo, unplugged;
};
static uint32_t get(struct model *m,unsigned off,unsigned n) { uint32_t v=0;memcpy(&v,m->reg+off,n);return v; }
static void put(struct model *m,unsigned off,unsigned n,uint32_t v) { memcpy(m->reg+off,&v,n); }
static uint32_t rd(void *ctx,unsigned off,unsigned n) { return get(ctx,off,n); }
static uint32_t response(struct model *m,uint32_t cmd) {
    unsigned node=(cmd>>20)&127,op=(cmd>>8)&0xfff,value=cmd&255;
    bool apple=m->vendor==0x10134208;
    if((op&0x700)!=0x700) { op&=0xf00;value=cmd&65535; }
    if(op==0xf00) {
        if(node==0) { if(value==0) return m->vendor;if(value==2) return m->bad_revision?0:0x100300;if(value==4) return 0x10001; }
        if(node==1) { if(value==5) return 1; }
        if(value==9) {
            if(apple) return node==0x10?0x40058d:(node==0x12||node==0x13)?0x400501:0xd043d&~(m->no_stereo?1u:0);
            return node==3?0x40010d:0x1d;
        }
        if(value==12) return apple&&(node==0x12||node==0x13)?0x50:0x1c;
        if(value==14) return 1;
        if(value==10) return apple?0x001e07f0:(1<<17)|(1<<6);
        if(value==11) return m->no_pcm?2:apple?3:1;
        if(value==18) return apple?(node==0x10?0x80034242:0x80017f7f):0x80034a4a;
    }
    if(op==0xf20) return m->subsystem;
    if(op==0xf02) return m->bad_connection||(m->bad_bass&&node==0x13)?7:(node==0x12?3:node==0x13?4:2);
    if(op==0xf1c) return m->bad_pin?0:node==0x12?0x90100110:node==0x13?0x90100112:0x002b4020;
    if(op==0xf15) return m->gpio_data;
    if(op==0xf16) return m->gpio_mask;
    if(op==0xf17) return m->gpio_direction;
    if(op==0x715) m->gpio_data=value;
    if(op==0x716) m->gpio_mask=value;
    if(op==0x717) m->gpio_direction=value;
    if(op==0x705 && value==0 && node<32) m->powered_nodes|=1u<<node;
    if(op==0x707 && apple) assert(m->powered_nodes&(1u<<node));
    if(op==0xf05) return m->power_delay?(m->power_delay--,0x30):0;
    if(op==0xf09) return m->unplugged?0:0x80000000;
    if(op==0x300) { assert(node<32);m->amp[node]=value; }
    if(op==0x707) { assert(node<32);m->pin_control[node]=value; }
    if(op==0x706) { assert(node<32);m->stream[node]=value; }
    if(op==0x500) { assert(node==0x24&&value<64);m->coefficient_index=value; }
    if(op==0x400) { assert(node==0x24);m->coefficient[m->coefficient_index]=value; }
    if(op<0x800) { m->mutations++;if(op>=0x715&&op<=0x717) m->gpio_writes++; }
    return 0;
}
static void wr(void *ctx,unsigned off,unsigned n,uint32_t v) {
    struct model *m=ctx;m->writes++;
    if(off==8) {
        if(m->reset_stuck) return;
        if(!(v&1)) m->reset_asserted=m->time;
        else m->reset_released=m->time;
    }
    if(off==0x68) {
        uint32_t old=get(m,off,2);
        if(v&2) { old&=~2u;m->stale_cleared=true; }
        if(v&1) {
            assert(m->stale_cleared);assert(m->time-m->reset_released>=521);
            old|=1;
            if(!m->command_stuck) {
                put(m,0x64,4,response(m,get(m,0x60,4)));
                old=m->wrong_response?0x16:2;m->completed++;
            }
        }
        put(m,off,n,old);return;
    }
    if(off==0xc0 && n==2) {
        if(m->stop_stuck && !(v&2) && (get(m,off,2)&2)) v|=2;
        if(v&2) m->run_tagged=(get(m,off+2,1)&0xf0)==0x10;
    }
    if(off==0xc3) { assert(n==1);put(m,off,1,get(m,off,1)&~v);return; }
    put(m,off,n,v);
}
static uint64_t now(void *ctx) { struct model *m=ctx;if(!m->frozen_clock) m->time+=10;return m->time; }
static void relax(void *ctx) { ((struct model *)ctx)->pauses++; }
static struct hda_io io(struct model *m) { return (struct hda_io){m,rd,wr,now,relax}; }
static void init(struct model *m) {
    memset(m,0,sizeof(*m));put(m,0,2,0x2201);put(m,8,4,1);put(m,14,2,1);
    m->vendor=0x1af40012;m->subsystem=m->vendor;
    m->gpio_mask=m->gpio_direction=0x30;m->gpio_data=0x20;
}
int main(void) {
    struct model m;struct hda_core h;struct hda_codec c;uint32_t v;
    init(&m);assert(hda_core_reset(&h,io(&m)));assert(h.stream==0xc0&&h.codecs==1);
    assert(m.reset_released-m.reset_asserted>=100);
    put(&m,0x68,2,2);put(&m,0x64,4,0xdeadbeef);
    assert(hda_core_verb(&h,0,0,0xf00,0,&v)&&v==0x1af40012);
    unsigned commands=m.completed;
    assert(!hda_core_verb(&h,15,0,0xf00,0,&v)&&m.completed==commands);
    assert(!hda_core_start(&h,3,4096,2));assert(!hda_core_start(&h,4096,4096,1));
    assert(hda_core_start(&h,4096,4096,2)&&h.playing&&m.run_tagged);
    assert(get(&m,0xc8,4)==4096&&get(&m,0xcc,2)==1&&get(&m,0xd2,2)==0x11);
    put(&m,0xc4,4,512);assert(hda_core_position(&h,&v)&&v==512);
    assert(hda_core_stop(&h)&&!h.playing);
    assert(hda_codec_discover(&c,&h,HDA_GENERIC)&&c.available&&!c.apple);
    assert(c.muted);assert(hda_codec_mute(&c,false)&&!c.muted);assert(hda_codec_mute(&c,true)&&c.muted);
    assert(!hda_codec_route(&c,HDA_HEADPHONES));
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b7e00;m.power_delay=3;
    assert(hda_core_reset(&h,io(&m)));assert(hda_codec_discover(&c,&h,HDA_AIR7_2)&&c.apple&&m.gpio_writes==3);
    assert((m.powered_nodes&((1u<<1)|(1u<<2)|(1u<<3)|(1u<<16)|(1u<<18)))==((1u<<1)|(1u<<2)|(1u<<3)|(1u<<16)|(1u<<18)));
    assert(m.gpio_mask==0x31&&m.gpio_direction==0x31&&m.gpio_data==0x21);
    assert(m.coefficient[0x33]==1&&m.coefficient[0x34]==0x1c01);
    assert(m.pin_control[0x12]==0x40&&m.pin_control[0x10]==0&&m.stream[3]==0x10&&m.stream[2]==0);
    assert(m.amp[3]==0xb0ff&&m.amp[0x12]==0); /* Captured speaker pin has no amplifier. */
    assert(hda_codec_mute(&c,false)&&m.amp[3]==0xb07f);
    assert(hda_codec_mute(&c,true)&&m.amp[3]==0xb0ff);
    bool plugged=false;assert(hda_codec_headphones(&c,&plugged)&&plugged);
    assert(hda_codec_route(&c,HDA_HEADPHONES)&&c.output==HDA_HEADPHONES);
    assert(m.gpio_data==0x20&&m.pin_control[0x12]==0&&m.pin_control[0x10]==0xc0);
    assert(m.stream[2]==0x10&&m.stream[3]==0&&m.amp[0x10]==0xb0c2);
    assert(hda_codec_mute(&c,false)&&m.amp[2]==0xb07f&&m.amp[0x10]==0xb042);
    m.unplugged=true;assert(hda_codec_headphones(&c,&plugged)&&!plugged);
    assert(hda_codec_route(&c,HDA_SPEAKERS)&&m.gpio_data==0x21);
    unsigned route_writes=m.writes;h.playing=true;
    assert(!hda_codec_route(&c,HDA_HEADPHONES)&&m.writes==route_writes);h.playing=false;
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b7e00;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_GENERIC)&&m.mutations==0);
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b7e00;m.no_pcm=true;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_AIR7_2)&&m.mutations==0);
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b7e00;m.no_stereo=true;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_AIR7_2)&&m.mutations==0);
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b7e00;m.power_delay=100;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_AIR7_2)&&h.error==HDA_POWER&&m.gpio_writes==0);
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b0000;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_AIR7_2)&&m.mutations==0);
    init(&m);m.bad_connection=true;
    assert(hda_core_reset(&h,io(&m)));assert(!hda_codec_discover(&c,&h,HDA_GENERIC)&&m.mutations==0);
    init(&m);m.vendor=0x10134208;m.subsystem=0x106b5e00;
    assert(hda_core_reset(&h,io(&m)) && hda_codec_discover(&c,&h,HDA_PRO11_2));
    assert(c.routes==3 && m.pin_control[0x12]==0x40 && m.pin_control[0x13]==0x40);
    assert(m.stream[3]==0x10 && m.stream[4]==0x10 && m.stream[2]==0);
    assert(m.amp[3]==0xb0ff && m.amp[4]==0xb0ff);
    assert(hda_codec_mute(&c,false) && m.amp[3]==0xb07f && m.amp[4]==0xb07f);
    assert(hda_codec_mute(&c,true) && m.amp[3]==0xb0ff && m.amp[4]==0xb0ff);
    assert(hda_codec_route(&c,HDA_HEADPHONES));
    assert(m.pin_control[0x12]==0 && m.pin_control[0x13]==0 && m.pin_control[0x10]==0xc0);
    assert(m.stream[3]==0 && m.stream[4]==0 && m.stream[2]==0x10 && m.gpio_data==0x20);
    assert(hda_codec_mute(&c,false) && m.amp[2]==0xb07f && m.amp[4]==0xb0ff);
    assert(hda_codec_route(&c,HDA_SPEAKERS) && m.gpio_data==0x21 && m.stream[4]==0x10);
    for(unsigned fault=0;fault<6;fault++) {
        init(&m);m.vendor=0x10134208;m.subsystem=0x106b5e00;
        if(fault==0)m.bad_bass=true;
        if(fault==1)m.bad_pin=true;
        if(fault==2)m.bad_revision=true;
        if(fault==3)m.subsystem=0x106b7e00;
        if(fault==4)m.subsystem=0x106b6c00;
        assert(hda_core_reset(&h,io(&m)));
        assert(!hda_codec_discover(&c,&h,fault==5?HDA_AIR7_2:HDA_PRO11_2) && m.mutations==0);
    }
    init(&m);m.reset_stuck=true;assert(!hda_core_reset(&h,io(&m))&&h.error==HDA_RESET_TIMEOUT);
    init(&m);m.frozen_clock=true;assert(!hda_core_reset(&h,io(&m))&&m.pauses<=2000000);
    init(&m);assert(hda_core_reset(&h,io(&m)));m.command_stuck=true;
    assert(!hda_core_verb(&h,0,0,0xf00,0,&v)&&h.error==HDA_COMMAND_TIMEOUT&&!h.ready);
    unsigned writes=m.writes;assert(!hda_core_verb(&h,0,0,0xf00,0,&v)&&m.writes==writes);
    init(&m);assert(hda_core_reset(&h,io(&m)));m.wrong_response=true;
    assert(!hda_core_verb(&h,0,0,0xf00,0,&v)&&h.error==HDA_RESPONSE_MISMATCH);
    init(&m);assert(hda_core_reset(&h,io(&m)));assert(hda_core_start(&h,4096,4096,2));
    m.stop_stuck=true;assert(!hda_core_stop(&h)&&h.playing&&!h.ready);
    init(&m);assert(hda_core_reset(&h,io(&m)));assert(hda_core_start(&h,4096,4096,2));
    put(&m,0xc3,1,0x10);assert(!hda_core_position(&h,&v)&&h.error==HDA_STREAM_ERROR&&!h.playing);
    puts("hda: reset timing, codec profiles, stale responses, DMA and bounded fault paths passed");
}
