#include <srvros/desk_app.h>
#include <srvros/gui.h>
#include <srvros/sys.h>
#include <string.h>

int desk_app_open(struct desk_app *a,const char *title,uint32_t width,uint32_t height) {
    *a=(struct desk_app){.last=srv_ticks(),.dirty=1,.focused=1};
    struct desk_packet p=desk_packet_new(DESK_OPEN);p.width=width;p.height=height;
    size_t n=strlen(title);if(n>=sizeof(p.text)) n=sizeof(p.text)-1;
    memcpy(p.text,title,n);
    if(desk_send(0,&p)) return -1;
    return 0;
}
int desk_app_poll(struct desk_app *a,struct desk_packet *event) {
    if(a->closed || a->failed) return -1;
    if(!a->pending && a->configure.generation>a->generation) {
        if(a->configure.generation==a->rejected_generation) {
            struct desk_packet refused=desk_packet_new(DESK_RESIZE_REFUSED);
            refused.generation=a->rejected_generation;refused.width=a->width;refused.height=a->height;
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
    struct desk_packet p;uint64_t source;int r=desk_receive(&source,&p);
    if(r<0) { a->failed=1;return -1; }
    if(r==1) {
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
        if(p.input_serial>a->input_serial) { a->input_serial=p.input_serial;a->dirty=1; }
        *event=p;return 1;
    }
    if(r==2) return 2;
    if((!a->server || a->pending || a->configure.generation>a->generation) && (uint64_t)srv_ticks()-a->last>500) {
        a->failed=1;return -1;
    }
    if(a->server && !desk_process_alive(a->server)) { a->closed=1;return -1; }
    return 0;
}
int desk_app_wait(struct desk_app *a,uint64_t ticks) {
    if(!a || !a->server || a->closed || a->failed || a->dirty || a->pending ||
       !a->surface || a->generation!=a->configure.generation || a->send_since ||
       a->menu_pending || ticks>100) return -1;
    return gui_wait(a->server,ticks);
}

int desk_app_canvas(struct desk_app *a,struct desk_canvas *canvas) {
    if(a->failed || a->closed) return -1;
    if(!a->dirty || a->pending || !a->surface || a->generation!=a->configure.generation ||
       (a->retry_after && (uint64_t)srv_ticks()<a->retry_after)) return 0;
    return desk_canvas_init(canvas,a->pixels,a->width,a->height,a->width)?-1:1;
}
int desk_app_present(struct desk_app *a) {
    if(a->failed || a->closed || a->pending || !a->surface) return -1;
    struct desk_packet p=desk_packet_new(DESK_COMMIT);
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
    if(!a->server) return -1;
    struct desk_packet p=desk_packet_new(DESK_TITLE);
    size_t n=strlen(title);if(n>=sizeof(p.text)) n=sizeof(p.text)-1;
    memcpy(p.text,title,n);return desk_send(a->server,&p);
}
int desk_app_cancel_close(struct desk_app *a) {
    struct desk_packet p=desk_packet_new(DESK_CANCEL_CLOSE);return desk_send(a->server,&p);
}
int desk_app_launch(struct desk_app *a,unsigned application,const char *path) {
    size_t length=strlen(path)+1;
    if(!a->server || application<1 || application>DESK_APP_COUNT || length>160) return -1;
    for(size_t offset=0;offset<length;) {
        struct desk_packet p=desk_packet_new(DESK_LAUNCH);
        size_t n=length-offset;if(n>sizeof(p.text)) n=sizeof(p.text);
        p.value=application;p.serial=offset;p.height=n;p.width=offset+n==length;
        memcpy(p.text,path+offset,n);
        if(desk_send(a->server,&p)) return -1;
        offset+=n;
    }
    return 0;
}
int desk_app_preferences(struct desk_app *a,const struct desk_preferences *preferences,unsigned mask) {
    if(!a->server || !preferences || !mask || (mask&~DESK_PREF_ALL)) return -1;
    struct desk_packet p=desk_packet_new(DESK_PREFERENCES_SET);p.value=mask;
    memcpy(p.text,preferences,sizeof(*preferences));return desk_send(a->server,&p);
}
int desk_app_text_size(struct desk_app *a,unsigned size) {
    if(size!=16 && size!=20) return -1;
    struct desk_preferences p={.text_size=size};return desk_app_preferences(a,&p,DESK_PREF_TEXT);
}
void desk_app_close(struct desk_app *a) {
    if(a->server) { struct desk_packet p=desk_packet_new(DESK_CLOSE);desk_send(a->server,&p); }
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
    if(!a->server || a->closed || a->failed) return;
    for(unsigned budget=0;budget<4 && a->menu_pending;budget++) {
        struct desk_packet p=desk_packet_new(DESK_MENU_SET);
        p.serial=a->menu_revision;p.generation=a->menu_next;p.width=a->menu_count;
        p.value=!a->menu_count || a->menu_next+1==a->menu_count;
        if(a->menu_count) memcpy(p.text,&a->menus[a->menu_next],sizeof(a->menus[0]));
        if(desk_send(a->server,&p)) return;
        if(p.value) a->menu_pending=0;else a->menu_next++;
    }
}
