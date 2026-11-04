#ifndef SRVROS_DESK_PATHS_H
#define SRVROS_DESK_PATHS_H
#include <srvros/profile_session.h>
#define DESK_WORKSPACE_HOME SRV_PROFILE_LEGACY_WORK_HOME
#define DESK_RAM_HOME SRV_PROFILE_LEGACY_RAM_HOME
/* Invalid explicit profile context returns an empty path, never a fallback.
 * Application startup must reject this context before accessing files. */
static inline const char *desk_home(void) {
    struct srv_profile_session session;
    const char *desktop, *home;
    if (!srv_profile_session_from_env(&session)) return "";
    if (!session.id[0]) return !strcmp(session.home, DESK_WORKSPACE_HOME) ? DESK_WORKSPACE_HOME : DESK_RAM_HOME;
    desktop = getenv("SRVROS_DESKTOP_HOME");
    home = getenv("HOME");
    return desktop ? desktop : home;
}
#endif
