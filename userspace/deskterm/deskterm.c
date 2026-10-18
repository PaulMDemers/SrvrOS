#include <srvros/desk_app.h>
#include <srvros/desk_terminal_view.h>
#include <srvros/desk_ui.h>
#include <srvros/desk_text.h>
#include <srvros/desk_clipboard.h>
#include <srvros/desk_log.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

#define INPUT_BYTES (DESK_CLIPBOARD_BYTES+32)
struct terminal_app {
    struct desk_app window;
    struct desk_terminal screen;
    struct desk_term_line lines[576],alternate[DESK_TERM_ROWS];
    struct desk_terminal_view view;
    struct desk_ui toolbar,dialog;
    struct desk_button buttons[4],close_buttons[2];
    struct desk_clipboard_client clipboard;
    char clipboard_store[DESK_CLIPBOARD_BYTES+1],copy[DESK_CLIPBOARD_BYTES+1];
    uint8_t input[INPUT_BYTES];unsigned input_head,input_size;
    uint64_t child,exit_status,blink;
    int master,started,exited,quit,modal,clip_paste,pty_eof;
    char status[160];
};
static struct terminal_app app;
static void status(const char *message) { snprintf(app.status,sizeof(app.status),"%s",message);app.window.dirty=1; }
static int enqueue(const void *data,size_t size) {
    if(size>INPUT_BYTES-app.input_size || app.master<0 || app.exited) return -1;
    unsigned position=(app.input_head+app.input_size)%INPUT_BYTES;
    size_t first=INPUT_BYTES-position;if(first>size) first=size;
    memcpy(app.input+position,data,first);memcpy(app.input,(const uint8_t *)data+first,size-first);
    app.input_size+=size;return 0;
}
static void layout(void) {
    int w=app.window.width,h=app.window.height;
    int button_width=(w-48)/4;if(button_width>84) button_width=84;
    for(int i=0;i<4;i++) desk_button_configure(&app.buttons[i],(struct desk_rect){12+i*(button_width+8),12,button_width,28},1);
    desk_ui_layout_changed(&app.toolbar);desk_ui_focus(&app.toolbar,0);
    desk_terminal_view_configure(&app.view,(struct desk_rect){12,50,w-24,h-86},1);
    if(app.master>=0) {
        struct srv_winsize size={.row=app.view.rows,.col=app.view.columns,
            .xpixel=app.view.columns*app.view.cell_width,.ypixel=app.view.rows*app.view.cell_height};
        if(srv_ioctl(app.master,SRV_TIOCSWINSZ,&size)<0) status("The shell's window size could not be updated.");
    }
    int width=w-32;if(width>480) width=480;int x=(w-width)/2,y=(h-160)/2;
    desk_button_configure(&app.close_buttons[0],(struct desk_rect){x+width-220,y+112,96,30},1);
    desk_button_configure(&app.close_buttons[1],(struct desk_rect){x+width-112,y+112,96,30},1);
    desk_ui_layout_changed(&app.dialog);
    desk_terminal_view_event(&app.view,(struct desk_widget_event){.type=app.window.focused && !app.modal?DESK_WIDGET_FOCUS_IN:DESK_WIDGET_FOCUS_OUT});
    app.window.dirty=1;
}
static void start(void) {
    app.started=1;int pair[2];
    if(srv_pty_create(pair)<0) { status("A terminal session could not be created.");return; }
    app.master=pair[0];
    if(srv_fcntl(app.master,SRV_F_SETFL,SRV_FD_NONBLOCK)<0) goto failed;
    layout();
    char *argv[]={"/fat/bin/sh",NULL};
    /* Keep per-window command history in memory. Empty HISTFILE explicitly
     * disables disk history, avoiding cross-session overwrites. */
    char *environment[]={"PATH=/fat/bin:/:/fat","HOME=/fat","PWD=/","TERM=vt100","HISTFILE=","PS1=\\w $ ",NULL};
    struct srv_exec_request request={.path=argv[0],.argv=argv,.envp=environment,
        .flags=SRV_EXEC_BACKGROUND,.stdin_fd=pair[1],.stdout_fd=pair[1],.stderr_fd=pair[1],
        .process_group=UINT64_MAX,.foreground=1};
    long pid=srv_exec(&request);
    if(pid<=0) goto failed;
    app.child=pid;srv_close(pair[1]);status("Ctrl-Shift-C copies selection · Ctrl-Shift-V pastes · Ctrl-C interrupts");
    desk_log("deskterm: ready shell=%llu columns=%u rows=%u\n",(unsigned long long)app.child,app.view.columns,app.view.rows);
    return;
failed:
    srv_close(pair[1]);srv_close(app.master);app.master=-1;status("The shell could not be started.");
}
static void close_request(void) {
    if(!app.child || app.exited) { app.quit=1;return; }
    app.modal=1;desk_terminal_view_event(&app.view,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_OUT});
    desk_ui_window_focus(&app.toolbar,0);desk_ui_window_focus(&app.dialog,1);desk_ui_focus(&app.dialog,2);app.window.dirty=1;
}
static void dialog_action(struct desk_action a) {
    if(a.flags&DESK_WIDGET_REDRAW) app.window.dirty=1;
    if(!(a.flags&DESK_WIDGET_ACTIVATED)) return;
    if(a.id==1) app.quit=1;
    else if(a.id==2) {
        app.modal=0;desk_app_cancel_close(&app.window);desk_ui_window_focus(&app.dialog,0);desk_ui_window_focus(&app.toolbar,1);
        desk_terminal_view_event(&app.view,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});app.window.dirty=1;
    }
}
static void copy_selection(void) {
    if(app.clipboard.mode) { status("The clipboard is busy. Try again.");return; }
    long length=desk_terminal_view_copy(&app.view,app.copy,sizeof(app.copy));
    if(length<0) { status("The selection is larger than the 64 KiB clipboard.");return; }
    if(!length) { status("Drag across terminal text to select it first.");return; }
    app.clip_paste=0;
    if(desk_clipboard_copy(&app.clipboard,app.window.server,app.copy,length,srv_ticks())) status("Copy could not be started.");
}
static void paste(void) {
    if(app.exited || !app.child) { status("This session has ended. Open a new terminal to continue.");return; }
    if(app.clipboard.mode) { status("The clipboard is busy. Try again.");return; }
    app.clip_paste=1;
    if(desk_clipboard_paste(&app.clipboard,app.window.server,srv_ticks())) status("Paste could not be started.");
}
static void clipboard_result(int result) {
    if(result<0) { status("The clipboard transfer failed. Try again.");return; }
    if(app.clip_paste && app.modal) { status("Paste was canceled while the close dialog was open.");return; }
    if(!app.clip_paste) { status("Selection copied.");desk_log("deskterm: clipboard copied\n");return; }
    if(!desk_text_utf8_valid(app.clipboard_store,app.clipboard.length)) { status("The clipboard does not contain valid text.");return; }
    for(unsigned i=0;i<app.clipboard.length;i++) {
        unsigned c=(unsigned char)app.clipboard_store[i];
        if((c<32 && c!='\n' && c!='\r' && c!='\t') || c==127) { status("Paste was not sent: the text contains control characters.");return; }
    }
    unsigned size=app.clipboard.length,bracket=app.screen.bracketed_paste;
    if(size+12*bracket>INPUT_BYTES-app.input_size || app.exited || app.master<0) {
        status("The input queue is full or the session has ended. Paste was not sent.");return;
    }
    if(bracket) enqueue("\033[200~",6);
    enqueue(app.clipboard_store,size);
    if(bracket) enqueue("\033[201~",6);
    desk_terminal_view_follow(&app.view);status("Clipboard text pasted.");desk_log("deskterm: clipboard pasted bytes=%u\n",size);
}
static void toolbar_action(struct desk_action a) {
    if(a.flags&DESK_WIDGET_REDRAW) app.window.dirty=1;
    if(!(a.flags&DESK_WIDGET_ACTIVATED)) return;
    if(a.id==1) { if(desk_app_launch(&app.window,DESK_APP_TERMINAL,"")) status("A new terminal could not be opened."); }
    else if(a.id==2) copy_selection();else if(a.id==3) paste();
    else if(a.id==4) { if(enqueue("\014",1)) status("This session cannot accept input."); }
    desk_ui_focus(&app.toolbar,0);desk_terminal_view_event(&app.view,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
}
static void keyboard(const struct srv_key_event *k) {
    if(app.modal) {
        if((k->flags&SRV_KEY_DOWN_EVENT) && k->usage==SRV_KEY_ESCAPE) dialog_action((struct desk_action){2,DESK_WIDGET_ACTIVATED});
        else dialog_action(desk_ui_key(&app.dialog,k));
        return;
    }
    if(!(k->flags&SRV_KEY_DOWN_EVENT)) return;
    int command=((k->modifiers&SRV_KEY_CTRL) && (k->modifiers&SRV_KEY_SHIFT)) || (k->modifiers&SRV_KEY_META);
    if(command) {
        if(k->usage==6) { copy_selection();return; }
        if(k->usage==25) { paste();return; }
        if(k->usage==4) { desk_terminal_view_select_all(&app.view);app.window.dirty=1;return; }
        if(k->usage==26) { close_request();return; }
        if(k->usage==17) { toolbar_action((struct desk_action){1,DESK_WIDGET_ACTIVATED});return; }
    }
    if((k->modifiers&SRV_KEY_SHIFT) && (k->usage==SRV_KEY_PAGEUP || k->usage==SRV_KEY_PAGEDOWN)) {
        desk_terminal_view_scroll(&app.view,(k->usage==SRV_KEY_PAGEUP?-1:1)*(int)app.view.rows*app.view.cell_height);app.window.dirty=1;return;
    }
    char bytes[32];size_t length=desk_terminal_key(&app.screen,k,bytes,sizeof(bytes));
    if(length) {
        if(enqueue(bytes,length)) status("The input queue is full or this session has ended.");
        else { desk_terminal_view_follow(&app.view);app.window.dirty=1; }
    }
}
enum { TERM_CLOSE=10,TERM_SELECT_ALL,TERM_PAGE_UP,TERM_PAGE_DOWN,TERM_FOLLOW };
static void menus(void) {
    unsigned disabled=app.modal?DESK_MENU_DISABLED:0;
    unsigned stopped=disabled|(app.exited || !app.child?DESK_MENU_DISABLED:0);
    struct desk_menu_item items[]={
        {0,DESK_MENU_HEADING,"File",""},{1,disabled,"New terminal","Ctrl-Shift-N"},
        {TERM_CLOSE,disabled,"Close window","Ctrl-Shift-W"},
        {0,DESK_MENU_HEADING,"Edit",""},
        {2,disabled|(app.clipboard.mode || app.view.anchor==app.view.caret?DESK_MENU_DISABLED:0),"Copy","Ctrl-Shift-C"},
        {3,stopped|(app.clipboard.mode?DESK_MENU_DISABLED:0),"Paste","Ctrl-Shift-V"},
        {TERM_SELECT_ALL,disabled,"Select all","Ctrl-Shift-A"},
        {0,DESK_MENU_HEADING,"View",""},{4,stopped,"Clear screen","Ctrl-L"},
        {TERM_PAGE_UP,disabled,"Page up","Shift-PageUp"},{TERM_PAGE_DOWN,disabled,"Page down","Shift-PageDown"},
        {TERM_FOLLOW,disabled,"Follow output",""}
    };
    desk_app_menus(&app.window,items,sizeof(items)/sizeof(items[0]));desk_app_menu_tick(&app.window);
}
static void menu_action(unsigned id) {
    if(app.modal) return;
    if(id<=4) toolbar_action((struct desk_action){id,DESK_WIDGET_ACTIVATED});
    else if(id==TERM_CLOSE) close_request();
    else if(id==TERM_SELECT_ALL) { desk_terminal_view_select_all(&app.view);app.window.dirty=1; }
    else if(id==TERM_PAGE_UP || id==TERM_PAGE_DOWN) {
        desk_terminal_view_scroll(&app.view,(id==TERM_PAGE_UP?-1:1)*(int)app.view.rows*app.view.cell_height);app.window.dirty=1;
    } else if(id==TERM_FOLLOW) { desk_terminal_view_follow(&app.view);app.window.dirty=1; }
}
static void pump(void) {
    if(app.master<0) return;
    for(unsigned batch=0;batch<4 && app.input_size;batch++) {
        unsigned size=INPUT_BYTES-app.input_head;if(size>app.input_size) size=app.input_size;if(size>4096) size=4096;
        long n=srv_write(app.master,app.input+app.input_head,size);
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0) { app.input_size=0;break; }
        app.input_head=(app.input_head+n)%INPUT_BYTES;app.input_size-=n;
    }
    for(unsigned batch=0;batch<4 && !app.pty_eof;batch++) {
        char output[4096];long n=srv_read(app.master,output,sizeof(output));
        if(n==SRV_ERR_AGAIN || n==SRV_ERR_INTR) break;
        if(n<=0) { app.pty_eof=1;break; }
        desk_terminal_feed(&app.screen,output,n);
    }
    if(INPUT_BYTES-app.input_size>=DESK_TERM_REPLY && app.screen.reply_size) {
        char replies[DESK_TERM_REPLY];size_t size=desk_terminal_reply(&app.screen,replies,sizeof(replies));enqueue(replies,size);
    }
    if(desk_terminal_view_sync(&app.view)) app.window.dirty=1;
    if(app.child && !app.exited) {
        long result=srv_wait(app.child,&app.exit_status,SRV_WAIT_NOHANG);
        if(result==(long)app.child) {
            app.exited=1;app.input_size=0;char text[160];snprintf(text,sizeof(text),"Session ended · status %llu · New opens another terminal",(unsigned long long)app.exit_status);status(text);
            desk_app_title(&app.window,"Terminal — Session ended");desk_log("deskterm: shell exited status=%llu\n",(unsigned long long)app.exit_status);
        }
    }
}
static void draw(struct desk_canvas *c) {
    const struct desk_theme *theme=&desk_theme_quiet;
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},theme->surface);
    desk_ui_draw(c,&app.toolbar,theme);desk_terminal_view_draw(c,&app.view,theme,!app.exited && !(app.blink&1));
    desk_label_draw(c,(struct desk_rect){12,c->height-28,c->width-24,24},theme->body,app.status,theme->muted);
    if(app.modal) {
        int w=c->width-32;if(w>480) w=480;int x=(c->width-w)/2,y=(c->height-160)/2;
        desk_fill(c,(struct desk_rect){x+4,y+5,w,160},0xb9c2b4);desk_fill(c,(struct desk_rect){x,y,w,160},theme->border);
        desk_fill(c,(struct desk_rect){x+1,y+1,w-2,158},theme->surface);
        desk_label_draw(c,(struct desk_rect){x+16,y+16,w-32,28},theme->heading,"Close this terminal?",theme->text);
        desk_label_draw(c,(struct desk_rect){x+16,y+55,w-32,25},theme->body,"The shell and its commands will receive hangup.",theme->text);
        desk_label_draw(c,(struct desk_rect){x+16,y+81,w-32,25},theme->body,"Cancel returns to the session.",theme->muted);
        desk_ui_draw(c,&app.dialog,theme);
    }
}
int main(void) {
    app.master=-1;
    if(desk_terminal_init(&app.screen,app.lines,576,app.alternate,80,1)) return 1;
    desk_terminal_view_init(&app.view,&app.screen);desk_ui_init(&app.toolbar);desk_ui_init(&app.dialog);
    static const char *labels[]={"New","Copy","Paste","Clear"};
    for(int i=0;i<4;i++) desk_ui_add(&app.toolbar,i+1,DESK_CONTROL_BUTTON,&app.buttons[i],labels[i]);
    desk_ui_add(&app.dialog,1,DESK_CONTROL_BUTTON,&app.close_buttons[0],"Close");
    desk_ui_add(&app.dialog,2,DESK_CONTROL_BUTTON,&app.close_buttons[1],"Cancel");
    desk_clipboard_init(&app.clipboard,app.clipboard_store);
    desk_log("deskterm: starting\n");if(desk_app_open(&app.window,"Terminal",820,500)) return 1;
    while(!app.quit && !app.window.failed && !app.window.closed) {
        if(desk_clipboard_tick(&app.clipboard,srv_ticks())<0) clipboard_result(-1);
        for(unsigned batch=0;batch<32;batch++) {
            struct desk_packet p;int result=desk_app_poll(&app.window,&p);if(result<=0) break;if(result==2) continue;
            if(p.type==DESK_CONFIGURE) { layout();if(!app.started) start(); }
            else if(p.type==DESK_FOCUS) {
                desk_terminal_view_event(&app.view,(struct desk_widget_event){.type=p.value && !app.modal?DESK_WIDGET_FOCUS_IN:DESK_WIDGET_FOCUS_OUT});
                desk_ui_window_focus(app.modal?&app.dialog:&app.toolbar,p.value!=0);app.window.dirty=1;
            } else if(p.type==DESK_MENU_ACTION) menu_action(p.value);
            else if(p.type==DESK_KEY) { struct srv_key_event k;memcpy(&k,p.text,sizeof(k));keyboard(&k); }
            else if(p.type==DESK_POINTER) {
                struct desk_widget_event event={.type=p.value&DESK_POINTER_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=p.x,.y=p.y,.down=(p.value&DESK_POINTER_LEFT)!=0,.pressed=(p.value&DESK_POINTER_PRESS)!=0};
                if(app.modal) dialog_action(desk_ui_pointer(&app.dialog,event,srv_monotonic_us()));
                else {
                    toolbar_action(desk_ui_pointer(&app.toolbar,event,srv_monotonic_us()));
                    if(desk_terminal_view_event(&app.view,event)) app.window.dirty=1;
                }
            } else if(p.type==DESK_SCROLL && !app.modal) {
                if(desk_terminal_view_scroll(&app.view,p.y)) app.window.dirty=1;
            } else if(p.type==DESK_CLIPBOARD_ACK || p.type==DESK_CLIPBOARD_DATA) {
                int done=desk_clipboard_receive(&app.clipboard,&p);if(done) clipboard_result(done);
            } else if(p.type==DESK_CLOSE_REQUEST) close_request();
        }
        if(app.quit || app.window.closed || app.window.failed) break;
        pump();uint64_t blink=(uint64_t)srv_ticks()/50;
        if(blink!=app.blink) { app.blink=blink;if(app.view.focused && !app.modal) app.window.dirty=1; }
        menus();
        struct desk_canvas canvas;if(desk_app_canvas(&app.window,&canvas)==1) { draw(&canvas);desk_app_present(&app.window); }
        srv_sleep_ticks(1);
    }
    if(app.master>=0) srv_close(app.master);
    if(app.child && !app.exited) for(unsigned i=0;i<100;i++) {
        long result=srv_wait(app.child,&app.exit_status,SRV_WAIT_NOHANG);if(result!=0) break;srv_sleep_ticks(1);
    }
    desk_app_close(&app.window);desk_log("deskterm: exited failure=%d\n",app.window.failed);return app.window.failed?1:0;
}
