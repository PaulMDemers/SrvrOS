#ifndef SRVROS_DESK_COLOR_PANEL_H
#define SRVROS_DESK_COLOR_PANEL_H
#include <srvros/desk_ui.h>
enum { DESK_COLOR_RED=1,DESK_COLOR_GREEN,DESK_COLOR_BLUE,DESK_COLOR_COPY,DESK_COLOR_CANCEL,DESK_COLOR_ACCEPT,DESK_COLOR_SWATCH=16 };
#define DESK_COLOR_SWATCHES 12
/* Modal RGB draft; the owner commits only on ACCEPT. COPY requests a clipboard
 * write of hex (seven ASCII bytes); this widget never performs I/O. */
struct desk_color_panel {
    struct desk_rect frame;
    struct desk_ui ui;
    struct desk_slider channels[3];
    struct desk_button copy,cancel,accept,swatches[DESK_COLOR_SWATCHES];
    uint32_t color;
    char hex[8],labels[3][8];
};
void desk_color_panel_init(struct desk_color_panel *panel);
void desk_color_panel_open(struct desk_color_panel *panel,uint32_t rgb);
void desk_color_panel_configure(struct desk_color_panel *panel,struct desk_rect bounds);
unsigned desk_color_panel_focus(struct desk_color_panel *panel,int active);
struct desk_action desk_color_panel_key(struct desk_color_panel *panel,const struct srv_key_event *key);
struct desk_action desk_color_panel_pointer(struct desk_color_panel *panel,struct desk_widget_event event,uint64_t time_us);
void desk_color_panel_draw(struct desk_canvas *canvas,const struct desk_color_panel *panel,const struct desk_theme *theme);
#endif
