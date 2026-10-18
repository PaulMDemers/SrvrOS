#ifndef SRVROS_DESK_TERMINAL_VIEW_H
#define SRVROS_DESK_TERMINAL_VIEW_H
#include <srvros/desk_terminal.h>
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
struct desk_terminal_view {
    struct desk_terminal *terminal;
    struct desk_rect frame,content;
    struct desk_scrollbar scroll;
    unsigned columns,rows,cell_width,cell_height,anchor,caret;
    unsigned seen_history,seen_alt;
    uint64_t seen_revision,seen_discarded;
    int enabled,focused,dragging,scrollbar_capture;
};
void desk_terminal_view_init(struct desk_terminal_view *view,struct desk_terminal *terminal);
/* Computes cell geometry and resizes the screen model; the application then
 * publishes columns/rows to its PTY. Geometry changes cancel selection. */
void desk_terminal_view_configure(struct desk_terminal_view *view,struct desk_rect frame,int enabled);
unsigned desk_terminal_view_sync(struct desk_terminal_view *view);
unsigned desk_terminal_view_event(struct desk_terminal_view *view,struct desk_widget_event event);
unsigned desk_terminal_view_scroll(struct desk_terminal_view *view,int64_t pixels);
void desk_terminal_view_follow(struct desk_terminal_view *view);
void desk_terminal_view_select_all(struct desk_terminal_view *view);
/* Returns UTF-8 bytes, or -1 if capacity cannot hold the entire selection+NUL.
 * Soft wrapped lines join; hard line endings trim unused trailing spaces. */
int64_t desk_terminal_view_copy(const struct desk_terminal_view *view,char *output,size_t capacity);
void desk_terminal_view_draw(struct desk_canvas *canvas,const struct desk_terminal_view *view,
    const struct desk_theme *theme,int cursor_phase);
/* No transport side effects. Returns bytes (including NUL for Ctrl-Space), or
 * zero for unhandled/released keys or insufficient capacity. App shortcuts
 * such as Ctrl-Shift-C/V are handled before calling this encoder. */
size_t desk_terminal_key(const struct desk_terminal *terminal,const struct srv_key_event *key,
    void *output,size_t capacity);
#endif
