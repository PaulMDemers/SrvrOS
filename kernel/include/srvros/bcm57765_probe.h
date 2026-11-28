#ifndef SRVROS_BCM57765_PROBE_H
#define SRVROS_BCM57765_PROBE_H

#include <stdbool.h>

enum bcm57765_probe_result {
    BCM57765_REFUSED,
    BCM57765_INCOMPLETE,
    BCM57765_LINK_DOWN,
    BCM57765_LINK_UP,
    BCM57765_DHCP_OK,
    BCM57765_UNSAFE,
};

/* Run in the sole kernel address space with IRQs masked. */
enum bcm57765_probe_result bcm57765_probe(void);
/* Packet trial enters and returns with IRQs masked. No resident worker. */
enum bcm57765_probe_result bcm57765_packet_trial(void);
/* Start before the desktop with IRQs masked. Finish after the desktop exits
 * with IRQs enabled. Reports whether both cable-connected rounds passed. */
bool bcm57765_desktop_start(void);
bool bcm57765_desktop_finish(bool *retired);

enum bcm57765_wired_trial {
    BCM57765_WIRED_NORMAL,
    BCM57765_WIRED_CONNECTED,
    BCM57765_WIRED_DISCONNECTED,
};

/* Ordinary startup and the two attended qualification recipes use this path. */
bool bcm57765_wired_start(enum bcm57765_wired_trial trial);
bool bcm57765_wired_finish(bool *retired);
bool bcm57765_wired_fence(void);

#endif
