#ifndef SRVROS_BCM4331_PROBE_H
#define SRVROS_BCM4331_PROBE_H

enum bcm4331_probe_result {
    BCM4331_REFUSED,
    BCM4331_INCOMPLETE,
    BCM4331_TOPOLOGY_OK,
    BCM4331_FIRMWARE_OK,
    BCM4331_BEACON_OK,
    BCM4331_ASSOCIATED,
    BCM4331_DHCP_OK,
    BCM4331_UNSAFE,
};

enum bcm4331_probe_mode { BCM4331_TOPOLOGY, BCM4331_FIRMWARE, BCM4331_CHANNEL10, BCM4331_STATION, BCM4331_STATION_DHCP };

/* Isolated boot only, with IRQs masked and the sole kernel address space. */
enum bcm4331_probe_result bcm4331_probe(enum bcm4331_probe_mode mode);

#endif
