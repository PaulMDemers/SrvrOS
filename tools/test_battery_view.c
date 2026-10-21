#include <srvros/battery_snapshot.h>
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
    puts("battery-view: PASS cached ABI, disabled/stopping, freshness, validity and charge presentation");
}
