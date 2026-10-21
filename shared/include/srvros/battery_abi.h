#ifndef SRVROS_BATTERY_ABI_H
#define SRVROS_BATTERY_ABI_H
#include <stdint.h>
#define SRV_BATTERY_VERSION 1u
enum srv_battery_state { SRV_BATTERY_DISABLED,SRV_BATTERY_STARTING,SRV_BATTERY_ACTIVE,
    SRV_BATTERY_STOPPING,SRV_BATTERY_STOPPED,SRV_BATTERY_UNAVAILABLE,SRV_BATTERY_FAULTED };
#define SRV_BATTERY_VALID 1u
#define SRV_BATTERY_FRESH 2u
#define SRV_BATTERY_STALE 4u
#define SRV_BATTERY_AC_PRESENT 8u
#define SRV_BATTERY_SAMPLING 16u
#define SRV_BATTERY_SOC_VALID 1u
#define SRV_BATTERY_VOLTAGE_VALID 2u
#define SRV_BATTERY_CURRENT_VALID 4u
#define SRV_BATTERY_CAPACITY_VALID 8u
#define SRV_BATTERY_TEMPERATURE_VALID 16u
#define SRV_BATTERY_TIME_EMPTY_VALID 32u
#define SRV_BATTERY_TIME_FULL_VALID 64u
#define SRV_BATTERY_CAPACITY_UAH 1u
#define SRV_BATTERY_CAPACITY_UWH 2u
/* Read-only snapshot. Reads never start hardware work or wait for a sample.
 * VALID retains the last observation; only FRESH denotes current data.
 * Capacity units are explicit; time estimates are firmware minutes, not promises.
 * Zero-valued fields require their validity bit. UINT64_MAX age means no sample. */
struct srv_battery_status {
    uint32_t version,size,state,flags;
    uint32_t error,valid,soc,capacity_unit;
    uint64_t epoch,generation,age_us,ac_sampled_us,sampled_us;
    uint64_t voltage_uv;
    int64_t current_ua;
    uint64_t remaining_capacity,full_capacity,design_capacity;
    int32_t temperature_mc;
    uint32_t minutes_empty,minutes_full,reserved;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_battery_status)==128,"battery snapshot layout");
#else
_Static_assert(sizeof(struct srv_battery_status)==128,"battery snapshot layout");
#endif
#endif
