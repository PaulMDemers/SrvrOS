#include <assert.h>
#include <stdio.h>
#include <srvros/battery_control.h>
_Static_assert(sizeof(struct srv_battery_control)==40,"battery command ABI size");
int main(void) {
    struct battery_control_mailbox m={0};
    struct battery_service_status s={.state=BATTERY_SERVICE_STOPPED,.epoch=4};
    assert(!battery_control_bind(&m,0));assert(battery_control_bind(&m,41));assert(!battery_control_bind(&m,42));
    assert(!battery_control_view(&m,42,4).available);
    assert(battery_control_enqueue(&m,42,SRV_BATTERY_START,4,&s)==SRV_BATTERY_CONTROL_UNAVAILABLE);
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_START,3,&s)==SRV_BATTERY_CONTROL_STALE);
    assert(battery_control_enqueue(&m,41,99,4,&s)==SRV_BATTERY_CONTROL_INVALID);
    assert(!battery_control_enqueue(&m,41,SRV_BATTERY_START,4,&s));
    assert(m.serial==1 && battery_control_view(&m,41,4).pending==SRV_BATTERY_START);
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_START,4,&s)==SRV_BATTERY_CONTROL_BUSY);
    assert(battery_control_take(&m)==SRV_BATTERY_START);assert(!battery_control_take(&m));
    battery_control_complete(&m,0);s.state=BATTERY_SERVICE_STARTING;s.epoch=5;
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_START,5,&s)==SRV_BATTERY_CONTROL_STATE);
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_STOP,4,&s)==SRV_BATTERY_CONTROL_STALE);
    assert(!battery_control_enqueue(&m,41,SRV_BATTERY_STOP,5,&s));
    assert(battery_control_take(&m)==SRV_BATTERY_STOP);battery_control_complete(&m,0);
    s.state=BATTERY_SERVICE_STOPPING;
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_START,5,&s)==SRV_BATTERY_CONTROL_STATE);
    s.state=BATTERY_SERVICE_STOPPED;
    assert(!battery_control_enqueue(&m,41,SRV_BATTERY_START,5,&s));
    battery_control_revoke(&m);assert(!battery_control_take(&m));
    assert(m.result==SRV_BATTERY_CONTROL_CANCELLED && !battery_control_view(&m,41,5).available);
    assert(battery_control_bind(&m,99));
    assert(battery_control_enqueue(&m,41,SRV_BATTERY_STOP,5,&s)==SRV_BATTERY_CONTROL_UNAVAILABLE);
    s.state=BATTERY_SERVICE_UNAVAILABLE;
    assert(!battery_control_enqueue(&m,99,SRV_BATTERY_START,5,&s));
    assert(battery_control_take(&m));battery_control_complete(&m,SRV_BATTERY_CONTROL_FAILED);
    assert(battery_control_view(&m,99,5).result==SRV_BATTERY_CONTROL_FAILED);
    m.serial=UINT64_MAX;assert(battery_control_enqueue(&m,99,SRV_BATTERY_START,5,&s)==SRV_BATTERY_CONTROL_FAILED);
    puts("battery-control: PASS scope, epoch, single queue, cancellation and retirement policy");
}
