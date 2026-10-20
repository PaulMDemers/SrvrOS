#ifndef SRVROS_WORKSPACE_H
#define SRVROS_WORKSPACE_H
#include <stdbool.h>
#include <stdint.h>
bool workspace_mount(const char *uuid);
bool workspace_sync(void);
bool workspace_unmount(void);
bool workspace_active(void);
/* Last successfully validated/unmounted device, only while all writes remain
 * revoked. CPU0 diagnostic caller must retain this state throughout use. */
struct block_device;
struct block_device *workspace_parked_device(void);
/* Diagnostic-only single-file capability. Prepare allocates a unique file,
 * verifies its data extent, flushes/unmounts, and leaves all write aliases off.
 * A cycle accepts exactly 4096 bytes and performs raw write/flush/readback.
 * End revokes the capability even on failure; failed I/O blocks remount. */
bool workspace_scratch_prepare(void);
bool workspace_scratch_ready(void);
bool workspace_scratch_cycle(const uint8_t data[4096]);
bool workspace_scratch_end(void);
void workspace_status(void);
void workspace_boot(const char *cmdline);
#endif
