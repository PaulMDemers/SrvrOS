#include <srvros/desk_log.h>
#include <srvros/desktop_apps.h>
#include "scene.h"
#include "clipboard.h"
#include "menus.h"
#include "preferences.h"
#include <srvros/desk_text.h>
#include "io_trial.h"
#include <srvros/desktop.h>
#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <srvros/calendar.h>

static struct desk_clipboard_store clipboard;

struct client {
    uint64_t pid,generation,progress,present_input;
    int configure,awaiting,closing,close_requested,preferences_dirty;
    struct desk_packet release;
    struct desk_packet input[DESK_INPUT_QUEUE];
    unsigned input_head,input_count;
    uint64_t input_since;
    struct desk_menu_store menus;
    char launch_path[160];
    unsigned launch_length,launch_application;
    uint64_t notice_last,notice_at,notice_ack_serial;
    unsigned notice_last_result,notice_ack_result;
    int notice_ack_pending,notice_has_time;
};
struct session {
    struct desktop_scene scene;
    struct desk_damage damage;
    struct desk_rect menu_visible;
    uint64_t menu_owner;
    struct client clients[DESKD_WINDOWS];
    uint64_t children[DESKD_WINDOWS];
    int child_apps[DESKD_WINDOWS],exiting;
    struct desktop_preferences_store preferences;
    uint64_t clock_tick,exit_wait;
    int trace,pointer_blocked,overlay_return,overlay_down;
    uint64_t pointer_pid;
    int notice_visible;
    struct io_trial io;
    uint32_t *cache[DESKD_WINDOWS][2];
};
/* Two bounded caches per window: failed partial copies never touch the front.
 * Allocate once before the worker starts, not mmap/munmap on every commit. */
static void cache_free(struct session *s) {
    for(int i=0;i<DESKD_WINDOWS;i++) for(int j=0;j<2;j++) {
        desk_pixels_free(s->cache[i][j],DESK_MAX_WIDTH,DESK_MAX_HEIGHT);
        s->cache[i][j]=0;
    }
}
static int cache_init(struct session *s) {
    for(int i=0;i<DESKD_WINDOWS;i++) for(int j=0;j<2;j++) {
        s->cache[i][j]=desk_pixels_alloc(DESK_MAX_WIDTH,DESK_MAX_HEIGHT);
        if(!s->cache[i][j]) { cache_free(s); return -1; }
    }
    return 0;
}
static int clamp(int64_t v,int lo,int hi) { return v<lo?lo:v>hi?hi:v; }
static void close_client(struct session *s,int i);
static void global_menu_rebuild(struct session *s);
static void launcher_catalog(struct session *s);
static void launcher_open(struct session *s);
static void launcher_close(struct session *s);
static void calendar_open(struct session *s);
static void calendar_close(struct session *s);
static void calendar_today_update(struct session *s,long utc);
static void notifications_open(struct session *s);
static void notifications_close(struct session *s);
static void notifications_damage(struct session *s);
static void queue_input(struct session *s,int i,struct desk_packet p) {
    struct client *c=&s->clients[i];
    if (c->closing) return;
    if (c->input_count) {
        struct desk_packet *last=&c->input[(c->input_head+c->input_count-1)%DESK_INPUT_QUEUE];
        if (p.type==DESK_POINTER && p.value==DESK_POINTER_LEFT &&
            last->type==p.type && last->value==p.value && last->generation==p.generation) {
            *last=p; return;
        }
        if (p.type==DESK_SCROLL && last->type==p.type &&
            (!p.x || !last->x || (p.x>0)==(last->x>0)) &&
            (!p.y || !last->y || (p.y>0)==(last->y>0))) {
            last->x=clamp((int64_t)last->x+p.x,-16384,16384);
            last->y=clamp((int64_t)last->y+p.y,-16384,16384); return;
        }
    }
    if (c->input_count==DESK_INPUT_QUEUE) { close_client(s,i); return; }
    if (!c->input_count) c->input_since=srv_ticks();
    c->input[(c->input_head+c->input_count++)%DESK_INPUT_QUEUE]=p;
}
static void pointer_cancel(struct session *s) {
    uint64_t pid=s->pointer_pid;
    s->pointer_pid=0;
    if (!pid) return;
    if (io_trial_active(&s->io)) s->io.cancels++;
    s->pointer_blocked=1;
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->clients[i].pid==pid) {
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.value=DESK_POINTER_CANCEL; p.generation=s->clients[i].generation;
        queue_input(s,i,p); break;
    }
}
static void cancel_input(struct session *s,unsigned buttons) {
    s->pointer_pid=0; s->pointer_blocked=(buttons&MOUSE_LEFT)!=0;
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->clients[i].pid) {
        /* A queued release may not have reached the client yet. Cancel every
         * client when discarding input, even if server capture already ended. */
        s->clients[i].input_count=0;
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.value=DESK_POINTER_CANCEL; p.generation=s->clients[i].generation;
        queue_input(s,i,p);
    }
}
static void damage_window(struct session *s,int i) {
    struct desk_rect r=s->scene.windows[i].frame; r.w+=6; r.h+=8;
    desk_damage_add(&s->damage,r);
}
static void focus(struct session *s,int i) {
    if(s->scene.launcher_open || s->scene.calendar_open || s->scene.notifications.opened) { s->overlay_return=i;if(s->scene.launcher_open) launcher_catalog(s);return; }
    int previous=s->scene.focus;
    if (previous!=i) {
        pointer_cancel(s);
        if(s->scene.apps && previous>=0 && s->clients[previous].pid) {
            struct desk_packet p=desk_packet_new(DESK_FOCUS);p.value=0;queue_input(s,previous,p);
        }
    }
    if (previous>=0) damage_window(s,previous);
    s->scene.focus=i;
    if (i>=0) {
        s->scene.windows[i].minimized=0;
        s->scene.windows[i].z=++s->scene.stack_serial;
        damage_window(s,i);
        if(s->scene.apps && previous!=i) {
            struct desk_packet p=desk_packet_new(DESK_FOCUS);p.value=1;queue_input(s,i,p);
        }
    }
    if(s->scene.apps) {
        global_menu_rebuild(s);
        desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
        desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
    }
}
static void forget(struct session *s,int i) {
    if((s->scene.launcher_open || s->scene.calendar_open) && s->overlay_return==i) s->overlay_return=-1;
    desk_clipboard_forget(&clipboard,s->clients[i].pid);
    if (s->pointer_pid==s->clients[i].pid && s->pointer_pid) {
        s->pointer_pid=0; s->pointer_blocked=1;
    }
    struct desktop_window *w=&s->scene.windows[i];
    damage_window(s,i);
    if (s->trace) desk_log("deskd: retired pid=%llu\n",(unsigned long long)s->clients[i].pid);
    memset(w,0,sizeof(*w)); memset(&s->clients[i],0,sizeof(s->clients[i]));
    if(s->scene.apps) desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
    if (s->scene.focus==i) {
        focus(s,-1);
        for (int j=0;j<DESKD_WINDOWS;j++) if (s->clients[j].pid) focus(s,j);
    }
    global_menu_rebuild(s);
}
static void configure(struct session *s,int i,int width,int height) {
    if (s->pointer_pid==s->clients[i].pid) pointer_cancel(s);
    struct client *c=&s->clients[i];
    struct desktop_window *w=&s->scene.windows[i];
    damage_window(s,i);
    w->frame.w=width+2; w->frame.h=height+45;
    c->generation++; c->configure=1; c->awaiting=1; c->progress=srv_ticks();
    damage_window(s,i);
}
static void minimize(struct session *s,int i) {
    damage_window(s,i);s->scene.windows[i].minimized=1;focus(s,-1);
    int order[DESKD_WINDOWS];desktop_stack(&s->scene,order);
    for(int n=DESKD_WINDOWS-1;n>=0;n--) {
        int j=order[n];
        if(s->clients[j].pid && s->scene.windows[j].visible && !s->scene.windows[j].minimized) { focus(s,j);break; }
    }
}
static void maximize(struct session *s,int i) {
    struct desktop_window *w=&s->scene.windows[i];damage_window(s,i);
    if(w->maximized) {
        struct desk_rect restore=w->restore;w->frame.x=restore.x;w->frame.y=restore.y;
        configure(s,i,restore.w-2,restore.h-45);w->maximized=0;
    } else {
        w->restore=w->frame;
        int width=clamp(s->damage.width-16,280,DESK_MAX_WIDTH);
        int height=clamp(s->damage.height-177,240,DESK_MAX_HEIGHT);
        w->frame.x=(s->damage.width-width-2)/2;w->frame.y=52;
        configure(s,i,width,height);w->maximized=1;
    }
}
static void spawn_client(struct session *s) {
    for (int i=0;i<DESKD_WINDOWS;i++) if (!s->children[i]) {
        long pid=spawn_bg("/fat/bin/deskclient");
        if (pid>0) s->children[i]=(uint64_t)pid;
        else srv_puts("deskd: launch failed\n");
        return;
    }
}
static void spawn_app(struct session *s,int application,const char *path) {
    const struct desk_application_info *info=desk_application_info(application);
    if(!info) return;
    for(int i=0;i<DESKD_WINDOWS;i++) if(!s->children[i]) {
        char *argv[]={(char *)info->program,(char *)info->role,(char *)(path?path:""),NULL};
        struct srv_exec_request request={.path=argv[0],.argv=argv,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        long pid=srv_exec(&request);
        if(pid>0) { s->children[i]=pid;s->child_apps[i]=application; }
        else strcpy(s->scene.notice,"The application could not be opened.");
        desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
        return;
    }
    strcpy(s->scene.notice,"Close a window before opening another application.");
    desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
}
static void request_close(struct session *s,int i) {
    if(!s->scene.apps) { close_client(s,i);return; }
    if(s->clients[i].close_requested || !s->clients[i].pid) return;
    struct desk_packet p=desk_packet_new(DESK_CLOSE_REQUEST);
    queue_input(s,i,p);s->clients[i].close_requested=1;
}
static void cycle_focus(struct session *s,int reverse) {
    int start=s->scene.focus;
    for(int n=1;n<=DESKD_WINDOWS;n++) {
        int i=(start+(reverse?DESKD_WINDOWS-n:n)+DESKD_WINDOWS)%DESKD_WINDOWS;
        if(s->clients[i].pid && !s->clients[i].closing) { focus(s,i);return; }
    }
}
enum global_action { GLOBAL_ABOUT=65536,GLOBAL_SETTINGS,GLOBAL_TEXT,GLOBAL_TERMINAL,GLOBAL_EXIT,
    GLOBAL_MINIMIZE,GLOBAL_ZOOM,GLOBAL_NEXT,GLOBAL_LAUNCHER,GLOBAL_CALENDAR,GLOBAL_HELP,GLOBAL_NOTIFICATIONS,GLOBAL_WINDOW=65792 };
static void global_menu_damage(struct session *s) {
    desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
    if(s->menu_visible.w>0) desk_damage_add(&s->damage,s->menu_visible);
    s->menu_visible=desk_menu_popup(&s->scene.menu);
    if(s->menu_visible.w>0) {
        s->menu_visible.w+=3;s->menu_visible.h+=4;desk_damage_add(&s->damage,s->menu_visible);
    }
}
static void clock_update(struct session *s,int force) {
    if(!s->scene.apps) return;
    uint64_t now=srv_ticks();
    if(!force && now-s->clock_tick<100) return;
    s->clock_tick=now;
    struct desk_preferences p=s->preferences.value;
    if(s->damage.width<1000) p.clock_flags&=~DESK_CLOCK_DATE;
    long us=srv_realtime_us();char clock[64]="Clock unavailable";
    desk_clock_format(clock,sizeof(clock),us<0?-1:us/1000000,&p);
    calendar_today_update(s,us<0?-1:us/1000000);
    if(strcmp(clock,s->scene.clock)) {
        strcpy(s->scene.clock,clock);
        int reserve=s->damage.width<1000?284:434;
        desk_damage_add(&s->damage,(struct desk_rect){s->damage.width-reserve,0,reserve,44});
    }
}
static void preferences_changed(struct session *s) {
    if(s->scene.wallpaper!=(int)s->preferences.value.wallpaper) {
        s->scene.wallpaper=s->preferences.value.wallpaper;
        desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,s->damage.height});
    }
    clock_update(s,1);
    for(int n=0;n<DESKD_WINDOWS;n++) if(s->clients[n].pid) s->clients[n].preferences_dirty=1;
    if(s->trace) desk_log("deskd: preferences revision=%llu status=%u text=%u offset=%d flags=%u wallpaper=%u\n",
        (unsigned long long)s->preferences.revision,s->preferences.status,s->preferences.value.text_size,
        s->preferences.value.utc_offset_minutes,s->preferences.value.clock_flags,s->preferences.value.wallpaper);
}
static void global_menu_rebuild(struct session *s) {
    if(!s->scene.apps) return;
    struct desk_menu_item items[DESK_MENU_ITEMS]={
        {0,DESK_MENU_HEADING,"SrvrOS",""},{GLOBAL_ABOUT,0,"About SrvrOS",""},
        {GLOBAL_SETTINGS,0,"Settings",""},{GLOBAL_LAUNCHER,0,"Open application or window…","Cmd-Space"},
        {GLOBAL_CALENDAR,0,"Calendar",""},{GLOBAL_TEXT,0,"New Text document",""},
        {GLOBAL_TERMINAL,0,"New Terminal",""},{GLOBAL_HELP,0,"SrvrOS Help","F1"},
        {GLOBAL_NOTIFICATIONS,0,"Notifications","Cmd-Shift-N"},
        {0,DESK_MENU_SEPARATOR,"",""},
        {GLOBAL_EXIT,0,"Return to console",""}
    };
    unsigned count=11;
    if(s->scene.focus>=0) {
        struct desk_menu_store *m=&s->clients[s->scene.focus].menus;
        memcpy(items+count,m->items,m->count*sizeof(items[0]));count+=m->count;
    }
    items[count++]=(struct desk_menu_item){0,DESK_MENU_HEADING,"Window",""};
    unsigned disabled=s->scene.focus<0?DESK_MENU_DISABLED:0;
    items[count++]=(struct desk_menu_item){GLOBAL_MINIMIZE,disabled,"Minimize",""};
    items[count++]=(struct desk_menu_item){GLOBAL_ZOOM,disabled,"Zoom",""};
    items[count++]=(struct desk_menu_item){GLOBAL_NEXT,disabled,"Next window","Alt-Tab"};
    int have=0;
    for(int i=0;i<DESKD_WINDOWS;i++) if(s->clients[i].pid && !s->clients[i].closing) {
        if(!have++) items[count++]=(struct desk_menu_item){0,DESK_MENU_SEPARATOR,"",""};
        struct desk_menu_item *item=&items[count++];item->id=GLOBAL_WINDOW+i;
        item->flags=s->scene.focus==i?DESK_MENU_CHECKED:0;
        size_t n=strlen(s->scene.windows[i].title);if(n>=sizeof(item->label)) n=sizeof(item->label)-1;
        while(n && !desk_text_utf8_valid(s->scene.windows[i].title,n)) n--;
        if(n) memcpy(item->label,s->scene.windows[i].title,n);else strcpy(item->label,"Window");
    }
    uint64_t owner=s->scene.focus>=0?s->clients[s->scene.focus].pid:0;
    if(s->scene.menu.font && owner==s->menu_owner) desk_menu_replace(&s->scene.menu,items,count);
    else desk_menu_configure(&s->scene.menu,items,count,(struct desk_rect){10,0,s->damage.width-(s->damage.width<1000?284:434),44},
        (struct desk_rect){0,0,s->damage.width,s->damage.height},&desk_font_16);
    s->menu_owner=owner;
    global_menu_damage(s);
    if(s->scene.launcher_open) launcher_catalog(s);
}
static void global_menu_action(struct session *s,uint32_t id) {
    if(!id) return;
    if(id<65536) {
        int i=s->scene.focus;if(i<0 || s->clients[i].closing) return;
        struct desk_packet p=desk_packet_new(DESK_MENU_ACTION);p.value=id;p.serial=s->clients[i].menus.revision;
        queue_input(s,i,p);return;
    }
    if(id==GLOBAL_LAUNCHER) launcher_open(s);
    else if(id==GLOBAL_CALENDAR) calendar_open(s);
    else if(id==GLOBAL_NOTIFICATIONS) notifications_open(s);
    else if(id==GLOBAL_ABOUT) spawn_app(s,DESK_APP_HELP,"about");
    else if(id==GLOBAL_HELP) {
        const struct desk_application_info *info=s->scene.focus>=0?desk_application_info(s->scene.windows[s->scene.focus].application):NULL;
        spawn_app(s,DESK_APP_HELP,info?info->role:"welcome");
    }
    else if(id==GLOBAL_SETTINGS) spawn_app(s,DESK_APP_SETTINGS,"");
    else if(id==GLOBAL_TEXT) spawn_app(s,DESK_APP_TEXT,"");
    else if(id==GLOBAL_TERMINAL) spawn_app(s,DESK_APP_TERMINAL,"");
    else if(id==GLOBAL_EXIT) {
        s->exiting=1;for(int i=0;i<DESKD_WINDOWS;i++) if(s->clients[i].pid) request_close(s,i);
    } else if(id==GLOBAL_MINIMIZE && s->scene.focus>=0) minimize(s,s->scene.focus);
    else if(id==GLOBAL_ZOOM && s->scene.focus>=0) maximize(s,s->scene.focus);
    else if(id==GLOBAL_NEXT) cycle_focus(s,0);
    else if(id>=GLOBAL_WINDOW && id<GLOBAL_WINDOW+DESKD_WINDOWS && s->clients[id-GLOBAL_WINDOW].pid) focus(s,id-GLOBAL_WINDOW);
}
static void global_menu_result(struct session *s,struct desk_menu_result result) {
    if(result.redraw) global_menu_damage(s);
    global_menu_action(s,result.action);
}
static void launcher_damage(struct session *s) {
    struct desk_rect r=s->scene.launcher.frame;r.w+=5;r.h+=6;desk_damage_add(&s->damage,r);
}
static void launcher_catalog(struct session *s) {
    struct desk_palette_item items[DESKD_WINDOWS+DESK_APP_COUNT]={0};unsigned count=0;
    int order[DESKD_WINDOWS];desktop_stack(&s->scene,order);
    for(int n=DESKD_WINDOWS-1;n>=0;n--) {
        int i=order[n];uint64_t pid=s->clients[i].pid;
        if(!pid || s->clients[i].closing || pid>UINT64_MAX-DESK_APP_COUNT) continue;
        struct desk_palette_item *item=&items[count++];item->id=pid+DESK_APP_COUNT;
        snprintf(item->label,sizeof(item->label),"%s",s->scene.windows[i].title[0]?s->scene.windows[i].title:"Window");
        strcpy(item->detail,s->scene.windows[i].minimized?"Hidden window":"Open window");
    }
    for(unsigned app=1;app<=DESK_APP_COUNT;app++) {
        struct desk_palette_item *item=&items[count++];item->id=app;strcpy(item->label,desk_application_info(app)->title);strcpy(item->detail,"New app");
    }
    struct desk_palette *p=&s->scene.launcher;
    if(p->count!=count || memcmp(p->items,items,count*sizeof(*items))) {
        desk_palette_items(p,items,count);if(s->scene.launcher_open) launcher_damage(s);
    }
}
static void launcher_open(struct session *s) {
    if(s->scene.notifications.opened) notifications_close(s);
    if(s->exiting || s->scene.launcher_open) return;
    if(s->scene.calendar_open) calendar_close(s);
    s->overlay_return=s->scene.focus;s->overlay_down=0;
    desk_menu_cancel(&s->scene.menu);global_menu_damage(s);focus(s,-1);
    launcher_catalog(s);struct desk_palette *p=&s->scene.launcher;
    desk_palette_open(p,"Open application or window",p->items,p->count);
    desk_palette_configure(p,(struct desk_rect){0,44,s->damage.width,s->damage.height-120});
    s->scene.launcher_open=1;launcher_damage(s);
    if(s->trace) desk_log("deskd: launcher opened items=%u\n",p->count);
}
static void launcher_close(struct session *s) {
    if(!s->scene.launcher_open) return;
    launcher_damage(s);desk_palette_focus(&s->scene.launcher,0);s->scene.launcher_open=0;
    if(s->overlay_down) s->pointer_blocked=1;
    int i=s->overlay_return;if(i<0 || i>=DESKD_WINDOWS || !s->clients[i].pid || s->clients[i].closing) i=-1;
    focus(s,i);if(s->trace) srv_puts("deskd: launcher closed\n");
}
static void launcher_result(struct session *s,struct desk_palette_result result) {
    struct desk_palette *p=&s->scene.launcher;
    unsigned operation=result.flags&(DESK_EDIT_COPY|DESK_EDIT_CUT|DESK_EDIT_PASTE);
    if(operation) {
        struct desk_text_buffer *text=&p->text;int changed=0;
        size_t lo=text->caret<text->anchor?text->caret:text->anchor,hi=text->caret>text->anchor?text->caret:text->anchor;
        if(operation==DESK_EDIT_PASTE) {
            if(clipboard.length) {
                if(memchr(clipboard.data,'\n',clipboard.length) || memchr(clipboard.data,'\r',clipboard.length) ||
                    desk_text_buffer_insert(text,(const char *)clipboard.data,clipboard.length)<0)
                    desk_palette_message(p,"Clipboard text must fit on one search line.");
                else changed=1;
            }
        } else if(hi>lo && !desk_clipboard_write_local(&clipboard,text->data+lo,hi-lo)) {
            if(operation==DESK_EDIT_CUT) { desk_text_buffer_insert(text,"",0);changed=1; }
        }
        if(changed) { desk_edit_reveal(&p->query);desk_palette_filter(p); }
        launcher_damage(s);
    }
    if(result.flags&DESK_WIDGET_REDRAW) launcher_damage(s);
    if(result.cancel) { launcher_close(s);return; }
    if(!result.id) return;
    launcher_close(s);
    if(result.id<=DESK_APP_COUNT) {
        if(s->trace) desk_log("deskd: launcher application=%u\n",(unsigned)result.id);
        spawn_app(s,result.id,"");
    } else {
        uint64_t pid=result.id-DESK_APP_COUNT;
        for(int i=0;i<DESKD_WINDOWS;i++) if(s->clients[i].pid==pid && !s->clients[i].closing) {
            focus(s,i);if(s->trace) desk_log("deskd: launcher window=%llu\n",(unsigned long long)pid);break;
        }
    }
}
enum { CALENDAR_GRID=1,CALENDAR_TODAY,CALENDAR_CLOSE };
static void calendar_damage(struct session *s) {
    struct desk_rect r=s->scene.calendar_frame;r.w+=5;r.h+=6;desk_damage_add(&s->damage,r);
    desk_damage_add(&s->damage,desktop_clock_rect(s->damage.width,s->scene.clock));
}
static void calendar_caption(struct session *s) {
    static const char *week[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    struct desk_date d=s->scene.calendar.selected;
    unsigned day=srv_civil_weekday(srv_civil_days(d.year,d.month,d.day));
    snprintf(s->scene.calendar_date,sizeof(s->scene.calendar_date),"%s · %04d-%02u-%02u",week[day],d.year,d.month,d.day);
    if(s->trace) desk_log("deskd: calendar selected=%04d-%02u-%02u\n",d.year,d.month,d.day);
}
static void calendar_today_update(struct session *s,long utc) {
    int offset=s->preferences.value.utc_offset_minutes,magnitude=offset<0?-offset:offset;
    char zone[32];if(offset) snprintf(zone,sizeof(zone),"UTC%c%02d:%02d",offset<0?'-':'+',magnitude/60,magnitude%60);else strcpy(zone,"UTC");
    struct desk_date date={0};time_t seconds=utc+(long)offset*60;struct tm local;
    if(utc>=0 && seconds>=-62135596800LL && seconds<=253402300799LL && gmtime_r(&seconds,&local))
        date=(struct desk_date){local.tm_year+1900,(unsigned)local.tm_mon+1,(unsigned)local.tm_mday};
    else strcpy(zone,"Clock unavailable");
    int changed=desk_calendar_today(&s->scene.calendar,date);
    if(strcmp(zone,s->scene.calendar_zone)) { strcpy(s->scene.calendar_zone,zone);changed=1; }
    if(s->scene.calendar_open && changed) {
        desk_button_configure(&s->scene.calendar_today,s->scene.calendar_today.frame,desk_date_valid(date));calendar_damage(s);
    }
}
static void calendar_open(struct session *s) {
    if(s->scene.notifications.opened) notifications_close(s);
    if(s->exiting || s->scene.calendar_open) return;
    if(s->scene.launcher_open) launcher_close(s);
    s->overlay_return=s->scene.focus;s->overlay_down=0;pointer_cancel(s);
    desk_menu_cancel(&s->scene.menu);global_menu_damage(s);focus(s,-1);clock_update(s,1);
    if(desk_date_valid(s->scene.calendar.today)) desk_calendar_select(&s->scene.calendar,s->scene.calendar.today);
    struct desk_rect r={s->damage.width-372,52,360,s->damage.height<496?s->damage.height-64:432};s->scene.calendar_frame=r;
    desk_calendar_configure(&s->scene.calendar,(struct desk_rect){r.x+16,r.y+48,r.w-32,r.h-144},1,0);
    desk_button_configure(&s->scene.calendar_today,(struct desk_rect){r.x+16,r.y+r.h-44,92,28},desk_date_valid(s->scene.calendar.today));
    desk_button_configure(&s->scene.calendar_close,(struct desk_rect){r.x+r.w-100,r.y+r.h-44,84,28},1);
    desk_ui_layout_changed(&s->scene.calendar_ui);desk_ui_window_focus(&s->scene.calendar_ui,1);desk_ui_focus(&s->scene.calendar_ui,CALENDAR_GRID);
    s->scene.calendar_open=1;calendar_caption(s);calendar_damage(s);
    if(s->trace) srv_puts("deskd: calendar opened\n");
}
static void calendar_close(struct session *s) {
    if(!s->scene.calendar_open) return;
    calendar_damage(s);desk_ui_window_focus(&s->scene.calendar_ui,0);s->scene.calendar_open=0;
    if(s->overlay_down) s->pointer_blocked=1;
    int i=s->overlay_return;if(i<0 || i>=DESKD_WINDOWS || !s->clients[i].pid || s->clients[i].closing) i=-1;
    focus(s,i);if(s->trace) srv_puts("deskd: calendar closed\n");
}
static void calendar_action(struct session *s,struct desk_action a) {
    if(a.id==CALENDAR_TODAY && (a.flags&DESK_WIDGET_ACTIVATED)) {
        if(desk_calendar_select(&s->scene.calendar,s->scene.calendar.today)>0) a.flags|=DESK_WIDGET_CHANGED;
        desk_ui_focus(&s->scene.calendar_ui,CALENDAR_GRID);a.flags|=DESK_WIDGET_REDRAW;
    }
    if(a.flags&DESK_WIDGET_CHANGED) calendar_caption(s);
    if(a.flags&DESK_WIDGET_REDRAW) calendar_damage(s);
    if(a.id==CALENDAR_CLOSE && (a.flags&DESK_WIDGET_ACTIVATED)) calendar_close(s);
}
static int known_child(struct session *s,uint64_t pid) {
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->children[i]==pid) return 1;
    return 0;
}
static int notifications_shortcut(const struct srv_key_event *key) {
    return (key->flags&SRV_KEY_DOWN_EVENT) && !(key->flags&SRV_KEY_REPEAT) &&
        (key->modifiers&SRV_KEY_META) && (key->modifiers&SRV_KEY_SHIFT) &&
        !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT)) && key->usage==17;
}
static void notifications_damage(struct session *s) {
    struct desktop_notice_view *v=&s->scene.notifications;
    desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
    struct desk_rect r=v->toast_frame;r.w+=5;r.h+=6;desk_damage_add(&s->damage,r);
    if(v->opened) { r=v->frame;r.w+=5;r.h+=6;desk_damage_add(&s->damage,r); }
}
static void notifications_open(struct session *s) {
    if(s->exiting || s->scene.notifications.opened) return;
    if(s->scene.launcher_open) launcher_close(s);
    if(s->scene.calendar_open) calendar_close(s);
    s->overlay_return=s->scene.focus;s->overlay_down=0;pointer_cancel(s);
    desk_menu_cancel(&s->scene.menu);global_menu_damage(s);focus(s,-1);
    desktop_notice_view_configure(&s->scene.notifications,s->damage.width,s->damage.height);
    desktop_notice_view_open(&s->scene.notifications);notifications_damage(s);
    if(s->trace) desk_log("deskd: notifications opened count=%u\n",s->scene.notifications.store.count);
}
static void notifications_close(struct session *s) {
    if(!s->scene.notifications.opened) return;
    notifications_damage(s);desktop_notice_view_close(&s->scene.notifications);
    if(s->overlay_down) s->pointer_blocked=1;
    int i=s->overlay_return;if(i<0 || i>=DESKD_WINDOWS || !s->clients[i].pid || s->clients[i].closing) i=-1;
    focus(s,i);if(s->trace) srv_puts("deskd: notifications closed\n");
}
static void notifications_apply(struct session *s,struct desktop_notice_action result) {
    if(result.close) notifications_close(s);
    if(result.application) {
        int owner=-1;for(int i=0;i<DESKD_WINDOWS;i++) if(s->clients[i].pid==result.owner && !s->clients[i].closing) owner=i;
        if(owner>=0) focus(s,owner);else spawn_app(s,result.application,"");
        if(s->trace) desk_log("deskd: notification opened owner=%llu application=%u\n",(unsigned long long)result.owner,result.application);
    }
    notifications_damage(s);
}
static void notifications_action(struct session *s,struct desk_action action) {
    if(!action.flags) return;
    notifications_apply(s,desktop_notice_view_action(&s->scene.notifications,action));
    if(s->trace && (action.flags&DESK_WIDGET_ACTIVATED)) desk_log("deskd: notification action=%u count=%u\n",action.id,s->scene.notifications.store.count);
}
static int notifications_toast_visible(const struct session *s) {
    return s->scene.notifications.toast_id && !s->scene.notifications.opened && !s->scene.launcher_open && !s->scene.calendar_open && s->scene.menu.open<0;
}
static void notifications_tick(struct session *s) {
    long us=srv_monotonic_us();int changed=desktop_notice_view_tick(&s->scene.notifications,us<0?0:(uint64_t)us);
    int visible=notifications_toast_visible(s);
    if(changed || visible!=s->notice_visible) notifications_damage(s);
    s->notice_visible=visible;desk_ui_window_focus(&s->scene.notifications.toast_ui,visible);
}
static void notification_received(struct session *s,int i,const struct desk_packet *p) {
    struct client *c=&s->clients[i];
    if(!s->scene.apps || c->closing || s->exiting || !p->serial) return;
    c->notice_ack_serial=p->serial;c->notice_ack_pending=1;
    if(p->serial<=c->notice_last) { c->notice_ack_result=p->serial==c->notice_last?c->notice_last_result:1;return; }
    c->notice_last=p->serial;c->notice_last_result=c->notice_ack_result=1;
    uint64_t now=srv_ticks();struct desk_notification text;memcpy(&text,p->text,sizeof(text));
    if(p->generation || p->surface || p->width || p->height || p->x || p->y || p->input_serial ||
        !desktop_notification_valid(&text,p->value) || (c->notice_has_time && (now<c->notice_at || now-c->notice_at<100))) return;
    long us=srv_monotonic_us();uint64_t id;
    int added=desktop_notification_add(&s->scene.notifications.store,c->pid,s->scene.windows[i].application,p->serial,us<0?0:(uint64_t)us,p->value,&text,&id);
    if(added<0) return;
    c->notice_ack_result=c->notice_last_result=0;c->notice_at=now;c->notice_has_time=1;
    if(added) { desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);notifications_damage(s); }
    if(s->trace) desk_log("deskd: notification accepted owner=%llu id=%llu count=%u\n",(unsigned long long)c->pid,(unsigned long long)id,s->scene.notifications.store.count);
}
static void close_client(struct session *s,int i) {
    if (s->pointer_pid==s->clients[i].pid && s->pointer_pid) {
        s->pointer_pid=0; s->pointer_blocked=1;
    }
    if (!s->clients[i].closing) {
        s->clients[i].closing=1;
        s->clients[i].progress=srv_ticks();
    }
}
static void incoming(struct session *s,uint64_t pid,struct desk_packet *p) {
    int i;
    for (i=0;i<DESKD_WINDOWS;i++) if (s->clients[i].pid==pid) break;
    /* Only applications launched by this session may become desktop clients. */
    if (i==DESKD_WINDOWS) {
        if (p->type!=DESK_OPEN || !known_child(s,pid)) return;
        for (i=0;i<DESKD_WINDOWS;i++) if (!s->clients[i].pid) break;
        if (i==DESKD_WINDOWS) return;
        struct desktop_window *w=&s->scene.windows[i];
        s->clients[i].pid=pid;s->clients[i].preferences_dirty=s->scene.apps;
        w->visible=1;
        for(int n=0;n<DESKD_WINDOWS;n++) if(s->children[n]==pid) w->application=s->child_apps[n];
        w->frame.x=32+24*(i%4); w->frame.y=64+24*(i%4);
        if(!s->scene.apps) { w->frame.x=32+40*i;w->frame.y=64+36*i; }
        memcpy(w->title,p->text,sizeof(p->text)); w->title[sizeof(p->text)]=0;
        configure(s,i,clamp(p->width,280,s->damage.width-w->frame.x-8<DESK_MAX_WIDTH?
            s->damage.width-w->frame.x-8:DESK_MAX_WIDTH),
            clamp(p->height,240,s->damage.height-w->frame.y-(s->scene.apps?125:85)<DESK_MAX_HEIGHT?
            s->damage.height-w->frame.y-(s->scene.apps?125:85):DESK_MAX_HEIGHT));
        focus(s,i);
        desk_log("deskd: opened pid=%llu window=%d\n",(unsigned long long)pid,i);
        return;
    }
    struct client *c=&s->clients[i]; struct desktop_window *w=&s->scene.windows[i];
    if (p->type==DESK_CLOSE) { forget(s,i); return; }
    if(p->type==DESK_NOTIFY) { notification_received(s,i,p);return; }
    if(p->type==DESK_MENU_SET) {
        if(s->scene.apps && desk_menu_receive(&c->menus,p) && s->scene.focus==i) global_menu_rebuild(s);
        return;
    }
    if(p->type==DESK_CLIPBOARD_SET || p->type==DESK_CLIPBOARD_GET) {
        if(s->scene.apps) desk_clipboard_request(&clipboard,pid,s->scene.focus==i,p,srv_ticks());
        return;
    }
    if(p->type==DESK_PREFERENCES_SET) {
        if(s->scene.apps && w->application==DESK_APP_SETTINGS && s->scene.focus==i) {
            struct desk_preferences value;memcpy(&value,p->text,sizeof(value));
            int changed=desktop_preferences_set(&s->preferences,&value,p->value,srv_ticks());
            if(changed>0) preferences_changed(s);else if(!changed) c->preferences_dirty=1;
        }
        return;
    }
    if(p->type==DESK_CANCEL_CLOSE) { c->close_requested=0;s->exiting=0;return; }
    if(p->type==DESK_TITLE) {
        memcpy(w->title,p->text,sizeof(p->text));w->title[sizeof(p->text)]=0;
        damage_window(s,i);
        global_menu_rebuild(s);
        if(s->scene.focus==i) desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
        return;
    }
    if(p->type==DESK_LAUNCH && s->scene.apps) {
        if(!p->serial) { c->launch_length=0;c->launch_application=p->value; }
        if(p->serial!=c->launch_length || p->height>sizeof(p->text) || !p->height ||
           p->height>sizeof(c->launch_path)-c->launch_length || p->value!=c->launch_application || p->width>1) {
            c->launch_length=0;return;
        }
        memcpy(c->launch_path+c->launch_length,p->text,p->height);c->launch_length+=p->height;
        if(p->width) {
            if(c->launch_path[c->launch_length-1]==0 && strlen(c->launch_path)==c->launch_length-1)
                spawn_app(s,c->launch_application,c->launch_path);
            c->launch_length=0;
        }
        return;
    }
    if (p->type!=DESK_COMMIT) return;
    /* A conforming client has at most one unreleased commit. */
    if (c->release.type) { close_client(s,i); return; }
    c->release=*p; c->release.type=DESK_RELEASE; c->progress=srv_ticks();
    if (p->generation!=c->generation || p->width!=(uint32_t)(w->frame.w-2) ||
        p->height!=(uint32_t)(w->frame.h-45) || c->closing) {
        desk_log("deskd: stale commit released pid=%llu\n",(unsigned long long)pid); return;
    }
    /* Keep displaying the front until a complete copy succeeds. */
    uint32_t *next=w->pixels==s->cache[i][0]?s->cache[i][1]:s->cache[i][0];
    if (!next || p->width>DESK_MAX_WIDTH || p->height>DESK_MAX_HEIGHT ||
        gui_surface_copy(p->surface,0,0,p->width,p->height,next,p->width)!=0) {
        close_client(s,i); return;
    }
    c->present_input=p->input_serial;
    int first_frame=w->pixels==NULL;
    w->pixels=next; w->width=p->width; w->height=p->height;
    c->awaiting=0;
    damage_window(s,i);
    if (s->trace && (!s->scene.apps || first_frame)) desk_log("deskd: commit pid=%llu generation=%llu size=%ux%u\n",
        (unsigned long long)pid,(unsigned long long)p->generation,p->width,p->height);
}
static void service(struct session *s) {
    for (int i=0;i<DESKD_WINDOWS;i++) {
        struct client *c=&s->clients[i];
        if (!c->pid) continue;
        if (!desk_process_alive(c->pid)) { forget(s,i); continue; }
        if ((c->release.type || c->configure || c->awaiting || c->closing) && (uint64_t)srv_ticks()-c->progress>500) {
            srv_kill((int64_t)c->pid); forget(s,i); continue;
        }
        if (c->release.type) {
            if (desk_send(c->pid,&c->release)!=0) continue;
            c->release.type=0; c->progress=srv_ticks();
        }
        if (c->input_count && (uint64_t)srv_ticks()-c->input_since>500) close_client(s,i);
        if (c->closing) {
            struct desk_packet p=desk_packet_new(DESK_CLOSE); desk_send(c->pid,&p);
        } else if (c->configure) {
            struct desk_packet p=desk_packet_new(DESK_CONFIGURE);
            p.generation=c->generation;
            p.width=s->scene.windows[i].frame.w-2; p.height=s->scene.windows[i].frame.h-45;
            if (desk_send(c->pid,&p)==0) c->configure=0;
        }
        if(c->notice_ack_pending && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_NOTIFY_ACK);p.serial=c->notice_ack_serial;p.value=c->notice_ack_result;
            if(!desk_send(c->pid,&p)) c->notice_ack_pending=0;
        }
        if(c->preferences_dirty && !c->configure && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_PREFERENCES);p.value=s->preferences.status;p.serial=s->preferences.revision;
            memcpy(p.text,&s->preferences.value,sizeof(s->preferences.value));
            if(!desk_send(c->pid,&p)) c->preferences_dirty=0;
        }
        if (c->input_count && !c->configure && !c->closing &&
            desk_send(c->pid,&c->input[c->input_head])==0) {
            c->input_head=(c->input_head+1)%DESK_INPUT_QUEUE; c->input_count--;
            c->input_since=srv_ticks();
        }
        if(s->scene.apps && !c->closing) desk_clipboard_service(&clipboard,c->pid,srv_ticks());
    }
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->children[i]) {
        uint64_t status;
        long waited=srv_wait(s->children[i],&status,SRV_WAIT_NOHANG);
        /* An interrupted wait is not proof of death. Init only adopts a
         * child after its parent exits; live clients retain their own results. */
        if (waited==(long)s->children[i] || (waited<0 && !desk_process_alive(s->children[i])))
            s->children[i]=0;
    }
}
static int hit(struct session *s,int x,int y) {
    int order[DESKD_WINDOWS];desktop_stack(&s->scene,order);
    for(int n=DESKD_WINDOWS-1;n>=0;n--) {
        int i=order[n];struct desktop_window *w=&s->scene.windows[i];
        if(!w->visible || w->minimized) continue;
        struct desk_rect r=w->frame;
        if(x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h) return i;
    }
    return -1;
}
static int global_click(struct session *s,int x,int y) {
    if(!s->scene.apps) return 0;
    if(desk_widget_contains(desktop_alert_rect(s->damage.width,s->scene.clock),x,y)) { notifications_open(s);return 1; }
    if(desk_widget_contains(desktop_clock_rect(s->damage.width,s->scene.clock),x,y)) { calendar_open(s);return 1; }
    for(int app=1;app<=DESK_APP_COUNT;app++) {
        struct desk_rect r=desktop_dock_item(s->damage.width,s->damage.height,app);
        if(x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h) {
            int found=-1;
            for(int i=0;i<DESKD_WINDOWS;i++) if(s->clients[i].pid && s->scene.windows[i].application==app &&
                (found<0 || s->scene.windows[i].z>s->scene.windows[found].z)) found=i;
            if(found>=0) focus(s,found);else spawn_app(s,app,"");
            return 1;
        }
    }
    return 0;
}
static void scroll_at(struct session *s,int x,int y,int wx,int wy) {
    int i=hit(s,x,y);
    if (i<0 || s->clients[i].closing) return;
    struct desktop_window *w=&s->scene.windows[i];
    if (y<w->frame.y+44 || x<=w->frame.x || x>=w->frame.x+w->frame.w-1 || y>=w->frame.y+w->frame.h-1) return;
    struct desk_packet p=desk_packet_new(DESK_SCROLL);
    p.x=clamp(wx,-16384,16384); p.y=clamp(wy,-16384,16384);
    p.width=(uint32_t)(x-w->frame.x-1);p.height=(uint32_t)(y-w->frame.y-44);
    queue_input(s,i,p);
}
static void pointer_at(struct session *s,int x,int y,unsigned buttons,int pressed) {
    if (s->pointer_blocked) {
        if (!(buttons&MOUSE_LEFT)) s->pointer_blocked=0;
        return;
    }
    if (pressed && !s->pointer_pid) {
        int i=hit(s,x,y);
        if (i<0 || s->clients[i].closing || s->clients[i].awaiting) return;
        struct desk_rect w=s->scene.windows[i].frame;
        if (x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1) return;
        s->pointer_pid=s->clients[i].pid;
    }
    if (!s->pointer_pid) return;
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->clients[i].pid==s->pointer_pid) {
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.x=x-s->scene.windows[i].frame.x-1; p.y=y-s->scene.windows[i].frame.y-44;
        p.generation=s->clients[i].generation;
        p.value=(buttons&MOUSE_LEFT?DESK_POINTER_LEFT:0)|(pressed?DESK_POINTER_PRESS:0);
        queue_input(s,i,p); break;
    }
    if (!(buttons&MOUSE_LEFT)) s->pointer_pid=0;
}
int main(int argc,char **argv) {
    if (argc==2 && strcmp(argv[1],"--license")==0) { srv_puts(desk_font_license); return 0; }
    int apps=argc==1 || (argc==2 && (!strcmp(argv[1],"--apps") || !strcmp(argv[1],"--apps-trace")));
    int scroll_smoke=argc==2 && strcmp(argv[1],"--scroll-smoke")==0,scroll_phase=0;
    int smoke=scroll_smoke || (argc==2 && strcmp(argv[1],"--smoke")==0);
    int lifecycle=argc==2 && strcmp(argv[1],"--lifecycle-smoke")==0,phase=0;
    int io_mode=argc==3 && strcmp(argv[1],"--io-test")==0 && desk_trial_run_valid(argv[2]);
    struct gfx_info info={0};
    if (gfx_info(&info)!=0 || info.width<640 || info.height<480 || info.width>3840 || info.height>2160) return 1;
    uint32_t *pixels=desk_pixels_alloc(info.width,info.height);
    struct desk_canvas canvas;
    if (!pixels || desk_canvas_init(&canvas,pixels,info.width,info.height,info.width)!=0) return 1;
    if (gui_register_server()!=0) { srv_puts("deskd: another display server is active\n"); return 1; }
    if (gfx_console_mute(1)!=0) return 1;
    struct session s={.scene={.focus=-1,.cursor_x=24,.cursor_y=56},
        .trace=lifecycle || (argc==2 && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--apps-trace")))};
    desktop_notice_view_init(&s.scene.notifications);desktop_notice_view_configure(&s.scene.notifications,info.width,info.height);
    s.scene.apps=apps;desk_menu_init(&s.scene.menu);desk_palette_init(&s.scene.launcher);
    desk_calendar_init(&s.scene.calendar);desk_ui_init(&s.scene.calendar_ui);
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_GRID,DESK_CONTROL_CALENDAR,&s.scene.calendar,NULL);
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_TODAY,DESK_CONTROL_BUTTON,&s.scene.calendar_today,"Today");
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_CLOSE,DESK_CONTROL_BUTTON,&s.scene.calendar_close,"Close");
    desk_ui_window_focus(&s.scene.calendar_ui,0);
    if(apps && gui_keyboard_mode(1)!=0) { gfx_console_mute(0);return 1; }
    if(cache_init(&s)!=0) { gfx_console_mute(0); desk_pixels_free(pixels,info.width,info.height); return 1; }
    s.scene.trackpad_trial=io_mode?3:argc==2 && strcmp(argv[1],"--gesture-test")==0?2:
        (argc==2 && strcmp(argv[1],"--trackpad-test")==0);
    desk_damage_init(&s.damage,info.width,info.height);
    if(apps) { desktop_preferences_init(&s.preferences);clock_update(&s,1); }
    global_menu_rebuild(&s);
    desk_damage_add(&s.damage,(struct desk_rect){0,0,info.width,info.height});
    if(apps) spawn_app(&s,DESK_APP_TEXT,"");else spawn_client(&s);
    if (io_mode) spawn_client(&s); /* Load both studies before storage starts. */
    int io_started=0,io_ready=0,stop_warning=0;
    uint64_t start=srv_ticks(),frames=0,bytes=0;
    int quit=0,failed=0,drag=-1,resize=-1,grabx=0,graby=0,pending_key=0,io_leaving=0;
    uint64_t key_pid=0,key_since=0,key_serial=0;
    uint8_t buttons=0;
    srv_puts("deskd: ready; independent clients, sealed surfaces\n");
    while (!quit) {
        io_trial_loop_begin(&s.io);
        for (int n=0;n<32;n++) {
            struct desk_packet p; uint64_t pid;
            int r=desk_receive(&pid,&p);
            if (r<=0) break;
            if (r==1) incoming(&s,pid,&p);
        }
        service(&s);
        if(apps) { if(desktop_preferences_poll(&s.preferences,srv_ticks())) preferences_changed(&s);clock_update(&s,0);notifications_tick(&s); }
        if (io_mode && !io_ready && s.scene.windows[0].pixels && s.scene.windows[1].pixels) {
            io_ready=1; srv_puts("deskd: trial ready; Space starts USB work\n");
        }
        if (io_mode && !io_ready && (uint64_t)srv_ticks()-start>500) { failed=1; break; }
        io_trial_poll(&s.io);
        if (io_leaving && s.io.finished) { quit=1; break; }
        if (scroll_smoke && s.scene.windows[0].pixels) {
            struct desk_rect r=s.scene.windows[0].frame;
            if (!scroll_phase) { scroll_at(&s,r.x+50,r.y+150,0,120); scroll_phase=1; }
            else if (scroll_phase==1 && (uint64_t)srv_ticks()-start>75) {
                scroll_at(&s,r.x+50,r.y+150,0,-24); scroll_phase=2;
            }
        }
        if (lifecycle) {
            if (phase==0 && s.scene.windows[0].pixels && (uint64_t)srv_ticks()-start>=100) {
                srv_puts("deskd: lifecycle killing client\n");
                srv_kill(s.clients[0].pid); phase=1;
            } else if (phase==1 && !s.children[0] && !s.clients[0].pid) {
                srv_puts("deskd: lifecycle replacing client\n"); spawn_client(&s); phase=2;
            } else if (phase==2 && s.scene.windows[0].pixels && (uint64_t)srv_ticks()-start>=200) {
                srv_puts("deskd: lifecycle abrupt server exit\n");
                srv_exit(0); /* Deliberately exercise kernel cleanup, not the normal exit path. */
            }
            if ((uint64_t)srv_ticks()-start>600) { failed=1; break; }
        }
        for (int n=0;!apps && n<32;n++) {
            int key=pending_key?pending_key:kbhit();
            if (key<=0) break;
            if (!pending_key) {
                if (io_mode && io_ready && !io_started && key==' ') {
                    io_trial_start(&s.io,argv[2]); io_started=1; continue;
                }
                if (key!=27) io_trial_input(&s.io,0);
                if (key==27) {
                    if (io_mode && io_started) {
                        if (!stop_warning && !s.io.finished && !s.io.stopping &&
                            s.io.progress.state!=DESK_IO_FAILED && io_trial_next_check(&s.io)) {
                            stop_warning=1;
                            srv_puts("deskd: coverage incomplete; Escape again saves an incomplete run\n");
                            continue;
                        }
                        io_trial_stop(&s.io); io_leaving=1; continue;
                    }
                    quit=1; break;
                }
                stop_warning=0;
                if (key==14) { spawn_client(&s); continue; }
                if (key==9) {
                    int other=1-s.scene.focus;
                    if (other>=0 && other<DESKD_WINDOWS && s.clients[other].pid) {
                        if (io_trial_active(&s.io)) s.io.focus_changes++;
                        focus(&s,other);
                    }
                    continue;
                }
                key_pid=s.scene.focus>=0?s.clients[s.scene.focus].pid:0;
                key_since=srv_ticks();
                key_serial=key_pid?io_trial_text_begin(&s.io,key_pid):0;
            }
            if (!key_pid || !desk_process_alive(key_pid)) { pending_key=0; continue; }
            struct desk_packet p=desk_packet_new(DESK_TEXT); p.value=key; p.input_serial=key_serial;
            if (desk_send(key_pid,&p)!=0) {
                pending_key=key;
                if ((uint64_t)srv_ticks()-key_since>500) { srv_kill(key_pid); pending_key=0; }
                break;
            }
            pending_key=0;
            if (io_trial_active(&s.io)) s.io.keys++;
        }
        for(int n=0;apps && n<32;n++) {
            struct srv_key_event key;int read=gui_keyboard_read(&key);
            if(read<=0) break;
            if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_CTRL) &&
                (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_ESCAPE) { quit=1;break; }
            if(notifications_shortcut(&key)) {
                drag=resize=-1;pointer_cancel(&s);if(s.scene.notifications.opened) notifications_close(&s);else notifications_open(&s);continue;
            }
            if((key.flags&SRV_KEY_DOWN_EVENT) && !(key.flags&SRV_KEY_REPEAT) && (key.modifiers&SRV_KEY_META) && key.usage==SRV_KEY_SPACE) {
                drag=resize=-1;pointer_cancel(&s);if(s.scene.launcher_open) launcher_close(&s);else launcher_open(&s);continue;
            }
            if((key.flags&SRV_KEY_DOWN_EVENT) && !(key.flags&SRV_KEY_REPEAT) &&
                !(key.modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META|SRV_KEY_SHIFT)) && key.usage==SRV_KEY_F1) {
                drag=resize=-1;pointer_cancel(&s);
                if(s.scene.launcher_open) launcher_close(&s);
                if(s.scene.calendar_open) calendar_close(&s);
                if(s.scene.notifications.opened) notifications_close(&s);
                desk_menu_cancel(&s.scene.menu);global_menu_damage(&s);global_menu_action(&s,GLOBAL_HELP);continue;
            }
            if(s.scene.notifications.opened && !((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_TAB)) {
                if((key.flags&SRV_KEY_DOWN_EVENT) && key.usage==SRV_KEY_ESCAPE) notifications_close(&s);
                else {
                    struct desk_action action=desk_ui_key(&s.scene.notifications.ui,&key);
                    if((key.flags&SRV_KEY_DOWN_EVENT) && key.usage==SRV_KEY_DELETE && !(key.modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) action=(struct desk_action){NOTICE_REMOVE,DESK_WIDGET_ACTIVATED};
                    if(action.flags) notifications_action(&s,action);
                }
                continue;
            }
            if(s.scene.calendar_open && !((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_TAB)) {
                if((key.flags&SRV_KEY_DOWN_EVENT) && key.usage==SRV_KEY_ESCAPE) calendar_close(&s);
                else calendar_action(&s,desk_ui_key(&s.scene.calendar_ui,&key));
                continue;
            }
            if(s.scene.launcher_open && !((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_TAB)) {
                launcher_result(&s,desk_palette_key(&s.scene.launcher,&key));continue;
            }
            if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_TAB) {
                if(s.scene.launcher_open) launcher_close(&s);
                if(s.scene.calendar_open) calendar_close(&s);
                if(s.scene.notifications.opened) notifications_close(&s);
                cycle_focus(&s,(key.modifiers&SRV_KEY_SHIFT)!=0);continue;
            }
            struct desk_menu_result menu_result=desk_menu_key(&s.scene.menu,&key);
            if(menu_result.consumed) { drag=resize=-1;pointer_cancel(&s);global_menu_result(&s,menu_result);if(!(key.flags&SRV_KEY_CANCEL)) continue; }
            if(s.scene.focus>=0) {
                struct desk_packet p=desk_packet_new(DESK_KEY);memcpy(p.text,&key,sizeof(key));
                queue_input(&s,s.scene.focus,p);
            }
        }
        if(apps && s.exiting) {
            int live=0;for(int i=0;i<DESKD_WINDOWS;i++) live|=s.clients[i].pid!=0;
            if(!live) {
                if(!s.exit_wait) s.exit_wait=srv_ticks();
                int saving=s.preferences.job.busy || (s.preferences.writable && s.preferences.dirty);
                if(!saving || (uint64_t)srv_ticks()-s.exit_wait>1000) { quit=1;break; }
                if(!s.scene.notice[0]) { strcpy(s.scene.notice,"Finishing saved preferences…");desk_damage_add(&s.damage,(struct desk_rect){0,0,s.damage.width,s.damage.height}); }
            }
        }
        struct mouse_event e;
        for (int n=0;n<32 && mouse_scan(&e)>0;n++) {
            if (e.dx || e.dy || e.wheel_x || e.wheel_y || e.buttons!=buttons) {
                stop_warning=0; io_trial_input(&s.io,1);
            }
            if (io_trial_active(&s.io)) {
                if (e.dx || e.dy) s.io.motions++;
                if (e.wheel_x || e.wheel_y) s.io.wheels++;
            }
            desk_damage_add(&s.damage,(struct desk_rect){s.scene.cursor_x,s.scene.cursor_y,10,17});
            int x=s.scene.cursor_x=clamp((int64_t)s.scene.cursor_x+e.dx,0,canvas.width-1);
            int y=s.scene.cursor_y=clamp((int64_t)s.scene.cursor_y+e.dy,0,canvas.height-1);
            if (s.trace && !apps) desk_log("deskd: pointer x=%d y=%d buttons=%u\n",x,y,(unsigned)e.buttons);
            if(apps && s.scene.notifications.opened) {
                drag=resize=-1;s.overlay_down=(e.buttons&MOUSE_LEFT)!=0;
                struct desk_widget_event event={.type=e.flags&MOUSE_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,.x=x,.y=y,.down=s.overlay_down,.pressed=s.overlay_down && !(buttons&MOUSE_LEFT)};
                if(event.type==DESK_WIDGET_POINTER && event.pressed && !desk_widget_contains(s.scene.notifications.frame,x,y)) notifications_close(&s);
                else {
                    if(e.wheel_y && !e.buttons) notifications_action(&s,desk_ui_scroll(&s.scene.notifications.ui,x,y,0,e.wheel_y));
                    notifications_action(&s,desk_ui_pointer(&s.scene.notifications.ui,event,srv_monotonic_us()));
                }
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if(apps && notifications_toast_visible(&s) && drag<0 && resize<0 && !s.pointer_pid && !s.pointer_blocked &&
                (s.scene.notifications.toast_ui.capture>=0 || desk_widget_contains(s.scene.notifications.toast_frame,x,y))) {
                struct desk_widget_event event={.type=e.flags&MOUSE_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,.x=x,.y=y,.down=(e.buttons&MOUSE_LEFT)!=0,.pressed=(e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)};
                struct desk_action action=desk_ui_pointer(&s.scene.notifications.toast_ui,event,srv_monotonic_us());
                if(action.flags) notifications_apply(&s,desktop_notice_toast_action(&s.scene.notifications,action));
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if(apps && s.scene.calendar_open) {
                drag=resize=-1;s.overlay_down=(e.buttons&MOUSE_LEFT)!=0;
                struct desk_widget_event event={.type=e.flags&MOUSE_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=x,.y=y,.down=s.overlay_down,.pressed=s.overlay_down && !(buttons&MOUSE_LEFT)};
                if(event.type==DESK_WIDGET_POINTER && event.pressed && !desk_widget_contains(s.scene.calendar_frame,x,y)) calendar_close(&s);
                else calendar_action(&s,desk_ui_pointer(&s.scene.calendar_ui,event,srv_monotonic_us()));
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if(apps && s.scene.launcher_open) {
                drag=resize=-1;s.overlay_down=(e.buttons&MOUSE_LEFT)!=0;
                struct desk_widget_event event={.type=e.flags&MOUSE_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                    .x=x,.y=y,.down=s.overlay_down,.pressed=s.overlay_down && !(buttons&MOUSE_LEFT)};
                if(e.wheel_y && !e.buttons) launcher_result(&s,desk_palette_scroll(&s.scene.launcher,x,y,e.wheel_y));
                launcher_result(&s,desk_palette_pointer(&s.scene.launcher,event,srv_monotonic_us()));
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if (e.flags&MOUSE_CANCEL) {
                drag=resize=-1; buttons=e.buttons;
                cancel_input(&s,e.buttons);
                if(apps) global_menu_result(&s,desk_menu_pointer(&s.scene.menu,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL}));
            }
            if(apps && drag<0 && resize<0 && !s.pointer_pid && !(e.flags&MOUSE_CANCEL)) {
                struct desk_menu_result result=desk_menu_pointer(&s.scene.menu,(struct desk_widget_event){
                    .type=DESK_WIDGET_POINTER,.x=x,.y=y,.down=(e.buttons&MOUSE_LEFT)!=0,
                    .pressed=(e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)});
                global_menu_result(&s,result);
                if(result.consumed) {
                    s.pointer_blocked=(e.buttons&MOUSE_LEFT)!=0;buttons=e.buttons;
                    desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
                }
            }
            if ((e.wheel_x || e.wheel_y) && drag<0 && resize<0 && !e.buttons && !s.pointer_pid)
                scroll_at(&s,x,y,e.wheel_x,e.wheel_y);
            if ((e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)) {
                if(global_click(&s,x,y)) {
                    pointer_cancel(&s);s.pointer_blocked=1;buttons=e.buttons;
                    desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
                }
                int i=hit(&s,x,y);
                if (i>=0) {
                    if (s.scene.focus!=i && io_trial_active(&s.io)) s.io.focus_changes++;
                    focus(&s,i); struct desk_rect w=s.scene.windows[i].frame;
                    if (x>=w.x+w.w-40 && y<w.y+43) request_close(&s,i);
                    else if (apps && x>=w.x+w.w-72 && y<w.y+43) { maximize(&s,i);s.pointer_blocked=1; }
                    else if (apps && x>=w.x+w.w-104 && y<w.y+43) { minimize(&s,i);s.pointer_blocked=1; }
                    else if (!s.scene.windows[i].maximized && x>=w.x+w.w-24 && y>=w.y+w.h-24) resize=i;
                    else if (!s.scene.windows[i].maximized && y<w.y+43) { drag=i; grabx=x-w.x; graby=y-w.y; }
                }
            }
            if (!(e.buttons&MOUSE_LEFT)) drag=resize=-1;
            if (drag>=0 && s.clients[drag].pid) {
                struct desktop_window *w=&s.scene.windows[drag]; damage_window(&s,drag);
                w->frame.x=clamp(x-grabx,0,canvas.width-w->frame.w-6);
                w->frame.y=clamp(y-graby,44,canvas.height-w->frame.h-(s.scene.apps?80:36)); damage_window(&s,drag);
            }
            if (resize>=0 && s.clients[resize].pid) {
                struct desk_rect w=s.scene.windows[resize].frame;
                int width=clamp(x-w.x-1,280,clamp(canvas.width-w.x-8,280,DESK_MAX_WIDTH));
                int height=clamp(y-w.y-44,240,clamp(canvas.height-w.y-(s.scene.apps?125:85),240,DESK_MAX_HEIGHT));
                if (w.w!=width+2 || w.h!=height+45) configure(&s,resize,width,height);
            }
            if (drag<0 && resize<0 && !(e.flags&MOUSE_CANCEL))
                pointer_at(&s,x,y,e.buttons,(e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT));
            buttons=e.buttons;
            desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});
        }
        if (io_mode) {
            const char *next=!io_started? (io_ready?"READY - press Space when you are ready to start the USB test.":"Loading both studies..."):stop_warning?
                "Checks incomplete. Continue below, or press Escape again to save an incomplete run.":
                io_trial_next_check(&s.io);
            if(!next) next="Coverage complete. Press Escape for save checks; Ubuntu will verify timing and data.";
            if(strcmp(next,s.scene.io_next)) {
                strcpy(s.scene.io_next,next);
                desk_damage_add(&s.damage,(struct desk_rect){24,canvas.height-269+12,canvas.width-48,24});
            }
            char status[160]; const char *states[]={"STARTING","RUNNING","COMPLETE","STOPPED","FAILED"};
            snprintf(status,sizeof(status),"USB %s%s | rounds %u/128 | keys %llu | moves %llu | scroll %llu | focus %llu | cancel %llu",
                (!io_started && io_ready)?"WAITING FOR SPACE":states[s.io.progress.state],s.io.stopping && !s.io.finished?" (stopping)":"",
                s.io.progress.rounds,(unsigned long long)s.io.keys,(unsigned long long)s.io.motions,
                (unsigned long long)s.io.wheels,(unsigned long long)s.io.focus_changes,(unsigned long long)s.io.cancels);
            char checks[160];
            snprintf(checks,sizeof(checks),"Scroll: %s | Tab focus twice: %s | Hold thumb + Tab: %s | active: %llu/100 | text: %llu/32",
                s.io.wheels?"OK":"WAIT",s.io.focus_changes>=2?"OK":"WAIT",s.io.cancels?"OK":"WAIT",
                (unsigned long long)s.io.active_gap.count,(unsigned long long)s.io.text_present.count);
            if(strcmp(checks,s.scene.io_check)) {
                strcpy(s.scene.io_check,checks);
                desk_damage_add(&s.damage,(struct desk_rect){24,canvas.height-269+12+24*7,canvas.width-48,24});
            }
            if (strcmp(status,s.scene.io_status)) {
                strcpy(s.scene.io_status,status);
                desk_damage_add(&s.damage,(struct desk_rect){0,canvas.height-36,canvas.width,36});
            }
        }
        long render_start=io_mode && s.damage.count?srv_monotonic_us():0;
        for (unsigned n=0;n<s.damage.count;n++) {
            struct desk_rect r=s.damage.rects[n]; desk_clip(&canvas,r); desktop_draw(&canvas,&s.scene);
            if (blitrect(r.x,r.y,r.w,r.h,pixels+(size_t)r.y*canvas.stride+r.x,canvas.stride)!=0) failed=1;
            bytes+=(uint64_t)r.w*r.h*4;
        }
        if (!failed && s.damage.count) for (int i=0;i<DESKD_WINDOWS;i++)
            io_trial_text_present(&s.io,s.clients[i].pid,s.clients[i].present_input);
        io_trial_loop_end(&s.io,render_start>0?(uint64_t)render_start:0);
        if (s.damage.count) frames++;
        s.damage.count=0;
        if (failed || (smoke && (uint64_t)srv_ticks()-start>=200)) break;
        srv_sleep_ticks(1);
    }
    if (io_trial_finish(&s.io)!=0) failed=1;
    /* Bound shutdown even when a client stopped reading. Only kill children
     * launched by this session; all queues/surfaces also have kernel cleanup. */
    for (int i=0;i<DESKD_WINDOWS;i++) if (s.clients[i].pid) close_client(&s,i);
    uint64_t until=(uint64_t)srv_ticks()+30;
    while ((uint64_t)srv_ticks()<until) { service(&s); srv_sleep_ticks(1); }
    for (int i=0;i<DESKD_WINDOWS;i++) {
        if (s.children[i]) { uint64_t status; srv_kill(s.children[i]); srv_wait(s.children[i],&status,0); }
        if (s.clients[i].pid) forget(&s,i);
    }
    if(apps) {
        if(s.preferences.dirty) desk_log("deskd: preferences were not fully saved (status=%u)\n",s.preferences.status);
        desktop_preferences_close(&s.preferences);
    }
    cache_free(&s);
    desk_pixels_free(pixels,info.width,info.height); gfx_console_mute(0);
    desk_log("deskd: %s frames=%llu fb_bytes=%llu\n",failed?"FAIL":"exited",
        (unsigned long long)frames,(unsigned long long)bytes);
    return failed?1:0;
}
