#ifndef SRVROS_DESK_PATHS_H
#define SRVROS_DESK_PATHS_H
#include <stdlib.h>
#include <string.h>
/* Supplied by the desktop, not discovered with filesystem I/O in event loops. */
#define DESK_WORKSPACE_HOME "/work/srvros-work"
#define DESK_RAM_HOME "/fat/home"
static inline const char *desk_home(void) {
    const char *home=getenv("SRVROS_DESKTOP_HOME");
    return home && !strcmp(home,DESK_WORKSPACE_HOME)?DESK_WORKSPACE_HOME:DESK_RAM_HOME;
}
#endif
