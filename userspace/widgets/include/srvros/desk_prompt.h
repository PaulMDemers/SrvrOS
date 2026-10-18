#ifndef SRVROS_DESK_PROMPT_H
#define SRVROS_DESK_PROMPT_H
#include <srvros/desk_dialog.h>
#define DESK_PROMPT_BYTES 256
enum { DESK_PROMPT_CANCEL=1,DESK_PROMPT_ACCEPT,DESK_PROMPT_FIELD };
/* Reusable single-line modal prompt. Owned bounded text; no filesystem policy.
 * The app handles ACCEPT/CANCEL and editor clipboard flags on FIELD. */
struct desk_prompt {
    struct desk_dialog dialog;
    struct desk_edit edit;
    struct desk_text_buffer text;
    char storage[2][DESK_PROMPT_BYTES];
};
void desk_prompt_init(struct desk_prompt *prompt);
int desk_prompt_open(struct desk_prompt *prompt,const char *title,const char *message,const char *initial,const char *accept);
void desk_prompt_configure(struct desk_prompt *prompt,struct desk_rect bounds);
unsigned desk_prompt_focus(struct desk_prompt *prompt,int active);
struct desk_action desk_prompt_key(struct desk_prompt *prompt,const struct srv_key_event *key);
struct desk_action desk_prompt_pointer(struct desk_prompt *prompt,struct desk_widget_event event,uint64_t time_us);
void desk_prompt_draw(struct desk_canvas *canvas,const struct desk_prompt *prompt,const struct desk_theme *theme);
#endif
