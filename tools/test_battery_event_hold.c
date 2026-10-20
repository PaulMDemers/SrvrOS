#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_event_hold.h"
struct model { uint8_t enable,status;unsigned writes;bool ignore_mask,ignore_restore; };
static uint8_t read_port(void *ctx,uint16_t p) {
    struct model *m=ctx;assert(p==0x189c||p==0x188c);return p==0x189c?m->enable:m->status;
}
static void write_port(void *ctx,uint16_t p,uint8_t v) {
    struct model *m=ctx;m->writes++;
    assert(p==0x189c); /* The lease itself must never acknowledge status. */
    if((m->ignore_mask&&!v)||(m->ignore_restore&&v))return;
    m->enable=v;
}
static struct battery_event_hold configured(uint8_t original) {
    return (struct battery_event_hold){.status=0x188c,.enable=0x189c,.mask=0x40,.original=original};
}
int main(void) {
    for(unsigned bit=0;bit<8;bit++)for(unsigned original=0;original<=(1u<<bit);original+=(1u<<bit)) {
        uint8_t mask=1u<<bit;
        struct model m={.enable=original,.status=0xff};
        struct battery_event_ops io={&m,read_port,write_port};
        struct battery_event_hold h=configured(original);h.mask=mask;
        assert(battery_event_hold_begin(&h,io)&&h.active&&!m.enable);
        assert(!battery_event_hold_begin(&h,io)&&m.writes==1);
        for(unsigned v=0;v<256;v++) {
            uint8_t filtered=battery_event_hold_filter(&h,0x188c,true,v);
            assert(filtered==(v&(uint8_t)~mask));
            assert(battery_event_hold_filter(&h,0x188c,false,v)==filtered);
            assert(battery_event_hold_filter(&h,0x188d,true,v)==v);
            assert(battery_event_hold_filter(&h,0x189c,true,v)==v);
            m.status&=~filtered;assert(m.status&mask);
        }
        assert(h.protected_writes==128);
        assert(battery_event_hold_restore(&h,io)&&!h.active&&m.enable==original&&m.writes==2);
        assert(!battery_event_hold_restore(&h,io));
    }
    struct model m={.enable=0x40};struct battery_event_ops io={&m,read_port,write_port};
    struct battery_event_hold h=configured(0x40);
    m.enable=0x80;assert(!battery_event_hold_begin(&h,io)&&!h.active&&!m.writes);
    m.enable=0x40;m.ignore_mask=true;
    assert(!battery_event_hold_begin(&h,io)&&h.active&&h.poisoned);
    assert(!battery_event_hold_restore(&h,io)&&m.writes==1);
    m=(struct model){.enable=0x40};h=configured(0x40);
    assert(battery_event_hold_begin(&h,io));m.ignore_restore=true;
    assert(!battery_event_hold_restore(&h,io)&&h.poisoned&&h.active&&!m.enable);
    m=(struct model){.enable=0x40};h=configured(0x40);
    assert(battery_event_hold_begin(&h,io));m.enable=1;
    assert(!battery_event_hold_restore(&h,io)&&h.poisoned&&m.writes==1);
    h=(struct battery_event_hold){0};assert(battery_event_hold_begin(&h,io));assert(battery_event_hold_restore(&h,io));
    h.enable=1;assert(!battery_event_hold_begin(&h,io));
    h=configured(0x80);assert(!battery_event_hold_begin(&h,io));
    h=configured(0x40);h.status=h.enable;assert(!battery_event_hold_begin(&h,io));
    puts("battery-event-hold: PASS pending status preserved, exact restore, interference and readback failures contained");
}
