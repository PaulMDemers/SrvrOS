#include <srvros/desk_app.h>
#include <srvros/desk_connection.h>
#include <srvros/desk_application.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <string.h>

int desk_app_open(struct desk_app *a,const char *title,uint32_t width,uint32_t height) {
    *a=(struct desk_app){.last=srv_ticks(),.dirty=1,.focused=1,.window=DESK_DEFAULT_WINDOW};
    struct desk_packet p=desk_packet_new(DESK_OPEN);p.window=a->window;p.width=width;p.height=height;
    size_t n=strlen(title);if(n>=sizeof(p.text)) n=sizeof(p.text)-1;
    memcpy(p.text,title,n);
    if(desk_send(0,&p)) return -1;
    return 0;
}
static int app_prepare(struct desk_app *a,struct desk_packet *event) {
    if(a->closed || a->failed) return -1;
    if(!a->pending && a->configure.generation>a->generation) {
        if(a->configure.generation==a->rejected_generation) {
            struct desk_packet refused=desk_packet_new(DESK_RESIZE_REFUSED);
            refused.window=a->window;refused.generation=a->rejected_generation;refused.width=a->width;refused.height=a->height;
            if(!desk_send(a->server,&refused)) { a->last=srv_ticks();a->configure.generation=a->generation; }
        } else {
            int same=a->surface && a->width==a->configure.width && a->height==a->configure.height;
            uint32_t *pixels=same?a->pixels:desk_pixels_alloc(a->configure.width,a->configure.height);
            uint64_t surface=a->surface;
            if(!pixels || (!same && gui_surface_create(a->configure.width,a->configure.height,DESK_SURFACE_STREAM,&surface))) {
                if(!same)desk_pixels_free(pixels,a->configure.width,a->configure.height);
                if(!a->surface) { a->failed=1;return -1; }
                a->rejected_generation=a->configure.generation;a->last=srv_ticks();
                return 2;
            }
            if(!same) {
                if(a->surface)gui_surface_destroy(a->surface);
                desk_pixels_free(a->pixels,a->width,a->height);
            }
            a->pixels=pixels;a->surface=surface;a->width=a->configure.width;a->height=a->configure.height;
            a->generation=a->configure.generation;a->retry_after=0;a->dirty=1;a->last=srv_ticks();
            *event=a->configure;return 1;
        }
    }
    return 0;
}
static int app_receive(struct desk_app *a,uint64_t source,const struct desk_packet *packet,struct desk_packet *event) {
    struct desk_packet p=*packet;
    if(p.window!=a->window) return 2;
    if(!a->server && p.type==DESK_CONFIGURE) a->server=source;
    if(!a->server || a->server!=source) return 2;
    if(p.type==DESK_CONFIGURE && p.generation>a->configure.generation && p.generation>a->rejected_generation &&
       p.width>=280 && p.height>=240 && p.width<=DESK_MAX_WIDTH && p.height<=DESK_MAX_HEIGHT) {
        a->configure=p;a->last=srv_ticks();return 2;
    }
    if(p.type==DESK_CONFIGURE)return 2;
    if(p.type==DESK_RELEASE && p.surface==a->surface && p.serial==a->pending && p.generation==a->pending_generation) {
        a->pending=0;a->last=srv_ticks();
        if(p.value==DESK_RELEASE_RETRY) { a->dirty=1;a->retry_after=a->last+5; }
        return 2;
    }
    if(p.type==DESK_RELEASE)return 2;
    if(p.type==DESK_CLOSE) { a->closed=1;*event=p;return 1; }
    if(p.type==DESK_MENU_ACTION) {
        int valid=0;
        if(p.serial==a->menu_revision) for(unsigned i=0;i<a->menu_count;i++)
            if(a->menus[i].id==p.value && p.value && !(a->menus[i].flags&(DESK_MENU_DISABLED|DESK_MENU_HEADING|DESK_MENU_SEPARATOR))) valid=1;
        if(!valid || !a->focused) return 2;
    }
    if(p.type==DESK_FOCUS) a->focused=p.value!=0;
    if((p.type==DESK_POINTER || p.type==DESK_CONTEXT) && (p.generation!=a->generation || a->configure.generation!=a->generation)) return 2;
    if(p.type!=DESK_OPEN_DOCUMENT && p.input_serial>a->input_serial) { a->input_serial=p.input_serial;a->dirty=1; }
    *event=p;return 1;
}
static int app_check(struct desk_app *a) {
    if((!a->server || a->pending || a->configure.generation>a->generation) && (uint64_t)srv_ticks()-a->last>500) {
        a->failed=1;return -1;
    }
    if(a->server && !desk_process_alive(a->server)) { a->closed=1;return -1; }
    return 0;
}
int desk_app_poll(struct desk_app *a,struct desk_packet *event) {
    if(a->connection) return -1;
    int prepared=app_prepare(a,event);if(prepared) return prepared;
    struct desk_packet p;uint64_t source;int r=desk_receive(&source,&p);
    if(r<0) { a->failed=1;return -1; }
    if(r==1) return app_receive(a,source,&p,event);
    if(r==2) return 2;
    return app_check(a);
}
static int app_quiescent(const struct desk_app *a) {
    return a && !a->connection && a->server && !a->closed && !a->failed && !a->dirty && !a->pending &&
       a->surface && a->generation==a->configure.generation && !a->send_since && !a->menu_pending;
}
static int wait_io(uint64_t server,uint64_t *epoch,struct srv_gui_wait *r) {
    if(!r)return -1;
    r->abi_version=SRV_GUI_WAIT_VERSION;r->struct_size=sizeof(*r);r->server=server;r->cancel_epoch=*epoch;
    int result=gui_wait_io(r);
    if(result<0)return result;
    *epoch=r->cancel_epoch;return (int)r->ready;
}
int desk_app_wait(struct desk_app *a,uint64_t ticks) {
    if(!app_quiescent(a) || ticks>100)return -1;
    return gui_wait(a->server,ticks);
}
int desk_app_wait_io(struct desk_app *a,struct srv_gui_wait *r) {
    if(!app_quiescent(a))return -1;
    return wait_io(a->server,&a->wait_epoch,r);
}

int desk_app_canvas(struct desk_app *a,struct desk_canvas *canvas) {
    if(a->failed || a->closed || a->closing || a->opening) return -1;
    if(!a->dirty || a->pending || !a->surface || a->generation!=a->configure.generation ||
       (a->retry_after && (uint64_t)srv_ticks()<a->retry_after)) return 0;
    return desk_canvas_init(canvas,a->pixels,a->width,a->height,a->width)?-1:1;
}
int desk_app_present(struct desk_app *a) {
    if(a->failed || a->closed || a->closing || a->opening || a->pending || !a->surface) return -1;
    struct desk_packet p=desk_packet_new(DESK_COMMIT);p.window=a->window;
    p.surface=a->surface;p.serial=a->serial+1;p.generation=a->generation;
    p.width=a->width;p.height=a->height;p.input_serial=a->input_serial;
    if(gui_surface_blit(a->surface,0,0,a->width,a->height,a->pixels,a->width)) { a->failed=1;return -1; }
    if(desk_send(a->server,&p)) {
        if(!a->send_since) a->send_since=srv_ticks();
        if((uint64_t)srv_ticks()-a->send_since>500) { a->failed=1;return -1; }
        return 0;
    }
    a->retry_after=0;a->send_since=0;a->pending=++a->serial;a->pending_generation=a->generation;a->dirty=0;a->last=srv_ticks();return 1;
}
int desk_app_title(struct desk_app *a,const char *title) {
    if(!a->server || a->opening || a->closing || a->closed) return -1;
    struct desk_packet p=desk_packet_new(DESK_TITLE);p.window=a->window;
    size_t n=strlen(title);if(n>=sizeof(p.text)) n=sizeof(p.text)-1;
    memcpy(p.text,title,n);return desk_send(a->server,&p);
}
int desk_app_cancel_close(struct desk_app *a) {
    if(a->opening || a->closing || a->closed)return -1;
    struct desk_packet p=desk_packet_new(DESK_CANCEL_CLOSE);p.window=a->window;return desk_send(a->server,&p);
}
int desk_app_launch(struct desk_app *a,unsigned application,const char *path) {
    size_t length=strlen(path)+1;
    if(!a->server || a->opening || a->closing || a->closed || application<1 || application>DESK_APP_COUNT || length>160) return -1;
    for(size_t offset=0;offset<length;) {
        struct desk_packet p=desk_packet_new(DESK_LAUNCH);p.window=a->window;
        size_t n=length-offset;if(n>sizeof(p.text)) n=sizeof(p.text);
        p.value=application;p.serial=offset;p.height=n;p.width=offset+n==length;
        memcpy(p.text,path+offset,n);
        if(desk_send(a->server,&p)) return -1;
        offset+=n;
    }
    return 0;
}
int desk_app_preferences(struct desk_app *a,const struct desk_preferences *preferences,unsigned mask) {
    if(!a->server || a->opening || a->closing || a->closed || !preferences || !mask || (mask&~DESK_PREF_ALL)) return -1;
    struct desk_packet p=desk_packet_new(DESK_PREFERENCES_SET);p.window=a->window;p.value=mask;
    memcpy(p.text,preferences,sizeof(*preferences));return desk_send(a->server,&p);
}
int desk_app_text_size(struct desk_app *a,unsigned size) {
    if(size!=16 && size!=20) return -1;
    struct desk_preferences p={.text_size=size};return desk_app_preferences(a,&p,DESK_PREF_TEXT);
}
void desk_app_close(struct desk_app *a) {
    if(a->connection) { desk_window_close(a);return; }
    if(a->server) { struct desk_packet p=desk_packet_new(DESK_CLOSE);p.window=a->window;desk_send(a->server,&p); }
    if(a->surface && !a->pending) gui_surface_destroy(a->surface);
    desk_pixels_free(a->pixels,a->width,a->height);
    a->pixels=0;a->closed=1;
}

int desk_app_menus(struct desk_app *a,const struct desk_menu_item *items,unsigned count) {
    if(count>DESK_APP_MENU_ITEMS || (count && !items)) return -1;
    if(a->menu_revision && a->menu_count==count && (!count || !memcmp(a->menus,items,count*sizeof(*items)))) return 0;
    if(count) memcpy(a->menus,items,count*sizeof(*items));
    a->menu_count=count;a->menu_next=0;a->menu_pending=1;
    if(!++a->menu_revision) ++a->menu_revision;
    return 0;
}
void desk_app_menu_tick(struct desk_app *a) {
    if(!a->server || a->closed || a->failed || a->opening || a->closing) return;
    for(unsigned budget=0;budget<4 && a->menu_pending;budget++) {
        struct desk_packet p=desk_packet_new(DESK_MENU_SET);p.window=a->window;
        p.serial=a->menu_revision;p.generation=a->menu_next;p.width=a->menu_count;
        p.value=!a->menu_count || a->menu_next+1==a->menu_count;
        if(a->menu_count) memcpy(p.text,&a->menus[a->menu_next],sizeof(a->menus[0]));
        if(desk_send(a->server,&p)) return;
        if(p.value) a->menu_pending=0;else a->menu_next++;
    }
}

void desk_connection_init(struct desk_connection *c) {
    *c=(struct desk_connection){.next_window=DESK_DEFAULT_WINDOW};
}
int desk_window_open(struct desk_connection *c,struct desk_app *a,const char *title,uint32_t width,uint32_t height) {
    if(!c || !a || !title || c->failed || c->opening || !c->next_window ||
       width<280 || height<240 || width>DESK_MAX_WIDTH || height>DESK_MAX_HEIGHT) return -1;
    for(struct desk_app *w=c->windows;w;w=w->next) if(w==a) return -1;
    *a=(struct desk_app){.connection=c,.window=c->next_window++,.server=c->server,
        .last=srv_ticks(),.dirty=1,.opening=1,.next=c->windows};
    a->open_request=desk_packet_new(DESK_OPEN);a->open_request.window=a->window;
    a->open_request.value=DESK_OPEN_CONNECTION;a->open_request.width=width;a->open_request.height=height;
    size_t n=strlen(title);if(n>=sizeof(a->open_request.text))n=sizeof(a->open_request.text)-1;
    memcpy(a->open_request.text,title,n);
    c->windows=a;c->opening=a;
    return 0;
}
int desk_window_open_dialog(struct desk_connection *c,struct desk_app *a,struct desk_app *parent,
    const char *title,uint32_t width,uint32_t height) {
    if(!c || !parent || parent->connection!=c || parent->parent_window || parent->opening ||
       parent->closing || parent->closed || parent->failed)return -1;
    for(struct desk_app *w=c->windows;w;w=w->next)if(w->parent_window==parent->window)return -1;
    if(desk_window_open(c,a,title,width,height))return -1;
    a->parent_window=parent->window;a->open_request.generation=parent->window;
    a->open_request.value|=DESK_OPEN_MODAL;return 0;
}
static int live_dialog(struct desk_app *parent) {
    for(struct desk_app *w=parent->connection->windows;w;w=w->next)
        if(w->parent_window==parent->window && !w->closed)return 1;
    return 0;
}
void desk_window_close(struct desk_app *a) {
    if(!a || !a->connection || a->closed || a->closing) return;
    for(struct desk_app *w=a->connection->windows;w;w=w->next)
        if(w->parent_window==a->window && !w->closed)desk_window_close(w);
    a->closing=1;a->last=srv_ticks();
}
int desk_window_dispose(struct desk_app *a) {
    if(!a || !a->connection || !a->closed || a->pending || a->opening) return -1;
    struct desk_connection *c=a->connection;
    for(struct desk_app *w=c->windows;w;w=w->next)if(w->parent_window==a->window)return -1;
    struct desk_app **link=&c->windows;
    while(*link && *link!=a)link=&(*link)->next;
    if(!*link)return -1;
    if(a->surface && gui_surface_destroy(a->surface))return -1;
    desk_pixels_free(a->pixels,a->width,a->height);a->pixels=0;a->surface=0;
    *link=a->next;
    if(c->cursor==a)c->cursor=a->next?a->next:c->windows;
    a->connection=0;a->next=0;return 0;
}
static int connection_failure(struct desk_connection *c,int disconnected) {
    c->failed=1;c->disconnected=disconnected;
    if(disconnected) {
        c->opening=0;
        for(struct desk_app *a=c->windows;a;a=a->next) {
            a->pending=0;a->opening=0;a->closed=1;
        }
    }
    return -1;
}
int desk_connection_poll(struct desk_connection *c,struct desk_app **target,struct desk_packet *event) {
    if(!c || !target || !event)return -1;
    *target=0;
    if(c->failed)return -1;
    /* Rotate maintenance even when incoming traffic never empties. */
    struct desk_app *a=c->cursor?c->cursor:c->windows;
    if(a) {
        c->cursor=a->next?a->next:c->windows;
        if(!a->closed) {
            if(a->opening && !a->open_sent) {
                if(!desk_send(c->server,&a->open_request))a->open_sent=1;
            } else if(!a->opening && a->closing && !a->close_sent) {
                if(live_dialog(a))a->last=srv_ticks();
                else {
                    struct desk_packet p=desk_packet_new(DESK_CLOSE);p.window=a->window;
                    if(!desk_send(c->server,&p))a->close_sent=1;
                }
            } else if(!a->opening && !a->closing) {
                int r=app_prepare(a,event);
                if(r<0) {
                    desk_window_close(a);*target=a;
                    *event=desk_packet_new(DESK_WINDOW_FAILED);event->window=a->window;
                    return 1;
                }
                if(r) { *target=a;return r; }
            }
            if((a->opening || a->closing || a->pending || a->configure.generation>a->generation) &&
               (uint64_t)srv_ticks()-a->last>500) return connection_failure(c,0);
        }
    }
    struct desk_packet p;uint64_t source;int r=desk_receive(&source,&p);
    if(r<0)return connection_failure(c,0);
    if(!r && c->server && !desk_process_alive(c->server))return connection_failure(c,1);
    if(r!=1)return r;
    if(c->server && source!=c->server)return 2;
    for(a=c->windows;a && a->window!=p.window;a=a->next) {}
    if(!a || a->closed)return 2;
    if(p.type==DESK_OPEN_REPLY) {
        if(c->opening!=a || !a->open_sent || p.value>DESK_OPEN_MEMORY)return 2;
        if(!c->server) {
            c->server=source;
            for(struct desk_app *w=c->windows;w;w=w->next)w->server=source;
        }
        c->opening=0;a->opening=0;a->open_result=p.value;a->last=srv_ticks();
        if(p.value)a->closed=1;
        *target=a;*event=p;return 1;
    }
    if(!c->server || a->opening)return 2;
    if(p.type==DESK_CLOSE_ACK) {
        if(!a->closing || !a->close_sent || a->pending)return connection_failure(c,0);
        a->closed=1;*target=a;*event=p;return 1;
    }
    if(p.type==DESK_CLOSE) { desk_window_close(a);return 2; }
    if(a->closing && p.type!=DESK_RELEASE)return 2;
    r=app_receive(a,source,&p,event);*target=a;return r;
}
static int connection_quiescent(struct desk_connection *c) {
    if(!c || c->failed || !c->server || c->opening)return 0;
    for(struct desk_app *a=c->windows;a;a=a->next) {
        if(a->closed)continue;
        if(a->failed || a->closing || a->dirty || a->pending || !a->surface ||
           a->generation!=a->configure.generation || a->send_since || a->menu_pending)return 0;
    }
    return 1;
}
int desk_connection_wait(struct desk_connection *c,uint64_t ticks) {
    if(!connection_quiescent(c) || ticks>100)return -1;
    return gui_wait(c->server,ticks);
}
int desk_connection_wait_io(struct desk_connection *c,struct srv_gui_wait *r) {
    if(!connection_quiescent(c))return -1;
    return wait_io(c->server,&c->wait_epoch,r);
}

int desk_dialog_window_sync(struct desk_dialog_window *d,struct desk_app *parent,
    int wanted,const char *title,uint32_t width,uint32_t height) {
    if(!d || !parent || !parent->connection || !title || strlen(title)>=sizeof(d->title))return -1;
    if(d->window.connection) {
        if(!wanted || parent->closing || parent->closed)desk_window_close(&d->window);
        if(d->window.closed) {
            int refused=d->window.open_result!=DESK_OPEN_OK || d->window.failed;
            if(desk_window_dispose(&d->window))return 0;
            d->window=(struct desk_app){0};d->refused=refused && wanted;
            return d->refused?-1:0;
        }
        if(!d->window.opening && !d->window.closing && strcmp(d->title,title) && !desk_app_title(&d->window,title))strcpy(d->title,title);
        /* OPEN_REPLY reserves the window; CONFIGURE and its canvas can arrive
         * later. A zero-sized layout would discard the dialog's initial focus. */
        return !d->window.opening && !d->window.closing && d->window.generation!=0;
    }
    if(!wanted) { d->refused=0;return 0; }
    if(d->refused)return -1;
    if(parent->opening || parent->closing || parent->closed || parent->connection->opening)return 0;
    if(desk_window_open_dialog(parent->connection,&d->window,parent,title,width,height)) {
        d->refused=1;return -1;
    }
    strcpy(d->title,title);return 0;
}

void desk_application_quit(struct desk_application_lifecycle *a) { if(a)a->quitting=1; }
int desk_application_close_next(struct desk_application_lifecycle *a,uint64_t window) {
    if(!a || !a->quitting || a->closing_window || !window)return 0;
    a->closing_window=window;return 1;
}
void desk_application_closed(struct desk_application_lifecycle *a,uint64_t window) {
    if(a && a->closing_window==window)a->closing_window=0;
}
void desk_application_cancel_quit(struct desk_application_lifecycle *a,uint64_t window) {
    if(a && a->closing_window==window) {a->quitting=0;a->closing_window=0;}
}

int desk_application_service(struct desk_app *a,unsigned capabilities) {
    if(!a || !a->server || a->opening || a->closing || a->closed || a->parent_window ||
       (capabilities&~DESK_SERVICE_OPEN_DOCUMENT))return -1;
    struct desk_packet p=desk_packet_new(DESK_APP_SERVICE);p.window=a->window;p.value=capabilities;
    return desk_send(a->server,&p);
}
void desk_open_document_finish(struct desk_open_document *r,int accepted) {
    if(!r || !r->request)return;
    uint64_t window=r->reply.window;
    r->reply=desk_packet_new(DESK_OPEN_DOCUMENT_REPLY);r->reply.window=window;
    r->reply.serial=r->request;r->reply.value=accepted?0:1;r->reply_pending=1;
    r->last_request=r->request;r->request=0;r->ready=0;r->length=0;
}
int desk_open_document_receive(struct desk_open_document *r,const struct desk_packet *p) {
    if(!r || !p || p->type!=DESK_OPEN_DOCUMENT || !p->input_serial || r->reply_pending || r->ready)return 0;
    if(!p->serial) {
        if(p->input_serial<=r->last_request || r->request)return 0;
        r->request=p->input_serial;r->length=0;r->reply.window=p->window;
    }
    if(p->input_serial!=r->request)return 0;
    if(p->window!=r->reply.window || p->serial!=r->length || !p->height || p->height>sizeof(p->text) ||
       p->height>sizeof(r->path)-r->length || p->width>1) {
        desk_open_document_finish(r,0);return -1;
    }
    memcpy(r->path+r->length,p->text,p->height);r->length+=p->height;
    if(!p->width)return 0;
    if(r->path[0]!='/' || r->path[r->length-1] || strlen(r->path)!=r->length-1) {
        desk_open_document_finish(r,0);return -1;
    }
    r->ready=1;return 1;
}
void desk_open_document_tick(struct desk_open_document *r,uint64_t server) {
    if(r && r->reply_pending && server && !desk_send(server,&r->reply))r->reply_pending=0;
}
