#ifndef SRVROS_DESKTOP_PREFERENCES_H
#define SRVROS_DESKTOP_PREFERENCES_H
#include <stdint.h>
/* Values shared by Settings, the desktop and clients; no pointers on the wire. */
struct desk_preferences {
    uint32_t text_size;
    int32_t utc_offset_minutes;
    uint32_t clock_flags,wallpaper;
    uint32_t pointer_speed,pointer_acceleration,disable_while_typing;
    uint32_t tap_to_click,two_finger_tap;
};
#define DESK_CLOCK_24H 1u
#define DESK_CLOCK_DATE 2u
#define DESK_PREF_TEXT 1u
#define DESK_PREF_OFFSET 2u
#define DESK_PREF_FORMAT 4u
#define DESK_PREF_DATE 8u
#define DESK_PREF_CLOCK (DESK_PREF_OFFSET|DESK_PREF_FORMAT|DESK_PREF_DATE)
#define DESK_PREF_WALLPAPER 16u
#define DESK_PREF_POINTER_SPEED 32u
#define DESK_PREF_POINTER_ACCELERATION 64u
#define DESK_PREF_DISABLE_WHILE_TYPING 128u
#define DESK_PREF_TAP_CLICK 256u
#define DESK_PREF_TWO_FINGER_TAP 512u
#define DESK_PREF_POINTER (DESK_PREF_TAP_CLICK|DESK_PREF_TWO_FINGER_TAP|DESK_PREF_POINTER_SPEED|DESK_PREF_POINTER_ACCELERATION|DESK_PREF_DISABLE_WHILE_TYPING)
#define DESK_PREF_ALL 1023u
enum desk_preferences_status { DESK_PREF_LOADING, DESK_PREF_SAVED, DESK_PREF_SAVING,
    DESK_PREF_SESSION_ONLY, DESK_PREF_CONFLICT };
#endif
