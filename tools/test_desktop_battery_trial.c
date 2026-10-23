#include "../userspace/deskd/battery_trial.h"
#include <assert.h>
#include <stdio.h>
static struct srv_battery_status sample(unsigned gen,unsigned ac) {
    return (struct srv_battery_status){.version=1,.size=128,.state=SRV_BATTERY_ACTIVE,
        .flags=SRV_BATTERY_VALID|SRV_BATTERY_FRESH|(ac?SRV_BATTERY_AC_PRESENT:0),
        .valid=15,.soc=97,.capacity_unit=1,.epoch=1,.generation=gen,.age_us=100,
        .ac_sampled_us=gen*2000000ull,.sampled_us=gen*2000000ull+100,
        .voltage_uv=12000000,.current_ua=ac?0:-500000,.full_capacity=5000000,.remaining_capacity=4800000};
}
int main(void) {
    struct desk_battery_trial t={0};
    for(unsigned phase=0;phase<3;phase++) {
        for(unsigned i=1;i<=3;i++) {
            struct srv_battery_status s=sample(phase*3+i,phase!=1);
            int got=desk_battery_trial_step(&t,&s,s.sampled_us+100);
            assert(got==(i==3));
            assert(!desk_battery_trial_step(&t,&s,s.sampled_us+100));
        }
        desk_battery_trial_advance(&t,t.last_now);
    }
    assert(t.phase==3);
    t=(struct desk_battery_trial){0};struct srv_battery_status s=sample(1,1);
    assert(!desk_battery_trial_step(&t,&s,s.sampled_us));
    s=sample(3,1);assert(!desk_battery_trial_step(&t,&s,s.sampled_us));assert(t.stable==1);
    s=sample(4,0);assert(!desk_battery_trial_step(&t,&s,s.sampled_us));assert(!t.stable);
    s=sample(5,1);s.flags|=SRV_BATTERY_STALE;assert(!desk_battery_trial_step(&t,&s,s.sampled_us));assert(!t.stable);
    s=sample(6,1);s.epoch=2;assert(desk_battery_trial_step(&t,&s,s.sampled_us)==-1);
    t=(struct desk_battery_trial){0};assert(desk_battery_trial_step(&t,NULL,DESK_BATTERY_PHASE_US)==-1);
    for(unsigned field=0;field<7;field++) {
        t=(struct desk_battery_trial){0};s=sample(1,1);
        if(field==0)s.soc=101;
        if(field==1)s.full_capacity=0;
        if(field==2)s.valid&=~SRV_BATTERY_CURRENT_VALID;
        if(field==3)s.capacity_unit=0;
        if(field==4)s.ac_sampled_us=s.sampled_us+1;
        if(field==5)s.age_us=5000001;
        if(field==6)s.current_ua=-1;
        assert(!desk_battery_trial_step(&t,&s,s.sampled_us));assert(!t.stable);
    }
    puts("desktop-battery-trial: PASS distinct samples, phase gating, stale/invalid/epoch refusal and deadline");
}
