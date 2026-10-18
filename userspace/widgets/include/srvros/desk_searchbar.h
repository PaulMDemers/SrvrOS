#ifndef SRVROS_DESK_SEARCHBAR_H
#define SRVROS_DESK_SEARCHBAR_H
#include <srvros/desk_ui.h>
#define DESK_SEARCH_QUERY 256
enum { DESK_SEARCH_QUERY_FIELD=1,DESK_SEARCH_PREVIOUS,DESK_SEARCH_NEXT,DESK_SEARCH_CLOSE,DESK_SEARCH_CASE,
    DESK_SEARCH_REPLACEMENT_FIELD,DESK_SEARCH_REPLACE,DESK_SEARCH_REPLACE_ALL };
/* Modeless search controls. The owner searches its document, presents feedback,
 * handles clipboard flags and decides how focus moves to/from its content. */
struct desk_searchbar {
    struct desk_rect frame;
    struct desk_ui ui;
    struct desk_text_buffer text,replacement;
    struct desk_edit query,with;
    struct desk_button previous,next,close,replace,replace_all;
    struct desk_check match_case;
    char storage[2][DESK_SEARCH_QUERY],replacement_storage[2][DESK_SEARCH_QUERY];
    int replacing;
};
void desk_searchbar_init(struct desk_searchbar *bar);
/* NULL preserves the previous query. A seed must be valid single-line UTF-8. */
int desk_searchbar_open(struct desk_searchbar *bar,const char *seed);
/* Find needs 72 px; replacement mode needs 144 px. Neither performs an edit.
 * Empty replacement is intentional and means removal. Enter in With requests
 * Replace; Ctrl-Enter requests Replace all. */
void desk_searchbar_replacing(struct desk_searchbar *bar,int enabled);
void desk_searchbar_configure(struct desk_searchbar *bar,struct desk_rect frame,int enabled);
unsigned desk_searchbar_focus(struct desk_searchbar *bar,int active);
struct desk_action desk_searchbar_key(struct desk_searchbar *bar,const struct srv_key_event *key);
struct desk_action desk_searchbar_pointer(struct desk_searchbar *bar,struct desk_widget_event event,uint64_t time_us);
void desk_searchbar_draw(struct desk_canvas *canvas,const struct desk_searchbar *bar,const struct desk_theme *theme);
#endif
