#include <srvros/desk_battery.h>
#include <srvros/desk_log.h>
#include <srvros/desk_paths.h>
#include <srvros/desktop_apps.h>
#include "scene.h"
#include "clipboard.h"
#include "menus.h"
#include "preferences.h"
#include <srvros/desk_text.h>
#include "io_trial.h"
#include "input_timing.h"
#include <srvros/desk_capture_job.h>
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
#define DESK_INPUT_BATCH 8

struct client {
    uint64_t pid,generation,progress,present_input;
    int configure,awaiting,closing,close_requested,preferences_dirty;
    struct desk_packet release,brightness_reply,audio_reply;
    struct desk_packet input[DESK_INPUT_QUEUE];
    unsigned input_head,input_count,input_dropped;
    uint64_t input_since;
    int input_blocked;
    uint64_t control_since;
    int control_blocked,close_sent,pointer_cancel_pending;
    struct desk_menu_store menus;
    char launch_path[160];
    unsigned launch_length,launch_application;
    uint64_t notice_last,notice_at,notice_ack_serial;
    unsigned notice_last_result,notice_ack_result;
    int notice_ack_pending,notice_has_time;
};
struct session {
    const char *home;
    struct desktop_scene scene;
    struct desk_damage damage;
    struct desk_rect menu_visible;
    uint64_t menu_owner;
    struct client clients[DESKD_WINDOWS];
    uint64_t children[DESKD_WINDOWS];
    int child_apps[DESKD_WINDOWS],exiting,ram_text;
    uint64_t battery_tick,battery_reads,battery_first,battery_last;
    struct desktop_preferences_store preferences;
    uint64_t clock_tick,exit_wait;
    int trace,pointer_blocked,overlay_return,overlay_down;
    uint64_t pointer_pid;
    int notice_visible,context_down;
    unsigned child_reaped,child_errors;
    unsigned modifiers;
    struct io_trial io;
    struct desk_capture_job capture;
    int capture_pending,capture_success;
    unsigned capture_finished;
    uint64_t capture_serial;
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
static void context_close(struct session *s);
static void capture_request(struct session *s);
static int disposable_input(const struct desk_packet *p) {
    return p->type==DESK_SCROLL || (p->type==DESK_POINTER &&
        (p->value==DESK_POINTER_HOVER || p->value==DESK_POINTER_LEFT));
}
static void queue_input(struct session *s,int i,struct desk_packet p) {
    struct client *c=&s->clients[i];
    if (c->closing) return;
    if (c->input_count) {
        struct desk_packet *last=&c->input[(c->input_head+c->input_count-1)%DESK_INPUT_QUEUE];
        if (p.type==DESK_POINTER && (p.value==DESK_POINTER_LEFT || p.value==DESK_POINTER_HOVER) &&
            last->type==p.type && last->value==p.value && last->generation==p.generation) {
            *last=p; return;
        }
        if (p.type==DESK_SCROLL && last->type==p.type &&
            last->generation==p.generation && last->width==p.width && last->height==p.height &&
            (!p.x || !last->x || (p.x>0)==(last->x>0)) &&
            (!p.y || !last->y || (p.y>0)==(last->y>0))) {
            last->x=clamp((int64_t)last->x+p.x,-16384,16384);
            last->y=clamp((int64_t)last->y+p.y,-16384,16384); return;
        }
    }
    if (c->input_count==DESK_INPUT_QUEUE) {
        /* Slow drawing is not a reason to close a document. Bound replaceable
         * motion/scroll traffic; preserve ordered key, button and focus edges. */
        unsigned discard=DESK_INPUT_QUEUE;
        if (!disposable_input(&p)) {
            for (unsigned n=0;n<c->input_count;n++)
                if(disposable_input(&c->input[(c->input_head+n)%DESK_INPUT_QUEUE])) { discard=n;break; }
        }
        if (disposable_input(&p) || discard<DESK_INPUT_QUEUE) {
            if (!c->input_dropped++) desk_log("deskd: input pressure pid=%llu; dropping motion, keeping app open\n",(unsigned long long)c->pid);
            if (disposable_input(&p)) return;
            for(unsigned n=discard;n+1<c->input_count;n++)
                c->input[(c->input_head+n)%DESK_INPUT_QUEUE]=c->input[(c->input_head+n+1)%DESK_INPUT_QUEUE];
            c->input_count--;
        } else {
            desk_log("deskd: closing pid=%llu reason=ordered-input-overflow\n",(unsigned long long)c->pid);
            close_client(s,i); return;
        }
    }
    if (!c->input_count) { c->input_since=srv_ticks(); c->input_blocked=0; }
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
        /* Drop uncertain pointer actions, retaining ordered text/keys and other
         * input. Keep cancellation outside the bounded queue so a full keyboard
         * queue can still be canceled without data loss or closing the client. */
        struct client *c=&s->clients[i];
        unsigned kept=0;
        for(unsigned n=0;n<c->input_count;n++) {
            struct desk_packet p=c->input[(c->input_head+n)%DESK_INPUT_QUEUE];
            if(p.type==DESK_POINTER || p.type==DESK_SCROLL) continue;
            c->input[(c->input_head+kept++)%DESK_INPUT_QUEUE]=p;
        }
        c->input_count=kept;c->input_blocked=0;c->pointer_cancel_pending=1;
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
        if((s->scene.apps || s->ram_text) && previous>=0 && s->clients[previous].pid) {
            struct desk_packet p=desk_packet_new(DESK_FOCUS);p.value=0;queue_input(s,previous,p);
        }
    }
    if (previous>=0) damage_window(s,previous);
    s->scene.focus=i;
    if (i>=0) {
        s->scene.windows[i].minimized=0;
        s->scene.windows[i].z=++s->scene.stack_serial;
        damage_window(s,i);
        if((s->scene.apps || s->ram_text) && previous!=i) {
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
    desk_log("deskd: retired pid=%llu\n",(unsigned long long)s->clients[i].pid);
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
        int width=clamp(s->damage.width-18,280,DESK_MAX_WIDTH);
        int height=clamp(s->damage.height-177,240,DESK_MAX_HEIGHT);
        w->frame.x=(s->damage.width-width-2)/2;w->frame.y=52;
        configure(s,i,width,height);w->maximized=1;
    }
    global_menu_rebuild(s);
    if(s->trace) desk_log("deskd: zoom pid=%llu maximized=%d frame=%d,%d,%d,%d\n",(unsigned long long)s->clients[i].pid,w->maximized,w->frame.x,w->frame.y,w->frame.w,w->frame.h);
}
static void arrange_window(struct session *s,int i,unsigned placement) {
    struct desktop_window *w=&s->scene.windows[i];struct desk_rect target;
    struct desk_rect original=w->maximized?w->restore:w->frame;
    if(!desktop_window_placement(s->damage.width,s->damage.height,original,placement,&target)) return;
    if(s->pointer_pid==s->clients[i].pid) pointer_cancel(s);
    damage_window(s,i);
    if(!w->maximized) w->restore=w->frame;
    int resize=w->frame.w!=target.w || w->frame.h!=target.h;
    w->frame.x=target.x;w->frame.y=target.y;w->maximized=placement!=DESKTOP_CENTER;
    if(resize) configure(s,i,target.w-2,target.h-45);else damage_window(s,i);
    global_menu_rebuild(s);
    if(s->trace) desk_log("deskd: arranged pid=%llu mode=%u frame=%d,%d,%d,%d generation=%llu\n",
        (unsigned long long)s->clients[i].pid,placement,w->frame.x,w->frame.y,w->frame.w,w->frame.h,(unsigned long long)s->clients[i].generation);
}
static void spawn_client(struct session *s) {
    for (int i=0;i<DESKD_WINDOWS;i++) if (!s->children[i]) {
        long pid=spawn_bg("/fat/bin/deskclient");
        if (pid>0) s->children[i]=(uint64_t)pid;
        else srv_puts("deskd: launch failed\n");
        return;
    }
}
/* Diagnostic Text uses either RAM only or its normal asynchronous file worker. */
static void spawn_capture_text(struct session *s,int files) {
    char *argv[]={"/fat/bin/desktools","text",files?"--coexist-files":"--coexist-capture",NULL};
    char *env[]={"HOME=" DESK_RAM_HOME,"SRVROS_DESKTOP_HOME=" DESK_RAM_HOME,NULL};
    struct srv_exec_request request={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    long pid=srv_exec(&request);
    if(pid>0) { s->children[0]=pid;s->child_apps[0]=DESK_APP_TEXT; }
}
static struct srv_key_event capture_key(unsigned step) {
    /* Twelve ordered editing operations, repeated five times; each ends in AA.
     * Clipboard operations retain the normal asynchronous IPC path. */
    static const struct {unsigned usage,modifiers,codepoint;} script[]={
        {4,SRV_KEY_CTRL,0},{SRV_KEY_BACKSPACE,0,0},{4,SRV_KEY_SHIFT,'A'},
        {5,0,'b'},{SRV_KEY_LEFT,0,0},{SRV_KEY_DELETE,0,0},
        {SRV_KEY_END,0,0},{4,SRV_KEY_CTRL,0},{6,SRV_KEY_CTRL,0},
        {SRV_KEY_END,0,0},{25,SRV_KEY_CTRL,0},{SRV_KEY_RIGHT,0,0}};
    unsigned i=step%12;
    return (struct srv_key_event){.abi_version=SRV_KEY_EVENT_VERSION,.struct_size=sizeof(struct srv_key_event),.usage=script[i].usage,.modifiers=script[i].modifiers,
        .codepoint=script[i].codepoint,.flags=SRV_KEY_DOWN_EVENT};
}
static void spawn_app(struct session *s,int application,const char *path) {
    const struct desk_application_info *info=desk_application_info(application);
    if(!info) return;
    for(int i=0;i<DESKD_WINDOWS;i++) if(!s->children[i]) {
        char *argv[]={(char *)info->program,(char *)info->role,(char *)(path?path:""),NULL};
        char home[192],desktop_home[192];
        snprintf(home,sizeof(home),"HOME=%s",s->home?s->home:DESK_RAM_HOME);
        snprintf(desktop_home,sizeof(desktop_home),"SRVROS_DESKTOP_HOME=%s",s->home?s->home:DESK_RAM_HOME);
        char *env[]={"PATH=/fat/bin:/:/fat",home,desktop_home,NULL};
        struct srv_exec_request request={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        long pid=srv_exec(&request);
        if(pid>0) { s->children[i]=pid;s->child_apps[i]=application;s->scene.notice[0]=0; }
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
    desk_log("deskd: close-request pid=%llu tick=%llu\n",(unsigned long long)s->clients[i].pid,(unsigned long long)srv_ticks());
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
    GLOBAL_MINIMIZE,GLOBAL_ZOOM,GLOBAL_NEXT,GLOBAL_LAUNCHER,GLOBAL_CALENDAR,GLOBAL_HELP,GLOBAL_NOTIFICATIONS,GLOBAL_FILES,GLOBAL_SYSTEM,GLOBAL_CAPTURE,GLOBAL_FIND,GLOBAL_TILE_LEFT,GLOBAL_TILE_RIGHT,GLOBAL_CENTER,GLOBAL_WINDOW=65792 };
static unsigned arrange_shortcut(const struct srv_key_event *key) {
    if(!(key->flags&SRV_KEY_DOWN_EVENT) || (key->flags&SRV_KEY_REPEAT) ||
       !(key->modifiers&SRV_KEY_META) || !(key->modifiers&SRV_KEY_ALT) || (key->modifiers&(SRV_KEY_CTRL|SRV_KEY_SHIFT))) return 0;
    return key->usage==SRV_KEY_LEFT?GLOBAL_TILE_LEFT:key->usage==SRV_KEY_RIGHT?GLOBAL_TILE_RIGHT:
        key->usage==SRV_KEY_UP?GLOBAL_CENTER:key->usage==SRV_KEY_DOWN?GLOBAL_ZOOM:0;
}
static void global_menu_damage(struct session *s) {
    desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
    if(s->menu_visible.w>0) desk_damage_add(&s->damage,s->menu_visible);
    s->menu_visible=desk_menu_popup(&s->scene.menu);
    if(s->menu_visible.w>0) {
        s->menu_visible.w+=3;s->menu_visible.h+=4;desk_damage_add(&s->damage,s->menu_visible);
    }
}
static void battery_update(struct session *s,int force) {
    uint64_t now=srv_ticks();
    if(!force && now-s->battery_tick<100)return;
    s->battery_tick=now;
    struct srv_battery_status value={0};
    long result=srv_battery_status(&value);
    struct desk_battery_view view=desk_battery_view(result?NULL:&value);
    if(view.fresh) {
        s->battery_reads++;
        if(!s->battery_first)s->battery_first=value.generation;
        s->battery_last=value.generation;
    }
    if(strcmp(view.label,s->scene.battery)) {
        strcpy(s->scene.battery,view.label);
        desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
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
        int reserve=s->damage.width<1000?428:578;
        desk_damage_add(&s->damage,(struct desk_rect){s->damage.width-reserve,0,reserve,44});
    }
}
static void pointer_preferences(struct session *s) {
    const struct desk_preferences *p=&s->preferences.value;
    gui_pointer_config(p->pointer_speed,(p->pointer_acceleration?SRV_POINTER_ACCELERATION:0)|
        (p->disable_while_typing?SRV_POINTER_DISABLE_WHILE_TYPING:0)|
        (p->tap_to_click?SRV_POINTER_TAP_CLICK:0)|(p->two_finger_tap?SRV_POINTER_TWO_FINGER_TAP:0));
}
static void preferences_changed(struct session *s) {
    pointer_preferences(s);
    if(s->scene.wallpaper!=(int)s->preferences.value.wallpaper) {
        s->scene.wallpaper=s->preferences.value.wallpaper;
        desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,s->damage.height});
    }
    clock_update(s,1);
    for(int n=0;n<DESKD_WINDOWS;n++) if(s->clients[n].pid) s->clients[n].preferences_dirty=1;
    if(s->trace) desk_log("deskd: preferences revision=%llu status=%u text=%u offset=%d flags=%u wallpaper=%u pointer_speed=%u acceleration=%u disable_while_typing=%u tap_to_click=%u two_finger_tap=%u\n",
        (unsigned long long)s->preferences.revision,s->preferences.status,s->preferences.value.text_size,
        s->preferences.value.utc_offset_minutes,s->preferences.value.clock_flags,s->preferences.value.wallpaper,
        s->preferences.value.pointer_speed,s->preferences.value.pointer_acceleration,s->preferences.value.disable_while_typing,
        s->preferences.value.tap_to_click,s->preferences.value.two_finger_tap);
}
static void global_menu_rebuild(struct session *s) {
    if(!s->scene.apps) return;
    const struct desk_menu_item system_items[]={
        {0,DESK_MENU_HEADING,"SrvrOS",""},{GLOBAL_ABOUT,0,"About SrvrOS",""},
        {GLOBAL_SETTINGS,0,"Settings",""},{GLOBAL_LAUNCHER,0,"Open application or window…","Cmd-Space"},
        {GLOBAL_FIND,0,"Find Files…","Cmd-Shift-F"},
        {GLOBAL_CALENDAR,0,"Calendar",""},{GLOBAL_TEXT,0,"New Text document",""},
        {GLOBAL_TERMINAL,0,"New Terminal",""},{GLOBAL_HELP,0,"SrvrOS Help","F1"},
        {GLOBAL_NOTIFICATIONS,0,"Notifications","Cmd-Shift-N"},
        {GLOBAL_CAPTURE,s->capture.busy || s->capture_pending?DESK_MENU_DISABLED:0,"Save screenshot","Cmd-Shift-3"},
        {0,DESK_MENU_SEPARATOR,"",""},
        {GLOBAL_EXIT,0,"Return to console",""}
    };
    struct desk_menu_item items[DESK_MENU_ITEMS]={0};
    unsigned count=sizeof(system_items)/sizeof(*system_items);
    memcpy(items,system_items,sizeof(system_items));
    if(s->scene.focus>=0) {
        struct desk_menu_store *m=&s->clients[s->scene.focus].menus;
        memcpy(items+count,m->items,m->count*sizeof(items[0]));count+=m->count;
    }
    unsigned disabled=s->scene.focus<0?DESK_MENU_DISABLED:0;
    struct desk_rect unused,original={0};if(s->scene.focus>=0) original=s->scene.windows[s->scene.focus].frame;
    unsigned tile_disabled=disabled|(!desktop_window_placement(s->damage.width,s->damage.height,original,DESKTOP_TILE_LEFT,&unused)?DESK_MENU_DISABLED:0);
    items[count++]=(struct desk_menu_item){0,DESK_MENU_HEADING,"Arrange",""};
    items[count++]=(struct desk_menu_item){GLOBAL_TILE_LEFT,tile_disabled,"Tile left","Cmd-Alt-Left"};
    items[count++]=(struct desk_menu_item){GLOBAL_TILE_RIGHT,tile_disabled,"Tile right","Cmd-Alt-Right"};
    items[count++]=(struct desk_menu_item){GLOBAL_CENTER,disabled,"Center","Cmd-Alt-Up"};
    items[count++]=(struct desk_menu_item){0,DESK_MENU_HEADING,"Window",""};
    items[count++]=(struct desk_menu_item){GLOBAL_MINIMIZE,disabled,"Minimize",""};
    items[count]=(struct desk_menu_item){GLOBAL_ZOOM,disabled,"Maximize","Cmd-Alt-Down"};
    if(s->scene.focus>=0 && s->scene.windows[s->scene.focus].maximized) strcpy(items[count].label,"Restore size");
    count++;
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
    else desk_menu_configure(&s->scene.menu,items,count,(struct desk_rect){10,0,s->damage.width-(s->damage.width<1000?428:578),44},
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
    else if(id==GLOBAL_CAPTURE) capture_request(s);
    else if(id==GLOBAL_FIND) spawn_app(s,DESK_APP_FIND,"");
    else if(id==GLOBAL_FILES) spawn_app(s,DESK_APP_FILES,"");
    else if(id==GLOBAL_SYSTEM) spawn_app(s,DESK_APP_SYSTEM,"");
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
    else if(s->scene.focus>=0 && (id==GLOBAL_TILE_LEFT || id==GLOBAL_TILE_RIGHT || id==GLOBAL_CENTER))
        arrange_window(s,s->scene.focus,id==GLOBAL_TILE_LEFT?DESKTOP_TILE_LEFT:id==GLOBAL_TILE_RIGHT?DESKTOP_TILE_RIGHT:DESKTOP_CENTER);
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
    context_close(s);
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
    context_close(s);
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
    context_close(s);
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
        if(owner>=0 && !result.target[0]) focus(s,owner);else spawn_app(s,result.application,result.target);
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
    return s->scene.notifications.toast_id && !s->scene.notifications.opened && !s->scene.launcher_open && !s->scene.calendar_open && s->scene.menu.open<0 && !desk_popup_active(&s->scene.context);
}
static void notifications_tick(struct session *s) {
    long us=srv_monotonic_us();int changed=desktop_notice_view_tick(&s->scene.notifications,us<0?0:(uint64_t)us);
    int visible=notifications_toast_visible(s);
    if(changed || visible!=s->notice_visible) notifications_damage(s);
    s->notice_visible=visible;desk_ui_window_focus(&s->scene.notifications.toast_ui,visible);
}
static int capture_shortcut(const struct srv_key_event *key) {
    return (key->flags&SRV_KEY_DOWN_EVENT) && !(key->flags&SRV_KEY_REPEAT) &&
        (key->modifiers&SRV_KEY_META) && (key->modifiers&SRV_KEY_SHIFT) &&
        !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT)) && key->usage==32;
}
static void capture_notice(struct session *s,int success,const char *message) {
    struct desk_notification text={0};strcpy(text.title,success?"Screenshot saved":"Screenshot not saved");
    snprintf(text.message,sizeof(text.message),"%s",message);
    uint64_t id;long us=srv_monotonic_us();
    if(desktop_notification_add(&s->scene.notifications.store,UINT64_MAX,DESK_APP_FILES,++s->capture_serial,
        us<0?0:(uint64_t)us,success?DESK_NOTICE_SUCCESS:DESK_NOTICE_WARNING,&text,&id)>0) {
        if(success) { char folder[160];snprintf(folder,sizeof(folder),"%s/Screenshots",s->home?s->home:DESK_RAM_HOME);
            desktop_notification_target(&s->scene.notifications.store,id,folder); }
        desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);notifications_damage(s);
    }
}
static void capture_request(struct session *s) {
    if(s->exiting || s->capture.busy || s->capture_pending) return;
    if(!desk_capture_bytes(s->damage.width,s->damage.height)) {
        capture_notice(s,0,"This display exceeds the 8 MiB capture limit.");return;
    }
    s->capture_pending=1;
}
static void capture_tick(struct session *s,const uint32_t *pixels) {
    int result=desk_capture_job_poll(&s->capture);
    if(result) {
        int success=result>0 && !s->capture.response.status;
        s->capture_success=success;s->capture_finished++;
        const char *name=strrchr(s->capture.request.path,'/');
        capture_notice(s,success,success?(name?name+1:s->capture.request.path):"Save or verification failed. Check Files.");
        desk_log("deskd: screenshot %s status=%u path=%s\n",success?"saved":"failed",s->capture.response.status,s->capture.request.path);
        global_menu_rebuild(s);
    }
    if(!s->capture_pending) return;
    s->capture_pending=0;
    char path[DESK_CAPTURE_PATH],date[32]="undated";long us=srv_realtime_us();
    time_t seconds=us<0?0:us/1000000;struct tm utc;
    if(us>=0 && gmtime_r(&seconds,&utc)) strftime(date,sizeof(date),"%Y%m%d-%H%M%S",&utc);
    snprintf(path,sizeof(path),"%s/Screenshots/Screen-%s-%llu-%u.bmp",s->home?s->home:DESK_RAM_HOME,date,(unsigned long long)srv_getpid(),s->capture.next_id+1);
    if(desk_capture_job_start(&s->capture,pixels,s->damage.width,s->damage.height,path)) {
        s->capture_success=0;s->capture_finished++;capture_notice(s,0,"The capture worker could not start.");
    }
    else if(s->trace) desk_log("deskd: screenshot captured path=%s width=%d height=%d\n",path,s->damage.width,s->damage.height);
    global_menu_rebuild(s);
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
        s->clients[i].close_sent=0;
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
        if(s->scene.apps || s->ram_text) desk_clipboard_request(&clipboard,pid,s->scene.focus==i,p,srv_ticks());
        return;
    }
    if(p->type==DESK_AUDIO) {
        int result=-1;struct srv_audio_status audio={0};
        (void)gui_audio(SRV_AUDIO_QUERY,0,&audio);
        if(p->serial && s->scene.apps &&
           (p->value==SRV_AUDIO_QUERY || (w->application==DESK_APP_SETTINGS && s->scene.focus==i)))
            result=gui_audio(p->value,p->width,&audio);
        c->audio_reply=desk_packet_new(DESK_AUDIO);c->audio_reply.serial=p->serial;
        c->audio_reply.value=result?1:0;memcpy(c->audio_reply.text,&audio,sizeof(audio));
        if(p->value) desk_log("deskd: audio operation=%u value=%u result=%d state=%u\n",p->value,p->width,result,audio.state);
        return;
    }
    if(p->type==DESK_BRIGHTNESS) {
        int result=-1;
        if(p->serial && s->scene.apps && w->application==DESK_APP_SETTINGS &&
           (!p->value || (s->scene.focus==i && p->value>=5 && p->value<=100)))
            result=gui_brightness(p->value);
        c->brightness_reply=desk_packet_new(DESK_BRIGHTNESS);
        c->brightness_reply.serial=p->serial;c->brightness_reply.value=(uint32_t)result;
        if(p->value) desk_log("deskd: brightness requested=%u result=%d\n",p->value,result);
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
    if (c->release.type) { desk_log("deskd: closing pid=%llu reason=unreleased-buffer\n",(unsigned long long)pid);close_client(s,i); return; }
    c->release=*p; c->release.type=DESK_RELEASE; c->progress=srv_ticks();
    if (p->generation!=c->generation || p->width!=(uint32_t)(w->frame.w-2) ||
        p->height!=(uint32_t)(w->frame.h-45) || c->closing) {
        desk_log("deskd: stale commit released pid=%llu\n",(unsigned long long)pid); return;
    }
    /* Keep displaying the front until a complete copy succeeds. */
    uint32_t *next=w->pixels==s->cache[i][0]?s->cache[i][1]:s->cache[i][0];
    if (!next || p->width>DESK_MAX_WIDTH || p->height>DESK_MAX_HEIGHT ||
        gui_surface_copy(p->surface,0,0,p->width,p->height,next,p->width)!=0) {
        desk_log("deskd: closing pid=%llu reason=surface-copy\n",(unsigned long long)pid);
        close_client(s,i); return;
    }
    c->present_input=p->input_serial;
    int first_frame=w->pixels==NULL || c->awaiting;
    w->pixels=next; w->width=p->width; w->height=p->height;
    c->awaiting=0;
    damage_window(s,i);
    if (s->trace && (!s->scene.apps || first_frame)) desk_log("deskd: commit pid=%llu generation=%llu size=%ux%u\n",
        (unsigned long long)pid,(unsigned long long)p->generation,p->width,p->height);
}
/* Queue age also includes time the compositor could not run. RELEASE and
 * CONFIGURE must get a delivery attempt before timeout attribution, just like
 * input. Start the response deadline only once the client can see the request. */
static int control_send(struct session *s,int i,const struct desk_packet *p) {
    struct client *c=&s->clients[i];
    c->input_blocked=0; /* Control traffic interrupted input delivery attempts. */
    if(!desk_send(c->pid,p)) {
        c->control_blocked=0;c->progress=srv_ticks();return 1;
    }
    uint64_t tick=srv_ticks();
    if(!c->control_blocked) { c->control_blocked=1;c->control_since=tick; }
    else if(tick-c->control_since>500) {
        desk_log("deskd: killing pid=%llu reason=control-timeout type=%u blocked_ticks=%llu\n",
            (unsigned long long)c->pid,p->type,(unsigned long long)(tick-c->control_since));
        srv_kill((int64_t)c->pid);forget(s,i);
    }
    return 0;
}
static void service(struct session *s) {
    for (int i=0;i<DESKD_WINDOWS;i++) {
        struct client *c=&s->clients[i];
        if (!c->pid) continue;
        if (!desk_process_alive(c->pid)) { forget(s,i); continue; }
        if (c->release.type) {
            if (!control_send(s,i,&c->release)) continue;
            c->release.type=0;
        }
        if (c->closing) {
            if(!c->close_sent) {
                struct desk_packet p=desk_packet_new(DESK_CLOSE);
                if(!control_send(s,i,&p)) continue;
                c->close_sent=1;
            }
        } else if (c->configure) {
            struct desk_packet p=desk_packet_new(DESK_CONFIGURE);
            p.generation=c->generation;
            p.width=s->scene.windows[i].frame.w-2; p.height=s->scene.windows[i].frame.h-45;
            if(!control_send(s,i,&p)) continue;
            c->configure=0;
        }
        if(c->pointer_cancel_pending && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_POINTER);
            p.value=DESK_POINTER_CANCEL;p.generation=c->generation;
            if(!control_send(s,i,&p)) continue;
            c->pointer_cancel_pending=0;
        }
        if ((c->awaiting || c->closing) && (uint64_t)srv_ticks()-c->progress>500) {
            desk_log("deskd: killing pid=%llu reason=client-timeout closing=%d awaiting=%d configure=%d release=%u tick=%llu\n",
                (unsigned long long)c->pid,c->closing,c->awaiting,c->configure,c->release.type,(unsigned long long)srv_ticks());
            srv_kill((int64_t)c->pid); forget(s,i); continue;
        }
        if(c->notice_ack_pending && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_NOTIFY_ACK);p.serial=c->notice_ack_serial;p.value=c->notice_ack_result;
            if(!desk_send(c->pid,&p)) c->notice_ack_pending=0;
        }
        if(c->audio_reply.type && !c->configure && !c->closing &&
           !desk_send(c->pid,&c->audio_reply)) c->audio_reply.type=0;
        if(c->brightness_reply.type && !c->configure && !c->closing &&
           !desk_send(c->pid,&c->brightness_reply)) c->brightness_reply.type=0;
        if(c->preferences_dirty && !c->configure && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_PREFERENCES);p.value=s->preferences.status;p.serial=s->preferences.revision;
            memcpy(p.text,&s->preferences.value,sizeof(s->preferences.value));
            if(!desk_send(c->pid,&p)) c->preferences_dirty=0;
        }
        /* Input throughput must not be limited to the render-loop rate. Keep
         * a per-client budget so one busy window cannot monopolize service. */
        for(unsigned delivered=0;delivered<DESK_INPUT_BATCH && c->input_count && !c->configure && !c->closing;delivered++) {
            if(desk_send(c->pid,&c->input[c->input_head])!=0) {
                /* Queued age includes time the compositor was not scheduled or
                 * was copying/drawing. Try delivery before blaming the client.
                 * Only a sustained series of blocked sends starts this timer. */
                uint64_t tick=srv_ticks();
                if(!c->input_blocked) { c->input_blocked=1;c->input_since=tick; }
                else if(tick-c->input_since>500) {
                    desk_log("deskd: closing pid=%llu reason=input-timeout queue=%u blocked_ticks=%llu type=%u awaiting=%d progress_age=%llu\n",
                        (unsigned long long)c->pid,c->input_count,(unsigned long long)(tick-c->input_since),
                        c->input[c->input_head].type,c->awaiting,(unsigned long long)(tick-c->progress));
                    close_client(s,i);
                }
                break;
            }
            c->input_head=(c->input_head+1)%DESK_INPUT_QUEUE; c->input_count--;
            c->input_since=srv_ticks();c->input_blocked=0;
        }
        if(!c->input_count || c->configure || c->closing) c->input_blocked=0;
        if((s->scene.apps || s->ram_text) && !c->closing) desk_clipboard_service(&clipboard,c->pid,srv_ticks());
    }
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->children[i]) {
        uint64_t status=0;
        long waited=srv_wait(s->children[i],&status,SRV_WAIT_NOHANG);
        /* An interrupted wait is not proof of death. Init only adopts a
         * child after its parent exits; live clients retain their own results. */
        if (waited==(long)s->children[i]) { s->child_reaped++;if(status)s->child_errors++; }
        else if(waited<0 && !desk_process_alive(s->children[i])) s->child_errors++;
        if (waited==(long)s->children[i])
            desk_log("deskd: child-exit pid=%llu status=%llu tick=%llu\n",(unsigned long long)s->children[i],(unsigned long long)status,(unsigned long long)srv_ticks());
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
    p.generation=s->clients[i].generation;
    p.x=clamp(wx,-16384,16384); p.y=clamp(wy,-16384,16384);
    p.width=(uint32_t)(x-w->frame.x-1);p.height=(uint32_t)(y-w->frame.y-44);
    queue_input(s,i,p);
}
static void context_damage(struct session *s) {
    struct desk_rect r=s->scene.context.frame;if(r.w>0) { r.w+=4;r.h+=5;desk_damage_add(&s->damage,r); }
}
static void context_close(struct session *s) {
    if(!desk_popup_active(&s->scene.context)) return;
    context_damage(s);desk_popup_cancel(&s->scene.context);
}
static void desktop_context_result(struct session *s,struct desk_menu_result result) {
    if(result.redraw) context_damage(s);
    if(result.action) { if(s->trace) desk_log("deskd: desktop context action=%u\n",result.action);global_menu_action(s,result.action); }
}
static void desktop_context_open(struct session *s,int x,int y,int keyboard) {
    if(s->exiting) return;
    const struct desk_menu_item items[]={
        {GLOBAL_FILES,0,"Open Files",""},{GLOBAL_TEXT,0,"New Text document",""},{GLOBAL_TERMINAL,0,"New Terminal",""},
        {0,DESK_MENU_SEPARATOR,"",""},{GLOBAL_SETTINGS,0,"Settings",""},{GLOBAL_SYSTEM,0,"System information",""},{GLOBAL_HELP,0,"SrvrOS Help","F1"}
    };
    context_close(s);pointer_cancel(s);desk_menu_cancel(&s->scene.menu);global_menu_damage(s);
    if(!desk_popup_open(&s->scene.context,items,sizeof(items)/sizeof(*items),x,y,(struct desk_rect){0,44,s->damage.width,s->damage.height-124},&desk_font_16,keyboard)) {
        context_damage(s);if(s->trace) srv_puts("deskd: desktop context opened\n");
    }
}
static int context_key(const struct srv_key_event *key) {
    return (key->flags&SRV_KEY_DOWN_EVENT) && !(key->flags&SRV_KEY_REPEAT) &&
        !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) &&
        ((key->usage==SRV_KEY_F10 && (key->modifiers&SRV_KEY_SHIFT)) || key->usage==0x65);
}
static void context_at(struct session *s,int x,int y,int keyboard) {
    int i=keyboard?s->scene.focus:hit(s,x,y);
    if(i<0) {
        if(keyboard || (y>=44 && y<s->damage.height-80)) desktop_context_open(s,keyboard?24:x,keyboard?64:y,keyboard);
        return;
    }
    if(s->clients[i].closing || s->clients[i].awaiting) return;
    struct desk_rect w=s->scene.windows[i].frame;
    if(!keyboard && (x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1)) return;
    pointer_cancel(s);focus(s,i);
    struct desk_packet p=desk_packet_new(DESK_CONTEXT);p.value=keyboard!=0;p.generation=s->clients[i].generation;
    if(!keyboard) { p.x=x-w.x-1;p.y=y-w.y-44; }
    queue_input(s,i,p);
    if(s->trace) desk_log("deskd: context pid=%llu keyboard=%d\n",(unsigned long long)s->clients[i].pid,keyboard);
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
    if (!s->pointer_pid) {
        if(!s->scene.apps || buttons) return;
        int i=hit(s,x,y);if(i<0 || s->clients[i].closing || s->clients[i].awaiting) return;
        struct desk_rect w=s->scene.windows[i].frame;
        if(x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1) return;
        struct desk_packet p=desk_packet_new(DESK_POINTER);p.value=DESK_POINTER_HOVER;p.generation=s->clients[i].generation;p.x=x-w.x-1;p.y=y-w.y-44;
        queue_input(s,i,p);return;
    }
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->clients[i].pid==s->pointer_pid) {
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.x=x-s->scene.windows[i].frame.x-1; p.y=y-s->scene.windows[i].frame.y-44;
        p.generation=s->clients[i].generation;
        p.value=(buttons&MOUSE_LEFT?DESK_POINTER_LEFT:0)|(pressed?DESK_POINTER_PRESS:0);
        queue_input(s,i,p); break;
    }
    if (!(buttons&MOUSE_LEFT)) s->pointer_pid=0;
}
static void timing_print(const char *name,const struct perf_sample *p) {
    desk_log("cursor-timing: %s count=%llu mean_us=%llu max_us=%llu p95_bound_us=%llu p99_bound_us=%llu\n",
        name,(unsigned long long)p->count,(unsigned long long)(p->count?p->total/p->count:0),
        (unsigned long long)p->max,(unsigned long long)perf_percentile(p,95),(unsigned long long)perf_percentile(p,99));
}
int main(int argc,char **argv) {
    unsigned scale=1;int explicit_scale=0;
    for (int i=1;i<argc;i++) if (!strncmp(argv[i],"--scale=",8)) {
        explicit_scale=1;
        if (!strcmp(argv[i],"--scale=1")) scale=1;
        else if (!strcmp(argv[i],"--scale=2")) scale=2;
        else { srv_puts("deskd: scale must be 1 or 2\n"); return 1; }
        for (int j=i;j<argc;j++) argv[j]=argv[j+1];
        argc--; i--;
    }
    if (argc==2 && strcmp(argv[1],"--license")==0) { srv_puts(desk_font_license); return 0; }
    int coexist_files=argc==2 && !strcmp(argv[1],"--coexist-files");
    int coexist_text=coexist_files || (argc==2 && !strcmp(argv[1],"--coexist-text"));
    int coexist=coexist_text || (argc==2 && !strcmp(argv[1],"--coexist-capture"));
    int idle_capture=argc==2 && !strcmp(argv[1],"--idle-capture");
    int network_trial=argc==2 && !strcmp(argv[1],"--network-trial");
    int hardware_trial=network_trial || (argc==2 && !strcmp(argv[1],"--hardware-trial"));
    int apps=idle_capture || hardware_trial || argc==1 || (argc==2 && (!strcmp(argv[1],"--apps") || !strcmp(argv[1],"--apps-trace")));
    int scroll_smoke=argc==2 && strcmp(argv[1],"--scroll-smoke")==0,scroll_phase=0;
    int smoke=scroll_smoke || (argc==2 && strcmp(argv[1],"--smoke")==0);
    int lifecycle=argc==2 && strcmp(argv[1],"--lifecycle-smoke")==0,phase=0;
    int io_mode=argc==3 && strcmp(argv[1],"--io-test")==0 && desk_trial_run_valid(argv[2]);
    struct gfx_info info={0};
    if (gfx_info(&info)!=0 || info.width<640 || info.height<480 || info.width>3840 || info.height>2160) return 1;
    if((idle_capture || coexist) && !explicit_scale) {
        scale=desk_capture_scale(info.width,info.height);
        if(!scale) { srv_puts("deskd: framebuffer exceeds idle capture limits\n");return 1; }
    }
    uint32_t width=(info.width+scale-1)/scale,height=(info.height+scale-1)/scale;
    if (width<640 || height<480) { srv_puts("deskd: scaled desktop requires at least 640x480 logical pixels\n"); return 1; }
    desk_log("deskd: display physical=%llux%llu logical=%ux%u scale=%u screenshots=logical\n",
        (unsigned long long)info.width,(unsigned long long)info.height,width,height,scale);
    uint32_t *pixels=desk_pixels_alloc(width,height);
    struct desk_canvas canvas;
    if (!pixels || desk_canvas_init(&canvas,pixels,width,height,width)!=0) return 1;
    if (gui_register_server()!=0) { srv_puts("deskd: another display server is active\n"); return 1; }
    if (gfx_console_mute(1)!=0) return 1;
    struct session s={.ram_text=coexist_text,.home=(hardware_trial || idle_capture)?DESK_WORKSPACE_HOME:DESK_RAM_HOME,.scene={.focus=-1,.cursor_x=24,.cursor_y=56},
        .trace=coexist || idle_capture || lifecycle || (argc==2 && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--apps-trace")))};
    desk_capture_job_init(&s.capture);
    desktop_notice_view_init(&s.scene.notifications);desktop_notice_view_configure(&s.scene.notifications,width,height);
    s.scene.apps=apps;desk_popup_init(&s.scene.context);desk_menu_init(&s.scene.menu);desk_palette_init(&s.scene.launcher);
    desk_calendar_init(&s.scene.calendar);desk_ui_init(&s.scene.calendar_ui);
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_GRID,DESK_CONTROL_CALENDAR,&s.scene.calendar,NULL);
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_TODAY,DESK_CONTROL_BUTTON,&s.scene.calendar_today,"Today");
    desk_ui_add(&s.scene.calendar_ui,CALENDAR_CLOSE,DESK_CONTROL_BUTTON,&s.scene.calendar_close,"Close");
    desk_ui_window_focus(&s.scene.calendar_ui,0);
    if((apps || coexist_text) && gui_keyboard_mode(1)!=0) { gfx_console_mute(0);return 1; }
    if(cache_init(&s)!=0) { gfx_console_mute(0); desk_pixels_free(pixels,width,height); return 1; }
    s.scene.trackpad_trial=io_mode?3:argc==2 && strcmp(argv[1],"--gesture-test")==0?2:
        (argc==2 && strcmp(argv[1],"--trackpad-test")==0);
    desk_damage_init(&s.damage,width,height);
    if(apps) { desktop_preferences_init_path(&s.preferences,(hardware_trial || idle_capture)?DESKTOP_WORKSPACE_PREFERENCES_PATH:DESKTOP_PREFERENCES_PATH);
        pointer_preferences(&s);clock_update(&s,1); }
    battery_update(&s,1);
    global_menu_rebuild(&s);
    desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
    if(coexist_text) spawn_capture_text(&s,coexist_files);
    else if(network_trial) spawn_app(&s,DESK_APP_TERMINAL,"network-trial");
    else if(apps) spawn_app(&s,hardware_trial?DESK_APP_HELP:DESK_APP_TEXT,hardware_trial?"laptop-trial":"");else spawn_client(&s);
    if (io_mode) spawn_client(&s); /* Load both studies before storage starts. */
    if(idle_capture) strcpy(s.scene.notice,"Automatic idle check: leave input alone. Logs and a screenshot will be saved before return.");
    /* Diagnostic: no app menus, preferences, Home or screenshot jobs.
     * The study client exercises independent IPC/surface publication. Generated
     * font-toggle events are not physical keyboard input. */
    if(coexist) strcpy(s.scene.notice,coexist_files?"Battery overlap: Text save/rename/reopen in test image. No input needed.":"Battery overlap: RAM only. No input needed; returns automatically.");
    uint64_t coexist_begin=0,coexist_last=0,coexist_gap=0,coexist_loops=0;
    uint64_t coexist_next=0,coexist_sent=0,coexist_ack=0,coexist_frames=0;
    int coexist_complete=0,coexist_script_done=0;
    uint64_t coexist_keys=0,coexist_key_ack=0;
    int idle_phase=0;
    unsigned idle_capture_serial=0;
    uint64_t idle_deadline=0,idle_frames=0,idle_bytes=0;
    struct srv_scheduler_status idle_before={0};
    int io_started=0,io_ready=0,stop_warning=0;
    uint64_t start=srv_ticks(),frames=0,bytes=0;
    int quit=0,failed=0,drag=-1,resize=-1,grabx=0,graby=0,pending_key=0,io_leaving=0;
    struct desktop_input_timing timing={0};
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
        if(coexist) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(coexist_last && now-coexist_last>coexist_gap) coexist_gap=now-coexist_last;
            coexist_last=now;coexist_loops++;
            if(!coexist_begin) {
                if(s.scene.windows[0].pixels && s.clients[0].generation) {
                    coexist_begin=now;coexist_next=now;coexist_frames=frames;
                    desk_log("desktop-coexist: ready start_us=%llu storage=%s generated=%s\n",(unsigned long long)now,coexist_files?"image":"ram",coexist_text?"text-edit":"font-toggle");
                } else if((uint64_t)srv_ticks()-start>=500) { failed=1;break; }
            } else if(!s.clients[0].pid || !s.scene.windows[0].pixels) { failed=1;break; }
            if(coexist_begin && now>=coexist_next && coexist_sent==coexist_ack && now-coexist_begin<60000000 && (!coexist_text || coexist_sent<60)) {
                struct desk_packet p=desk_packet_new(coexist_text?DESK_KEY:DESK_TEXT);p.value=12;p.input_serial=coexist_sent+1;
                if(coexist_text) { struct srv_key_event key=capture_key(coexist_sent);memcpy(p.text,&key,sizeof(key));
                    queue_input(&s,0,p);coexist_sent++;coexist_next=now+700000;
                } else if(desk_send(s.clients[0].pid,&p)==0) { coexist_sent++;coexist_next=now+1000000; }
            }
            if(coexist_begin && coexist_sent!=coexist_ack && now>coexist_next+4000000) { failed=1;break; }
        }
        if(apps || coexist)battery_update(&s,0);
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
        for (int n=0;!apps && !coexist_text && n<32;n++) {
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
                if (key==14) { if(!coexist) spawn_client(&s); continue; }
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
        for(int n=0;(apps || coexist_text) && n<32;n++) {
            struct srv_key_event key;int read=gui_keyboard_read(&key);
            if(read<=0) break;
            s.modifiers=key.flags&SRV_KEY_CANCEL?0:key.modifiers;
            if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_CTRL) &&
                (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_ESCAPE) { quit=1;break; }
            if(coexist_text) {
                if(coexist_script_done && s.scene.focus>=0) {
                    struct desk_packet p=desk_packet_new(DESK_KEY);memcpy(p.text,&key,sizeof(key));
                    p.input_serial=1000+(++coexist_keys);queue_input(&s,s.scene.focus,p);
                }
                continue;
            }
            if((key.flags&SRV_KEY_DOWN_EVENT) && !(key.flags&SRV_KEY_REPEAT) &&
                (key.modifiers&SRV_KEY_META) && (key.modifiers&SRV_KEY_SHIFT) && !(key.modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT)) && key.usage==9) {
                drag=resize=-1;pointer_cancel(&s);
                if(s.scene.launcher_open) launcher_close(&s);
                if(s.scene.calendar_open) calendar_close(&s);
                if(s.scene.notifications.opened) notifications_close(&s);
                context_close(&s);desk_menu_cancel(&s.scene.menu);global_menu_damage(&s);global_menu_action(&s,GLOBAL_FIND);continue;
            }
            unsigned placement=arrange_shortcut(&key);
            if(placement) {
                drag=resize=-1;pointer_cancel(&s);
                if(s.scene.launcher_open) launcher_close(&s);
                if(s.scene.calendar_open) calendar_close(&s);
                if(s.scene.notifications.opened) notifications_close(&s);
                context_close(&s);desk_menu_cancel(&s.scene.menu);global_menu_damage(&s);global_menu_action(&s,placement);continue;
            }
            if(capture_shortcut(&key)) { capture_request(&s);continue; }
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
                context_close(&s);desk_menu_cancel(&s.scene.menu);global_menu_damage(&s);global_menu_action(&s,GLOBAL_HELP);continue;
            }
            if(desk_popup_active(&s.scene.context)) {
                if((key.flags&SRV_KEY_DOWN_EVENT) && (key.modifiers&SRV_KEY_ALT) && key.usage==SRV_KEY_TAB) context_close(&s);
                else { desktop_context_result(&s,desk_popup_key(&s.scene.context,&key));continue; }
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
            if(s.scene.menu.open<0 && context_key(&key)) { drag=resize=-1;context_at(&s,0,0,1);continue; }
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
                if(!s.capture.busy && !s.capture_pending && (!saving || (uint64_t)srv_ticks()-s.exit_wait>1000)) { quit=1;break; }
                if(!s.scene.notice[0]) { strcpy(s.scene.notice,s.capture.busy?"Finishing screenshot save…":"Finishing saved preferences…");desk_damage_add(&s.damage,(struct desk_rect){0,0,s.damage.width,s.damage.height}); }
            }
        }
        long poll_clock=hardware_trial?srv_monotonic_us():0;
        uint64_t poll_time=poll_clock>0?(uint64_t)poll_clock:0;
        if(hardware_trial) desktop_timing_poll(&timing,poll_time);
        struct mouse_event e;
        for (int n=0;n<32 && mouse_scan(&e)>0;n++) {
            if(hardware_trial) {
                long now=srv_monotonic_us();
                desktop_timing_input(&timing,poll_time,now>0?(uint64_t)now:0,e.dx||e.dy);
            }
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
            if(s.context_down) {
                if(!e.buttons || (e.flags&MOUSE_CANCEL)) { s.context_down=0;s.pointer_blocked=0; }
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if(apps && desk_popup_active(&s.scene.context)) {
                drag=resize=-1;
                if(e.wheel_y && !e.buttons) desktop_context_result(&s,desk_popup_scroll(&s.scene.context,e.wheel_y));
                else desktop_context_result(&s,desk_popup_pointer(&s.scene.context,(struct desk_widget_event){.type=e.flags&MOUSE_CANCEL?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,.x=x,.y=y,.down=(e.buttons&MOUSE_LEFT)!=0,.pressed=(e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)}));
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
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
            if(apps && !buttons && !(e.flags&MOUSE_CANCEL) && drag<0 && resize<0 && !s.pointer_pid &&
                ((e.buttons&MOUSE_RIGHT) || ((e.buttons&MOUSE_LEFT) && (s.modifiers&SRV_KEY_CTRL)))) {
                context_at(&s,x,y,0);s.context_down=1;s.pointer_blocked=1;
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
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
                    unsigned control=desktop_titlebar_hit(w,apps,x,y);
                    if (control==DESKTOP_CLOSE) request_close(&s,i);
                    else if (control==DESKTOP_MAXIMIZE) { maximize(&s,i);s.pointer_blocked=1; }
                    else if (control==DESKTOP_MINIMIZE) { minimize(&s,i);s.pointer_blocked=1; }
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
            /* A stationary two-finger scroll must not insert HOVER between
             * every scroll packet: that defeats coalescing and floods clients. */
            if (drag<0 && resize<0 && !(e.flags&MOUSE_CANCEL) &&
                (e.dx || e.dy || e.buttons!=buttons))
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
        long render_start=(io_mode || hardware_trial) && s.damage.count?srv_monotonic_us():0;
        for (unsigned n=0;n<s.damage.count;n++) {
            struct desk_rect r=s.damage.rects[n]; desk_clip(&canvas,r); desktop_draw(&canvas,&s.scene);
            if (blitrect_scaled((uint64_t)r.x*scale,(uint64_t)r.y*scale,r.w,r.h,pixels+(size_t)r.y*canvas.stride+r.x,canvas.stride,scale)!=0) failed=1;
            uint64_t physical_w=(uint64_t)r.w*scale,physical_h=(uint64_t)r.h*scale;
            if(physical_w>info.width-(uint64_t)r.x*scale) physical_w=info.width-(uint64_t)r.x*scale;
            if(physical_h>info.height-(uint64_t)r.y*scale) physical_h=info.height-(uint64_t)r.y*scale;
            bytes+=physical_w*physical_h*4;
        }
        if(hardware_trial) {
            long end=s.damage.count?srv_monotonic_us():0;
            desktop_timing_end(&timing,poll_time,render_start>0?(uint64_t)render_start:0,end>0?(uint64_t)end:0,!failed && s.damage.count);
        }
        if (!failed && s.damage.count) for (int i=0;i<DESKD_WINDOWS;i++)
            io_trial_text_present(&s.io,s.clients[i].pid,s.clients[i].present_input);
        io_trial_loop_end(&s.io,render_start>0?(uint64_t)render_start:0);
        if (s.damage.count) frames++;
        if(coexist && !failed && s.damage.count && s.clients[0].present_input==coexist_sent)
            coexist_ack=coexist_sent;
        if(coexist_text && !failed && s.damage.count && coexist_keys && s.clients[0].present_input==1000+coexist_keys)
            coexist_key_ack=coexist_keys;
        if(coexist_text && !coexist_script_done && coexist_ack==60) {
            coexist_script_done=1;srv_puts("desktop-text: script presented; keyboard ready\n");
        }
        s.damage.count=0;
        if(apps && !failed) capture_tick(&s,pixels);
        if(coexist && coexist_begin && !failed && (uint64_t)srv_monotonic_us()-coexist_begin>=60000000 && coexist_sent==coexist_ack) {
            coexist_complete=coexist_ack>=55 && frames-coexist_frames>=55 &&
                (!coexist_text || (coexist_ack==60 && coexist_keys==coexist_key_ack));
            if(!coexist_complete) failed=1;
            quit=1;
        }
        if(idle_capture && !failed) {
            uint64_t now=srv_ticks();
            if(idle_phase && !s.clients[0].generation) failed=1;
            if(!idle_phase && now-start>=500) {
                if(!s.clients[0].generation) { if(now-start>=3000) failed=1; }
                else if(srv_scheduler_status(&idle_before) || idle_before.version!=SRV_SCHEDULER_VERSION) failed=1;
                else {
                    idle_phase=1;idle_deadline=now+6000;idle_frames=frames;idle_bytes=bytes;
                    desk_log("desktop-idle: measuring 60 seconds with Text open; no user input required\n");
                }
            } else if(idle_phase==1 && now>=idle_deadline) {
                struct srv_scheduler_status after={0};
                if(srv_scheduler_status(&after) || after.version!=SRV_SCHEDULER_VERSION ||
                   after.monotonic_us<idle_before.monotonic_us+59000000 ||
                   after.sampled_ticks<=idle_before.sampled_ticks || after.idle_ticks<idle_before.idle_ticks ||
                   after.idle_ticks-idle_before.idle_ticks>after.sampled_ticks-idle_before.sampled_ticks ||
                   after.switches<idle_before.switches || after.preemptions<idle_before.preemptions) failed=1;
                else {
                    desk_log("desktop-idle: elapsed_us=%llu sampled_ticks=%llu idle_ticks=%llu switches=%llu preemptions=%llu frames=%llu fb_bytes=%llu measured=yes\n",
                        (unsigned long long)(after.monotonic_us-idle_before.monotonic_us),
                        (unsigned long long)(after.sampled_ticks-idle_before.sampled_ticks),
                        (unsigned long long)(after.idle_ticks-idle_before.idle_ticks),
                        (unsigned long long)(after.switches-idle_before.switches),
                        (unsigned long long)(after.preemptions-idle_before.preemptions),
                        (unsigned long long)(frames-idle_frames),(unsigned long long)(bytes-idle_bytes));
                    desk_log("desktop-idle-window: start_us=%llu end_us=%llu\n",
                        (unsigned long long)idle_before.monotonic_us,(unsigned long long)after.monotonic_us);
                    idle_phase=2;idle_deadline=now+3000;idle_capture_serial=s.capture_finished;
                    capture_request(&s);
                }
            } else if(idle_phase==2) {
                if(s.capture_finished>idle_capture_serial) {
                    if(!s.capture_success) failed=1;
                    else { idle_phase=3;quit=1; }
                } else if(now>=idle_deadline) failed=1;
            }
        }
        if (failed || (smoke && (uint64_t)srv_ticks()-start>=200)) break;
        srv_sleep_ticks(1);
    }
    uint64_t coexist_end=(uint64_t)srv_monotonic_us();
    if(idle_capture) {
        if(idle_phase!=3) failed=1;
        desk_log("desktop-idle: completed=%s screenshot=%s\n",failed?"NO":"yes",idle_phase==3?"yes":"NO");
    }
    if(hardware_trial) {
        desk_log("cursor-timing: software-only; excludes USB arrival and display scanout; percentile bounds are histogram buckets\n");
        desk_log("cursor-wait: policy=fixed-one-tick\n");
        timing_print("poll-gap",&timing.poll_gap);timing_print("consecutive-motion-gap",&timing.active_gap);
        timing_print("consume-to-final-blit",&timing.consume_to_blit);timing_print("draw-blit",&timing.draw_blit);
    }
    if (io_trial_finish(&s.io)!=0) failed=1;
    /* Bound shutdown even when a client stopped reading. Only kill children
     * launched by this session; all queues/surfaces also have kernel cleanup. */
    for (int i=0;i<DESKD_WINDOWS;i++) if (s.clients[i].pid) close_client(&s,i);
    uint64_t until=(uint64_t)srv_ticks()+30;
    while ((uint64_t)srv_ticks()<until) { service(&s); srv_sleep_ticks(1); }
    for (int i=0;i<DESKD_WINDOWS;i++) {
        if (s.children[i]) {
            /* A storage wait can cross the grace deadline after the client
             * exits. Reap that completion before treating it as a forced kill. */
            uint64_t status=0,pid=s.children[i];
            long waited=srv_wait(pid,&status,SRV_WAIT_NOHANG);
            if(waited!=(long)pid) {
                srv_kill(pid);waited=srv_wait(pid,&status,0);
                if(coexist) failed=1;
            }
            if(waited==(long)pid) {
                s.child_reaped++;if(status)s.child_errors++;
                desk_log("deskd: child-exit pid=%llu status=%llu tick=%llu\n",
                    (unsigned long long)pid,(unsigned long long)status,(unsigned long long)srv_ticks());
            } else s.child_errors++;
            s.children[i]=0;
        }
        if (s.clients[i].pid) forget(&s,i);
    }
    if(coexist && (s.child_reaped!=1 || s.child_errors)) failed=1;
    if(coexist)desk_log("desktop-battery: reads=%llu first_gen=%llu last_gen=%llu cached=yes\n",
        (unsigned long long)s.battery_reads,(unsigned long long)s.battery_first,(unsigned long long)s.battery_last);
    if(coexist_text) desk_log("desktop-text: generated=%llu presented=%llu keyboard_sent=%llu keyboard_presented=%llu storage=%s\n",
        (unsigned long long)coexist_sent,(unsigned long long)coexist_ack,
        (unsigned long long)coexist_keys,(unsigned long long)coexist_key_ack,coexist_files?"image":"ram");
    if(coexist) {
        if(!coexist_complete) failed=1;
        desk_log("desktop-coexist: completed=%s start_us=%llu end_us=%llu loops=%llu max_gap_us=%llu generated=%llu presented=%llu frames=%llu storage=%s\n",
            failed?"NO":"yes",(unsigned long long)coexist_begin,(unsigned long long)coexist_end,
            (unsigned long long)coexist_loops,(unsigned long long)coexist_gap,
            (unsigned long long)coexist_sent,(unsigned long long)coexist_ack,(unsigned long long)(frames-coexist_frames),coexist_files?"image":"ram");
    }
    if(apps) {
        if(s.preferences.dirty) desk_log("deskd: preferences were not fully saved (status=%u)\n",s.preferences.status);
        desktop_preferences_close(&s.preferences);
        desk_capture_job_close(&s.capture);
    }
    cache_free(&s);
    desk_pixels_free(pixels,width,height); gfx_console_mute(0);
    desk_log("deskd: %s frames=%llu fb_bytes=%llu\n",failed?"FAIL":"exited",
        (unsigned long long)frames,(unsigned long long)bytes);
    return failed?1:0;
}
