#include <srvros/desk_paths.h>
#include "app.h"
#include <srvros/desk_log.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
_Static_assert(DESK_PICKER_PATH==DESK_FILE_PATH,"picker/worker path limit");
static void browse(struct tool *t) {
    if(!t->worker_ready || desk_file_job_start(&t->job,DESK_FILE_LIST,t->picker.target,NULL,0,0,0)) {
        desk_picker_error(&t->picker,"The folder could not be opened. Check the path.");t->window.dirty=1;return;
    }
    t->picker_job=1;desk_picker_loading(&t->picker);desk_picker_focus(&t->picker,t->window.focused);t->window.dirty=1;
}
static void open_failed(struct tool *t,const char *message) {
    t->pending=t->closing=0;desk_app_cancel_close(&t->window);tool_status(t,message);
}
void tool_picker_open(struct tool *t,int mode) {
    if(t->job.busy || t->modal || t->picker_mode) return;
    if(!t->worker_ready) { open_failed(t,"The file worker is unavailable. Your document was kept.");return; }
    tool_text_search_close(t);
    char folder[DESK_FILE_PATH],name[DESK_PICKER_NAME];
    const char *candidate=t->path_text.data;
    if(candidate[0]=='/') snprintf(folder,sizeof(folder),"%s",candidate);
    else snprintf(folder,sizeof(folder),"%s/Untitled.txt",desk_home());
    char *slash=strrchr(folder,'/');
    const char *leaf=slash?slash+1:"Untitled.txt";
    if(!leaf[0] || strlen(leaf)>=sizeof(name)) leaf="Untitled.txt";
    strcpy(name,mode==DESK_PICKER_SAVE?leaf:"");
    if(slash && slash!=folder) *slash=0;else strcpy(folder,"/");
    if(desk_picker_open(&t->picker,mode,folder,name)) { open_failed(t,"The file picker could not open this path.");return; }
    t->picker_mode=mode;desk_ui_window_focus(&t->ui,0);
    desk_picker_configure(&t->picker,(struct desk_rect){0,0,t->window.width,t->window.height});
    browse(t);desk_log("desktools: picker mode=%d opened\n",mode);
}
void tool_picker_cancel(struct tool *t) {
    t->picker_mode=0;t->pending=0;t->closing=0;desk_picker_focus(&t->picker,0);
    desk_app_cancel_close(&t->window);desk_ui_window_focus(&t->ui,t->window.focused);desk_ui_focus(&t->ui,TOOL_BODY);
    tool_status(t,"File selection canceled. Your document was kept.");
}
void tool_picker_result(struct tool *t,struct desk_picker_result a) {
    if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
    unsigned id=a.field==DESK_PICKER_LOCATION?TOOL_PICK_LOCATION:a.field==DESK_PICKER_FILENAME?TOOL_PICK_NAME:0;
    if(id) tool_clipboard_action(t,(struct desk_action){id,a.flags});
    if(a.operation==DESK_PICKER_CANCEL) { tool_picker_cancel(t);return; }
    if(a.operation==DESK_PICKER_BROWSE) { browse(t);return; }
    if(a.operation==DESK_PICKER_ACCEPT) {
        char path[DESK_FILE_PATH];strcpy(path,t->picker.target);int mode=t->picker_mode;
        t->picker_mode=0;desk_picker_focus(&t->picker,0);desk_ui_window_focus(&t->ui,t->window.focused);
        if(mode==DESK_PICKER_SAVE) tool_text_save_path(t,path);else tool_text_open_path(t,path);
    }
}
void tool_picker_finish(struct tool *t,int result) {
    t->picker_job=0;
    if(!t->picker_mode) {
        if(t->closing) { t->closing=0;tool_text_request(t,TOOL_PENDING_CLOSE); }
        return;
    }
    if(result<0 || t->job.response.status) desk_picker_error(&t->picker,"Could not read this folder. Check the path and volume.");
    else {
        int count=tool_listing_decode(t);
        if(count<0 || desk_picker_listing(&t->picker,t->job.request.path,t->items,count))
            desk_picker_error(&t->picker,"The folder listing was invalid.");
        else desk_log("desktools: picker ready mode=%d items=%d\n",t->picker_mode,count);
    }
    desk_picker_focus(&t->picker,t->window.focused);t->window.dirty=1;
}
