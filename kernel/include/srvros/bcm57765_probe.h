#ifndef SRVROS_BCM57765_PROBE_H
#define SRVROS_BCM57765_PROBE_H

enum bcm57765_probe_result {
    BCM57765_REFUSED,
    BCM57765_INCOMPLETE,
    BCM57765_LINK_DOWN,
    BCM57765_LINK_UP,
    BCM57765_UNSAFE,
};

/* Run in the sole kernel address space with IRQs masked. */
enum bcm57765_probe_result bcm57765_probe(void);

#endif
