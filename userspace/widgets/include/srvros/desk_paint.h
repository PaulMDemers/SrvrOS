#ifndef SRVROS_DESK_PAINT_H
#define SRVROS_DESK_PAINT_H
#include <srvros/desk_image.h>
#include <srvros/desk_bitmap.h>
enum desk_paint_tool { DESK_PAINT_PEN,DESK_PAINT_LINE,DESK_PAINT_BOX,DESK_PAINT_PAN };
/* Reusable editable image view. Model/arena remain caller-owned. A drag is one
 * transaction; focus loss, cancel, resize or tool change restores its start. */
struct desk_paint {
    struct desk_image view;
    struct desk_bitmap *bitmap;
    enum desk_paint_tool tool;
    uint32_t color;
    unsigned size;
    int drawing,start_x,start_y,last_x,last_y;
};
void desk_paint_init(struct desk_paint *paint);
int desk_paint_bind(struct desk_paint *paint,struct desk_bitmap *bitmap);
void desk_paint_configure(struct desk_paint *paint,struct desk_rect frame,int enabled);
void desk_paint_tool(struct desk_paint *paint,enum desk_paint_tool tool,uint32_t rgb,unsigned size);
unsigned desk_paint_event(struct desk_paint *paint,struct desk_widget_event event);
unsigned desk_paint_key(struct desk_paint *paint,const struct srv_key_event *key);
unsigned desk_paint_scroll(struct desk_paint *paint,int32_t dx,int32_t dy);
void desk_paint_draw(struct desk_canvas *canvas,const struct desk_paint *paint,const struct desk_theme *theme);
#endif
