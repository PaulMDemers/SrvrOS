#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_app.h>
#include <srvros/desk_connection.h>
#include <srvros/gui.h>
#include <srvros/sys.h>

static uint64_t ticks=1,next_surface=10,source=42;
static int alive=1,queued,full,created,destroyed,blits,sends,waits,wait_result,pixel_failure,surface_failure;
int gui_wait(uint64_t server,uint64_t deadline) { assert(server==42 && deadline==10);waits++;return wait_result; }
static struct desk_packet incoming,last_sent;
long srv_ticks(void) { return ticks; }
int desk_process_alive(uint64_t pid) { assert(pid==42);return alive; }
uint32_t *desk_pixels_alloc(uint32_t w,uint32_t h) { return pixel_failure?NULL:calloc((size_t)w*h,4); }
void desk_pixels_free(uint32_t *p,uint32_t w,uint32_t h) { (void)w;(void)h;free(p); }
struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.window=DESK_DEFAULT_WINDOW,.type=type,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet)};
}
int desk_send(uint64_t target,const struct desk_packet *p) {
    assert(target==0 || target==42);if(full) return -1;last_sent=*p;sends++;return 0;
}
int desk_receive(uint64_t *sender,struct desk_packet *p) {
    if(!queued) return 0;
    queued=0;*sender=source;*p=incoming;return 1;
}
int gui_surface_create(uint64_t w,uint64_t h,uint64_t flags,uint64_t *id) {
    assert(w>=280 && h>=240 && flags==DESK_SURFACE_STREAM);if(surface_failure)return -1;*id=next_surface++;created++;return 0;
}
int gui_surface_destroy(uint64_t id) { assert(id>=10);destroyed++;return 0; }
int gui_surface_blit(uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *p,uint64_t stride) {
    assert(id>=10 && x==0 && y==0 && w==stride && h>=240 && p);blits++;return 0;
}
static void receive(unsigned type,uint64_t generation) {
    incoming=desk_packet_new(type);incoming.generation=generation;queued=1;
}
static void refused_resize(void) {
    struct desk_app a;struct desk_packet event;
    assert(!desk_app_open(&a,"Pressure",400,300) && !a.pixels);
    receive(DESK_CONFIGURE,1);incoming.width=400;incoming.height=300;
    assert(desk_app_poll(&a,&event)==2 && desk_app_poll(&a,&event)==1);
    uint32_t *pixels=a.pixels;uint64_t surface=a.surface;pixels[0]=77;
    for(unsigned phase=0;phase<2;phase++) {
        unsigned gen=2+phase*2;
        receive(DESK_CONFIGURE,gen);incoming.width=600;incoming.height=500;
        assert(desk_app_poll(&a,&event)==2);
        if(!phase)pixel_failure=1;else surface_failure=1;
        assert(desk_app_poll(&a,&event)==2 && !a.failed && a.pixels==pixels && a.surface==surface && pixels[0]==77);
        full=1;assert(desk_app_poll(&a,&event)>=0 && a.configure.generation==gen);
        full=0;assert(desk_app_poll(&a,&event)>=0 && last_sent.type==DESK_RESIZE_REFUSED && last_sent.generation==gen);
        pixel_failure=surface_failure=0;
        receive(DESK_CONFIGURE,gen);incoming.width=600;incoming.height=500;
        assert(desk_app_poll(&a,&event)==2 && a.configure.generation==a.generation); /* Stale. */
        receive(DESK_CONFIGURE,gen+1);incoming.width=400;incoming.height=300;
        assert(desk_app_poll(&a,&event)==2 && desk_app_poll(&a,&event)==1);
        assert(a.pixels==pixels && a.surface==surface && pixels[0]==77 && a.generation==gen+1);
    }
    struct desk_canvas canvas;pixels[0]=88;
    assert(desk_app_present(&a)==1);
    receive(DESK_RELEASE,a.generation);incoming.surface=surface;incoming.serial=a.pending+1;incoming.value=DESK_RELEASE_RETRY;
    assert(desk_app_poll(&a,&event)==2 && a.pending && !a.retry_after);
    incoming.serial=a.pending;queued=1;
    assert(desk_app_poll(&a,&event)==2 && !a.pending && a.dirty && !a.failed);
    assert(a.pixels==pixels && pixels[0]==88 && !desk_app_canvas(&a,&canvas));
    ticks+=4;assert(!desk_app_canvas(&a,&canvas));
    ticks++;assert(desk_app_canvas(&a,&canvas)==1 && desk_app_present(&a)==1 && !a.retry_after);
    receive(DESK_RELEASE,a.generation);incoming.surface=surface;incoming.serial=a.pending;
    assert(desk_app_poll(&a,&event)==2 && !a.pending && !a.dirty);
    desk_app_close(&a);
}
static int connection_receive(struct desk_connection *c,struct desk_app **target,struct desk_packet *event) {
    int r=0;for(int n=0;n<8 && queued;n++)r=desk_connection_poll(c,target,event);
    assert(!queued);return r;
}
static void connection_configure(struct desk_connection *c,struct desk_app *a) {
    struct desk_app *target;struct desk_packet event;
    receive(DESK_CONFIGURE,1);incoming.window=a->window;incoming.width=400;incoming.height=300;
    assert(connection_receive(c,&target,&event)==2);
    for(int n=0;n<8 && !a->surface;n++)desk_connection_poll(c,&target,&event);
    assert(a->surface && a->width==400 && a->height==300);
}
static void connection_windows(void) {
    alive=1;full=0;source=42;queued=0;
    struct desk_connection c;struct desk_app a,b,*target;struct desk_packet event;
    desk_connection_init(&c);
    assert(!desk_window_open(&c,&a,"First",400,300));
    assert(desk_window_open(&c,&b,"Second",400,300)<0); /* One creation credit. */
    full=1;assert(!desk_connection_poll(&c,&target,&event) && !a.open_sent);
    full=0;assert(!desk_connection_poll(&c,&target,&event) && a.open_sent && last_sent.window==1);
    receive(DESK_OPEN_REPLY,0);assert(connection_receive(&c,&target,&event)==1 && target==&a && !a.opening);
    connection_configure(&c,&a);a.pixels[0]=0x123456;assert(desk_app_present(&a)==1);
    assert(desk_app_poll(&a,&event)<0); /* A second queue reader is forbidden. */
    assert(!desk_window_open(&c,&b,"Second",400,300));
    for(int n=0;n<8 && !b.open_sent;n++)desk_connection_poll(&c,&target,&event);
    assert(b.open_sent && b.window==2);
    /* No pre-admission service traffic. */
    int sent_before=sends;b.menu_pending=1;desk_app_menu_tick(&b);
    assert(sends==sent_before && b.menu_pending && desk_app_title(&b,"Early")<0);b.menu_pending=0;
    receive(DESK_OPEN_REPLY,0);incoming.window=2;assert(connection_receive(&c,&target,&event)==1 && target==&b);
    connection_configure(&c,&b);b.pixels[0]=0xabcdef;assert(desk_app_present(&b)==1);
    receive(DESK_RELEASE,1);incoming.window=2;incoming.surface=a.surface;incoming.serial=a.pending;
    assert(connection_receive(&c,&target,&event)==2 && a.pending && b.pending);
    receive(DESK_RELEASE,1);incoming.window=2;incoming.surface=b.surface;incoming.serial=b.pending;
    assert(connection_receive(&c,&target,&event)==2 && !b.pending && a.pending);
    receive(DESK_FOCUS,0);incoming.window=2;incoming.value=1;
    assert(connection_receive(&c,&target,&event)==1 && target==&b && b.focused && !a.focused);
    source=99;receive(DESK_CLOSE,0);incoming.window=2;
    assert(connection_receive(&c,&target,&event)==2 && !b.closing);source=42;
    desk_window_close(&a);assert(desk_window_dispose(&a)<0 && desk_app_present(&a)<0);
    full=1;for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);assert(!a.close_sent);
    full=0;for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);assert(a.close_sent);
    receive(DESK_RELEASE,1);incoming.surface=a.surface;incoming.serial=a.pending;
    assert(connection_receive(&c,&target,&event)==2 && !a.pending && desk_window_dispose(&a)<0);
    receive(DESK_CLOSE_ACK,0);assert(connection_receive(&c,&target,&event)==1 && target==&a && a.closed);
    assert(!desk_window_dispose(&a) && b.pixels[0]==0xabcdef && !b.closed && !b.failed);
    assert(!desk_window_open(&c,&a,"Refused",400,300) && a.window==3);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_OPEN_REPLY,0);incoming.window=3;incoming.value=DESK_OPEN_CAPACITY;
    assert(connection_receive(&c,&target,&event)==1 && a.closed && a.open_result==DESK_OPEN_CAPACITY);
    assert(!desk_window_dispose(&a) && !b.closed);
    assert(!desk_window_open(&c,&a,"Reused storage",400,300) && a.window==4);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_OPEN_REPLY,0);incoming.window=4;assert(connection_receive(&c,&target,&event)==1);
    connection_configure(&c,&a);
    receive(DESK_CLOSE,0);incoming.window=3;assert(connection_receive(&c,&target,&event)==2 && !a.closing);
    desk_window_close(&a);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_CLOSE_ACK,0);incoming.window=4;assert(connection_receive(&c,&target,&event)==1);
    assert(!desk_window_dispose(&a));
    assert(!desk_window_open(&c,&a,"No canvas",400,300) && a.window==5);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_OPEN_REPLY,0);incoming.window=5;assert(connection_receive(&c,&target,&event)==1);
    receive(DESK_CONFIGURE,1);incoming.window=5;incoming.width=400;incoming.height=300;
    assert(connection_receive(&c,&target,&event)==2);pixel_failure=1;
    for(int n=0;n<3 && !a.closing;n++)desk_connection_poll(&c,&target,&event);
    pixel_failure=0;assert(a.closing && a.failed && !c.failed && event.type==DESK_WINDOW_FAILED && !b.closed);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_CLOSE_ACK,0);incoming.window=5;assert(connection_receive(&c,&target,&event)==1);
    assert(!desk_window_dispose(&a));
    assert(!desk_window_open(&c,&a,"Final",400,300) && a.window==6);
    for(int n=0;n<3;n++)desk_connection_poll(&c,&target,&event);
    receive(DESK_OPEN_REPLY,0);incoming.window=6;assert(connection_receive(&c,&target,&event)==1);
    connection_configure(&c,&a);
    assert(desk_app_present(&a)==1);b.dirty=1;assert(desk_app_present(&b)==1);
    alive=0;assert(desk_connection_poll(&c,&target,&event)<0 && c.disconnected && !a.pending && !b.pending);
    assert(!desk_window_dispose(&a) && !desk_window_dispose(&b) && !c.windows);alive=1;
    desk_connection_init(&c);assert(!desk_window_open(&c,&a,"Timeout",400,300));
    assert(!desk_connection_poll(&c,&target,&event));ticks+=501;
    receive(DESK_KEY,0);incoming.window=99;
    assert(desk_connection_poll(&c,&target,&event)<0 && c.failed);queued=0;
    puts("desk-connection: PASS two windows, single reader, creation credit/refusal, independent focus/frames, blocked close, stale IDs, server death and flood deadline");
}
int main(void) {
    struct desk_app stable={.server=42,.surface=1,.generation=7,.configure={.generation=7}};
    assert(desk_app_wait(&stable,10)==0 && waits==1);
    wait_result=1;assert(desk_app_wait(&stable,10)==1 && waits==2);
    wait_result=-1;assert(desk_app_wait(&stable,10)==-1 && waits==3);
    assert(desk_app_wait(NULL,10)==-1 && desk_app_wait(&stable,101)==-1);
    struct desk_app blocked=stable;
#define REFUSE(field,value) do { blocked=stable;blocked.field=value;assert(desk_app_wait(&blocked,10)==-1 && waits==3); } while(0)
    REFUSE(server,0);REFUSE(surface,0);REFUSE(generation,8);REFUSE(closed,1);REFUSE(failed,1);
    REFUSE(dirty,1);REFUSE(pending,1);REFUSE(send_since,1);REFUSE(menu_pending,1);
#undef REFUSE
    struct desk_app a;struct desk_packet event;struct desk_canvas canvas;
    assert(!desk_app_open(&a,"Test",400,300));assert(last_sent.type==DESK_OPEN);
    assert(!desk_app_canvas(&a,&canvas));
    receive(DESK_CONFIGURE,1);incoming.width=400;incoming.height=300;incoming.window=2;
    assert(desk_app_poll(&a,&event)==2 && !a.server && !a.configure.generation);
    receive(DESK_CONFIGURE,1);incoming.width=400;incoming.height=300;
    assert(desk_app_poll(&a,&event)==2 && a.server==42);
    assert(desk_app_poll(&a,&event)==1 && event.type==DESK_CONFIGURE && created==1);
    assert(desk_app_canvas(&a,&canvas)==1 && canvas.width==400);
    full=1;assert(!desk_app_present(&a) && a.dirty && !a.pending);
    full=0;assert(desk_app_present(&a)==1 && a.pending==1 && !a.dirty);
    receive(DESK_CONFIGURE,2);incoming.width=500;incoming.height=350;
    assert(desk_app_poll(&a,&event)==2 && created==1 && !destroyed);
    receive(DESK_POINTER,1);assert(desk_app_poll(&a,&event)==2);
    receive(DESK_CONTEXT,1);assert(desk_app_poll(&a,&event)==2);
    receive(DESK_RELEASE,1);incoming.surface=a.surface;incoming.serial=999;
    desk_app_poll(&a,&event);assert(a.pending==1);
    receive(DESK_RELEASE,1);incoming.surface=a.surface;incoming.serial=1;incoming.window=2;
    assert(desk_app_poll(&a,&event)==2 && a.pending==1);
    receive(DESK_CLOSE,1);incoming.window=2;assert(desk_app_poll(&a,&event)==2 && !a.closed);
    receive(DESK_RELEASE,1);incoming.surface=a.surface;incoming.serial=1;
    assert(desk_app_poll(&a,&event)==2 && !a.pending);
    assert(desk_app_poll(&a,&event)==1 && a.width==500 && created==2 && destroyed==1);
    receive(DESK_CONTEXT,1);assert(desk_app_poll(&a,&event)==2);
    receive(DESK_CONTEXT,2);assert(desk_app_poll(&a,&event)==1 && event.type==DESK_CONTEXT);
    receive(DESK_KEY,2);incoming.input_serial=17;
    assert(desk_app_poll(&a,&event)==1 && a.input_serial==17 && a.dirty);
    assert(desk_app_present(&a)==1 && last_sent.input_serial==17);
    receive(DESK_FOCUS,2);incoming.value=0;assert(desk_app_poll(&a,&event)==1 && !a.focused);
    receive(DESK_RELEASE,2);incoming.surface=a.surface;incoming.serial=2;desk_app_poll(&a,&event);
    assert(!desk_app_title(&a,"Changed") && last_sent.type==DESK_TITLE);
    assert(!desk_app_launch(&a,DESK_APP_TEXT,"/fat/document.txt") && last_sent.type==DESK_LAUNCH && last_sent.width==1);
    char path[160];memset(path,'a',159);path[0]='/';path[159]=0;
    int before=sends;assert(!desk_app_launch(&a,DESK_APP_TEXT,path) && sends==before+2 && last_sent.serial==88);
    assert(desk_app_launch(&a,99,path)<0);
    receive(DESK_CLOSE_REQUEST,2);assert(desk_app_poll(&a,&event)==1 && !a.closed);
    assert(!desk_app_cancel_close(&a) && last_sent.type==DESK_CANCEL_CLOSE);
    struct desk_menu_item menu[]={ {0,DESK_MENU_HEADING,"File",""},{1,0,"New","Ctrl-N"},{2,DESK_MENU_DISABLED,"Save","Ctrl-S"} };
    assert(!desk_app_menus(&a,menu,3));uint64_t revision=a.menu_revision;
    full=1;desk_app_menu_tick(&a);assert(a.menu_pending && !a.menu_next);
    full=0;before=sends;desk_app_menu_tick(&a);assert(!a.menu_pending && sends==before+3 && last_sent.value==1);
    assert(!desk_app_menus(&a,menu,3) && a.menu_revision==revision && !a.menu_pending);
    receive(DESK_MENU_ACTION,0);incoming.serial=revision;incoming.value=1;assert(desk_app_poll(&a,&event)==2); /* Unfocused. */
    a.focused=1;receive(DESK_MENU_ACTION,0);incoming.serial=revision;incoming.value=1;assert(desk_app_poll(&a,&event)==1);
    receive(DESK_MENU_ACTION,0);incoming.serial=revision;incoming.value=2;assert(desk_app_poll(&a,&event)==2);
    receive(DESK_MENU_ACTION,0);incoming.serial=revision-1;incoming.value=1;assert(desk_app_poll(&a,&event)==2);
    receive(DESK_MENU_ACTION,0);incoming.serial=revision;incoming.value=999;assert(desk_app_poll(&a,&event)==2);
    assert(!desk_app_menus(&a,NULL,0));desk_app_menu_tick(&a);assert(last_sent.type==DESK_MENU_SET && !last_sent.width && last_sent.value);
    alive=0;assert(desk_app_poll(&a,&event)<0 && a.closed);
    desk_app_close(&a);assert(destroyed==2 && !a.pixels);
    alive=1;assert(!desk_app_open(&a,"Timeout",400,300));ticks+=501;
    assert(desk_app_poll(&a,&event)<0 && a.failed);desk_app_close(&a);
    refused_resize();
    connection_windows();
    puts("desk-app: PASS ownership, release-before-resize, stale input, retry, launch chunks, close veto, server loss and timeout");
}
