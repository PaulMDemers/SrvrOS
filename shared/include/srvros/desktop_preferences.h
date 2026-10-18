#ifndef SRVROS_DESKTOP_PREFERENCES_H
#define SRVROS_DESKTOP_PREFERENCES_H
#include <stdint.h>
/* Values shared by Settings, the desktop and clients; no pointers on the wire. */
struct desk_preferences {
    uint32_t text_size;
    int32_t utc_offset_minutes;
    uint32_t clock_flags,wallpaper;
};
#define DESK_CLOCK_24H 1u
#define DESK_CLOCK_DATE 2u
#define DESK_PREF_TEXT 1u
#define DESK_PREF_OFFSET 2u
#define DESK_PREF_FORMAT 4u
#define DESK_PREF_DATE 8u
#define DESK_PREF_CLOCK (DESK_PREF_OFFSET|DESK_PREF_FORMAT|DESK_PREF_DATE)
#define DESK_PREF_WALLPAPER 16u
#define DESK_PREF_ALL 31u
enum desk_preferences_status { DESK_PREF_LOADING, DESK_PREF_SAVED, DESK_PREF_SAVING,
    DESK_PREF_SESSION_ONLY, DESK_PREF_CONFLICT };
#endif
