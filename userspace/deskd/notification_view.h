#ifndef DESKD_NOTIFICATION_VIEW_H
#define DESKD_NOTIFICATION_VIEW_H
#include "notifications.h"
#include <srvros/desk_ui.h>
#include <srvros/desk_notice.h>
enum { NOTICE_LIST=1,NOTICE_OPEN,NOTICE_REMOVE,NOTICE_CLEAR,NOTICE_CLOSE,NOTICE_TOAST_OPEN,NOTICE_TOAST_CLOSE };
struct desktop_notice_action { uint64_t owner;unsigned application;int close; };
struct desktop_notice_view {
    struct desktop_notifications store;
    struct desk_ui ui,toast_ui;
    struct desk_list list;
    struct desk_button open,remove,clear,close,toast_open,toast_close;
    struct desk_list_item items[DESKTOP_NOTICES];
    struct desk_notice detail,toast;
    struct desk_rect frame,toast_frame;
    uint64_t toast_id,pending_toast,toast_started;
    int opened;
};
void desktop_notice_view_init(struct desktop_notice_view *view);
void desktop_notice_view_configure(struct desktop_notice_view *view,int width,int height);
void desktop_notice_view_refresh(struct desktop_notice_view *view);
void desktop_notice_view_open(struct desktop_notice_view *view);
void desktop_notice_view_close(struct desktop_notice_view *view);
void desktop_notice_view_arrived(struct desktop_notice_view *view,uint64_t id,uint64_t now_us);
int desktop_notice_view_tick(struct desktop_notice_view *view,uint64_t now_us);
struct desktop_notice_action desktop_notice_view_action(struct desktop_notice_view *view,struct desk_action action);
struct desktop_notice_action desktop_notice_toast_action(struct desktop_notice_view *view,struct desk_action action);
void desktop_notice_view_draw(struct desk_canvas *canvas,const struct desktop_notice_view *view,const struct desk_theme *theme);
#endif
