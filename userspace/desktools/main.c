#include "app.h"
#include "calculator.h"
#include <srvros/desk_log.h>
#include <srvros/sys.h>
#include <string.h>
#include <stdio.h>

static struct tool app;
static const char *roles[]={"files","text","calculator","system","settings"};
static const char *titles[]={"Files","Text — Untitled","Calculator","System","Settings"};
void tool_status(struct tool *t,const char *message) {
    snprintf(t->status,sizeof(t->status),"%s",message);if(t->picker_mode) desk_picker_message(&t->picker,message);
    if(t->prompt_mode) snprintf(t->prompt.dialog.message,sizeof(t->prompt.dialog.message),"%s",message);t->window.dirty=1;
}
void tool_launch(struct tool *t,int application,const char *path) {
    if(desk_app_launch(&t->window,application,path)) tool_status(t,"The application could not be opened. Try again.");
}
void tool_dialog(struct tool *t,int mode) {
    tool_text_search_close(t);t->modal=mode;desk_ui_window_focus(&t->ui,0);
    struct desk_dialog_choice choices[]={ {1001,"Save",1},{1002,"Discard",1},{1003,"Cancel",1} };
    if(mode==2) { strcpy(choices[0].label,"Replace");choices[1]=choices[2]; }
    desk_dialog_open(&t->dialog,mode==2?"Replace this file?":"Save your changes?",
        mode==2?"A file already exists at this path. Replace saves this document over it. Cancel keeps the existing file.":
        "This document has unsaved edits. Save keeps them on disk. Discard loses these edits. Cancel returns to the document.",
        choices,mode==2?2:3,1003,1003);
    tool_layout(t);desk_dialog_focus(&t->dialog,t->window.focused);t->window.dirty=1;
}
void tool_layout(struct tool *t) {
    int w=t->window.width,h=t->window.height;
    if(w<280 || h<240) return;
    int busy=t->job.busy,read_busy=busy && t->job.request.operation==DESK_FILE_READ;
    if(t->role==DESK_APP_TEXT) {
        for(int i=0;i<3;i++) desk_button_configure(&t->buttons[i],(struct desk_rect){16+i*84,16,76,28},!busy);
        desk_edit_configure(&t->path,(struct desk_rect){16,56,w-32,34},&desk_font_16,!read_busy && (t->role==DESK_APP_TEXT || !busy));
        int search=t->search_open?80:0;
        if(t->search_open) desk_searchbar_configure(&t->search,(struct desk_rect){16,102,w-32,72},!read_busy);
        desk_edit_configure(&t->body,(struct desk_rect){16,102+search,w-32,h-144-search},t->body_size==20?&desk_font_20:&desk_font_16,!read_busy);
    } else if(t->role==DESK_APP_FILES) tool_files_layout(t);
    else if(t->role==DESK_APP_CALCULATOR) {
        desk_edit_configure(&t->body,(struct desk_rect){24,56,w-48,38},&desk_font_20,1);
        desk_button_configure(&t->buttons[0],(struct desk_rect){24,110,112,32},1);
        desk_button_configure(&t->buttons[1],(struct desk_rect){148,110,88,32},1);
    } else if(t->role==DESK_APP_SYSTEM) {
        tool_system_layout(t);
    } else if(t->role==DESK_APP_SETTINGS) {
        tool_settings_pointer(t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});tool_settings_layout(t);
    }
    desk_ui_layout_changed(&t->ui);
    if(t->picker_mode) desk_picker_configure(&t->picker,(struct desk_rect){0,0,w,h});
    if(t->prompt_mode) desk_prompt_configure(&t->prompt,(struct desk_rect){0,0,w,h});
    if(t->modal) desk_dialog_configure(&t->dialog,(struct desk_rect){0,0,w,h});
    t->window.dirty=1;
}
void tool_dispatch(struct tool *t,struct desk_action a) {
    if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
    tool_clipboard_action(t,a);
    if(t->role==DESK_APP_TEXT) tool_text_action(t,a);
    else if(t->role==DESK_APP_FILES) tool_files_action(t,a);
    else if(t->role==DESK_APP_CALCULATOR) {
        if((a.id==TOOL_BODY && (a.flags&DESK_EDIT_SUBMIT)) || (a.id==1 && (a.flags&DESK_WIDGET_ACTIVATED))) {
            double result;const char *error=desk_calculate(t->body_text.data,&result);
            if(error) tool_status(t,error);
            else { snprintf(t->status,sizeof(t->status),"= %.12g",result);t->window.dirty=1; }
        } else if(a.id==2 && (a.flags&DESK_WIDGET_ACTIVATED)) {
            desk_text_buffer_set(&t->body_text,"",0);tool_status(t,"Ready.");desk_ui_focus(&t->ui,TOOL_BODY);
        }
    } else if(t->role==DESK_APP_SETTINGS) tool_settings_action(t,a);
    else if(t->role==DESK_APP_SYSTEM) tool_system_action(t,a);
}
static void key(struct tool *t,const struct srv_key_event *k) {
    if(t->role==DESK_APP_SETTINGS && (k->flags&SRV_KEY_DOWN_EVENT) &&
        !(k->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) &&
        (k->usage==SRV_KEY_PAGEUP || k->usage==SRV_KEY_PAGEDOWN)) {
        tool_settings_scroll(t,(t->window.height-108)*(k->usage==SRV_KEY_PAGEUP?-1:1));return;
    }
    if(t->prompt_mode) { tool_files_prompt(t,desk_prompt_key(&t->prompt,k));return; }
    if(t->picker_mode) { tool_picker_result(t,desk_picker_key(&t->picker,k));return; }
    if(t->modal) {
        struct desk_action a=desk_dialog_key(&t->dialog,k);
        if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
        if(a.flags&DESK_WIDGET_ACTIVATED) { if(t->role==DESK_APP_FILES) tool_files_dialog(t,a.id);else tool_text_dialog(t,a.id); }
        return;
    }
    if((k->flags&SRV_KEY_DOWN_EVENT) && (k->modifiers&(SRV_KEY_CTRL|SRV_KEY_META))) {
        if(k->usage==0x1a) { if(t->role==DESK_APP_TEXT) tool_text_request(t,TOOL_PENDING_CLOSE);else t->closing=1;return; }
        if(t->role==DESK_APP_FILES) {
            if(k->usage==0x0f) { desk_ui_focus(&t->ui,TOOL_PATH);desk_text_buffer_select(&t->path_text,0,t->path_text.length);t->window.dirty=1;return; }
            if(k->usage==0x15) { tool_files_action(t,(struct desk_action){TOOL_REFRESH,DESK_WIDGET_ACTIVATED});return; }
        }
        if(t->role==DESK_APP_TEXT) {
            if(k->usage==0x09) { tool_text_search_open(t);return; }
            if(k->usage==0x0a) { tool_text_search_step(t,(k->modifiers&SRV_KEY_SHIFT)!=0,0);return; }
            if(k->usage==0x16) { tool_text_save(t,(k->modifiers&SRV_KEY_SHIFT)!=0);return; }
            if(k->usage==0x12) { tool_picker_open(t,DESK_PICKER_OPEN);return; }
            if(k->usage==0x0f) { t->search_focus=0;tool_content_focus(t,t->window.focused);desk_ui_focus(&t->ui,TOOL_PATH);desk_text_buffer_select(&t->path_text,0,t->path_text.length);t->window.dirty=1;return; }
            if(k->usage==0x11) { tool_text_request(t,TOOL_PENDING_NEW);return; }
        }
    }
    if(t->role==DESK_APP_FILES && (k->flags&SRV_KEY_DOWN_EVENT) && !(k->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) &&
        t->ui.focus>=0 && t->ui.controls[t->ui.focus].id==TOOL_LIST && (k->usage==0x3b || k->usage==SRV_KEY_DELETE)) {
        tool_files_action(t,(struct desk_action){k->usage==0x3b?TOOL_RENAME:TOOL_DELETE,DESK_WIDGET_ACTIVATED});return;
    }
    if(t->role==DESK_APP_FILES && (k->flags&SRV_KEY_DOWN_EVENT) && (k->modifiers&SRV_KEY_ALT)) {
        unsigned action=k->usage==SRV_KEY_UP?TOOL_UP:k->usage==SRV_KEY_LEFT?TOOL_BACK:k->usage==SRV_KEY_RIGHT?TOOL_FORWARD:k->usage==SRV_KEY_HOME?TOOL_HOME:0;
        if(action) { tool_files_action(t,(struct desk_action){action,DESK_WIDGET_ACTIVATED});return; }
    }
    if(t->role==DESK_APP_FILES && (k->flags&SRV_KEY_DOWN_EVENT) && k->usage==SRV_KEY_F5) { tool_files_action(t,(struct desk_action){TOOL_REFRESH,DESK_WIDGET_ACTIVATED});return; }
    if(t->role==DESK_APP_SYSTEM && (k->flags&SRV_KEY_DOWN_EVENT) && k->usage==0x3e) { tool_system_refresh(t,1);return; }
    if(t->role==DESK_APP_TEXT) {
        if((k->flags&SRV_KEY_DOWN_EVENT) && k->usage==0x3c) { tool_text_search_step(t,(k->modifiers&SRV_KEY_SHIFT)!=0,0);return; }
        if(t->search_open && (k->flags&SRV_KEY_DOWN_EVENT) && k->usage==SRV_KEY_ESCAPE) { tool_text_search_close(t);return; }
        if(t->search_open && t->search_focus) { tool_text_search_action(t,desk_searchbar_key(&t->search,k));return; }
    }
    tool_dispatch(t,desk_ui_key(&t->ui,k));
}
static void draw(struct tool *t,struct desk_canvas *c) {
    const struct desk_theme *theme=&desk_theme_quiet;
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},theme->surface);
    if(t->role==DESK_APP_SETTINGS) tool_settings_draw(t,c);else if(t->role==DESK_APP_FILES) tool_files_draw(t,c);else desk_ui_draw(c,&t->ui,theme);
    if(t->search_open) desk_searchbar_draw(c,&t->search,theme);
    if(t->role==DESK_APP_CALCULATOR) {
        desk_label_draw(c,(struct desk_rect){24,20,c->width-48,28},theme->heading,"Calculator",theme->text);
        desk_label_draw(c,(struct desk_rect){24,164,c->width-48,28},theme->body,"Use +, -, *, / and parentheses. Enter evaluates.",theme->muted);
    } else if(t->role==DESK_APP_SYSTEM) {
        tool_system_draw(t,c);
    }
    desk_label_draw(c,(struct desk_rect){16,c->height-28,c->width-32,24},theme->body,t->status,theme->muted);
    if(t->picker_mode) desk_picker_draw(c,&t->picker,theme);
    if(t->modal) desk_dialog_draw(c,&t->dialog,theme);
    if(t->prompt_mode) desk_prompt_draw(c,&t->prompt,theme);
}
int main(int argc,char **argv) {
    struct tool *t=&app;t->role=DESK_APP_TEXT;t->body_size=16;t->title_dirty=1;
    if(argc>1) for(unsigned i=0;i<sizeof(roles)/sizeof(*roles);i++) if(!strcmp(argv[1],roles[i])) t->role=i+1;
    desk_ui_init(&t->ui);desk_dialog_init(&t->dialog);desk_prompt_init(&t->prompt);desk_picker_init(&t->picker);
    desk_clipboard_init(&t->clip,t->clipboard);
    desk_text_buffer_init(&t->path_text,t->path_store[0],t->path_store[1],DESK_FILE_PATH);
    desk_text_buffer_init(&t->body_text,t->body_store[0],t->body_store[1],DESK_FILE_BYTES+1);
    desk_text_buffer_init(&t->name_text,t->name_store[0],t->name_store[1],128);
    desk_edit_init(&t->path,&t->path_text,0);desk_edit_init(&t->body,&t->body_text,t->role==DESK_APP_TEXT);
    desk_edit_init(&t->name,&t->name_text,0);
    if(t->role==DESK_APP_TEXT || t->role==DESK_APP_FILES) t->worker_ready=desk_file_job_init(&t->job)==0;
    desk_log("desktools: starting role=%s\n",roles[t->role-1]);
    if(desk_app_open(&t->window,titles[t->role-1],t->role==DESK_APP_SYSTEM?760:t->role<=2?720:560,t->role==DESK_APP_SYSTEM?500:t->role==DESK_APP_SETTINGS?560:t->role<=2?480:320)) return 1;
    if(t->role==DESK_APP_TEXT) tool_text_init(t,argc>2?argv[2]:"");
    else if(t->role==DESK_APP_FILES) tool_files_init(t,argc>2?argv[2]:"");
    else if(t->role==DESK_APP_CALCULATOR) {
        desk_ui_add(&t->ui,TOOL_BODY,DESK_CONTROL_EDIT,&t->body,NULL);
        desk_ui_add(&t->ui,1,DESK_CONTROL_BUTTON,&t->buttons[0],"Calculate");
        desk_ui_add(&t->ui,2,DESK_CONTROL_BUTTON,&t->buttons[1],"Clear");tool_status(t,"Ready.");
    } else if(t->role==DESK_APP_SETTINGS) tool_settings_init(t);
    else if(t->role==DESK_APP_SYSTEM) tool_system_init(t);
    while(!t->quit && !t->window.failed && !t->window.closed) {
        if(desk_clipboard_tick(&t->clip,srv_ticks())<0) tool_clipboard_result(t,-1);
        for(unsigned n=0;n<32;n++) {
            struct desk_packet p;int r=desk_app_poll(&t->window,&p);
            if(r<=0) break;
            if(r==2) continue;
            if(p.type==DESK_CLIPBOARD_ACK || p.type==DESK_CLIPBOARD_DATA) {
                int result=desk_clipboard_receive(&t->clip,&p);if(result) tool_clipboard_result(t,result);
            } else if(p.type==DESK_CONFIGURE) { tool_layout(t);if(t->role==DESK_APP_TEXT || t->role==DESK_APP_CALCULATOR) desk_ui_focus(&t->ui,TOOL_BODY); }
            else if(p.type==DESK_PREFERENCES) {
                if(t->role==DESK_APP_SETTINGS) tool_settings_receive(t,&p);
                else {
                    struct desk_preferences preferences;memcpy(&preferences,p.text,sizeof(preferences));
                    if(desk_preferences_valid(&preferences) && t->body_size!=(int)preferences.text_size) { t->body_size=preferences.text_size;tool_layout(t); }
                }
            } else if(p.type==DESK_FOCUS) {
                if(t->role==DESK_APP_SETTINGS && !p.value) tool_settings_pointer(t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
                if(t->prompt_mode) desk_prompt_focus(&t->prompt,p.value!=0);else if(t->picker_mode) desk_picker_focus(&t->picker,p.value!=0);else if(t->modal) desk_dialog_focus(&t->dialog,p.value!=0);else tool_content_focus(t,p.value!=0);t->window.dirty=1;
            } else if(p.type==DESK_MENU_ACTION) tool_menu_action(t,p.value);
            else if(p.type==DESK_KEY) { struct srv_key_event k;memcpy(&k,p.text,sizeof(k));key(t,&k); }
            else if(p.type==DESK_POINTER) {
                struct desk_widget_event e={.type=p.value&DESK_POINTER_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=p.x,.y=p.y,.down=(p.value&DESK_POINTER_LEFT)!=0,.pressed=(p.value&DESK_POINTER_PRESS)!=0};
                if(t->role==DESK_APP_SETTINGS && tool_settings_pointer(t,e)) continue;
                if(t->prompt_mode) { tool_files_prompt(t,desk_prompt_pointer(&t->prompt,e,srv_monotonic_us()));continue; }
                if(t->picker_mode) { tool_picker_result(t,desk_picker_pointer(&t->picker,e,srv_monotonic_us()));continue; }
                if(t->search_open && !t->modal) {
                    if(e.type==DESK_WIDGET_CANCEL) tool_text_search_action(t,desk_searchbar_pointer(&t->search,e,srv_monotonic_us()));
                    if(e.pressed) { t->search_focus=desk_widget_contains(t->search.frame,e.x,e.y);tool_content_focus(t,t->window.focused); }
                    if(t->search_focus) { tool_text_search_action(t,desk_searchbar_pointer(&t->search,e,srv_monotonic_us()));continue; }
                }
                struct desk_action a=t->modal?desk_dialog_pointer(&t->dialog,e,srv_monotonic_us()):desk_ui_pointer(&t->ui,e,srv_monotonic_us());
                if(t->modal) { if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;if(a.flags&DESK_WIDGET_ACTIVATED) { if(t->role==DESK_APP_FILES) tool_files_dialog(t,a.id);else tool_text_dialog(t,a.id); } }
                else tool_dispatch(t,a);
            } else if(p.type==DESK_SCROLL && !t->modal && !t->prompt_mode) {
                if(t->role==DESK_APP_SETTINGS) tool_settings_scroll(t,p.y);
                else if(t->picker_mode) tool_picker_result(t,desk_picker_scroll(&t->picker,p.width,p.height,p.y));
                else tool_dispatch(t,desk_ui_scroll(&t->ui,p.width,p.height,p.x,p.y));
            }
            else if(p.type==DESK_CLOSE_REQUEST) {
                if(t->prompt_mode) tool_files_prompt(t,(struct desk_action){DESK_PROMPT_CANCEL,DESK_WIDGET_ACTIVATED});
                if(t->role==DESK_APP_FILES && t->modal) tool_files_dialog(t,1003);
                if(t->picker_mode) tool_picker_cancel(t);
                if(t->role==DESK_APP_TEXT) tool_text_request(t,TOOL_PENDING_CLOSE);else t->closing=1;
            }
        }
        if(t->job.busy) { int result=desk_file_job_poll(&t->job);if(result) {
            desk_log("desktools: file-result role=%s op=%u status=%u bytes=%u\n",roles[t->role-1],t->job.request.operation,t->job.response.status,t->job.response.length);
            if(t->picker_job) tool_picker_finish(t,result);else if(t->role==DESK_APP_TEXT) tool_text_finish(t,result);else tool_files_finish(t,result);
        } }
        if(t->closing && !t->job.busy && t->role!=DESK_APP_TEXT) t->quit=1;
        if(t->role==DESK_APP_SYSTEM) tool_system_refresh(t,0);
        if(t->role==DESK_APP_TEXT && t->title_dirty && t->window.server) {
            const char *name=t->saved_path[0]?strrchr(t->saved_path,'/')+1:"Untitled";char title[88];
            snprintf(title,sizeof(title),"Text — %s%s",name,tool_modified(t)?" *":"");
            if(!desk_app_title(&t->window,title)) t->title_dirty=0;
        }
        tool_menu_refresh(t);desk_app_menu_tick(&t->window);
        struct desk_canvas c;if(desk_app_canvas(&t->window,&c)==1) { draw(t,&c);desk_app_present(&t->window); }
        srv_sleep_ticks(1);
    }
    if(t->worker_ready) desk_file_job_close(&t->job);
    desk_app_close(&t->window);desk_log("desktools: exited role=%s failure=%d\n",roles[t->role-1],t->window.failed);
    return t->window.failed?1:0;
}
