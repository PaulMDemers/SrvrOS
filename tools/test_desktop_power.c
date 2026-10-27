#include <assert.h>
#include <stdio.h>
#include "../userspace/deskd/power_trial.h"
static struct desk_power_trial settled(void) {
    struct desk_power_trial t={.origin=1000000};
    assert(desk_power_step(&t,2000000,0,0,0,1,0)==0);
    assert(desk_power_step(&t,3000000,1,1,1,1,0)==1 && t.stage==1);
    assert(desk_power_step(&t,32999999,1,1,2,1,0)==0);
    assert(desk_power_step(&t,33000000,1,1,3,1,0)==1 && t.stage==2);
    return t;
}
int main(void) {
    struct desk_power_trial t=settled();
    assert(desk_power_step(&t,93000000,1,1,10,1,0)==1 && t.stage==3);
    struct desk_power_trial apps=t;
    assert(desk_power_step(&t,153000000,1,1,20,1,6)==1 && t.stage==4);
    assert(desk_power_step(&t,213000000,1,1,30,1,6)==1 && t.stage==5);
    assert(t.start[0]==33000000 && t.end[2]==213000000 && t.generation[2]==30);
    t=apps;assert(desk_power_step(&t,153000000,1,1,20,0,6)==-1);
    t=apps;assert(desk_power_step(&t,153000000,1,1,20,1,5)==-1);
    t=settled();assert(desk_power_step(&t,93000000,1,1,3,1,0)==-1);
    t=settled();assert(desk_power_step(&t,34000000,1,0,4,1,0)==-1);
    t=settled();assert(desk_power_step(&t,32000000,1,1,4,1,0)==-1);
    t=settled();assert(desk_power_step(&t,98000001,1,1,10,1,0)==-1);
    t=(struct desk_power_trial){.origin=1};assert(desk_power_step(&t,30000002,0,0,0,1,0)==-1);
    t=(struct desk_power_trial){.origin=1};assert(desk_power_step(&t,30000002,1,1,1,1,0)==-1);
    puts("desktop-power: PASS phases, fresh generations, drain, cycle minimum and bounded timing");
}
