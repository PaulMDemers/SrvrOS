#include "runtime.h"
#include <srvros/desk_application.h>
#include <srvros/desk_connection.h>
#include <srvros/desk_log.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

/* Window and document lifetimes differ: a worker may still be retiring after
 * its GUI record is acknowledged. Keep its owner until accepted work and the
 * child wait result have both drained. */
struct text_document {
    struct tool tool;
    struct desk_dialog_window dialog;
    struct text_document *next;
    uint64_t requester,logged_job;
    char initial_path[DESK_FILE_PATH];
    int ready,retired,worker_closed,bad_path;
};
void tool_new_window(struct tool *t) {
    if(t->role==DESK_APP_TEXT && t->window.connection) {
        if(!t->quit && !t->window.closing)t->new_window=1;
    } else tool_launch(t,t->role,t->role==DESK_APP_FILES?t->saved_path:"");
}
static struct text_document *find(struct text_document *list,struct desk_app *window) {
    for(;list;list=list->next)if(&list->tool.window==window || &list->dialog.window==window)return list;
    return NULL;
}
static void notice(struct text_document *list,uint64_t requester,const char *message) {
    desk_log("text-runtime: %s\n",message);
    struct text_document *fallback=NULL;
    for(;list;list=list->next)if(list->ready && !list->retired && !list->tool.quit) {
        if(list->tool.window.window==requester) { tool_status(&list->tool,message);return; }
        if(!fallback)fallback=list;
    }
    if(fallback)tool_status(&fallback->tool,message);
}
static int create(struct desk_connection *connection,struct text_document **list,uint64_t requester,const char *path) {
    /* Documents contain sizeable edit/job buffers. Keep them out of the small
     * legacy brk arena, as we already do for independently retired canvases. */
    struct text_document *d=mmap(NULL,sizeof(*d),PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
    if(d==MAP_FAILED) { notice(*list,requester,"There is not enough memory for another document.");return -1; }
    tool_runtime_init(&d->tool,DESK_APP_TEXT);d->requester=requester;
    if(strlen(path)>=sizeof(d->initial_path))d->bad_path=1;else strcpy(d->initial_path,path);
    if(desk_window_open(connection,&d->tool.window,"Text — Untitled",720,480)) {
        munmap(d,sizeof(*d));notice(*list,requester,"Another window is still opening. Try again shortly.");return -1;
    }
    d->tool.clip.window=d->tool.window.window;d->next=*list;*list=d;
    return 0;
}
/* A document releases its binding only after its accepted operation drains.
 * Idle transports belong to the application, so a sibling can reuse them. */
static int retire_job(struct text_document *d) {
    if(d->tool.job.busy)return 0;
    if(!d->worker_closed) {
        if(d->tool.worker_ready && desk_file_job_detach(&d->tool.job))return 0;
        d->worker_closed=1;d->tool.worker_ready=0;
        desk_log("text-runtime: job-detached window=%llu\n",(unsigned long long)d->tool.window.window);
    }
    return 1;
}
static void cancel_dialog(struct text_document *d) {
    if(d->tool.picker_mode)tool_picker_cancel(&d->tool);
    else if(d->tool.modal)tool_text_dialog(&d->tool,1003);
}
static void dialog_event(struct text_document *d,struct desk_packet p) {
    struct tool *t=&d->tool;struct desk_action action={0};
    if(t->picker_mode)desk_picker_focus(&t->picker,d->dialog.window.focused);
    else if(t->modal)desk_dialog_focus(&t->dialog,d->dialog.window.focused);
    if(p.type==DESK_CLOSE_REQUEST || p.type==DESK_WINDOW_FAILED) {cancel_dialog(d);return;}
    if(p.type==DESK_FOCUS) {
        if(t->picker_mode)desk_picker_focus(&t->picker,p.value!=0);
        else desk_dialog_focus(&t->dialog,p.value!=0);
    } else if(p.type==DESK_KEY) {
        struct srv_key_event key;memcpy(&key,p.text,sizeof(key));
        if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&(SRV_KEY_CTRL|SRV_KEY_META)) && key.usage==0x14) {cancel_dialog(d);t->quit_application=1;return;}
        if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_CTRL) && key.usage==0x1a) {cancel_dialog(d);return;}
        if(t->picker_mode)tool_picker_result(t,desk_picker_key(&t->picker,&key));
        else action=desk_dialog_key(&t->dialog,&key);
    } else if(p.type==DESK_POINTER) {
        struct desk_widget_event e={.type=p.value&DESK_POINTER_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
            .x=p.x,.y=p.y,.down=(p.value&DESK_POINTER_LEFT)!=0,.pressed=(p.value&DESK_POINTER_PRESS)!=0};
        if(t->picker_mode)tool_picker_result(t,desk_picker_pointer(&t->picker,e,srv_monotonic_us()));
        else action=desk_dialog_pointer(&t->dialog,e,srv_monotonic_us());
    } else if(p.type==DESK_SCROLL && t->picker_mode)tool_picker_result(t,desk_picker_scroll(&t->picker,p.width,p.height,p.y));
    else if(p.type==DESK_CLIPBOARD_ACK || p.type==DESK_CLIPBOARD_DATA) {
        int done=desk_clipboard_receive(&t->clip,&p);if(done)tool_clipboard_result(t,done);
    }
    if(action.flags&DESK_WIDGET_ACTIVATED)tool_text_dialog(t,action.id);
    d->dialog.window.dirty=1;
}
static void dialog_tick(struct text_document *d) {
    struct tool *t=&d->tool;
    int wanted=!d->retired && (t->modal || t->picker_mode);
    int ready=desk_dialog_window_sync(&d->dialog,&t->window,wanted,
        t->picker_mode?(t->picker_mode==DESK_PICKER_SAVE?"Save document":"Open document"):t->modal==2?"Replace file":"Unsaved document",
        t->picker_mode?720:560,t->picker_mode?480:280);
    int previous_separate=t->dialog_separate;
    t->dialog_separate=wanted && ready>=0;
    if(previous_separate!=t->dialog_separate)t->window.dirty=1;
    if(wanted && ready<0) {
        struct desk_rect bounds={0,0,t->window.width,t->window.height};
        if(t->picker_mode) {desk_picker_configure(&t->picker,bounds);if(desk_picker_focus(&t->picker,t->window.focused))t->window.dirty=1;}
        else {desk_dialog_configure(&t->dialog,bounds);if(desk_dialog_focus(&t->dialog,t->window.focused))t->window.dirty=1;}
    }
    if(!t->clip.mode)t->clip.window=t->dialog_separate && d->dialog.window.connection?d->dialog.window.window:t->window.window;
    if(!wanted || ready!=1)return;
    struct desk_app *window=&d->dialog.window;
    struct desk_rect bounds={0,0,window->width,window->height};
    if(t->picker_mode) {desk_picker_configure(&t->picker,bounds);desk_picker_focus(&t->picker,window->focused);}
    else {desk_dialog_configure(&t->dialog,bounds);desk_dialog_focus(&t->dialog,window->focused);}
    if(t->window.dirty)window->dirty=1;
    struct desk_canvas canvas;
    if(desk_app_canvas(window,&canvas)==1) {
        desk_fill(&canvas,canvas.clip,desk_theme_quiet.surface);
        if(t->picker_mode)desk_picker_draw(&canvas,&t->picker,&desk_theme_quiet);
        else desk_dialog_draw(&canvas,&t->dialog,&desk_theme_quiet);
        desk_app_present(window);
    }
}
int tool_file_capture_capacity_setup(void);
int tool_text_runtime(const char *path) {
    int capacity=strcmp(path,"--capacity-files")==0;
    if(capacity) { if(tool_file_capture_capacity_setup())return 1;path=""; }
    unsigned capture_checkpoints=0;
    struct desk_application_lifecycle application={0};
    struct desk_open_document open_request={0};uint64_t service_window=0;
    struct desk_connection connection;desk_connection_init(&connection);
    struct desk_file_pool pool;struct desk_file_channel channels[2];
    if(desk_file_pool_init(&pool,channels,2))return 1;
    struct text_document *documents=NULL;int failed=0;
    unsigned admitted_count=0,retired_count=0,refused_count=0;
    srv_puts("desktools: starting role=text\n");
    if(create(&connection,&documents,0,path))return 1;
    while(documents) {
        for(unsigned batch=0;!failed && batch<32;batch++) {
            struct desk_app *window;struct desk_packet p;
            int r=desk_connection_poll(&connection,&window,&p);
            if(r<0) { failed=1;break; }
            if(!r)break;if(r==2)continue;
            struct text_document *d=find(documents,window);
            if(!d) { failed=1;break; }
            struct tool *t=&d->tool;
            if(window==&d->dialog.window) {dialog_event(d,p);continue;}
            if(p.type==DESK_OPEN_DOCUMENT) {desk_open_document_receive(&open_request,&p);continue;}
            if(p.type==DESK_OPEN_REPLY) {
                if(p.value) {
                    refused_count++;
                    notice(documents,d->requester,p.value==DESK_OPEN_CAPACITY?
                        "The desktop is full. Close a window before opening another.":"The new document window could not be opened. Your other documents were kept.");
                    d->retired=1;if(desk_window_dispose(window))failed=1;
                } else {
                    d->ready=1;admitted_count++;t->worker_ready=desk_file_job_bind(&t->job,&pool)==0;
                    tool_text_init(t,d->initial_path);
                    if(d->bad_path)tool_status(t,"This path is too long. Choose a file to open.");
                    else if(!t->worker_ready)tool_status(t,"The file worker is unavailable. Your document was kept.");
                    desk_log("text-runtime: admitted window=%llu worker=%llu\n",(unsigned long long)window->window,(unsigned long long)t->job.pid);
                }
            } else if(p.type==DESK_CLOSE_ACK) {
                d->retired=1;t->pending=t->closing=0;
                desk_application_closed(&application,window->window);
                if(service_window==window->window)service_window=0;
                desk_log("text-runtime: closed window=%llu bytes=%u\n",(unsigned long long)window->window,(unsigned)t->body_text.length);
            } else if(p.type==DESK_WINDOW_FAILED) {
                notice(documents,d->requester,"A document window could not be displayed. Other windows remain open.");
            } else if(d->ready && !d->retired && !t->quit) {
                tool_runtime_event(t,p);
                if(capacity && p.type==DESK_KEY && p.input_serial && p.input_serial<=60 && !(p.input_serial%12)) {
                    tool_file_capture_begin(t,p.input_serial);
                }
            }
        }
        desk_open_document_tick(&open_request,connection.server);
        if(open_request.ready && !connection.opening) {
            int accepted=!application.quitting && !create(&connection,&documents,0,open_request.path);
            desk_log("text-runtime: open-document accepted=%u\n",accepted);
            desk_open_document_finish(&open_request,accepted);
        }
        int busy=failed || connection.opening!=NULL || open_request.reply_pending || open_request.ready,drained=1;
        struct text_document **link=&documents;
        while(*link) {
            struct text_document *d=*link;struct tool *t=&d->tool;
            if(failed) { d->retired=1;t->pending=t->closing=0; }
            if(t->job.busy) {
                int result=tool_runtime_file_poll(t,&d->logged_job);
                if(result) {
                    if(d->retired) {
                        desk_chooser_cancel(&t->chooser);
                        if(t->chooser.pending)desk_chooser_finish(&t->chooser,t->chooser.pending_id,result);
                    } else if(capacity && tool_file_capture_busy()) {
                        tool_file_capture_finish(t,result);
                        if(!tool_file_capture_busy() && !t->window.failed)capture_checkpoints++;
                    } else if(t->chooser.pending)tool_picker_finish(t,result);else tool_text_finish(t,result);
                }
            }
            if(capacity && t->window.failed)failed=1;
            if(t->close_canceled) {desk_application_cancel_quit(&application,t->window.window);t->close_canceled=0;}
            if(t->quit_application) {t->quit_application=0;desk_application_quit(&application);}
            dialog_tick(d);
            if(d->retired) {
                if(d->dialog.window.connection) {busy=1;link=&d->next;continue;}
                if(!failed && t->window.connection && desk_window_dispose(&t->window)) {failed=1;link=&d->next;continue;}
                int already_retired=d->worker_closed;
                if(!retire_job(d)) { busy=1;drained=0;link=&d->next;continue; }
                if(d->ready && !already_retired)retired_count++;
                if(t->window.connection && !failed) { failed=1;link=&d->next;continue; }
                if(failed) { link=&d->next;continue; } /* Process exit owns uncertain GUI resources. */
                *link=d->next;free(t->body_history);munmap(d,sizeof(*d));continue;
            }
            drained=0;
            if(d->ready && !failed) {
                if(!capacity && !application.quitting && !service_window && !t->window.closing && !desk_application_service(&t->window,DESK_SERVICE_OPEN_DOCUMENT))service_window=t->window.window;
                if(application.quitting && service_window==t->window.window && !desk_application_service(&t->window,0))service_window=0;
                if(t->quit && !t->job.busy)desk_window_close(&t->window);
                if(!t->quit && !t->window.closing) {
                    if(desk_clipboard_tick(&t->clip,srv_ticks())<0)tool_clipboard_result(t,-1);
                    tool_runtime_draw(t);
                }
                busy|=t->clip.mode || t->title_dirty || t->quit || t->window.closing || t->new_window;
            }
            link=&d->next;
        }
        if(failed && drained)break;
        if(!failed && application.quitting && !application.closing_window) {
            for(struct text_document *d=documents;d;d=d->next)if(d->ready && !d->retired && !d->tool.quit && !d->tool.window.closing) {
                if(desk_application_close_next(&application,d->tool.window.window)) {
                    d->tool.close_canceled=0;tool_text_request(&d->tool,TOOL_PENDING_CLOSE);busy=1;
                }
                break;
            }
        }
        if(!failed && !application.quitting && !connection.opening) {
            for(struct text_document *d=documents;d;d=d->next)if(d->tool.new_window && !d->retired && !d->tool.quit) {
                d->tool.new_window=0;create(&connection,&documents,d->tool.window.window,"");busy=1;break;
            }
        }
        if(documents) {
            struct srv_gui_wait wait={.deadline_us=(uint64_t)srv_monotonic_us()+100000};
            if(busy || desk_file_pool_interests(&pool,&wait)!=1 || desk_connection_wait_io(&connection,&wait)<0)
                srv_sleep_ticks(1);
        }
    }
    desk_log("text-runtime: documents admitted=%u retired=%u refused=%u\n",admitted_count,retired_count,refused_count);
    if(tool_file_pool_retire(&pool,DESK_APP_TEXT))failed=1;
    if(capacity) {
        if(admitted_count!=1 || retired_count!=1 || refused_count || capture_checkpoints!=5 ||
           !tool_file_capture_report())failed=1;
        desk_log("document-capacity: result=%s checkpoints=%u pooled=yes retired=%u\n",failed?"FAIL":"PASS",capture_checkpoints,retired_count);
    }
    desk_log("desktools: exited role=text failure=%d\n",failed);return failed?1:0;
}
