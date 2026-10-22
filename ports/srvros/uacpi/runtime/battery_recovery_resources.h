#ifndef SRVROS_BATTERY_RECOVERY_RESOURCES_H
#define SRVROS_BATTERY_RECOVERY_RESOURCES_H
#include <srvros/ec_transport.h>
/* CPU0 snapshot with IRQs masked. Necessary refusal checks for the VM recovery
 * studies, NOT authority for native recovery: firmware can change FACS after a
 * read, and this does not prove independence of NVS/SMI/AML side effects. */
struct battery_recovery_resources {
    bool known,lease,ec_poisoned,sampler_active,sampler_waiting,smbus_busy;
    bool global_api,global_aml,global_mutex_busy,global_pending;
    enum ec_phase ec_phase;
    uint32_t facs_bits;
};
enum battery_recovery_block {
    BATTERY_RECOVERY_UNKNOWN=1u, BATTERY_RECOVERY_LEASE=2u,
    BATTERY_RECOVERY_EC=4u, BATTERY_RECOVERY_SAMPLE=8u,
    BATTERY_RECOVERY_SMBUS=16u, BATTERY_RECOVERY_GLOBAL_OS=32u,
    BATTERY_RECOVERY_GLOBAL_FIRMWARE=64u
};
static inline unsigned battery_recovery_blockers(struct battery_recovery_resources r) {
    unsigned blocked=0;
    if(!r.known)blocked|=BATTERY_RECOVERY_UNKNOWN;
    if(r.lease)blocked|=BATTERY_RECOVERY_LEASE;
    if(r.ec_poisoned || (r.ec_phase!=EC_IDLE && r.ec_phase!=EC_DONE))blocked|=BATTERY_RECOVERY_EC;
    if(r.sampler_active || r.sampler_waiting)blocked|=BATTERY_RECOVERY_SAMPLE;
    if(r.smbus_busy)blocked|=BATTERY_RECOVERY_SMBUS;
    if(r.global_api || r.global_aml || r.global_mutex_busy || r.global_pending)blocked|=BATTERY_RECOVERY_GLOBAL_OS;
    if(r.facs_bits&3)blocked|=BATTERY_RECOVERY_GLOBAL_FIRMWARE;
    return blocked;
}
#endif
