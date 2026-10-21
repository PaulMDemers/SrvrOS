#ifndef SRVROS_BATTERY_SNAPSHOT_H
#define SRVROS_BATTERY_SNAPSHOT_H
#include <srvros/battery_abi.h>
#include <srvros/battery_service.h>
/* Pure conversion: do not copy private structure padding or raw firmware words. */
static inline struct srv_battery_status battery_snapshot(const struct battery_service_status *s) {
    return (struct srv_battery_status){.version=SRV_BATTERY_VERSION,.size=sizeof(struct srv_battery_status),
        .state=s->state,.flags=s->flags,.error=s->error,.valid=s->sample.valid,
        .soc=s->sample.relative_soc,.capacity_unit=s->sample.capacity_unit,
        .epoch=s->epoch,.generation=s->sample.generation,.age_us=s->age_us,
        .ac_sampled_us=s->ac_sampled_us,.sampled_us=s->sample.completed_us,
        .voltage_uv=s->sample.voltage_uv,.current_ua=s->sample.current_ua,
        .remaining_capacity=s->sample.remaining_capacity,.full_capacity=s->sample.full_capacity,
        .design_capacity=s->sample.design_capacity,.temperature_mc=s->sample.temperature_mc,
        .minutes_empty=s->sample.minutes_empty,.minutes_full=s->sample.minutes_full};
}
_Static_assert((unsigned)BATTERY_SERVICE_FAULTED==(unsigned)SRV_BATTERY_FAULTED,"state ABI");
_Static_assert(BATTERY_CACHE_SAMPLING==SRV_BATTERY_SAMPLING,"cache ABI");
_Static_assert(SBS_VALID_TIME_FULL==SRV_BATTERY_TIME_FULL_VALID,"sample ABI");
#endif
