#ifndef SRVROS_AHCI_H
#define SRVROS_AHCI_H

#include <stdbool.h>

bool ahci_init(void);
void ahci_print_status(void);
struct block_device;
/* Synchronous terminal diagnostics on the selected raw disk. No callbacks
 * or scheduler service turns are admitted by AHCI during the operation. */
bool ahci_run_polled(struct block_device *device,bool (*work)(void *),void *context);

#endif
