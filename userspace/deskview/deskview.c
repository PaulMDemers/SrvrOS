#include <srvros/desk_app.h>
#include <srvros/desk_ui.h>
#include <srvros/desk_file_picker.h>
#include <srvros/desk_listing.h>
#include <srvros/desk_file_job.h>
#include <srvros/desk_image_job.h>
#include <srvros/desk_clipboard.h>
#include <srvros/desk_log.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

enum { VIEW_OPEN=1,VIEW_CANCEL,VIEW_FIT,VIEW_ACTUAL,VIEW_OUT,VIEW_IN,VIEW_IMAGE,VIEW_CLOSE };
struct preview_app {
    struct desk_app window;
    struct desk_ui ui;
    struct desk_button buttons[6];
    struct desk_image view;
    struct desk_image_job image_job;
    struct desk_file_job file_job;
    struct desk_file_picker picker;
    struct desk_file_entry entries[DESK_FILE_ENTRIES];
    struct desk_list_item items[DESK_FILE_ENTRIES];
    char details[DESK_FILE_ENTRIES][32];
    struct desk_clipboard_client clipboard;
    unsigned char clipboard_data[DESK_CLIPBOARD_BYTES+1];
    uint64_t clip_revision;
    size_t clip_caret,clip_anchor;
    unsigned clip_action,clip_field;
    uint32_t *pixels;
    char path[DESK_IMAGE_PATH],pending_path[DESK_IMAGE_PATH],status[160];
    int file_ready,picker_open,quit,started,title_dirty;
};
static struct preview_app app;
static void status(const char *message) { snprintf(app.status,sizeof(app.status),"%s",message);app.window.dirty=1; }
static void layout(void) {
    int w=app.window.width,h=app.window.height,x=12;
    static const int small[]={54,54,36,36,28,28},large[]={72,72,60,60,36,36};
    for(unsigned i=0;i<6;i++) {
        int width=w<420?small[i]:large[i];
        int enabled=i==0?!app.image_job.busy && !app.image_job.retiring_pid && !app.file_job.busy:
            i==1?app.image_job.busy:app.pixels!=NULL;
        desk_button_configure(&app.buttons[i],(struct desk_rect){x,12,width,28},enabled);x+=width+4;
    }
    app.ui.controls[1].label=w<420?"Stop":"Cancel";
    desk_image_configure(&app.view,(struct desk_rect){12,52,w-24,h-112},1);
    desk_ui_layout_changed(&app.ui);desk_ui_window_focus(&app.ui,app.window.focused && !app.picker_open);
    desk_picker_configure(&app.picker,(struct desk_rect){0,0,w,h});
    app.window.dirty=1;
}
static void load_image(const char *path) {
    if(desk_image_job_start(&app.image_job,path,srv_ticks())) { status("The image could not be opened. Try again in a moment.");return; }
    snprintf(app.pending_path,sizeof(app.pending_path),"%s",path);status("Opening image…");layout();desk_ui_focus(&app.ui,VIEW_IMAGE);
    desk_log("deskview: loading path=%s\n",path);
}
static const char *image_error(unsigned result) {
    switch(result) {
    case DESK_IMAGE_NOT_FOUND: return "The image was not found. Check its location.";
    case DESK_IMAGE_FORMAT: return "This file could not be decoded as PNG, JPEG or BMP.";
    case DESK_IMAGE_LIMIT: return "Image limit: 8 MiB file, 4 million pixels, 4096 pixels per side.";
    case DESK_IMAGE_MEMORY: return "There is not enough memory to open this image.";
    default: return "The image could not be read. Check the file and volume.";
    }
}
static void image_finish(int result) {
    desk_log("deskview: image-result transport=%d status=%u retained=%d received=%zu expected=%u elapsed_ticks=%llu\n",result,app.image_job.response.status,app.pixels!=NULL,app.image_job.received,app.image_job.response.length,(unsigned long long)((uint64_t)srv_ticks()-app.image_job.started));
    if(result<0) status("Opening did not finish. The previous image was kept; try again.");
    else if(app.image_job.response.status) status(image_error(app.image_job.response.status));
    else {
        uint32_t *pixels=desk_image_job_take(&app.image_job);
        if(!pixels) status("The image transfer did not finish. The previous image was kept.");
        else {
            desk_image_pixels_release(app.pixels,app.view.width,app.view.height);app.pixels=pixels;strcpy(app.path,app.pending_path);app.title_dirty=1;
            desk_image_set(&app.view,pixels,app.image_job.response.width,app.image_job.response.height);
            status("Drag or scroll to pan · + / − zoom · 0 fits · 1 shows actual size");
            desk_log("deskview: loaded path=%s width=%u height=%u bytes=%u\n",app.path,app.view.width,app.view.height,app.image_job.response.length);
        }
    }
    layout();desk_ui_focus(&app.ui,VIEW_IMAGE);
}
static void browse(void) {
    if(!app.file_ready || desk_file_job_start(&app.file_job,DESK_FILE_LIST,app.picker.target,NULL,0,0,0))
        desk_picker_error(&app.picker,"The folder could not be opened. Check the path.");
    else desk_picker_loading(&app.picker);
    desk_picker_focus(&app.picker,app.window.focused);app.window.dirty=1;
}
static void picker_close(void) {
    app.picker_open=0;app.clip_action=0;desk_picker_focus(&app.picker,0);layout();desk_ui_focus(&app.ui,VIEW_IMAGE);
}
static void picker_open(void) {
    if(app.image_job.busy || app.image_job.retiring_pid || app.file_job.busy || app.picker_open) return;
    if(app.file_ready && app.file_job.broken) { desk_file_job_close(&app.file_job);app.file_ready=0; }
    if(!app.file_ready) app.file_ready=!desk_file_job_init(&app.file_job);
    if(!app.file_ready) { status("The folder could not be opened. Try opening again.");return; }
    char directory[DESK_IMAGE_PATH]="/fat/home";
    if(app.path[0]) { strcpy(directory,app.path);char *slash=strrchr(directory,'/');if(slash==directory) slash[1]=0;else if(slash) *slash=0; }
    if(desk_picker_open(&app.picker,DESK_PICKER_OPEN,directory,"")) return;
    app.picker_open=1;layout();browse();desk_log("deskview: picker opened\n");
}
static struct desk_edit *clipboard_target(void) {
    if(!app.picker_open) return NULL;
    return app.clip_field==DESK_PICKER_LOCATION?&app.picker.location:app.clip_field==DESK_PICKER_FILENAME?&app.picker.name:NULL;
}
static void clipboard_action(struct desk_picker_result r) {
    unsigned operation=r.flags&(DESK_EDIT_COPY|DESK_EDIT_CUT|DESK_EDIT_PASTE);if(!operation || app.clipboard.mode) return;
    app.clip_field=r.field;struct desk_edit *edit=clipboard_target();if(!edit) return;
    struct desk_text_buffer *t=edit->text;size_t lo=t->caret<t->anchor?t->caret:t->anchor,hi=t->caret>t->anchor?t->caret:t->anchor;
    if(operation!=DESK_EDIT_PASTE && lo==hi) return;
    int result=operation==DESK_EDIT_PASTE?desk_clipboard_paste(&app.clipboard,app.window.server,srv_ticks()):
        desk_clipboard_copy(&app.clipboard,app.window.server,t->data+lo,hi-lo,srv_ticks());
    if(result) return;
    app.clip_action=operation;app.clip_revision=t->revision;app.clip_caret=t->caret;app.clip_anchor=t->anchor;
}
static void clipboard_finish(int result) {
    unsigned action=app.clip_action;app.clip_action=0;
    if(!app.picker_open || !action || action==DESK_EDIT_COPY) return;
    struct desk_edit *edit=clipboard_target();if(!edit) return;struct desk_text_buffer *t=edit->text;
    if(result<0 || !edit->enabled || t->revision!=app.clip_revision || t->caret!=app.clip_caret || t->anchor!=app.clip_anchor) {
        desk_picker_message(&app.picker,"Clipboard transfer canceled. Repeat Cut or Paste when ready.");app.window.dirty=1;return;
    }
    size_t length=action==DESK_EDIT_CUT?0:app.clipboard.length;
    if(action==DESK_EDIT_PASTE && !length) return;
    if(memchr(app.clipboard_data,'\n',length) || memchr(app.clipboard_data,'\r',length) ||
        desk_text_buffer_insert(t,action==DESK_EDIT_CUT?"":(char *)app.clipboard_data,length)<0)
        desk_picker_message(&app.picker,"Use a single line that fits this field.");
    else desk_edit_reveal(edit);
    app.window.dirty=1;
}
static void picker_action(struct desk_picker_result r) {
    if(r.flags&DESK_WIDGET_REDRAW) app.window.dirty=1;
    clipboard_action(r);
    if(r.operation==DESK_PICKER_CANCEL) { picker_close();status("Open canceled. The current image was kept."); }
    else if(r.operation==DESK_PICKER_BROWSE) browse();
    else if(r.operation==DESK_PICKER_ACCEPT) { char path[DESK_IMAGE_PATH];strcpy(path,app.picker.target);picker_close();load_image(path); }
}
static void file_finish(int result) {
    if(!app.picker_open) { layout();return; }
    int count=-1;
    if(result>0 && !app.file_job.response.status)
        count=desk_listing_decode(app.entries,app.items,app.details,app.file_job.incoming,app.file_job.response.length,app.file_job.response.count);
    if(count<0 || desk_picker_listing(&app.picker,app.file_job.request.path,app.items,count)) desk_picker_error(&app.picker,"Could not read this folder. Check the path and volume.");
    else desk_log("deskview: picker ready items=%d\n",count);
    desk_picker_focus(&app.picker,app.window.focused);app.window.dirty=1;
}
static void action(struct desk_action a) {
    if(a.flags&DESK_WIDGET_REDRAW) app.window.dirty=1;
    if(a.id==VIEW_IMAGE && a.flags&DESK_WIDGET_CHANGED)
        desk_log("deskview: view scale=%u fit=%d x=%d y=%d\n",app.view.scale,app.view.fit,app.view.offset_x,app.view.offset_y);
    if(!(a.flags&DESK_WIDGET_ACTIVATED)) return;
    if(a.id==VIEW_OPEN) { picker_open();return; }
    if(a.id==VIEW_CLOSE) { app.quit=1;return; }
    if(a.id==VIEW_CANCEL) { desk_image_job_cancel(&app.image_job);status("Opening canceled. The current image was kept.");layout(); }
    else if(a.id==VIEW_FIT) desk_image_fit(&app.view);
    else if(a.id==VIEW_ACTUAL) desk_image_actual(&app.view);
    else if(a.id==VIEW_IN) desk_image_zoom(&app.view,1);
    else if(a.id==VIEW_OUT) desk_image_zoom(&app.view,-1);
    app.window.dirty=1;desk_ui_focus(&app.ui,VIEW_IMAGE);
    desk_log("deskview: action=%u scale=%u fit=%d x=%d y=%d\n",a.id,app.view.scale,app.view.fit,app.view.offset_x,app.view.offset_y);
}
static void key(const struct srv_key_event *k) {
    if(app.picker_open) { picker_action(desk_picker_key(&app.picker,k));return; }
    if(k->flags&SRV_KEY_DOWN_EVENT) {
        if((k->modifiers&(SRV_KEY_CTRL|SRV_KEY_META)) && k->usage==18) { picker_open();return; }
        if((k->modifiers&(SRV_KEY_CTRL|SRV_KEY_META)) && k->usage==26) { app.quit=1;return; }
        if(k->usage==SRV_KEY_ESCAPE && app.image_job.busy) { action((struct desk_action){VIEW_CANCEL,DESK_WIDGET_ACTIVATED});return; }
    }
    action(desk_ui_key(&app.ui,k));
}
static void menus(void) {
    unsigned modal=app.picker_open?DESK_MENU_DISABLED:0;
    unsigned empty=modal|(!app.pixels?DESK_MENU_DISABLED:0);
    struct desk_menu_item items[]={
        {0,DESK_MENU_HEADING,"File",""},
        {VIEW_OPEN,modal|(app.image_job.busy || app.image_job.retiring_pid || app.file_job.busy?DESK_MENU_DISABLED:0),"Open image…","Ctrl-O"},
        {VIEW_CANCEL,modal|(!app.image_job.busy?DESK_MENU_DISABLED:0),"Cancel opening","Esc"},
        {VIEW_CLOSE,modal,"Close window","Ctrl-W"},
        {0,DESK_MENU_HEADING,"View",""},
        {VIEW_FIT,empty|(app.view.fit?DESK_MENU_CHECKED:0),"Fit in window","0"},
        {VIEW_ACTUAL,empty|(!app.view.fit && app.view.scale==65536?DESK_MENU_CHECKED:0),"Actual size","1"},
        {VIEW_IN,empty|(app.view.scale>=262144?DESK_MENU_DISABLED:0),"Zoom in","+"},
        {VIEW_OUT,empty|(app.view.scale<=4096?DESK_MENU_DISABLED:0),"Zoom out","−"},
    };
    desk_app_menus(&app.window,items,sizeof(items)/sizeof(*items));desk_app_menu_tick(&app.window);
}
static void draw(struct desk_canvas *c) {
    const struct desk_theme *t=&desk_theme_quiet;desk_fill(c,(struct desk_rect){0,0,c->width,c->height},t->surface);desk_ui_draw(c,&app.ui,t);
    if(!app.pixels) {
        int y=app.view.frame.y+app.view.frame.h/2-24;
        desk_label_draw(c,(struct desk_rect){28,y,c->width-56,30},&desk_font_20,"Open an image",t->text);
        desk_label_draw(c,(struct desk_rect){28,y+32,c->width-56,24},t->body,"PNG · JPEG · BMP",t->muted);
    }
    char detail[128];
    if(app.pixels) snprintf(detail,sizeof(detail),"%u × %u pixels · %u%% · %s",app.view.width,app.view.height,(app.view.scale*100u+32768)/65536,app.view.fit?"Fit":"Zoom");
    else strcpy(detail,"Preview");
    desk_label_draw(c,(struct desk_rect){12,c->height-50,c->width-24,24},t->body,detail,t->text);
    desk_label_draw(c,(struct desk_rect){12,c->height-26,c->width-24,24},t->body,app.status,t->muted);
    if(app.picker_open) desk_picker_draw(c,&app.picker,t);
}
int main(int argc,char **argv) {
    desk_ui_init(&app.ui);desk_image_init(&app.view);desk_image_job_init(&app.image_job);desk_picker_init(&app.picker);desk_clipboard_init(&app.clipboard,app.clipboard_data);
    static const char *labels[]={"Open","Cancel","Fit","1:1","−","+"};
    for(unsigned i=0;i<6;i++) { desk_button_centered(&app.buttons[i],1);desk_ui_add(&app.ui,i+1,DESK_CONTROL_BUTTON,&app.buttons[i],labels[i]); }
    desk_ui_add(&app.ui,VIEW_IMAGE,DESK_CONTROL_IMAGE,&app.view,NULL);status("Open an image with Ctrl-O, or choose one in Files.");
    if(desk_app_open(&app.window,"Preview",760,520)) return 1;
    desk_log("deskview: starting\n");
    while(!app.quit && !app.window.failed && !app.window.closed) {
        if(desk_clipboard_tick(&app.clipboard,srv_ticks())<0) clipboard_finish(-1);
        for(unsigned batch=0;batch<32;batch++) {
            struct desk_packet p;int result=desk_app_poll(&app.window,&p);if(result<=0) break;if(result==2) continue;
            if(p.type==DESK_CONFIGURE) {
                layout();desk_ui_focus(&app.ui,VIEW_IMAGE);
                if(!app.started) { app.started=1;if(argc>2 && argv[2][0]) load_image(argv[2]); }
            } else if(p.type==DESK_FOCUS) {
                if(app.picker_open) desk_picker_focus(&app.picker,p.value!=0);else desk_ui_window_focus(&app.ui,p.value!=0);app.window.dirty=1;
            } else if(p.type==DESK_MENU_ACTION) action((struct desk_action){p.value,DESK_WIDGET_ACTIVATED});
            else if(p.type==DESK_KEY) { struct srv_key_event k;memcpy(&k,p.text,sizeof(k));key(&k); }
            else if(p.type==DESK_POINTER) {
                struct desk_widget_event e={.type=p.value&DESK_POINTER_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=p.x,.y=p.y,.down=(p.value&DESK_POINTER_LEFT)!=0,.pressed=(p.value&DESK_POINTER_PRESS)!=0};
                if(app.picker_open) picker_action(desk_picker_pointer(&app.picker,e,srv_monotonic_us()));
                else action(desk_ui_pointer(&app.ui,e,srv_monotonic_us()));
            } else if(p.type==DESK_SCROLL) {
                if(app.picker_open) picker_action(desk_picker_scroll(&app.picker,p.width,p.height,p.y));
                else action(desk_ui_scroll(&app.ui,p.width,p.height,p.x,p.y));
            } else if(p.type==DESK_CLIPBOARD_ACK || p.type==DESK_CLIPBOARD_DATA) {
                int done=desk_clipboard_receive(&app.clipboard,&p);if(done) clipboard_finish(done);
            } else if(p.type==DESK_CLOSE_REQUEST) app.quit=1;
        }
        if(app.quit || app.window.closed || app.window.failed) break;
        int retiring=app.image_job.retiring_pid!=0;
        int result=desk_image_job_poll(&app.image_job,srv_ticks());if(result) image_finish(result);else if(retiring && !app.image_job.retiring_pid) layout();
        if(app.file_job.busy) { int done=desk_file_job_poll(&app.file_job);if(done) file_finish(done); }
        if(app.title_dirty) {
            char title[88];const char *name=strrchr(app.path,'/');snprintf(title,sizeof(title),"Preview — %.75s",name?name+1:app.path);
            size_t length=strlen(title);while(length && !desk_text_utf8_valid(title,length)) title[--length]=0;
            if(!desk_app_title(&app.window,title)) app.title_dirty=0;
        }
        menus();struct desk_canvas canvas;if(desk_app_canvas(&app.window,&canvas)==1) { draw(&canvas);desk_app_present(&app.window); }
        srv_sleep_ticks(1);
    }
    if(app.file_ready) desk_file_job_close(&app.file_job);
    desk_image_job_close(&app.image_job);desk_image_pixels_release(app.pixels,app.view.width,app.view.height);desk_app_close(&app.window);
    desk_log("deskview: exited failure=%d\n",app.window.failed);return app.window.failed?1:0;
}
