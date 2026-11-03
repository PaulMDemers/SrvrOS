#ifndef SRVROS_VIRTIO_BLOCK_H
#define SRVROS_VIRTIO_BLOCK_H
#include <stdint.h>
/* Explicit VM opt-in caller only, once at boot in kernel CR3 after PCI/block init.
 * Firmware execution must be finished: this explicitly resets an unclaimed
 * modern block device left active by firmware before taking ownership.
 * Native boot must not invoke this. Registers at most two modern 1042 disks plus
 * bounded partition views. Missing FLUSH or offered RO exposes read-only media.
 * Disks and identities live until reboot; terminal faults cannot rebind them. */
unsigned virtio_block_init(void);
void virtio_block_print_status(void);
#endif
