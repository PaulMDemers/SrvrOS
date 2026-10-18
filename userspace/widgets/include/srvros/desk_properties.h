#ifndef SRVROS_DESK_PROPERTIES_H
#define SRVROS_DESK_PROPERTIES_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
#define DESK_PROPERTIES_FIELDS 16
#define DESK_PROPERTY_LABEL 48
#define DESK_PROPERTY_VALUE 384
struct desk_property { unsigned id;const char *label,*value; };
/* Owned, read-only label/value snapshot. Wide views use two columns; narrow
 * views stack labels above values. Text wraps; scrollbar and keys reveal all
 * fields. No allocation, filesystem access, clipboard or clock dependency. */
struct desk_properties {
    struct desk_rect frame;
    struct desk_scrollbar scroll;
    const struct desk_font *body;
    struct { unsigned id;char label[DESK_PROPERTY_LABEL],value[DESK_PROPERTY_VALUE];int y,height,label_height,value_height; } fields[DESK_PROPERTIES_FIELDS];
    unsigned count;
    int enabled,focused,stacked,label_width,value_width;
};
void desk_properties_init(struct desk_properties *properties);
int desk_properties_set(struct desk_properties *properties,const struct desk_property *fields,unsigned count);
void desk_properties_configure(struct desk_properties *properties,struct desk_rect frame,const struct desk_font *body,int enabled);
unsigned desk_properties_scroll(struct desk_properties *properties,int32_t delta);
unsigned desk_properties_event(struct desk_properties *properties,struct desk_widget_event event);
unsigned desk_properties_key(struct desk_properties *properties,const struct srv_key_event *key);
void desk_properties_draw(struct desk_canvas *canvas,const struct desk_properties *properties,const struct desk_theme *theme);
/* UTF-8 "Label: value\n" lines. NULL measures bytes, excluding NUL. Short
 * destinations remain unchanged. Values may contain explicit newlines/tabs. */
long desk_properties_text(const struct desk_properties *properties,char *output,size_t capacity);
#endif
