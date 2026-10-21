#ifndef SRVROS_DESK_BATTERY_H
#define SRVROS_DESK_BATTERY_H
#include <srvros/battery_abi.h>
struct desk_battery_view { char label[32],detail[128];int fresh; };
/* Pure presentation; no syscalls or hardware access. Never present stale SOC as live. */
struct desk_battery_view desk_battery_view(const struct srv_battery_status *status);
#endif
