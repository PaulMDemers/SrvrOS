#ifndef SRVROS_WORKSPACE_H
#define SRVROS_WORKSPACE_H
#include <stdbool.h>
bool workspace_mount(const char *uuid);
bool workspace_sync(void);
bool workspace_unmount(void);
bool workspace_active(void);
void workspace_status(void);
void workspace_boot(const char *cmdline);
#endif
