#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_startup_timing.h"
int main(void) {
    struct battery_startup_span s={0},nested={0};
    assert(battery_startup_begin(&s,100,10,true));
    assert(s.active && s.started_us==100 && s.argument==10);
    assert(battery_startup_begin(&nested,120,1,true));
    battery_startup_end(&nested,140);battery_startup_end(&s,180);
    assert(s.elapsed_us==80 && nested.elapsed_us==20 && !s.active);
    assert(!battery_startup_begin(&s,0,20,false));
    assert(s.calls==2 && s.requested==30 && s.samples==1 && s.elapsed_us==80);
    assert(battery_startup_begin(&s,200,30,true));battery_startup_end(&s,220);
    assert(s.elapsed_us==100 && s.max_us==80 && s.samples==2);
    battery_startup_end(&s,900);assert(s.elapsed_us==100);
    s=(struct battery_startup_span){0};
    for(unsigned i=0;i<130;i++) {
        bool sample=!(s.calls%64);
        if(battery_startup_begin(&s,i*10,16,sample))battery_startup_end(&s,i*10+2);
    }
    assert(s.calls==130 && s.samples==3 && s.requested==2080 && s.elapsed_us==6);
    puts("battery startup accounting: PASS");
}
