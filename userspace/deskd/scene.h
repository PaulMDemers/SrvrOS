#ifndef DESKD_SCENE_H
#define DESKD_SCENE_H
#include <srvros/desk_render.h>
#define DESKD_WINDOWS 2
struct desktop_window {
    struct desk_rect frame;
    const uint32_t *pixels;
    int width,height,visible;
    char title[96];
};
struct desktop_scene {
    struct desktop_window windows[DESKD_WINDOWS];
    int focus,cursor_x,cursor_y,trackpad_trial;
    char io_status[160],io_check[160],io_next[160];
};
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s);
#endif
