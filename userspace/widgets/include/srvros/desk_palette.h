#ifndef SRVROS_DESK_PALETTE_H
#define SRVROS_DESK_PALETTE_H
#include <srvros/desk_ui.h>
#define DESK_PALETTE_ITEMS 64
#define DESK_PALETTE_QUERY 128
struct desk_palette_item { uint64_t id;char label[96],detail[96]; };
struct desk_palette_result { uint64_t id;unsigned flags;int cancel; };
/* Owned searchable selection panel. The app supplies stable IDs and performs
 * activation. Search matches all space-separated tokens, ASCII case-insensitively
 * across label/detail; other UTF-8 bytes match exactly. No lookup or launching. */
struct desk_palette {
    struct desk_rect frame;
    struct desk_ui ui;
    struct desk_edit query;
    struct desk_text_buffer text;
    char storage[2][DESK_PALETTE_QUERY],title[96],notice[128];
    struct desk_list list;
    struct desk_palette_item items[DESK_PALETTE_ITEMS];
    struct desk_list_item visible[DESK_PALETTE_ITEMS];
    unsigned count;
};
void desk_palette_init(struct desk_palette *palette);
int desk_palette_open(struct desk_palette *palette,const char *title,const struct desk_palette_item *items,unsigned count);
int desk_palette_items(struct desk_palette *palette,const struct desk_palette_item *items,unsigned count);
void desk_palette_configure(struct desk_palette *palette,struct desk_rect bounds);
/* Call after an app-applied query edit, such as a clipboard reply. */
void desk_palette_filter(struct desk_palette *palette);
int desk_palette_message(struct desk_palette *palette,const char *message);
unsigned desk_palette_focus(struct desk_palette *palette,int active);
struct desk_palette_result desk_palette_key(struct desk_palette *palette,const struct srv_key_event *key);
struct desk_palette_result desk_palette_pointer(struct desk_palette *palette,struct desk_widget_event event,uint64_t time_us);
struct desk_palette_result desk_palette_scroll(struct desk_palette *palette,int x,int y,int delta);
void desk_palette_draw(struct desk_canvas *canvas,const struct desk_palette *palette,const struct desk_theme *theme);
#endif
