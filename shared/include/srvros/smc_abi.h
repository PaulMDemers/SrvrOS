#ifndef SRVROS_SMC_ABI_H
#define SRVROS_SMC_ABI_H
#include <stdint.h>
#define SRV_SMC_VERSION 1
#define SRV_SMC_DISABLED 0
#define SRV_SMC_UNAVAILABLE 1
#define SRV_SMC_STARTING 2
#define SRV_SMC_ACTIVE 3
#define SRV_SMC_FAULT 4
#define SRV_SMC_FAN_VALID 1u
struct srv_smc_status {
    uint32_t version,state,flags,fan_count;
    uint32_t fan_millirpm,error,phase,last_status;
    uint64_t sampled_us,samples,transactions,failures;
};
#ifdef __cplusplus
static_assert(sizeof(struct srv_smc_status)==64,"SMC snapshot wire layout");
#else
_Static_assert(sizeof(struct srv_smc_status)==64,"SMC snapshot wire layout");
#endif
#endif
