#ifndef SRVROS_DESK_MENU_H
#define SRVROS_DESK_MENU_H
#include <srvros/desk_widgets.h>
#include <srvros/desktop_menu.h>
#include <srvros/key_event.h>
#define DESK_MENU_ITEMS 64
#define DESK_MENU_GROUPS 8
/* Owned bounded menu snapshot. No allocation, IPC, filesystem or callbacks.
 * Configure cancels an open menu. Coordinates are canvas-local. An open menu
 * consumes pointer/key input; callers must not also dispatch it to controls.
 * Pointer release selects a row; Escape/outside press cancels. F10 opens the
 * first group. Arrow/Home/End navigation skips disabled rows and separators. */
struct desk_menu {
    struct desk_menu_item items[DESK_MENU_ITEMS];
    unsigned count,groups;
    unsigned headings[DESK_MENU_GROUPS];
    struct desk_rect bar,bounds;
    const struct desk_font *font;
    int open,selected,armed;
};
struct desk_menu_result { uint32_t action; int consumed,redraw; };
int desk_menu_valid(const struct desk_menu_item *items,unsigned count);
void desk_menu_init(struct desk_menu *menu);
int desk_menu_configure(struct desk_menu *menu,const struct desk_menu_item *items,unsigned count,
    struct desk_rect bar,struct desk_rect bounds,const struct desk_font *font);
void desk_menu_cancel(struct desk_menu *menu);
struct desk_rect desk_menu_heading(const struct desk_menu *menu,unsigned group);
struct desk_rect desk_menu_popup(const struct desk_menu *menu);
struct desk_rect desk_menu_row(const struct desk_menu *menu,unsigned index);
struct desk_menu_result desk_menu_pointer(struct desk_menu *menu,struct desk_widget_event event);
struct desk_menu_result desk_menu_key(struct desk_menu *menu,const struct srv_key_event *key);
void desk_menu_bar_draw(struct desk_canvas *canvas,const struct desk_menu *menu,const struct desk_theme *theme);
void desk_menu_popup_draw(struct desk_canvas *canvas,const struct desk_menu *menu,const struct desk_theme *theme);
#endif
