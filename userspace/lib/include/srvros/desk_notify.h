#ifndef SRVROS_DESK_NOTIFY_H
#define SRVROS_DESK_NOTIFY_H
#include <srvros/desktop.h>
#include <srvros/desktop_notice.h>
/* Single pending notification, copied before sending. ACK means accepted by
 * the desktop, not read by the user. Poll tick from the normal app event loop.
 * Retry after one second; time out after ten. No blocking or allocation. */
struct desk_notify_client {
    struct desk_notification text;
    uint64_t server,serial,started,last_send;
    unsigned tone;
    int busy,sent;
};
void desk_notify_init(struct desk_notify_client *client);
int desk_notify_begin(struct desk_notify_client *client,uint64_t server,unsigned tone,const char *title,const char *message,uint64_t tick);
int desk_notify_tick(struct desk_notify_client *client,uint64_t tick);
int desk_notify_receive(struct desk_notify_client *client,const struct desk_packet *packet);
#endif
