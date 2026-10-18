#ifndef SRVROS_DESK_LIST_H
#define SRVROS_DESK_LIST_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
#define DESK_LIST_MAX_ITEMS 4096
struct desk_list_item {
    const char *label, *detail;
    uint64_t id;
    int folder;
};
/* Items/strings are borrowed until set_items is called again. Selection is an
 * index; id is application-owned. No directory access or application callbacks. */
struct desk_list {
    struct desk_rect frame;
    const struct desk_list_item *items;
    int count, selected, focused, enabled, pressed, scrollbar_capture;
    int last_click;
    uint64_t last_click_us,typeahead_us;
    char typeahead[64];
    unsigned typeahead_length;
    struct desk_scrollbar scroll;
};
void desk_list_init(struct desk_list *list);
int desk_list_set_items(struct desk_list *list,const struct desk_list_item *items,int count);
void desk_list_configure(struct desk_list *list,struct desk_rect frame,int enabled);
unsigned desk_list_select(struct desk_list *list,int index);
unsigned desk_list_event(struct desk_list *list,struct desk_widget_event event,uint64_t time_us);
unsigned desk_list_key(struct desk_list *list,const struct srv_key_event *key);
unsigned desk_list_scroll(struct desk_list *list,int64_t delta);
void desk_list_draw(struct desk_canvas *canvas,const struct desk_list *list,const struct desk_theme *theme);
#endif
