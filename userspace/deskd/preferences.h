#ifndef DESKD_PREFERENCES_H
#define DESKD_PREFERENCES_H
#include <srvros/desk_preferences.h>
#include <srvros/desk_file_job.h>
#define DESKTOP_PREFERENCES_PATH "/fat/home/desktop.conf"
struct desktop_preferences_store {
    struct desk_preferences value,snapshot;
    struct desk_file_job job;
    uint64_t revision,changed_at;
    unsigned status,dirty_mask;
    uint32_t disk_crc;
    int initialized,loading,existing,writable,dirty;
};
void desktop_preferences_init(struct desktop_preferences_store *store);
/* Return 1 for a value/status change, 0 otherwise. */
int desktop_preferences_poll(struct desktop_preferences_store *store,uint64_t ticks);
int desktop_preferences_set(struct desktop_preferences_store *store,const struct desk_preferences *value,unsigned mask,uint64_t ticks);
void desktop_preferences_close(struct desktop_preferences_store *store);
#endif
