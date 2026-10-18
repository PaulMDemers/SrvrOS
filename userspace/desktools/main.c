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
    snprintf(t->status,sizeof(t->status),"%s",message);t->window.dirty=1;
}
void tool_launch(struct tool *t,int application,const char *path) {
    if(desk_app_launch(&t->window,application,path)) tool_status(t,"The application could not be opened. Try again.");
}
void tool_dialog(struct tool *t,int mode) {
    t->modal=mode;desk_ui_window_focus(&t->ui,0);desk_ui_init(&t->dialog);
    desk_ui_add(&t->dialog,1001,DESK_CONTROL_BUTTON,&t->dialog_buttons[0],mode==2?"Replace":"Save");
    if(mode==1) desk_ui_add(&t->dialog,1002,DESK_CONTROL_BUTTON,&t->dialog_buttons[1],"Discard");
    desk_ui_add(&t->dialog,1003,DESK_CONTROL_BUTTON,&t->dialog_buttons[2],"Cancel");
    tool_layout(t);desk_ui_focus(&t->dialog,1003);t->window.dirty=1;
}
void tool_layout(struct tool *t) {
    int w=t->window.width,h=t->window.height;
    if(w<280 || h<240) return;
    int busy=t->job.busy,read_busy=busy && t->job.request.operation==DESK_FILE_READ;
    if(t->role==DESK_APP_TEXT || t->role==DESK_APP_FILES) {
        for(int i=0;i<3;i++) desk_button_configure(&t->buttons[i],(struct desk_rect){16+i*84,16,76,28},!busy);
        desk_edit_configure(&t->path,(struct desk_rect){16,56,w-32,34},&desk_font_16,!read_busy && (t->role==DESK_APP_TEXT || !busy));
        if(t->role==DESK_APP_TEXT) {
            desk_edit_configure(&t->body,(struct desk_rect){16,102,w-32,h-144},t->body_size==20?&desk_font_20:&desk_font_16,!read_busy);
        } else {
            desk_list_configure(&t->list,(struct desk_rect){16,102,w-32,h-190},!busy);
            desk_edit_configure(&t->name,(struct desk_rect){16,h-76,w-156,34},&desk_font_16,!busy);
            desk_button_configure(&t->buttons[3],(struct desk_rect){w-132,h-73,116,28},!busy);
        }
    } else if(t->role==DESK_APP_CALCULATOR) {
        desk_edit_configure(&t->body,(struct desk_rect){24,56,w-48,38},&desk_font_20,1);
        desk_button_configure(&t->buttons[0],(struct desk_rect){24,110,112,32},1);
        desk_button_configure(&t->buttons[1],(struct desk_rect){148,110,88,32},1);
    } else if(t->role==DESK_APP_SETTINGS) {
        desk_button_configure(&t->buttons[0],(struct desk_rect){24,116,100,32},1);
        desk_button_configure(&t->buttons[1],(struct desk_rect){136,116,100,32},1);
    }
    desk_ui_layout_changed(&t->ui);
    if(t->modal) {
        int width=w-32;if(width>480) width=480;int x=(w-width)/2,y=(h-170)/2;
        int button_width=(width-40)/3;
        for(int i=0;i<3;i++) desk_button_configure(&t->dialog_buttons[i],(struct desk_rect){x+12+i*(button_width+8),y+120,button_width,32},1);
        desk_ui_layout_changed(&t->dialog);
    }
    t->window.dirty=1;
}
static void action(struct tool *t,struct desk_action a) {
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
    } else if(t->role==DESK_APP_SETTINGS && (a.flags&DESK_WIDGET_ACTIVATED)) {
        if(desk_app_text_size(&t->window,a.id==1?16:20)) tool_status(t,"The setting could not be applied. Try again.");
    }
}
static void key(struct tool *t,const struct srv_key_event *k) {
    if(t->modal) {
        if(k->usage==SRV_KEY_ESCAPE && (k->flags&SRV_KEY_DOWN_EVENT)) tool_text_dialog(t,1003);
        else { struct desk_action a=desk_ui_key(&t->dialog,k);if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;
            if(a.flags&DESK_WIDGET_ACTIVATED) tool_text_dialog(t,a.id); }
        return;
    }
    if((k->flags&SRV_KEY_DOWN_EVENT) && (k->modifiers&(SRV_KEY_CTRL|SRV_KEY_META))) {
        if(k->usage==0x1a) { if(t->role==DESK_APP_TEXT) tool_text_request(t,TOOL_PENDING_CLOSE);else t->closing=1;return; }
        if(t->role==DESK_APP_FILES) {
            if(k->usage==0x0f) { desk_ui_focus(&t->ui,TOOL_PATH);desk_text_buffer_select(&t->path_text,0,t->path_text.length);t->window.dirty=1;return; }
            if(k->usage==0x15) { tool_files_action(t,(struct desk_action){TOOL_REFRESH,DESK_WIDGET_ACTIVATED});return; }
        }
        if(t->role==DESK_APP_TEXT) {
            if(k->usage==0x16) { action(t,(struct desk_action){TOOL_SAVE,DESK_WIDGET_ACTIVATED});return; }
            if(k->usage==0x12) { desk_ui_focus(&t->ui,TOOL_PATH);desk_text_buffer_select(&t->path_text,0,t->path_text.length);t->window.dirty=1;return; }
            if(k->usage==0x11) { tool_text_request(t,TOOL_PENDING_NEW);return; }
        }
    }
    if(t->role==DESK_APP_FILES && (k->flags&SRV_KEY_DOWN_EVENT) && (k->modifiers&SRV_KEY_ALT) && k->usage==SRV_KEY_UP) {
        tool_files_action(t,(struct desk_action){TOOL_UP,DESK_WIDGET_ACTIVATED});return;
    }
    action(t,desk_ui_key(&t->ui,k));
}
static void system_refresh(struct tool *t) {
    uint64_t now=srv_ticks();if(now-t->refresh_tick<100) return;t->refresh_tick=now;
    struct srv_meminfo memory;unsigned processes=0;struct srv_process_info p;
    long index=0;while((index=srv_proc_list(index,&p))>0) processes++;
    if(srv_meminfo(&memory)==0) snprintf(t->status,sizeof(t->status),"Memory: %llu MiB used / %llu MiB total   ·   %u processes",
        (unsigned long long)(memory.used_bytes/(1024*1024)),(unsigned long long)(memory.total_bytes/(1024*1024)),processes);
    else snprintf(t->status,sizeof(t->status),"%u processes   ·   Memory information unavailable",processes);
    t->window.dirty=1;
}
static void draw(struct tool *t,struct desk_canvas *c) {
    const struct desk_theme *theme=&desk_theme_quiet;
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},theme->surface);
    desk_ui_draw(c,&t->ui,theme);
    if(t->role==DESK_APP_CALCULATOR) {
        desk_label_draw(c,(struct desk_rect){24,20,c->width-48,28},theme->heading,"Calculator",theme->text);
        desk_label_draw(c,(struct desk_rect){24,164,c->width-48,28},theme->body,"Use +, -, *, / and parentheses. Enter evaluates.",theme->muted);
    } else if(t->role==DESK_APP_SYSTEM) {
        desk_text(c,theme->heading,24,24,"SrvrOS",theme->text);
        desk_label_draw(c,(struct desk_rect){24,64,c->width-48,28},theme->body,"A small native desktop, built from scratch.",theme->text);
        desk_label_draw(c,(struct desk_rect){24,100,c->width-48,28},theme->body,"Target: MacBook Air · x86-64 · USB input and exFAT",theme->muted);
        desk_label_draw(c,(struct desk_rect){24,136,c->width-48,28},theme->body,"DejaVu Sans · software rendering · independent applications",theme->muted);
    } else if(t->role==DESK_APP_SETTINGS) {
        desk_text(c,theme->heading,24,24,"Appearance",theme->text);
        desk_label_draw(c,(struct desk_rect){24,72,c->width-48,28},theme->body,"Document text size",theme->text);
        desk_label_draw(c,(struct desk_rect){24,176,c->width-48,32},t->body_size==20?theme->heading:theme->body,"The quick brown fox jumps over the lazy dog.",theme->text);
    }
    desk_label_draw(c,(struct desk_rect){16,c->height-28,c->width-32,24},theme->body,t->status,theme->muted);
    if(t->modal) {
        int width=c->width-32;if(width>480) width=480;int x=(c->width-width)/2,y=(c->height-170)/2;
        desk_fill(c,(struct desk_rect){x+4,y+5,width,170},0xb9c2b4);
        desk_fill(c,(struct desk_rect){x,y,width,170},theme->border);
        desk_fill(c,(struct desk_rect){x+1,y+1,width-2,168},theme->surface);
        desk_label_draw(c,(struct desk_rect){x+16,y+18,width-32,30},theme->heading,t->modal==2?"Replace this file?":"Save your changes?",theme->text);
        desk_label_draw(c,(struct desk_rect){x+16,y+60,width-32,28},theme->body,t->modal==2?"A file already exists at this path.":"This document has unsaved edits.",theme->muted);
        desk_label_draw(c,(struct desk_rect){x+16,y+84,width-32,28},theme->body,"Cancel returns to your document.",theme->muted);
        desk_ui_draw(c,&t->dialog,theme);
    }
}
int main(int argc,char **argv) {
    struct tool *t=&app;t->role=DESK_APP_TEXT;t->body_size=16;t->title_dirty=1;
    if(argc>1) for(unsigned i=0;i<sizeof(roles)/sizeof(*roles);i++) if(!strcmp(argv[1],roles[i])) t->role=i+1;
    desk_ui_init(&t->ui);desk_ui_init(&t->dialog);desk_list_init(&t->list);
    desk_clipboard_init(&t->clip,t->clipboard);
    desk_text_buffer_init(&t->path_text,t->path_store[0],t->path_store[1],DESK_FILE_PATH);
    desk_text_buffer_init(&t->body_text,t->body_store[0],t->body_store[1],DESK_FILE_BYTES+1);
    desk_text_buffer_init(&t->name_text,t->name_store[0],t->name_store[1],128);
    desk_edit_init(&t->path,&t->path_text,0);desk_edit_init(&t->body,&t->body_text,t->role==DESK_APP_TEXT);
    desk_edit_init(&t->name,&t->name_text,0);
    if(t->role==DESK_APP_TEXT || t->role==DESK_APP_FILES) t->worker_ready=desk_file_job_init(&t->job)==0;
    desk_log("desktools: starting role=%s\n",roles[t->role-1]);
    if(desk_app_open(&t->window,titles[t->role-1],t->role<=2?720:560,t->role<=2?480:320)) return 1;
    if(t->role==DESK_APP_TEXT) tool_text_init(t,argc>2?argv[2]:"");
    else if(t->role==DESK_APP_FILES) tool_files_init(t,argc>2?argv[2]:"");
    else if(t->role==DESK_APP_CALCULATOR) {
        desk_ui_add(&t->ui,TOOL_BODY,DESK_CONTROL_EDIT,&t->body,NULL);
        desk_ui_add(&t->ui,1,DESK_CONTROL_BUTTON,&t->buttons[0],"Calculate");
        desk_ui_add(&t->ui,2,DESK_CONTROL_BUTTON,&t->buttons[1],"Clear");tool_status(t,"Ready.");
    } else if(t->role==DESK_APP_SETTINGS) {
        desk_ui_add(&t->ui,1,DESK_CONTROL_BUTTON,&t->buttons[0],"Standard");
        desk_ui_add(&t->ui,2,DESK_CONTROL_BUTTON,&t->buttons[1],"Larger");tool_status(t,"Document text size applies to this desktop session.");
    }
    while(!t->quit && !t->window.failed && !t->window.closed) {
        if(desk_clipboard_tick(&t->clip,srv_ticks())<0) tool_clipboard_result(t,-1);
        for(unsigned n=0;n<32;n++) {
            struct desk_packet p;int r=desk_app_poll(&t->window,&p);
            if(r<=0) break;
            if(r==2) continue;
            if(p.type==DESK_CLIPBOARD_ACK || p.type==DESK_CLIPBOARD_DATA) {
                int result=desk_clipboard_receive(&t->clip,&p);if(result) tool_clipboard_result(t,result);
            } else if(p.type==DESK_CONFIGURE) { tool_layout(t);if(t->role==DESK_APP_TEXT || t->role==DESK_APP_CALCULATOR) desk_ui_focus(&t->ui,TOOL_BODY); }
            else if(p.type==DESK_PREFERENCES && (p.value==16 || p.value==20)) {
                t->body_size=p.value;tool_layout(t);
                if(t->role==DESK_APP_SETTINGS) tool_status(t,p.value==16?"Standard text · applies to this desktop session.":"Larger text · applies to this desktop session.");
            } else if(p.type==DESK_FOCUS) {
                desk_ui_window_focus(t->modal?&t->dialog:&t->ui,p.value!=0);t->window.dirty=1;
            } else if(p.type==DESK_KEY) { struct srv_key_event k;memcpy(&k,p.text,sizeof(k));key(t,&k); }
            else if(p.type==DESK_POINTER) {
                struct desk_widget_event e={.type=p.value&DESK_POINTER_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=p.x,.y=p.y,.down=(p.value&DESK_POINTER_LEFT)!=0,.pressed=(p.value&DESK_POINTER_PRESS)!=0};
                struct desk_action a=desk_ui_pointer(t->modal?&t->dialog:&t->ui,e,srv_monotonic_us());
                if(t->modal) { if(a.flags&DESK_WIDGET_REDRAW) t->window.dirty=1;if(a.flags&DESK_WIDGET_ACTIVATED) tool_text_dialog(t,a.id); }
                else action(t,a);
            } else if(p.type==DESK_SCROLL && !t->modal) action(t,desk_ui_scroll(&t->ui,p.width,p.height,p.x,p.y));
            else if(p.type==DESK_CLOSE_REQUEST) {
                if(t->role==DESK_APP_TEXT) tool_text_request(t,TOOL_PENDING_CLOSE);else t->closing=1;
            }
        }
        if(t->job.busy) { int result=desk_file_job_poll(&t->job);if(result) {
            desk_log("desktools: file-result role=%s op=%u status=%u bytes=%u\n",roles[t->role-1],t->job.request.operation,t->job.response.status,t->job.response.length);
            if(t->role==DESK_APP_TEXT) tool_text_finish(t,result);else tool_files_finish(t,result);
        } }
        if(t->closing && !t->job.busy && t->role!=DESK_APP_TEXT) t->quit=1;
        if(t->role==DESK_APP_SYSTEM) system_refresh(t);
        if(t->role==DESK_APP_TEXT && t->title_dirty && t->window.server) {
            const char *name=t->saved_path[0]?strrchr(t->saved_path,'/')+1:"Untitled";char title[88];
            snprintf(title,sizeof(title),"Text — %s%s",name,tool_modified(t)?" *":"");
            if(!desk_app_title(&t->window,title)) t->title_dirty=0;
        }
        struct desk_canvas c;if(desk_app_canvas(&t->window,&c)==1) { draw(t,&c);desk_app_present(&t->window); }
        srv_sleep_ticks(1);
    }
    if(t->worker_ready) desk_file_job_close(&t->job);
    desk_app_close(&t->window);desk_log("desktools: exited role=%s failure=%d\n",roles[t->role-1],t->window.failed);
    return t->window.failed?1:0;
}
