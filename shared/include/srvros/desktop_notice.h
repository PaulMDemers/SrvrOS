#ifndef SRVROS_DESKTOP_NOTICE_H
#define SRVROS_DESKTOP_NOTICE_H
#include <stdint.h>
#define DESK_NOTIFY_TITLE 32
#define DESK_NOTIFY_MESSAGE 56
#define DESK_NOTIFY_MAX_TONE 2
struct desk_notification { char title[DESK_NOTIFY_TITLE],message[DESK_NOTIFY_MESSAGE]; };
_Static_assert(sizeof(struct desk_notification)==88,"notification packet payload");
#endif
