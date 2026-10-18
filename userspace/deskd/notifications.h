#ifndef DESKD_NOTIFICATIONS_H
#define DESKD_NOTIFICATIONS_H
#include <srvros/desktop_notice.h>
#define DESKTOP_NOTICES 16
#define DESKTOP_NOTICES_PER_OWNER 4
struct desktop_notice {
    uint64_t id,owner,serial,time_us;
    unsigned application,tone,unread;
    struct desk_notification text;
};
/* Oldest-first session history. No allocation, clock, I/O or focus policy.
 * Callers authenticate owner/application and enforce request ordering/rate. */
struct desktop_notifications { struct desktop_notice records[DESKTOP_NOTICES];unsigned count;uint64_t next_id; };
int desktop_notification_valid(const struct desk_notification *text,unsigned tone);
int desktop_notification_add(struct desktop_notifications *store,uint64_t owner,unsigned application,uint64_t serial,uint64_t time_us,unsigned tone,const struct desk_notification *text,uint64_t *id);
const struct desktop_notice *desktop_notification_find(const struct desktop_notifications *store,uint64_t id);
int desktop_notification_remove(struct desktop_notifications *store,uint64_t id);
void desktop_notifications_read(struct desktop_notifications *store);
unsigned desktop_notifications_unread(const struct desktop_notifications *store);
#endif
