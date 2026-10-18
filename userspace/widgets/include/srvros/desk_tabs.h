#ifndef SRVROS_DESK_TABS_H
#define SRVROS_DESK_TABS_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
#define DESK_TABS_LIMIT 16
#define DESK_TAB_LABEL 64
struct desk_tab { uint64_t id;char label[DESK_TAB_LABEL];int disabled; };
/* Owned item snapshots with stable nonzero IDs. Equal-width tabs; the owner
 * chooses a useful frame width. Left/Right/Home/End select an enabled page.
 * Page contents and their focus/state remain owned by the app. */
struct desk_tabs {
    struct desk_rect frame;
    struct desk_tab items[DESK_TABS_LIMIT];
    unsigned count;
    int selected,pressed,inside,enabled,requested,focused;
};
void desk_tabs_init(struct desk_tabs *tabs);
/* Atomic validation, preserving selection by ID when possible. */
int desk_tabs_items(struct desk_tabs *tabs,const struct desk_tab *items,unsigned count);
void desk_tabs_configure(struct desk_tabs *tabs,struct desk_rect frame,int enabled);
int desk_tabs_select(struct desk_tabs *tabs,uint64_t id);
uint64_t desk_tabs_selected(const struct desk_tabs *tabs);
struct desk_rect desk_tabs_item_rect(const struct desk_tabs *tabs,unsigned index);
unsigned desk_tabs_event(struct desk_tabs *tabs,struct desk_widget_event event);
unsigned desk_tabs_key(struct desk_tabs *tabs,const struct srv_key_event *key);
void desk_tabs_draw(struct desk_canvas *canvas,const struct desk_tabs *tabs,const struct desk_theme *theme);
#endif
