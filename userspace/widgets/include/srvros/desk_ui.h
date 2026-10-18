#ifndef SRVROS_DESK_UI_H
#define SRVROS_DESK_UI_H
#include <srvros/desk_edit.h>
#include <srvros/desk_list.h>
#include <srvros/desk_table.h>
#include <srvros/desk_calendar.h>
#include <srvros/desk_tabs.h>
#include <srvros/desk_image.h>
#define DESK_UI_MAX_CONTROLS 32
enum desk_control_kind { DESK_CONTROL_BUTTON, DESK_CONTROL_EDIT, DESK_CONTROL_LIST, DESK_CONTROL_CHECK, DESK_CONTROL_TABLE, DESK_CONTROL_CALENDAR, DESK_CONTROL_TABS, DESK_CONTROL_IMAGE };
struct desk_control {
    unsigned id;
    enum desk_control_kind kind;
    void *state;
    const char *label; /* Borrowed button label. */
};
struct desk_action { unsigned id, flags; };
struct desk_ui {
    struct desk_control controls[DESK_UI_MAX_CONTROLS];
    unsigned count;
    int focus, capture, active;
};
void desk_ui_init(struct desk_ui *ui);
int desk_ui_add(struct desk_ui *ui,unsigned id,enum desk_control_kind kind,void *state,const char *label);
unsigned desk_ui_focus(struct desk_ui *ui,unsigned id);
unsigned desk_ui_window_focus(struct desk_ui *ui,int active);
/* Call after layout/enabled-state updates. Cancels capture and repairs focus. */
unsigned desk_ui_layout_changed(struct desk_ui *ui);
struct desk_action desk_ui_pointer(struct desk_ui *ui,struct desk_widget_event event,uint64_t time_us);
struct desk_action desk_ui_key(struct desk_ui *ui,const struct srv_key_event *key);
struct desk_action desk_ui_scroll(struct desk_ui *ui,int32_t x,int32_t y,int32_t delta_x,int32_t delta_y);
void desk_ui_draw(struct desk_canvas *canvas,const struct desk_ui *ui,const struct desk_theme *theme);
#endif
