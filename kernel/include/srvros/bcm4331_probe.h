#ifndef SRVROS_BCM4331_PROBE_H
#define SRVROS_BCM4331_PROBE_H

#include <stdbool.h>

enum bcm4331_probe_result {
    BCM4331_REFUSED,
    BCM4331_INCOMPLETE,
    BCM4331_TOPOLOGY_OK,
    BCM4331_FIRMWARE_OK,
    BCM4331_BEACON_OK,
    BCM4331_ASSOCIATED,
    BCM4331_DHCP_OK,
    BCM4331_RECONNECT_OK,
    BCM4331_UNSAFE,
};

enum bcm4331_probe_mode {
    BCM4331_TOPOLOGY, BCM4331_FIRMWARE, BCM4331_CHANNEL10, BCM4331_STATION, BCM4331_STATION_DHCP,
    BCM4331_STATION_RECONNECT, BCM4331_STATION_DESKTOP,
};

/* Isolated boot only, with IRQs masked and the sole kernel address space. */
enum bcm4331_probe_result bcm4331_probe(enum bcm4331_probe_mode mode);

bool bcm4331_desktop_configured(void);
enum bcm4331_desktop_trial {
    BCM4331_DESKTOP_NORMAL, BCM4331_DESKTOP_SETTINGS, BCM4331_DESKTOP_SIGNAL_LOSS,
    BCM4331_DESKTOP_LOCAL_GAP, BCM4331_DESKTOP_RANGE,
};
bool bcm4331_desktop_start(enum bcm4331_desktop_trial trial);
void bcm4331_desktop_begin_automatic_trial(void);
bool bcm4331_desktop_finish(bool *retired);
bool bcm4331_desktop_fence(void);

#endif
