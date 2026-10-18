#ifndef SRVROS_DESK_IMAGE_H
#define SRVROS_DESK_IMAGE_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
#include <srvros/image_limits.h>
/* A borrowed immutable 0xAARRGGBB image. No decoding, allocation or I/O.
 * Fit never enlarges beyond 100%; explicit zoom steps cover 6.25%–400%.
 * Drag/scroll/arrows pan. +/− zoom, 0 fits, 1 chooses actual pixels. */
struct desk_image {
    struct desk_rect frame;
    const uint32_t *pixels;
    uint32_t width,height,scale;
    int32_t offset_x,offset_y,grab_x,grab_y,grab_offset_x,grab_offset_y;
    int fit,enabled,focused,dragging;
};
void desk_image_init(struct desk_image *image);
int desk_image_set(struct desk_image *image,const uint32_t *pixels,uint32_t width,uint32_t height);
void desk_image_configure(struct desk_image *image,struct desk_rect frame,int enabled);
unsigned desk_image_fit(struct desk_image *image);
unsigned desk_image_actual(struct desk_image *image);
unsigned desk_image_zoom(struct desk_image *image,int direction);
unsigned desk_image_scroll(struct desk_image *image,int32_t dx,int32_t dy);
unsigned desk_image_event(struct desk_image *image,struct desk_widget_event event);
unsigned desk_image_key(struct desk_image *image,const struct srv_key_event *key);
void desk_image_draw(struct desk_canvas *canvas,const struct desk_image *image,const struct desk_theme *theme);
#endif
