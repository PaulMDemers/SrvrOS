#include <assert.h>
#include <stdio.h>
#include "../ports/srvros/uacpi/runtime/battery_event_accounting.h"
int main(void) {
    struct battery_event_accounting a={.last_log_us=100};struct battery_event_totals t;
    assert(!battery_event_take(&a,5000100,false,&t));
    for(unsigned i=0;i<10000;i++)battery_event_record(&a,i%2==0,i%2==0,i%4==0);
    assert(!battery_event_take(&a,99,false,&t));
    assert(!battery_event_take(&a,5000099,false,&t));
    assert(battery_event_take(&a,5000100,false,&t));
    assert(t.events==10000 && t.alarms==5000 && t.acknowledgements==5000);
    assert(t.unknown==2500 && t.invalidations==2500 && !t.saturated);
    assert(!battery_event_take(&a,5000100,false,&t));
    battery_event_record(&a,false,false,false);
    assert(battery_event_take(&a,50000000,false,&t));
    assert(!battery_event_take(&a,50000000,false,&t));
    battery_event_record(&a,true,true,true);
    assert(battery_event_take(&a,50000001,true,&t) && t.events==10002);
    assert(battery_event_take(&a,50000001,true,&t)); /* Final always states totals. */
    a.totals=(struct battery_event_totals){UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX,false};
    battery_event_record(&a,true,true,true);battery_event_record(&a,true,true,false);
    assert(battery_event_take(&a,50000002,true,&t) && t.saturated);
    assert(t.events==UINT64_MAX && t.alarms==UINT64_MAX && t.acknowledgements==UINT64_MAX);
    assert(t.unknown==UINT64_MAX && t.invalidations==UINT64_MAX);
    a=(struct battery_event_accounting){.last_log_us=UINT64_MAX-5000000};
    battery_event_record(&a,false,false,false);
    assert(!battery_event_take(&a,0,false,&t));
    assert(battery_event_take(&a,UINT64_MAX,false,&t));
    assert(t.events==1 && !t.saturated);
    puts("battery-event-accounting: PASS burst coalescing, final snapshot, clock and saturation");
}
