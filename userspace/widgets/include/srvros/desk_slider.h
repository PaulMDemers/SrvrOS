#ifndef SRVROS_DESK_SLIDER_H
#define SRVROS_DESK_SLIDER_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
/* Horizontal scalar control. Bounds and value are inclusive signed 32-bit;
 * step is positive. Pointer values snap relative to minimum, including maximum.
 * Configure/disable/focus loss cancels capture, retaining the last chosen value.
 * No wheel changes: scrolling past a form must not silently change its values. */
struct desk_slider {
    struct desk_rect frame;
    int32_t minimum,maximum,value,grab;
    uint32_t step;
    int enabled,focused,dragging;
};
void desk_slider_init(struct desk_slider *slider);
int desk_slider_range(struct desk_slider *slider,int32_t minimum,int32_t maximum,uint32_t step);
void desk_slider_configure(struct desk_slider *slider,struct desk_rect frame,int enabled);
unsigned desk_slider_set(struct desk_slider *slider,int64_t value);
struct desk_rect desk_slider_thumb(const struct desk_slider *slider);
unsigned desk_slider_event(struct desk_slider *slider,struct desk_widget_event event);
unsigned desk_slider_key(struct desk_slider *slider,const struct srv_key_event *key);
void desk_slider_draw(struct desk_canvas *canvas,const struct desk_slider *slider,const struct desk_theme *theme);
#endif
