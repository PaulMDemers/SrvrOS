#ifndef SRVROS_DESKTOP_SESSION_H
#define SRVROS_DESKTOP_SESSION_H
#include <stdbool.h>
struct limine_memmap_response;
/* Explicit opt-in, mounted Home required. Returns only after the inherited app
 * lifetime, battery owner and logger retire. Home stays mounted for reuse. */
bool desktop_battery_session_run(void);
bool desktop_battery_power_session_run(void);
bool battery_home_desktop_power_session_run(void *,struct limine_memmap_response *);
bool desktop_battery_lifecycle_session_run(void);
bool desktop_battery_navigation_session_run(void);
bool battery_home_desktop_navigation_session_run(void *,struct limine_memmap_response *);
bool battery_home_desktop_lifecycle_session_run(void *,struct limine_memmap_response *);
bool desktop_battery_audio_io_session_run(bool smoke);
bool desktop_battery_audio_io_attended_run(bool smoke);
bool battery_home_desktop_audio_io_attended_run(void *,struct limine_memmap_response *);
bool battery_home_desktop_audio_io_attended_smoke_run(void *,struct limine_memmap_response *);
bool battery_home_desktop_audio_io_session_run(void *,struct limine_memmap_response *);
bool battery_home_desktop_audio_io_smoke_run(void *,struct limine_memmap_response *);
bool desktop_battery_capacity_session_run(void);
bool battery_home_desktop_capacity_session_run(void *,struct limine_memmap_response *);
bool desktop_battery_reopen_session_run(void);
bool battery_home_desktop_reopen_session_run(void *,struct limine_memmap_response *);
bool battery_home_desktop_session_run(void *,struct limine_memmap_response *);
#endif
