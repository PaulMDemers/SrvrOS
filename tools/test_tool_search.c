#include "../userspace/desktools/search.c"
#include <assert.h>
#include <stdio.h>
static struct tool t;
static unsigned clipboard_target;
void srv_puts(const char *s) { (void)s; }
void tool_status(struct tool *p,const char *s) { snprintf(p->status,sizeof(p->status),"%s",s); }
void tool_clipboard_action(struct tool *p,struct desk_action a) { (void)p;clipboard_target=a.id; }
void tool_layout(struct tool *p) {
    desk_searchbar_configure(&p->search,(struct desk_rect){16,102,688,72},1);
    desk_edit_configure(&p->body,(struct desk_rect){16,p->search_open?182:102,688,p->search_open?256:336},&desk_font_16,1);
    desk_ui_layout_changed(&p->ui);
}
int main(void) {
    desk_ui_init(&t.ui);desk_searchbar_init(&t.search);t.role=DESK_APP_TEXT;t.window.focused=1;
    desk_text_buffer_init(&t.body_text,t.body_store[0],t.body_store[1],sizeof(t.body_store[0]));desk_edit_init(&t.body,&t.body_text,1);
    desk_ui_add(&t.ui,TOOL_BODY,DESK_CONTROL_EDIT,&t.body,NULL);tool_layout(&t);desk_ui_focus(&t.ui,TOOL_BODY);
    desk_text_buffer_set(&t.body_text,"One two ONE",11);desk_text_buffer_select(&t.body_text,0,3);uint64_t revision=t.body_text.revision;
    tool_text_search_open(&t);assert(t.search_open && t.search_focus && t.search.query.focused && !t.ui.active && !strcmp(t.search.text.data,"One"));
    tool_text_search_step(&t,0,0);assert(t.body_text.anchor==8 && t.body_text.caret==11 && t.body_text.revision==revision);
    tool_text_search_step(&t,0,0);assert(t.body_text.anchor==0 && strstr(t.status,"wrapped"));
    tool_text_search_step(&t,1,0);assert(t.body_text.anchor==8);
    tool_content_focus(&t,0);assert(!t.search.ui.active && !t.ui.active);tool_content_focus(&t,1);assert(t.search.ui.active && !t.ui.active);
    t.search_focus=0;tool_content_focus(&t,1);assert(!t.search.ui.active && t.ui.active);
    t.search_focus=1;tool_content_focus(&t,1);tool_text_search_action(&t,(struct desk_action){DESK_SEARCH_QUERY_FIELD,DESK_EDIT_COPY});assert(clipboard_target==TOOL_FIND_QUERY);
    desk_text_buffer_set(&t.search.text,"absent",6);tool_text_search_step(&t,0,1);assert(strstr(t.status,"No matching") && t.body_text.anchor==8);
    desk_text_buffer_set(&t.search.text,"One",3);t.search.match_case.checked=1;tool_text_search_step(&t,0,1);assert(t.body_text.anchor==0);
    tool_text_search_action(&t,(struct desk_action){DESK_SEARCH_CLOSE,DESK_WIDGET_ACTIVATED});assert(!t.search_open && t.ui.active && t.body.focused);
    t.search.match_case.checked=0;tool_text_search_step(&t,0,0);assert(!t.search_open && t.body_text.anchor==8);
    t.modal=1;tool_text_search_open(&t);assert(!t.search_open);t.modal=0;t.job.busy=1;t.job.request.operation=DESK_FILE_READ;tool_text_search_open(&t);assert(!t.search_open);
    puts("tool-search: PASS selection seeding, next/previous/wrap, read-only selection, focus ownership, clipboard target, case/no-match, hidden-bar repeat and modal/I/O guards");
}
