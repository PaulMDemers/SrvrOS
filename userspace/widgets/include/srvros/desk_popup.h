#ifndef SRVROS_DESK_POPUP_H
#define SRVROS_DESK_POPUP_H
#include <srvros/desk_menu.h>
#define DESK_POPUP_ITEMS 32
/* Standalone one-level menu, with an owned snapshot and overflow scrolling.
 * No callbacks, IPC, allocation or I/O. Route events exclusively while active,
 * including the release after an outside press. Cancel on resize/focus loss. */
struct desk_popup {
    struct desk_menu_item items[DESK_POPUP_ITEMS];
    unsigned count;
    struct desk_rect frame,bounds;
    const struct desk_font *font;
    struct desk_scrollbar scroll;
    int opened,selected,armed,scroll_capture,swallow_release;
};
void desk_popup_init(struct desk_popup *popup);
int desk_popup_open(struct desk_popup *popup,const struct desk_menu_item *items,unsigned count,
    int x,int y,struct desk_rect bounds,const struct desk_font *font,int keyboard);
int desk_popup_active(const struct desk_popup *popup);
void desk_popup_cancel(struct desk_popup *popup);
struct desk_rect desk_popup_row(const struct desk_popup *popup,unsigned index);
struct desk_menu_result desk_popup_pointer(struct desk_popup *popup,struct desk_widget_event event);
struct desk_menu_result desk_popup_key(struct desk_popup *popup,const struct srv_key_event *key);
struct desk_menu_result desk_popup_scroll(struct desk_popup *popup,int64_t delta);
void desk_popup_draw(struct desk_canvas *canvas,const struct desk_popup *popup,const struct desk_theme *theme);
#endif
