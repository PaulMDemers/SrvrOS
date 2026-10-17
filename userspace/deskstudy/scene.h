#ifndef DESKSTUDY_SCENE_H
#define DESKSTUDY_SCENE_H
#include <srvros/desk_render.h>

/* A rendering laboratory, intentionally independent of GUI2/widgets.
 * This scene is also rendered by the host regression oracle. */
struct study_scene {
    struct desk_rect window;
    int32_t cursor_x, cursor_y;
    int large_text;
    char input[128];
};
void study_draw(struct desk_canvas *canvas, const struct study_scene *scene);
#endif
