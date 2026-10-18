#include "app.h"
#include <srvros/desk_log.h>
#include <string.h>
void tool_content_focus(struct tool *t,int active) {
    int searching=t->search_open && t->search_focus;
    desk_ui_window_focus(&t->ui,active && !searching);
    if(t->search_open) desk_searchbar_focus(&t->search,active && searching);
}
void tool_text_search_open(struct tool *t) {
    if(t->role!=DESK_APP_TEXT || t->modal || t->picker_mode || (t->job.busy && t->job.request.operation==DESK_FILE_READ)) return;
    char seed[DESK_SEARCH_QUERY];const char *initial=NULL;
    size_t lo=t->body_text.anchor<t->body_text.caret?t->body_text.anchor:t->body_text.caret;
    size_t hi=t->body_text.anchor>t->body_text.caret?t->body_text.anchor:t->body_text.caret;
    if(hi>lo && hi-lo<sizeof(seed) && !memchr(t->body_text.data+lo,'\n',hi-lo) && !memchr(t->body_text.data+lo,'\r',hi-lo)) {
        memcpy(seed,t->body_text.data+lo,hi-lo);seed[hi-lo]=0;initial=seed;
    }
    t->search_open=t->search_focus=1;tool_layout(t);
    if(desk_searchbar_open(&t->search,initial)) desk_searchbar_open(&t->search,NULL);
    tool_content_focus(t,t->window.focused);tool_status(t,"Find text · Enter next · Shift-Enter previous · Esc closes search");
    if(t->search.text.length) tool_text_search_step(t,0,1);
    desk_log("desktools: search opened\n");
}
void tool_text_search_close(struct tool *t) {
    if(!t->search_open) return;
    desk_searchbar_focus(&t->search,0);t->search_open=t->search_focus=0;
    tool_layout(t);tool_content_focus(t,t->window.focused);desk_ui_focus(&t->ui,TOOL_BODY);
    tool_status(t,"Search closed. The match remains selected.");desk_log("desktools: search closed\n");
}
void tool_text_search_step(struct tool *t,int backwards,int restart) {
    if(t->role!=DESK_APP_TEXT || t->modal || t->picker_mode || (t->job.busy && t->job.request.operation==DESK_FILE_READ)) return;
    if(!t->search_open && !t->search.text.length) { tool_text_search_open(t);return; }
    if(!t->search.text.length) { tool_status(t,"Enter text to find.");return; }
    size_t lo=t->body_text.anchor<t->body_text.caret?t->body_text.anchor:t->body_text.caret;
    size_t hi=t->body_text.anchor>t->body_text.caret?t->body_text.anchor:t->body_text.caret;
    struct desk_text_match match;
    int found=desk_text_find(&t->body_text,t->search.text.data,t->search.text.length,backwards || restart?lo:hi,backwards,t->search.match_case.checked,&match);
    if(found==1) {
        desk_text_buffer_select(&t->body_text,match.start,match.end);desk_edit_reveal(&t->body);
        tool_status(t,match.wrapped?"Match found · Search wrapped around the document":"Match found · Enter next · Shift-Enter previous");
        desk_log("desktools: search match=%llu..%llu wrapped=%d\n",(unsigned long long)match.start,(unsigned long long)match.end,match.wrapped);
    } else { tool_status(t,found<0?"This search text is invalid.":"No matching text.");desk_log("desktools: search no-match\n"); }
    t->window.dirty=1;
}
void tool_text_search_action(struct tool *t,struct desk_action a) {
    if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
    if(a.id==DESK_SEARCH_QUERY_FIELD) {
        tool_clipboard_action(t,(struct desk_action){TOOL_FIND_QUERY,a.flags});
        if(a.flags&DESK_WIDGET_CHANGED) tool_text_search_step(t,0,1);
    }
    if(a.id==DESK_SEARCH_CASE && (a.flags&DESK_WIDGET_CHANGED)) tool_text_search_step(t,0,1);
    if(a.flags&DESK_WIDGET_ACTIVATED) {
        if(a.id==DESK_SEARCH_CLOSE) tool_text_search_close(t);
        if(a.id==DESK_SEARCH_PREVIOUS || a.id==DESK_SEARCH_NEXT) tool_text_search_step(t,a.id==DESK_SEARCH_PREVIOUS,0);
    }
}
