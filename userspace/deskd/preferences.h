#ifndef DESKD_PREFERENCES_H
#define DESKD_PREFERENCES_H
#include <srvros/desk_preferences.h>
#include <srvros/desk_file_job.h>
/* Selected only for an explicit, already mounted hardware-trial workspace. */
#define DESKTOP_WORKSPACE_PREFERENCES_PATH "/work/srvros-work/desktop.conf"
#define DESKTOP_PREFERENCES_PATH "/fat/home/desktop.conf"
struct desktop_preferences_store {
    const char *path;
    struct desk_preferences value,snapshot;
    struct desk_file_job job;
    uint64_t revision,changed_at;
    unsigned status,dirty_mask;
    uint32_t disk_crc;
    int initialized,loading,existing,writable,dirty;
    const char *portable_path;
    struct desk_preferences machine_value;
    uint32_t portable_crc;
    unsigned profile_phase,write_mask;
    int portable_existing;
    /* Final save is an explicit lifecycle mode, separate from the debounce. */
    int finish_requested,finish_retiring,finish_complete;

};
void desktop_preferences_init_path(struct desktop_preferences_store *store,const char *path);
void desktop_preferences_init(struct desktop_preferences_store *store);
void desktop_preferences_init_profile(struct desktop_preferences_store *store,const char *machine_path,const char *portable_path);
/* Return 1 for a value/status change, 0 otherwise. */
int desktop_preferences_poll(struct desktop_preferences_store *store,uint64_t ticks);
int desktop_preferences_set(struct desktop_preferences_store *store,const struct desk_preferences *value,unsigned mask,uint64_t ticks);
/* Flush dirty values without the normal debounce, then retire the helper.
 * Returns 0 while loading/writing/reaping, 1 only when known saved and retired,
 * -1 when durability cannot be established. Poll again while it returns 0. */
int desktop_preferences_finish(struct desktop_preferences_store *store,uint64_t ticks);
/* Abandon final retirement on a canceled close; accepted writes remain polled
 * by desktop_preferences_poll and later edits can continue using the helper. */
void desktop_preferences_finish_cancel(struct desktop_preferences_store *store);
void desktop_preferences_close(struct desktop_preferences_store *store);
#endif
