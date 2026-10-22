#ifndef SRVROS_BATTERY_QUERY_POLICY_H
#define SRVROS_BATTERY_QUERY_POLICY_H
#include <stdbool.h>
#include <stdint.h>
enum battery_query_policy { BATTERY_QUERY_LEGACY, BATTERY_QUERY_MODEL, BATTERY_QUERY_AIR72 };
enum battery_query_class {
    BATTERY_QUERY_NONE, BATTERY_QUERY_NOTIFY, BATTERY_QUERY_SHARED,
    BATTERY_QUERY_EMPTY, BATTERY_QUERY_UNKNOWN, BATTERY_QUERY_OS_BRANCH,
    BATTERY_QUERY_DEVICE_POLICY, BATTERY_QUERY_BAD_POLICY,
};
/* AIR72 requires the existing exact DSDT/SSDT/resource gate. Classify every
 * reachable branch, not a racy snapshot of shared GNVS OSYS. In particular Q10
 * is refused even under Darwin: its other branch acquires SMTX and writes SMBus.
 * QCF is bounded in BOTH states (no-op or Notify). This grants no new IO. */
static inline enum battery_query_class battery_query_classify(unsigned policy,uint8_t code) {
    if(policy>BATTERY_QUERY_AIR72)return BATTERY_QUERY_BAD_POLICY;
    if(!code)return BATTERY_QUERY_NONE;
    if(policy==BATTERY_QUERY_LEGACY)return BATTERY_QUERY_SHARED;
    if(policy==BATTERY_QUERY_MODEL)return code==0x20?BATTERY_QUERY_NOTIFY:BATTERY_QUERY_UNKNOWN;
    switch(code) {
    case 0x10:return BATTERY_QUERY_OS_BRANCH;
    case 0x20:case 0x21:return BATTERY_QUERY_SHARED;
    case 0x40:case 0x5a:case 0xcf:return BATTERY_QUERY_NOTIFY;
    case 0xce:case 0xd0:return BATTERY_QUERY_EMPTY;
    case 0x80:return BATTERY_QUERY_DEVICE_POLICY;
    default:return BATTERY_QUERY_UNKNOWN;
    }
}
static inline bool battery_query_rejected(enum battery_query_class c) {
    return c>=BATTERY_QUERY_UNKNOWN;
}
#endif
