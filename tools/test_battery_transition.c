#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_transition.h"
static struct sbs_snapshot sample(unsigned generation) {
    return (struct sbs_snapshot){.generation=generation,.started_us=generation*2000000ull+1,
        .completed_us=generation*2000000ull+100,.valid=15,.relative_soc=86,.current_ua=-250000,
        .voltage_uv=12000000,.full_capacity=5000000,.capacity_unit=SBS_CAPACITY_UAH};
}
static int step(struct battery_transition *t,unsigned gen,bool ac,bool coherent) {
    struct sbs_snapshot s=sample(gen);s.current_ua=ac?0:-250000;
    return battery_transition_step(t,&s,ac,coherent,gen*2000000ull,s.completed_us+1);
}
int main(void) {
    struct battery_transition t={0};
    for(unsigned g=1;g<=9;g++)assert(step(&t,g,g<=3||g>6,true)==(g==9));
    assert(t.phase==3&&!t.failed&&t.first_generation==7);
    t=(struct battery_transition){0};
    assert(!step(&t,1,true,true)&&!step(&t,2,true,false));assert(t.stable==0);
    assert(!step(&t,3,true,true)&&!step(&t,4,false,true));assert(t.stable==0);
    for(unsigned g=5;g<=7;g++)assert(!step(&t,g,true,true));
    assert(t.phase==1);
    struct sbs_snapshot s=sample(8);s.current_ua=0;
    assert(!battery_transition_step(&t,&s,false,true,16000000,s.completed_us+1)&&t.stable==0);
    s=sample(9);s.current_ua=250000;
    assert(!battery_transition_step(&t,&s,false,true,18000000,s.completed_us+1)&&t.stable==0);
    for(unsigned g=10;g<=12;g++)assert(!step(&t,g,false,true));
    assert(t.phase==2);
    for(unsigned g=13;g<=15;g++)assert(step(&t,g,true,true)==(g==15));
    for(unsigned bad=0;bad<12;bad++) {
        t=(struct battery_transition){0};s=sample(1);uint64_t now=s.completed_us+1,ac=2000000;
        switch(bad){case 0:now=BATTERY_TRANSITION_WAIT_US;break;case 1:s.valid=11;break;
            case 2:s.generation=2;break;case 3:ac=s.started_us+1;break;case 4:s.completed_us=now+1;break;
            case 5:s.relative_soc=101;break;case 6:now=s.completed_us+1000001;break;
            case 7:t.last_now=now+1;break;case 8:s.started_us=s.completed_us+1;break;
            case 9:s.voltage_uv=0;break;case 10:s.full_capacity=0;break;case 11:s.capacity_unit=0;break;}
        assert(battery_transition_step(&t,&s,true,true,ac,now)==-1&&t.failed);
        assert(step(&t,1,true,true)==-1);
    }
    t=(struct battery_transition){0};assert(!step(&t,1,true,true));assert(step(&t,1,true,true)==-1);
    t=(struct battery_transition){0};
    struct battery_service_status c={.flags=BATTERY_CACHE_FRESH|BATTERY_CACHE_AC_PRESENT};
    for(unsigned g=1;g<=3;g++) {
        c.sample=sample(g);c.sample.current_ua=0;c.ac_sampled_us=g*2000000ull;
        assert(!battery_transition_cached_step(&t,&c,c.sample.completed_us+1));
        assert(!battery_transition_cached_step(&t,&c,c.sample.completed_us+2));
    }
    assert(t.phase==1);
    c.sample=sample(5);c.ac_sampled_us=10000000;c.flags=BATTERY_CACHE_FRESH;
    assert(!battery_transition_cached_step(&t,&c,c.sample.completed_us+1)&&t.stable==1);
    c.flags=0;assert(!battery_transition_cached_step(&t,&c,c.sample.completed_us+2)&&!t.stable);
    for(unsigned g=6;g<=11;g++) {
        c.sample=sample(g);c.ac_sampled_us=g*2000000ull;c.flags=BATTERY_CACHE_FRESH;
        if(g>=9){c.flags|=BATTERY_CACHE_AC_PRESENT;c.sample.current_ua=0;}
        assert(battery_transition_cached_step(&t,&c,c.sample.completed_us+1)==(g==11));
    }
    assert(t.phase==3);
    t=(struct battery_transition){0};c.flags=0;
    assert(battery_transition_cached_step(&t,&c,BATTERY_TRANSITION_WAIT_US)==-1);
    t=(struct battery_transition){.last_generation=5};c.sample=sample(4);c.flags=BATTERY_CACHE_FRESH;
    assert(battery_transition_cached_step(&t,&c,c.sample.completed_us+1)==-1);
    puts("battery-transition: PASS consecutive coherent phases, discharge sign, stale/replayed samples and bounded waits");
}
