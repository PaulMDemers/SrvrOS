#ifndef SRVROS_DESK_EDIT_H
#define SRVROS_DESK_EDIT_H
#include <srvros/desk_widgets.h>
#include <srvros/desk_text.h>
#include <srvros/key_event.h>

#define DESK_EDIT_COPY 8u
#define DESK_EDIT_CUT 16u
#define DESK_EDIT_PASTE 32u
#define DESK_EDIT_SUBMIT 64u
#define DESK_EDIT_FULL 128u
/* Fixed-capacity UTF-8 field/editor view. The document and storage stay owned by
 * the caller. Keyboard shortcuts return clipboard requests to the app/router.
 * No clipboard, file, process or transport access occurs inside the control. */
struct desk_edit {
    struct desk_rect frame;
    struct desk_text_buffer *text;
    const struct desk_font *font;
    int multiline, enabled, focused, dragging;
    int32_t scroll_x, scroll_y, preferred_x;
};
void desk_edit_init(struct desk_edit *edit,struct desk_text_buffer *text,int multiline);
void desk_edit_configure(struct desk_edit *edit,struct desk_rect frame,const struct desk_font *font,int enabled);
unsigned desk_edit_event(struct desk_edit *edit,struct desk_widget_event event);
unsigned desk_edit_key(struct desk_edit *edit,const struct srv_key_event *key);
unsigned desk_edit_scroll(struct desk_edit *edit,int32_t x,int32_t y);
void desk_edit_reveal(struct desk_edit *edit);
void desk_edit_draw(struct desk_canvas *canvas,const struct desk_edit *edit,const struct desk_theme *theme);
#endif
