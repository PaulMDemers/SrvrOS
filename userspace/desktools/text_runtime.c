#include "runtime.h"
#include <srvros/desk_connection.h>
#include <srvros/desk_log.h>
#include <stdlib.h>
#include <string.h>

/* Window and document lifetimes differ: a worker may still be retiring after
 * its GUI record is acknowledged. Keep its owner until accepted work and the
 * child wait result have both drained. */
struct text_document {
    struct tool tool;
    struct text_document *next;
    uint64_t requester,logged_job,retiring_worker;
    char initial_path[DESK_FILE_PATH];
    int ready,retired,worker_closed,bad_path;
};
void tool_new_window(struct tool *t) {
    if(t->role==DESK_APP_TEXT && t->window.connection) {
        if(!t->quit && !t->window.closing)t->new_window=1;
    } else tool_launch(t,t->role,t->role==DESK_APP_FILES?t->saved_path:"");
}
static struct text_document *find(struct text_document *list,struct desk_app *window) {
    for(;list;list=list->next)if(&list->tool.window==window)return list;
    return NULL;
}
static void notice(struct text_document *list,uint64_t requester,const char *message) {
    struct text_document *fallback=NULL;
    for(;list;list=list->next)if(list->ready && !list->retired && !list->tool.quit) {
        if(list->tool.window.window==requester) { tool_status(&list->tool,message);return; }
        if(!fallback)fallback=list;
    }
    if(fallback)tool_status(&fallback->tool,message);
    desk_log("text-runtime: %s\n",message);
}
static int create(struct desk_connection *connection,struct text_document **list,uint64_t requester,const char *path) {
    struct text_document *d=calloc(1,sizeof(*d));
    if(!d) { notice(*list,requester,"There is not enough memory for another document.");return -1; }
    tool_runtime_init(&d->tool,DESK_APP_TEXT);d->requester=requester;
    if(strlen(path)>=sizeof(d->initial_path))d->bad_path=1;else strcpy(d->initial_path,path);
    if(desk_window_open(connection,&d->tool.window,"Text — Untitled",720,480)) {
        free(d);notice(*list,requester,"Another window is still opening. Try again shortly.");return -1;
    }
    d->tool.clip.window=d->tool.window.window;d->next=*list;*list=d;
    return 0;
}
/* No blocking wait and no kill: idle worker receives EOF, then is reaped even
 * when another document keeps this application process alive. */
static int retire_worker(struct text_document *d) {
    if(d->tool.job.busy)return 0;
    if(!d->worker_closed) {
        d->worker_closed=1;
        if(d->tool.worker_ready) {
            d->retiring_worker=d->tool.job.pid;
            desk_file_job_close(&d->tool.job);d->tool.worker_ready=0;
        }
    }
    if(d->retiring_worker) {
        uint64_t status=0;long pid=srv_wait(d->retiring_worker,&status,SRV_WAIT_NOHANG);
        if(pid!=(long)d->retiring_worker && (pid>=0 || desk_process_alive(d->retiring_worker)))return 0;
        desk_log("text-runtime: worker-retired pid=%llu window=%llu\n",(unsigned long long)d->retiring_worker,(unsigned long long)d->tool.window.window);
        d->retiring_worker=0;
    }
    return 1;
}
int tool_text_runtime(const char *path) {
    struct desk_connection connection;desk_connection_init(&connection);
    struct text_document *documents=NULL;int failed=0;
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
            if(p.type==DESK_OPEN_REPLY) {
                if(p.value) {
                    notice(documents,d->requester,p.value==DESK_OPEN_CAPACITY?
                        "The desktop is full. Close a window before opening another.":"The new document window could not be opened. Your other documents were kept.");
                    d->retired=1;if(desk_window_dispose(window))failed=1;
                } else {
                    d->ready=1;t->worker_ready=desk_file_job_init(&t->job)==0;
                    tool_text_init(t,d->initial_path);
                    if(d->bad_path)tool_status(t,"This path is too long. Choose a file to open.");
                    else if(!t->worker_ready)tool_status(t,"The file worker is unavailable. Your document was kept.");
                    desk_log("text-runtime: admitted window=%llu worker=%llu\n",(unsigned long long)window->window,(unsigned long long)t->job.pid);
                }
            } else if(p.type==DESK_CLOSE_ACK) {
                d->retired=1;t->pending=t->closing=0;
                desk_log("text-runtime: closed window=%llu bytes=%u\n",(unsigned long long)window->window,(unsigned)t->body_text.length);
                if(desk_window_dispose(window))failed=1;
            } else if(p.type==DESK_WINDOW_FAILED) {
                notice(documents,d->requester,"A document window could not be displayed. Other windows remain open.");
            } else if(d->ready && !d->retired && !t->quit)tool_runtime_event(t,p);
        }
        int busy=failed || connection.opening!=NULL,drained=1;
        struct text_document **link=&documents;
        while(*link) {
            struct text_document *d=*link;struct tool *t=&d->tool;
            if(failed) { d->retired=1;t->pending=t->closing=0; }
            if(t->job.busy) {
                int result=tool_runtime_file_poll(t,&d->logged_job);
                if(result && !d->retired) {
                    if(t->picker_job)tool_picker_finish(t,result);else tool_text_finish(t,result);
                }
            }
            if(d->retired) {
                if(!retire_worker(d)) { busy=1;drained=0;link=&d->next;continue; }
                if(t->window.connection && !failed) { failed=1;link=&d->next;continue; }
                if(failed) { link=&d->next;continue; } /* Process exit owns uncertain GUI resources. */
                *link=d->next;free(t->body_history);free(d);continue;
            }
            drained=0;
            if(d->ready && !failed) {
                if(t->quit && !t->job.busy)desk_window_close(&t->window);
                if(!t->quit && !t->window.closing) {
                    if(desk_clipboard_tick(&t->clip,srv_ticks())<0)tool_clipboard_result(t,-1);
                    tool_runtime_draw(t);
                }
                busy|=t->job.busy || t->clip.mode || t->title_dirty || t->quit || t->window.closing || t->new_window;
            }
            link=&d->next;
        }
        if(failed && drained)break;
        if(!failed && !connection.opening) {
            for(struct text_document *d=documents;d;d=d->next)if(d->tool.new_window && !d->retired && !d->tool.quit) {
                d->tool.new_window=0;create(&connection,&documents,d->tool.window.window,"");busy=1;break;
            }
        }
        if(documents && (busy || desk_connection_wait(&connection,10)<0))srv_sleep_ticks(1);
    }
    desk_log("desktools: exited role=text failure=%d\n",failed);return failed?1:0;
}
