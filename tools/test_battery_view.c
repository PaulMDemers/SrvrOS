#include <srvros/battery_snapshot.h>
#include <srvros/battery_cache.h>
#include <srvros/desk_battery.h>
#include <assert.h>
#include <string.h>
#include <stdio.h>
int main(void) {
    struct battery_service_status cached={.state=BATTERY_SERVICE_DISABLED,.age_us=UINT64_MAX};
    struct srv_battery_status s=battery_snapshot(&cached);
    assert(s.size==128 && s.version==1 && s.age_us==UINT64_MAX && !s.reserved);
    struct desk_battery_view v=desk_battery_view(&s);assert(!v.fresh && !strcmp(v.label,"Battery off"));
    cached.state=BATTERY_SERVICE_ACTIVE;cached.flags=BATTERY_CACHE_VALID|BATTERY_CACHE_FRESH|BATTERY_CACHE_AC_PRESENT|BATTERY_CACHE_SAMPLING;
    cached.epoch=3;cached.sample.generation=9;cached.sample.valid=SBS_VALID_SOC|SBS_VALID_CURRENT;
    cached.sample.relative_soc=74;cached.sample.current_ua=123000;cached.sample.capacity_unit=SBS_CAPACITY_UAH;
    s=battery_snapshot(&cached);v=desk_battery_view(&s);
    assert(s.epoch==3 && s.generation==9 && s.capacity_unit==SRV_BATTERY_CAPACITY_UAH);
    assert(v.fresh && !strcmp(v.label,"AC · 74%") && strstr(v.detail,"Charging"));
    s.current_ua=-200000;s.flags&=~SRV_BATTERY_AC_PRESENT;v=desk_battery_view(&s);
    assert(v.fresh && !strcmp(v.label,"74%") && strstr(v.detail,"Discharging"));
    s.valid&=~SRV_BATTERY_CURRENT_VALID;v=desk_battery_view(&s);assert(strstr(v.detail,"On battery"));
    s.flags=SRV_BATTERY_VALID|SRV_BATTERY_STALE;v=desk_battery_view(&s);assert(!v.fresh && !strstr(v.label,"74"));
    s.flags|=SRV_BATTERY_FRESH;v=desk_battery_view(&s);assert(!v.fresh); /* contradictory flags */
    s.flags=SRV_BATTERY_VALID|SRV_BATTERY_FRESH;s.soc=101;assert(!desk_battery_view(&s).fresh);
    s.soc=74;s.valid=0;assert(!desk_battery_view(&s).fresh);
    s.valid=SRV_BATTERY_SOC_VALID;
    for(unsigned state=0;state<=SRV_BATTERY_FAULTED;state++) {
        s.state=state;assert(desk_battery_view(&s).fresh==(state==SRV_BATTERY_ACTIVE));
    }
    s.state=SRV_BATTERY_ACTIVE;s.version++;assert(!desk_battery_view(&s).fresh);
    s.version--;s.size--;assert(!desk_battery_view(&s).fresh);
    assert(!desk_battery_view(NULL).fresh);
    s.version=SRV_BATTERY_VERSION;s.size=sizeof(s);s.state=SRV_BATTERY_STOPPED;s.epoch=7;
    struct srv_battery_control c={.version=SRV_BATTERY_CONTROL_VERSION,.size=sizeof(c),.epoch=7};
    assert(!desk_battery_controls(&s,&c).start);
    c.available=1;assert(desk_battery_controls(&s,&c).start);
    c.pending=SRV_BATTERY_START;assert(!desk_battery_controls(&s,&c).start && !desk_battery_controls(&s,&c).stop);
    c.pending=0;s.state=SRV_BATTERY_STARTING;assert(desk_battery_controls(&s,&c).stop);
    s.state=SRV_BATTERY_ACTIVE;assert(desk_battery_controls(&s,&c).stop);
    s.state=SRV_BATTERY_STOPPING;assert(!desk_battery_controls(&s,&c).start && !desk_battery_controls(&s,&c).stop);
    s.state=SRV_BATTERY_STOPPED;c.epoch=6;assert(!desk_battery_controls(&s,&c).start);
    c.epoch=7;c.result=SRV_BATTERY_CONTROL_FAILED;assert(strstr(desk_battery_controls(&s,&c).message,"failed"));
    /* Exercise the real cache-to-ABI-to-view boundary, including expiry,
     * rejected AC coherence and a later recovery, not just hand-built flags. */
    struct battery_cache cache={0};battery_cache_begin(&cache,1);
    cache.value.state=BATTERY_SERVICE_ACTIVE;
    struct sbs_snapshot sample={.generation=1,.started_us=100,.completed_us=200,
        .valid=SBS_VALID_SOC|SBS_VALID_VOLTAGE|SBS_VALID_CURRENT|SBS_VALID_CAPACITY,
        .relative_soc=74,.voltage_uv=8000000,.full_capacity=5000000,.capacity_unit=SBS_CAPACITY_UAH};
    assert(battery_cache_publish(&cache,&sample,true,true,90,200));
    battery_cache_read(&cache,200+BATTERY_SERVICE_FRESH_US,&cached);
    s=battery_snapshot(&cached);assert(desk_battery_view(&s).fresh);
    battery_cache_read(&cache,201+BATTERY_SERVICE_FRESH_US,&cached);
    s=battery_snapshot(&cached);v=desk_battery_view(&s);
    assert(!v.fresh && !strcmp(v.label,"Battery stale") && !strstr(v.label,"74"));
    sample.generation=2;sample.started_us=7000100;sample.completed_us=7000200;
    assert(!battery_cache_publish(&cache,&sample,false,false,7000090,7000200));
    battery_cache_read(&cache,7000200,&cached);s=battery_snapshot(&cached);
    assert(!desk_battery_view(&s).fresh);
    sample.generation=3;sample.started_us=8000100;sample.completed_us=8000200;
    assert(battery_cache_publish(&cache,&sample,false,true,8000090,8000200));
    battery_cache_read(&cache,8000200,&cached);s=battery_snapshot(&cached);
    assert(desk_battery_view(&s).fresh && !strcmp(desk_battery_view(&s).label,"74%"));
    cache.value.state=BATTERY_SERVICE_UNAVAILABLE;
    battery_cache_read(&cache,8000200,&cached);s=battery_snapshot(&cached);
    assert(!desk_battery_view(&s).fresh && !strcmp(desk_battery_view(&s).label,"Battery —"));
    puts("battery-view: PASS cached ABI, disabled/stopping, freshness, validity and charge presentation");
}
