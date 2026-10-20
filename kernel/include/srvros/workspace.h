#ifndef SRVROS_WORKSPACE_H
#define SRVROS_WORKSPACE_H
#include <stdbool.h>
bool workspace_mount(const char *uuid);
bool workspace_sync(void);
bool workspace_unmount(void);
bool workspace_active(void);
/* Last successfully validated/unmounted device, only while all writes remain
 * revoked. CPU0 diagnostic caller must retain this state throughout use. */
struct block_device;
struct block_device *workspace_parked_device(void);
void workspace_status(void);
void workspace_boot(const char *cmdline);
#endif
