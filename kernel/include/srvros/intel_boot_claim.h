#ifndef SRVROS_INTEL_BOOT_CLAIM_H
#define SRVROS_INTEL_BOOT_CLAIM_H
#include <stdbool.h>
#include <srvros/pci.h>

/* Boot CPU only, before competing clients exist. Immutable until reset: no
 * release, transfer, runtime concurrency or hardware-ownership proof. The
 * blocked policy reserves no device and admits neither participating client.
 * A software claim grants no GGTT interval, DMA, HDA, firmware or write policy. */
enum intel_boot_client {INTEL_BOOT_LEGACY=1, INTEL_BOOT_OBSERVER=2, INTEL_BOOT_BLOCKED=3};
bool intel_boot_select(unsigned client);
bool intel_boot_claim(unsigned client,const struct pci_device *);
bool intel_boot_check(unsigned client,const struct pci_device *);
unsigned intel_boot_policy(void);
unsigned intel_boot_claimed_client(void);
#endif
