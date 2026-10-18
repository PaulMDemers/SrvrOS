#ifndef DESKD_SCENE_H
#define DESKD_SCENE_H
#include <srvros/desk_render.h>
#define DESKD_WINDOWS 8
#define DESK_INPUT_QUEUE 64
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
    int focus,cursor_x,cursor_y,trackpad_trial,apps,menu;
    uint64_t stack_serial;
    char notice[96];
    char io_status[160],io_check[160],io_next[160];
};
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s);
void desktop_stack(const struct desktop_scene *scene,int order[DESKD_WINDOWS]);
struct desk_rect desktop_dock_item(int width,int height,int application);
#endif
