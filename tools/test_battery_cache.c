#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/battery_cache.h>

static struct sbs_snapshot sample(uint64_t generation) {
    return (struct sbs_snapshot){.generation=generation,.started_us=generation*2000000+1,
        .completed_us=generation*2000000+100,.valid=15,.relative_soc=86,
        .voltage_uv=12000000,.current_ua=-250000,.full_capacity=5000000,
        .remaining_capacity=4300000,.design_capacity=6000000,.capacity_unit=SBS_CAPACITY_UAH};
}
static bool publish(struct battery_cache *c,uint64_t generation,bool coherent) {
    struct sbs_snapshot s=sample(generation);
    return battery_cache_publish(c,&s,generation!=2,coherent,s.started_us-1,s.completed_us+1);
}
int main(void) {
    struct battery_cache c={0};struct battery_service_status out;
    battery_cache_read(&c,0,&out);
    assert(out.state==BATTERY_SERVICE_DISABLED&&out.flags==0&&out.age_us==UINT64_MAX);
    battery_cache_begin(&c,1);assert(!publish(&c,1,true));
    assert(c.value.attempts==0);c.value.state=BATTERY_SERVICE_ACTIVE;
    assert(publish(&c,1,true));struct battery_cache saved=c;
    battery_cache_read(&c,2000101,&out);
    assert(out.epoch==1&&out.flags==(BATTERY_CACHE_VALID|BATTERY_CACHE_FRESH|BATTERY_CACHE_AC_PRESENT));
    assert(out.age_us==1&&out.sample.relative_soc==86);
    assert(!memcmp(&c,&saved,sizeof(c))); /* Readers never mutate cache. */
    battery_cache_read(&c,2000100+BATTERY_SERVICE_FRESH_US,&out);
    assert(out.flags&BATTERY_CACHE_FRESH);
    battery_cache_read(&c,2000101+BATTERY_SERVICE_FRESH_US,&out);
    assert((out.flags&BATTERY_CACHE_STALE)&&!(out.flags&BATTERY_CACHE_FRESH));
    battery_cache_read(&c,1,&out);
    assert(out.age_us==UINT64_MAX&&(out.flags&BATTERY_CACHE_STALE));
    c.value.flags|=BATTERY_CACHE_SAMPLING;
    assert(!publish(&c,2,false));
    battery_cache_read(&c,4000101,&out);
    assert(out.sample.generation==1&&out.error==BATTERY_SERVICE_AC_CHANGED);
    assert(out.rejected==1&&(out.flags&BATTERY_CACHE_STALE)&&(out.flags&BATTERY_CACHE_SAMPLING));
    assert(!publish(&c,2,true)); /* An invalid observation cannot be replayed. */
    assert(publish(&c,3,true));
    battery_cache_read(&c,6000101,&out);
    assert(out.sample.generation==3&&out.error==BATTERY_SERVICE_OK&&(out.flags&BATTERY_CACHE_FRESH));
    c.value.state=BATTERY_SERVICE_STOPPING;assert(!publish(&c,4,true));
    battery_cache_read(&c,6000101,&out);
    assert(out.sample.generation==3&&(out.flags&BATTERY_CACHE_STALE));
    c.value.state=BATTERY_SERVICE_STOPPED;battery_cache_read(&c,6000101,&out);
    assert(!(out.flags&BATTERY_CACHE_FRESH));
    battery_cache_begin(&c,2);battery_cache_read(&c,8000000,&out);
    assert(out.epoch==2&&out.sample.generation==0&&out.flags==0&&out.attempts==0);
    for(unsigned bad=0;bad<10;bad++) {
        battery_cache_begin(&c,3);c.value.state=BATTERY_SERVICE_ACTIVE;
        assert(publish(&c,1,true));
        struct sbs_snapshot s=sample(2);uint64_t ac=s.started_us-1,now=s.completed_us+1;
        switch(bad) {
            case 0:s.generation=1;break;case 1:s.relative_soc=101;break;
            case 2:s.valid=7;break;case 3:s.voltage_uv=0;break;case 4:s.full_capacity=0;break;
            case 5:s.capacity_unit=0;break;case 6:ac=s.started_us+1;break;
            case 7:now=s.completed_us-1;break;case 8:now+=1000001;break;
            case 9:s.started_us=s.completed_us+1;break;
        }
        assert(!battery_cache_publish(&c,&s,false,true,ac,now));
        battery_cache_read(&c,now,&out);
        assert(out.sample.generation==1&&out.rejected==1&&out.error==BATTERY_SERVICE_BAD_SAMPLE);
        assert((out.flags&BATTERY_CACHE_STALE)&&!(out.flags&BATTERY_CACHE_FRESH));
    }
    puts("battery-cache: PASS freshness, invalid/coherence retention, replay, clock reversal and lifecycle isolation");
}
