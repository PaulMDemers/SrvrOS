#ifndef DESKD_SCENE_H
#define DESKD_SCENE_H
#include <srvros/desktop_protocol.h>
#include <srvros/desk_render.h>
#include <srvros/desk_menu.h>
#include <srvros/desk_popup.h>
#include <srvros/desk_palette.h>
#include "notification_view.h"
#define DESKD_WINDOWS DESK_MAX_WINDOWS
struct desktop_window {
    struct desk_rect frame;
    const uint32_t *pixels;
    int width,height,visible,minimized,application,maximized;
    uint64_t z;
    struct desk_rect restore;
    char title[96];
};
struct desktop_scene {
    struct desktop_window windows[DESKD_WINDOWS];
    int focus,cursor_x,cursor_y,trackpad_trial,apps;
    struct desk_menu menu;
    struct desk_popup context;
    struct desktop_notice_view notifications;
    struct desk_palette launcher;
    int launcher_open;
    struct desk_calendar calendar;
    struct desk_ui calendar_ui;
    struct desk_button calendar_today,calendar_close;
    struct desk_rect calendar_frame;
    int calendar_open;
    char calendar_date[64],calendar_zone[32];
    uint64_t stack_serial;
    char notice[96],clock[64],battery[32];
    int wallpaper;
    char io_status[160],io_check[160],io_next[160];
};
enum desktop_window_control { DESKTOP_CLOSE=1,DESKTOP_MAXIMIZE,DESKTOP_MINIMIZE };
struct desk_rect desktop_window_control_rect(struct desk_rect frame,unsigned control);
unsigned desktop_titlebar_hit(struct desk_rect frame,int standard,int x,int y);
enum desktop_placement { DESKTOP_TILE_LEFT=1,DESKTOP_TILE_RIGHT,DESKTOP_CENTER };
/* Pure geometry: keep decorated windows below the menu and above the dock,
 * honor client surface bounds, and leave output unchanged if there is no room. */
int desktop_window_placement(int screen_width,int screen_height,struct desk_rect original,
    unsigned placement,struct desk_rect *result);
struct desk_rect desktop_battery_rect(int width,const char *clock);
struct desk_rect desktop_alert_rect(int width,const char *clock);
struct desk_rect desktop_clock_rect(int width,const char *clock);
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s);
void desktop_stack(const struct desktop_scene *scene,int order[DESKD_WINDOWS]);
struct desk_rect desktop_dock_item(int width,int height,int application);
#endif
