#define _DEFAULT_SOURCE
#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
static int fail_map,fail_unmap;
static void *owned_map(void *p,size_t n,int prot,int flags,int fd,off_t offset) {
    if(fail_map) {fail_map=0;return MAP_FAILED;}
    return mmap(p,n,prot,flags,fd,offset);
}
static int owned_unmap(void *p,size_t n) {
    if(fail_unmap) {fail_unmap=0;return -1;}
    return munmap(p,n);
}
#define mmap owned_map
#define munmap owned_unmap
#include "../userspace/desktop_runtime/window_set.c"
#undef mmap
#undef munmap
#include <srvros/sys.h>
static struct desk_app *target;
static struct desk_packet packet;
static int queued,fault,callbacks;
long srv_monotonic_us(void) {return 1000;}
long srv_sleep_ticks(uint64_t n) {assert(n==1);return 0;}
void desk_connection_init(struct desk_connection *c) {*c=(struct desk_connection){.next_window=1};}
int desk_window_open(struct desk_connection *c,struct desk_app *w,const char *title,uint32_t width,uint32_t height) {
    assert(title && width==720 && height==480);
    if(c->opening)return -1;
    *w=(struct desk_app){.connection=c,.window=c->next_window++,.opening=1,.next=c->windows};
    c->windows=w;c->opening=w;return 0;
}
int desk_window_dispose(struct desk_app *w) {
    if(!w->closed)return -1;
    struct desk_app **link=&w->connection->windows;
    while(*link && *link!=w)link=&(*link)->next;
    assert(*link==w);*link=w->next;w->connection=NULL;return 0;
}
int desk_connection_poll(struct desk_connection *c,struct desk_app **w,struct desk_packet *p) {
    if(fault)return -1;
    if(!queued)return 0;
    queued=0;*w=target;*p=packet;
    if(packet.type==DESK_OPEN_REPLY) {target->opening=0;target->closed=packet.value!=0;c->opening=NULL;}
    if(packet.type==DESK_CLOSE_ACK)target->closed=1;
    return 1;
}
int desk_connection_wait_io(struct desk_connection *c,struct srv_gui_wait *wait) {(void)c;(void)wait;return 0;}
void desk_application_closed(struct desk_application_lifecycle *a,uint64_t w) {if(a->closing_window==w)a->closing_window=0;}
int desk_application_close_next(struct desk_application_lifecycle *a,uint64_t w) {
    if(!a->quitting || a->closing_window)return 0;
    a->closing_window=w;return 1;
}
struct model {struct desk_app window;uint64_t sentinel;};
static int event(void *context,struct desk_window_owner *owner,struct desk_app *w,struct desk_packet p) {
    (void)context;assert(w==owner->window || w==&owner->dialog.window);assert(p.type);
    assert(desk_window_owner_release(owner,1)==-1);
    assert(!desk_window_owner_new(owner->set,sizeof(struct model)));
    callbacks++;return 0;
}
static void deliver(struct desk_window_set *set,struct desk_app *w,unsigned type,unsigned value) {
    target=w;packet=(struct desk_packet){.type=type,.value=value};queued=1;
    assert(!desk_window_set_dispatch(set,event,NULL));
}
static struct desk_window_owner *open_model(struct desk_window_set *set,uint64_t value) {
    struct desk_window_owner *o=desk_window_owner_new(set,sizeof(struct model));assert(o);
    assert(!((uintptr_t)o->model%16));struct model *m=(void *)o->model;
    assert(!m->sentinel && !m->window.connection);m->sentinel=value;
    assert(!desk_window_owner_open(o,&m->window,0,"Test",720,480));return o;
}
int main(void) {
    struct desk_window_set set;desk_window_set_init(&set);
    assert(!desk_window_owner_new(&set,0) && !desk_window_owner_new(&set,SIZE_MAX));
    fail_map=1;assert(!desk_window_owner_new(&set,sizeof(struct model)) && !set.windows);
    struct desk_window_owner *owners[40];
    for(unsigned i=0;i<40;i++) {
        owners[i]=open_model(&set,i+100);deliver(&set,owners[i]->window,DESK_OPEN_REPLY,0);
        for(unsigned j=0;j<=i;j++)assert(((struct model *)(void *)owners[j]->model)->sentinel==j+100);
    }
    assert(set.admitted==40 && !set.refused && !set.retired);
    struct desk_window_owner *o=owners[39];
    o->dialog.window.connection=&set.connection;
    deliver(&set,&o->dialog.window,DESK_KEY,0);assert(!o->retired);
    deliver(&set,o->window,DESK_CLOSE_ACK,0);
    assert(desk_window_owner_dispose(o)==-1 && desk_window_owner_release(o,1)==-1);
    o->dialog.window.connection=NULL;assert(!desk_window_owner_dispose(o));
    assert(desk_window_owner_release(o,0)==-1 && ((struct model *)(void *)o->model)->sentinel==139);
    fail_unmap=1;assert(desk_window_owner_release(o,1)==-1 && set.windows==o && !set.retired);
    assert(!desk_window_owner_release(o,1) && set.retired==1);
    set.lifecycle.quitting=1;o=desk_window_set_next_close(&set);assert(o==owners[38]);
    assert(!desk_window_set_next_close(&set) && !desk_window_owner_new(&set,sizeof(struct model)));
    /* A canceled decision resumes admission without affecting sibling models. */
    set.lifecycle=(struct desk_application_lifecycle){0};
    o=open_model(&set,999);deliver(&set,o->window,DESK_OPEN_REPLY,DESK_OPEN_CAPACITY);
    assert(o->retired && !o->ready && set.refused==1);
    assert(!desk_window_owner_dispose(o) && !desk_window_owner_release(o,1) && set.retired==1);
    for(unsigned i=0;i<39;i++) {
        o=owners[i];deliver(&set,o->window,DESK_CLOSE_ACK,0);
        assert(!desk_window_owner_dispose(o));assert(!desk_window_owner_release(o,1));
    }
    assert(!set.windows && set.admitted==set.retired && callbacks==82);
    o=desk_window_owner_new(&set,sizeof(struct model));assert(o);
    struct desk_app foreign={0};assert(desk_window_owner_open(o,&foreign,0,"Test",720,480)==-1);
    assert(!desk_window_owner_release(o,1));
    o=open_model(&set,777);deliver(&set,o->window,DESK_OPEN_REPLY,0);
    fault=1;assert(desk_window_set_dispatch(&set,event,NULL)==-1 && set.failed && o->retired);
    assert(desk_window_owner_dispose(o)==-1 && desk_window_owner_release(o,1)==-1);
    assert(((struct model *)(void *)o->model)->sentinel==777);
    /* Model process-exit cleanup; the failed application cannot release uncertain GUI state. */
    assert(!munmap(o,o->mapping_bytes));
    puts("window-set: PASS 40 stable models, admission/refusal, dialog routing, serialized Quit, explicit work drain, failed-unmap retry and fatal-transport retention");
}
