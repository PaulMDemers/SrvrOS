#ifndef SRVROS_DESK_BATTERY_H
#define SRVROS_DESK_BATTERY_H
#include <srvros/battery_abi.h>
#include <srvros/battery_control_abi.h>
struct desk_battery_view { char label[32],detail[128];int fresh; };
/* Pure presentation; no syscalls or hardware access. Never present stale SOC as live. */
struct desk_battery_view desk_battery_view(const struct srv_battery_status *status);
struct desk_battery_controls { int start,stop;const char *message; };
struct desk_battery_controls desk_battery_controls(const struct srv_battery_status *,const struct srv_battery_control *);
/* Session-local, cached-only warning policy. Fresh AC or recovery above 25%
 * rearms low charge; recovery above 10% rearms critical charge. Stale/off
 * readings neither warn nor rearm. Returns 0, 1 (<=20%) or 2 (<=5%). */
struct desk_battery_alert { unsigned warned; };
unsigned desk_battery_alert_update(struct desk_battery_alert *,const struct srv_battery_status *);
#endif
