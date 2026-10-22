#ifndef SRVROS_BATTERY_RESET_POLICY_H
#define SRVROS_BATTERY_RESET_POLICY_H
#include <stdbool.h>
/* CPU0 caller snapshots under IRQ exclusion at the point of reset. An earlier
 * arm is intent only; it cannot authorize reset across later storage ownership.
 * Even a disposable image I/O owner must not be reset mid-operation. */
static inline bool battery_reset_allowed(bool armed,bool workspace,bool io) {
    return armed && !workspace && !io;
}
#endif
