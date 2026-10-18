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
    struct desk_scrollbar vertical,horizontal;
    int scrollbars,scroll_capture,show_vertical,show_horizontal,metrics_valid;
    int32_t content_width,content_height;
    uint64_t measure_revision;
    const struct desk_font *measure_font;
};
void desk_edit_init(struct desk_edit *edit,struct desk_text_buffer *text,int multiline);
/* Opt-in automatic bars for multiline views. Wheel/drag only move the viewport,
 * never the selection or undo history. Single-line fields never show bars.
 * After changing the text model externally, reveal refreshes metrics and caret. */
void desk_edit_scrollbars(struct desk_edit *edit,int enabled);
struct desk_rect desk_edit_viewport(const struct desk_edit *edit);
void desk_edit_configure(struct desk_edit *edit,struct desk_rect frame,const struct desk_font *font,int enabled);
unsigned desk_edit_event(struct desk_edit *edit,struct desk_widget_event event);
enum desk_edit_command { DESK_EDIT_SELECT_ALL=1,DESK_EDIT_COMMAND_COPY,DESK_EDIT_COMMAND_CUT,
    DESK_EDIT_COMMAND_PASTE,DESK_EDIT_UNDO,DESK_EDIT_REDO };
unsigned desk_edit_command(struct desk_edit *edit,enum desk_edit_command command);
unsigned desk_edit_key(struct desk_edit *edit,const struct srv_key_event *key);
unsigned desk_edit_scroll(struct desk_edit *edit,int32_t x,int32_t y);
void desk_edit_reveal(struct desk_edit *edit);
void desk_edit_draw(struct desk_canvas *canvas,const struct desk_edit *edit,const struct desk_theme *theme);
#endif
