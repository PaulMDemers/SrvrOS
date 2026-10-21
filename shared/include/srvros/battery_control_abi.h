#ifndef SRVROS_BATTERY_CONTROL_ABI_H
#define SRVROS_BATTERY_CONTROL_ABI_H
#include <stdint.h>
#define SRV_BATTERY_CONTROL_VERSION 1u
enum { SRV_BATTERY_QUERY, SRV_BATTERY_START, SRV_BATTERY_STOP };
enum { SRV_BATTERY_CONTROL_OK=0, SRV_BATTERY_CONTROL_QUEUED=1,
    SRV_BATTERY_CONTROL_UNAVAILABLE=-1, SRV_BATTERY_CONTROL_BUSY=-2,
    SRV_BATTERY_CONTROL_STALE=-3, SRV_BATTERY_CONTROL_STATE=-4,
    SRV_BATTERY_CONTROL_INVALID=-5, SRV_BATTERY_CONTROL_FAILED=-6,
    SRV_BATTERY_CONTROL_CANCELLED=-7 };
/* Acceptance queues one request; it does not promise ACTIVE or STOPPED.
 * expected_epoch must match the cached lifetime for every mutation. */
struct srv_battery_control {
    uint32_t version,size,available,pending;
    uint64_t serial,epoch;
    int32_t result;
    uint32_t reserved;
};
#endif
