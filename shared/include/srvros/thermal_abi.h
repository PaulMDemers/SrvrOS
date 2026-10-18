#ifndef SRVROS_THERMAL_ABI_H
#define SRVROS_THERMAL_ABI_H
#include <stdint.h>
#define SRV_THERMAL_VERSION 1
#define SRV_THERMAL_DISABLED 0
#define SRV_THERMAL_UNAVAILABLE 1
#define SRV_THERMAL_ACTIVE 2
#define SRV_THERMAL_CORE_VALID 1u
#define SRV_THERMAL_PACKAGE_VALID 2u
#define SRV_THERMAL_THROTTLING 4u
#define SRV_THERMAL_CRITICAL 8u
#define SRV_THERMAL_TARGET_VALID 16u
struct srv_thermal_status {
    uint32_t version,state,flags,cpu_model;
    uint64_t sampled_us,samples;
    int32_t core_mc,package_mc,target_mc;
    uint32_t reserved;
    uint64_t raw_target,raw_core,raw_package;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_thermal_status)==72,"thermal snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_thermal_status)==72,"thermal snapshot wire layout");
#endif
#endif
