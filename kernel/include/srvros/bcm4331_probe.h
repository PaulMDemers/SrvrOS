#ifndef SRVROS_BCM4331_PROBE_H
#define SRVROS_BCM4331_PROBE_H

#include <stdbool.h>

enum bcm4331_probe_result {
    BCM4331_REFUSED,
    BCM4331_INCOMPLETE,
    BCM4331_TOPOLOGY_OK,
    BCM4331_FIRMWARE_OK,
    BCM4331_UNSAFE,
};

/* Isolated boot only, with IRQs masked and the sole kernel address space. */
enum bcm4331_probe_result bcm4331_probe(bool start_firmware);

#endif
