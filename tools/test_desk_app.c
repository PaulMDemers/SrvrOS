#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_app.h>
#include <srvros/gui.h>
#include <srvros/sys.h>

static uint64_t ticks=1,next_surface=10,source=42;
static int alive=1,queued,full,created,destroyed,blits,sends,waits,wait_result;
int gui_wait(uint64_t server,uint64_t deadline) { assert(server==42 && deadline==10);waits++;return wait_result; }
static struct desk_packet incoming,last_sent;
long srv_ticks(void) { return ticks; }
int desk_process_alive(uint64_t pid) { assert(pid==42);return alive; }
uint32_t *desk_pixels_alloc(uint32_t w,uint32_t h) { return calloc((size_t)w*h,4); }
void desk_pixels_free(uint32_t *p,uint32_t w,uint32_t h) { (void)w;(void)h;free(p); }
struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.type=type,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet)};
}
int desk_send(uint64_t target,const struct desk_packet *p) {
    assert(target==0 || target==42);if(full) return -1;last_sent=*p;sends++;return 0;
}
int desk_receive(uint64_t *sender,struct desk_packet *p) {
    if(!queued) return 0;
    queued=0;*sender=source;*p=incoming;return 1;
}
int gui_surface_create(uint64_t w,uint64_t h,uint64_t flags,uint64_t *id) {
    assert(w>=280 && h>=240 && flags==DESK_SURFACE_STREAM);*id=next_surface++;created++;return 0;
}
int gui_surface_destroy(uint64_t id) { assert(id>=10);destroyed++;return 0; }
int gui_surface_blit(uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,const uint32_t *p,uint64_t stride) {
    assert(id>=10 && x==0 && y==0 && w==stride && h>=240 && p);blits++;return 0;
}
static void receive(unsigned type,uint64_t generation) {
    incoming=desk_packet_new(type);incoming.generation=generation;queued=1;
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
    puts("desk-app: PASS ownership, release-before-resize, stale input, retry, launch chunks, close veto, server loss and timeout");
}
