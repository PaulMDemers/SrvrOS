#ifndef SRVROS_DESK_DIALOG_H
#define SRVROS_DESK_DIALOG_H
#include <srvros/desk_ui.h>
#include <srvros/desk_flow.h>
#define DESK_DIALOG_CHOICES 3
struct desk_dialog_choice { unsigned id; char label[24]; int enabled; };
/* Owned modal presentation, built from the shared focus router and buttons.
 * Caller controls modality and performs returned actions. Escape activates the
 * enabled cancel ID; Enter activates the focused button. Opening defaults to
 * the caller's safe choice. Outside clicks never dismiss or activate anything.
 * Focus loss/resize cancels pointer capture. All text is copied and bounded. */
struct desk_dialog {
    struct desk_ui ui;
    struct desk_button buttons[DESK_DIALOG_CHOICES];
    struct desk_dialog_choice choices[DESK_DIALOG_CHOICES];
    struct desk_rect frame;
    unsigned count,cancel;
    char title[96],message[384];
};
void desk_dialog_init(struct desk_dialog *dialog);
int desk_dialog_open(struct desk_dialog *dialog,const char *title,const char *message,
    const struct desk_dialog_choice *choices,unsigned count,unsigned cancel,unsigned focus);
void desk_dialog_configure(struct desk_dialog *dialog,struct desk_rect bounds);
unsigned desk_dialog_focus(struct desk_dialog *dialog,int active);
struct desk_action desk_dialog_key(struct desk_dialog *dialog,const struct srv_key_event *key);
struct desk_action desk_dialog_pointer(struct desk_dialog *dialog,struct desk_widget_event event,uint64_t time_us);
void desk_dialog_draw(struct desk_canvas *canvas,const struct desk_dialog *dialog,const struct desk_theme *theme);
#endif
