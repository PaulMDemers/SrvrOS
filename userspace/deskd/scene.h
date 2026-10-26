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
    int width,height,visible,minimized,application,maximized,dialog;
    uint64_t z;
    struct desk_rect restore;
    char title[96];
};
struct desktop_scene {
    struct desktop_window windows[DESKD_WINDOWS]; /* Inline compatibility storage. */
    struct desktop_window **records;
    int capacity,*order;
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
    int automatic_test;
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
/* Movement may put content outside the output. Keep a useful titlebar segment
 * below the menu and above the dock; do not resize or allocate a surface. */
int desktop_window_drag(int width,int height,struct desk_rect original,
    int64_t x,int64_t y,struct desk_rect *result);
struct desk_rect desktop_battery_rect(int width,const char *clock);
struct desk_rect desktop_alert_rect(int width,const char *clock);
struct desk_rect desktop_clock_rect(int width,const char *clock);
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s);
/* Borrow one desktop_stack() result for an immutable frame's damage clips.
 * Occlusion=0 retains the unculled pixel oracle; neither path allocates or
 * changes scene/cache lifetime. Order must cover desktop_capacity(s) entries. */
void desktop_draw_ordered(struct desk_canvas *c,const struct desktop_scene *s,
    const int *order,int occlusion);
/* Registry/scratch storage belongs to the compositor. Scene code never allocates. */
#define desktop_window_at(s,i) ((s)->records?(s)->records[(i)]:&(s)->windows[(i)])
static inline int desktop_capacity(const struct desktop_scene *s) { return s->capacity?s->capacity:DESKD_WINDOWS; }
void desktop_stack(const struct desktop_scene *scene,int *order);
struct desktop_dock_entry {
    unsigned application,windows,minimized;
    int active;
    struct desk_rect frame;
};
/* Pinned apps first, then other admitted apps; one entry per catalog app,
 * independent of the number of windows. No allocation or retained pointers. */
unsigned desktop_dock_entries(const struct desktop_scene *,int width,int height,
    struct desktop_dock_entry entries[DESK_APP_COUNT]);
struct desk_rect desktop_dock_frame(const struct desktop_scene *,int width,int height);
unsigned desktop_dock_hit(const struct desktop_scene *,int width,int height,int x,int y);
#endif
