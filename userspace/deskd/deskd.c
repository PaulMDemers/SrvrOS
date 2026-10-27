#include <srvros/desk_battery.h>
#include "battery_trial.h"
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
#include "loop_timing.h"
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
#include <sys/mman.h>
#include <srvros/calendar.h>

static struct desk_clipboard_store clipboard;
#define DESK_INPUT_BATCH 8

#include "lifecycle_worker.h"
struct pixel_cache { uint32_t *pixels,width,height; struct life_memory *job; };
#define DESKD_CACHE_BUDGET DESK_CACHE_BYTES
struct client {
    uint64_t pid,window,identity,generation,progress,present_input,parent_window;
    int managed,retiring;
    int configure,awaiting,closing,close_requested,preferences_dirty;
    struct desk_packet release,brightness_reply,audio_reply;
    struct desk_packet input[DESK_INPUT_QUEUE];
    unsigned input_head,input_count,input_dropped;
    struct desk_rect resize_origin,resize_restore;
    int resize_maximized;
    uint64_t sent_generation;
    struct desk_rect sent_frame,sent_restore;
    int sent_maximized;
    uint64_t input_since;
    int input_blocked;
    uint64_t control_since;
    int control_blocked,close_sent,pointer_cancel_pending;
    struct desk_menu_store menus;
    struct desk_clipboard_peer clipboard;
    char launch_path[160];
    unsigned launch_length,launch_application,service_bits;
    uint64_t open_request,open_since;char open_path[160];
    uint64_t notice_last,notice_at,notice_ack_serial;
    unsigned notice_last_result,notice_ack_result;
    int notice_ack_pending,notice_has_time;
};
struct window_record { struct client client;struct desktop_window window;struct pixel_cache cache[2]; };
struct window_chunk { struct window_chunk *next;size_t bytes;struct window_record records[]; };
struct child_record { uint64_t pid,last_window,reply_since;unsigned mode,reply_blocked;struct desk_packet reply;int application; };
struct child_chunk { struct child_chunk *next;size_t bytes;struct child_record records[]; };
struct session {
    const char *home;
    struct lifecycle_worker *worker;
    int startup_trace;
    struct {uint64_t pid;struct desk_packet packet;} pending_open[LIFE_LAUNCH_SLOTS];
    unsigned pending_opens;
    int cache_pending;
    struct desktop_scene scene;
    struct desk_damage damage;
    struct desk_rect menu_visible;
    uint64_t menu_owner,menu_window;
    uint64_t window_serial,overlay_identity,activation_serial;
    struct window_record **records;
    struct window_chunk *window_chunks;
    void *window_table;size_t window_table_bytes;
    struct desk_menu_item *menu_build;
    struct desk_palette_item *palette_build;
    struct child_record **child_records;
    struct child_chunk *child_chunks;
    size_t child_table_bytes;
    int child_capacity,dynamic,refuse_dialogs;
    uint64_t registry_bytes,registry_peak,registry_budget;
    struct client clients[DESKD_WINDOWS];
    uint64_t children[DESKD_WINDOWS];
    uint64_t child_last_window[DESKD_WINDOWS],child_reply_since[DESKD_WINDOWS];
    unsigned child_mode[DESKD_WINDOWS],child_reply_blocked[DESKD_WINDOWS];
    struct desk_packet child_reply[DESKD_WINDOWS];
    int child_apps[DESKD_WINDOWS],exiting,ram_text;
    uint64_t battery_tick,battery_reads,battery_first,battery_last;
    struct srv_battery_status battery_status;
    int battery_result;
    struct desk_battery_alert battery_alert;
    uint64_t battery_notice_serial;
    uint64_t launch_notice_serial,launch_notice_at,last_launch_pid;
    struct desktop_preferences_store preferences;
    uint64_t clock_tick,exit_wait;
    int trace,pointer_blocked,overlay_return,overlay_down;
    unsigned launcher_application;
    int switching; /* Launcher storage holds an immutable MRU identity snapshot. */
    uint64_t pointer_pid,pointer_window;
    int notice_visible,context_down;
    unsigned child_reaped,child_errors;
    unsigned modifiers;
    struct io_trial io;
    struct desk_capture_job capture;
    int capture_pending,capture_success;
    unsigned capture_finished;
    uint64_t capture_serial;
    struct pixel_cache cache[DESKD_WINDOWS][2];
    uint64_t cache_bytes,cache_peak,memory_notice_at,memory_notice_serial;
};

#define session_client(s,i) ((i)<DESKD_WINDOWS?&(s)->clients[(i)]:&(s)->records[(i)]->client)
#define session_cache(s,i) ((i)<DESKD_WINDOWS?(s)->cache[(i)]:(s)->records[(i)]->cache)
static int session_capacity(const struct session *s) { return desktop_capacity(&s->scene); }
static int children_capacity(const struct session *s) { return s->child_capacity?s->child_capacity:DESKD_WINDOWS; }
#define session_children(s,i) ((i)<DESKD_WINDOWS?&(s)->children[(i)]:&(s)->child_records[(i)]->pid)
#define session_child_last_window(s,i) ((i)<DESKD_WINDOWS?&(s)->child_last_window[(i)]:&(s)->child_records[(i)]->last_window)
#define session_child_reply_since(s,i) ((i)<DESKD_WINDOWS?&(s)->child_reply_since[(i)]:&(s)->child_records[(i)]->reply_since)
#define session_child_mode(s,i) ((i)<DESKD_WINDOWS?&(s)->child_mode[(i)]:&(s)->child_records[(i)]->mode)
#define session_child_reply_blocked(s,i) ((i)<DESKD_WINDOWS?&(s)->child_reply_blocked[(i)]:&(s)->child_records[(i)]->reply_blocked)
#define session_child_reply(s,i) ((i)<DESKD_WINDOWS?&(s)->child_reply[(i)]:&(s)->child_records[(i)]->reply)
#define session_child_apps(s,i) ((i)<DESKD_WINDOWS?&(s)->child_apps[(i)]:&(s)->child_records[(i)]->application)

#include "registry.h"

static int client_send(const struct client *c,const struct desk_packet *packet) {
    struct desk_packet p=*packet;p.window=c->window?c->window:DESK_DEFAULT_WINDOW;
    return desk_send(c->pid,&p);
}
static int child_index(const struct session *s,uint64_t pid) {
    for(int i=0;i<children_capacity(s);i++)if((*session_children(s,i))==pid)return i;
    return -1;
}
static uint64_t loop_sample(uint64_t *last,uint64_t *maximum,uint64_t now) {
    uint64_t gap=*last?now-*last:0;
    if(gap>*maximum)*maximum=gap;
    *last=now;return gap;
}
static void loop_trace_stamp(struct loop_trace *t,unsigned stage) {
    if(t->active)loop_trace_mark(t,stage,(uint64_t)srv_monotonic_us());
}
static void loop_trace_report(const struct loop_trace *t,unsigned final) {
    if(!t->samples)return;
    const struct loop_record *w=&t->worst;
    /* One bounded console record; the ordinary 384-byte desk_log buffer is
     * too short for a complete worst-loop receipt. No heap/stdio buffering. */
    char text[1024];
    int length=snprintf(text,sizeof(text),"desktop-loop-worst: version=2 samples=%llu over_limit=%llu invalid=%llu seq=%llu start_us=%llu end_us=%llu gap_us=%llu cycle=%llu pid=%llu phase=%llu end_cycle=%llu end_pid=%llu end_phase=%llu control_us=%llu messages_us=%llu lifecycle_us=%llu service_us=%llu trial_us=%llu input_us=%llu render_us=%llu post_us=%llu wait_us=%llu final=%u\n",
        (unsigned long long)t->samples,(unsigned long long)t->over_limit,(unsigned long long)t->invalid,
        (unsigned long long)w->sequence,(unsigned long long)w->start,(unsigned long long)w->end,(unsigned long long)w->gap,
        (unsigned long long)w->cycle,(unsigned long long)w->pid,(unsigned long long)w->phase,
        (unsigned long long)w->end_cycle,(unsigned long long)w->end_pid,(unsigned long long)w->end_phase,
        (unsigned long long)w->stage_us[LOOP_CONTROL],(unsigned long long)w->stage_us[LOOP_MESSAGES],
        (unsigned long long)w->stage_us[LOOP_LIFECYCLE],(unsigned long long)w->stage_us[LOOP_SERVICE],
        (unsigned long long)w->stage_us[LOOP_TRIAL],(unsigned long long)w->stage_us[LOOP_INPUT],
        (unsigned long long)w->stage_us[LOOP_RENDER],(unsigned long long)w->stage_us[LOOP_POST],
        (unsigned long long)w->stage_us[LOOP_WAIT],final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    else srv_puts("desktop-loop-worst: invalid=record-overflow\n");
    const struct loop_wait_record *v=&w->wait;
    length=snprintf(text,sizeof(text),"desktop-loop-wait: seq=%llu valid=%llu begin_us=%llu gui_begin_us=%llu gui_end_us=%llu sleep_begin_us=%llu sleep_end_us=%llu end_us=%llu gui_result=%lld sleep_result=%lld final=%u\n",
        (unsigned long long)w->sequence,(unsigned long long)v->valid,(unsigned long long)v->begin,
        (unsigned long long)v->gui_begin,(unsigned long long)v->gui_end,
        (unsigned long long)v->sleep_begin,(unsigned long long)v->sleep_end,(unsigned long long)v->end,
        (long long)v->gui_result,(long long)v->sleep_result,final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    else srv_puts("desktop-loop-wait: invalid=record-overflow\n");
    const struct srv_sched_trace *q=&v->scheduler;
    length=snprintf(text,sizeof(text),"desktop-loop-sched: seq=%llu version=%llu start_us=%llu end_us=%llu start_tick=%llu end_tick=%llu blocked_us=%llu ready_us=%llu blocks=%llu signals=%llu timeouts=%llu dispatches=%llu exclusions=%llu invalid=%llu max_ready_us=%llu wake_us=%llu run_us=%llu wake_tick=%llu deadline_tick=%llu reason=%llu ready_exclusions=%llu late_ticks=%llu final=%u\n",
        (unsigned long long)w->sequence,(unsigned long long)q->version,
        (unsigned long long)q->start_us,(unsigned long long)q->end_us,
        (unsigned long long)q->start_tick,(unsigned long long)q->end_tick,
        (unsigned long long)q->blocked_us,(unsigned long long)q->ready_us,
        (unsigned long long)q->blocks,(unsigned long long)q->signals,(unsigned long long)q->timeouts,
        (unsigned long long)q->dispatches,(unsigned long long)q->exclusions,(unsigned long long)q->invalid,
        (unsigned long long)q->max_ready_us,(unsigned long long)q->wake_us,(unsigned long long)q->run_us,
        (unsigned long long)q->wake_tick,(unsigned long long)q->deadline_tick,(unsigned long long)q->reason,
        (unsigned long long)q->ready_exclusions,(unsigned long long)q->late_ticks,final);
    if(length>0 && (unsigned)length<sizeof(text))srv_puts(text);
    else srv_puts("desktop-loop-sched: invalid=record-overflow\n");
}
static int window_lifetime_valid(const struct session *s,int i,uint64_t identity) {
    return i>=0 && i<session_capacity(s) && session_client(s,i)->pid &&
        !session_client(s,i)->closing && session_client(s,i)->identity==identity;
}
static int pointer_owned(const struct session *s,int i) {
    return s->pointer_pid && s->pointer_pid==session_client(s,i)->pid &&
        s->pointer_window==session_client(s,i)->window;
}
/* Trace the separate owners rather than interpreting a launch count as memory. */
static void resource_trace(const struct session *s,const char *reason) {
    if(!s->trace) return;
    unsigned clients=0,children=0,peers=0;
    for(int i=0;i<session_capacity(s);i++) {
        clients+=session_client(s,i)->pid!=0;
        peers+=session_client(s,i)->clipboard.pid!=0;
    }
    for(int i=0;i<children_capacity(s);i++)children+=*session_children(s,i)!=0;
    desk_log("deskd: resources reason=%s clients=%u children=%u clipboard_peers=%u window_capacity=%u cache_bytes=%llu cache_budget=%llu gui_queue_budget=%llu gui_queue_depth=%u gui_surface_record_budget=%llu gui_surface_pixel_budget=%llu\n",
        reason,clients,children,peers,session_capacity(s),(unsigned long long)s->cache_bytes,
        (unsigned long long)DESKD_CACHE_BUDGET,(unsigned long long)DESK_GUI_QUEUE_BYTES,DESK_GUI_QUEUE_DEPTH,(unsigned long long)DESK_GUI_SURFACE_RECORD_BYTES,(unsigned long long)DESK_GUI_PIXEL_BYTES);
}
/* Only live windows own caches. A private spare receives each whole frame;
 * published pixels remain untouched until copy succeeds. Budget includes both. */
static void cache_drop(struct session *s,int i,int j) {
    struct pixel_cache *c=&session_cache(s,i)[j];
    if(c->job) { life_publish(&c->job->discard,1);*c=(struct pixel_cache){0};return; }
    if(c->pixels) {
        s->cache_bytes-=(uint64_t)c->width*c->height*4;
        desk_pixels_free(c->pixels,c->width,c->height);*c=(struct pixel_cache){0};
    }
}
static void cache_retire(struct session *s,int i) {
    for(int j=0;j<2;j++)cache_drop(s,i,j);
}
static void cache_free(struct session *s) {
    for(int i=0;i<session_capacity(s);i++)cache_retire(s,i);
}
static int cache_init(struct session *s) { (void)s;return 0; }
static void cache_reclaim(struct session *s,int keep) {
    for(int i=0;i<session_capacity(s);i++)if(i!=keep && !session_client(s,i)->awaiting)for(int j=0;j<2;j++)
        if(session_cache(s,i)[j].pixels!=desktop_window_at(&s->scene,i)->pixels)cache_drop(s,i,j);
}
static uint32_t *cache_prepare(struct session *s,int i,unsigned width,unsigned height) {
    s->cache_pending=0;
    if(!width || !height || width>DESK_MAX_WIDTH || height>DESK_MAX_HEIGHT)return NULL;
    int j=desktop_window_at(&s->scene,i)->pixels==session_cache(s,i)[0].pixels?1:0;
    struct pixel_cache *c=&session_cache(s,i)[j];uint64_t bytes=(uint64_t)width*height*4;
    if(c->job) {
        unsigned state=life_state(&c->job->state);
        if(state!=LIFE_DONE) {s->cache_pending=1;return NULL;}
        c->pixels=c->job->pixels;
        if(!c->pixels){cache_drop(s,i,j);return NULL;}
    }
    if(c->pixels && (uint64_t)c->width*c->height*4>=bytes)return c->pixels;
    if(bytes>DESKD_CACHE_BUDGET-s->cache_bytes)cache_reclaim(s,i);
    if(bytes>DESKD_CACHE_BUDGET-s->cache_bytes) {
        /* Dropping a spare transfers ownership, not free-byte credit. Wait
         * for enough pending releases; reserve only after completed unmaps. */
        uint64_t retiring=0;
        if(s->worker)for(unsigned n=0;n<LIFE_MEMORY_SLOTS;n++) {
            const struct life_memory *m=&s->worker->memory[n];
            if(life_state(&m->state)!=LIFE_IDLE && life_state(&m->discard))retiring+=(uint64_t)m->width*m->height*4;
        }
        if(bytes<=DESKD_CACHE_BUDGET-s->cache_bytes+retiring)s->cache_pending=1;
        return NULL;
    }
    if(s->worker) {
        for(unsigned n=0;n<LIFE_MEMORY_SLOTS;n++) {
            struct life_memory *m=&s->worker->memory[n];
            if(life_state(&m->state)!=LIFE_IDLE)continue;
            cache_drop(s,i,j);m->width=width;m->height=height;m->pixels=NULL;m->discard=0;
            *c=(struct pixel_cache){.width=width,.height=height,.job=m};
            s->cache_bytes+=bytes;if(s->cache_bytes>s->cache_peak)s->cache_peak=s->cache_bytes;
            life_publish(&m->state,LIFE_QUEUED);s->cache_pending=1;return NULL;
        }
        return NULL;
    }
    uint32_t *pixels=desk_pixels_alloc(width,height);
    if(!pixels) { cache_reclaim(s,i);pixels=desk_pixels_alloc(width,height); }
    if(!pixels)return NULL;
    cache_drop(s,i,j);
    *c=(struct pixel_cache){.pixels=pixels,.width=width,.height=height};s->cache_bytes+=bytes;
    if(s->cache_bytes>s->cache_peak)s->cache_peak=s->cache_bytes;
    return pixels;
}
static void cache_trim(struct session *s,int i,uint64_t bytes) {
    for(int j=0;j<2;j++)if(session_cache(s,i)[j].pixels!=desktop_window_at(&s->scene,i)->pixels &&
        (uint64_t)session_cache(s,i)[j].width*session_cache(s,i)[j].height*4>bytes)cache_drop(s,i,j);
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
    struct client *c=session_client(s,i);
    if (c->closing) return;
    p.window=c->window?c->window:DESK_DEFAULT_WINDOW;
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
    uint64_t pid=s->pointer_pid,window=s->pointer_window;
    s->pointer_pid=0;s->pointer_window=0;
    if (!pid) return;
    if (io_trial_active(&s->io)) s->io.cancels++;
    s->pointer_blocked=1;
    for (int i=0;i<session_capacity(s);i++) if (session_client(s,i)->pid==pid && session_client(s,i)->window==window) {
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.value=DESK_POINTER_CANCEL; p.generation=session_client(s,i)->generation;
        queue_input(s,i,p); break;
    }
}
static void cancel_input(struct session *s,unsigned buttons) {
    s->pointer_pid=0; s->pointer_blocked=(buttons&MOUSE_LEFT)!=0;
    for (int i=0;i<session_capacity(s);i++) if (session_client(s,i)->pid) {
        /* Drop uncertain pointer actions, retaining ordered text/keys and other
         * input. Keep cancellation outside the bounded queue so a full keyboard
         * queue can still be canceled without data loss or closing the client. */
        struct client *c=session_client(s,i);
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
    struct desk_rect r=desktop_window_at(&s->scene,i)->frame; r.w+=6; r.h+=8;
    desk_damage_add(&s->damage,r);
}
static int modal_child(const struct session *s,int parent) {
    if(parent<0)return -1;
    const struct client *p=session_client(s,parent);
    for(int j=0;j<session_capacity(s);j++) {
        const struct client *c=session_client(s,j);
        if(c->pid==p->pid && c->parent_window==p->window && !c->closing)return j;
    }
    return -1;
}
static void focus(struct session *s,int i) {
    int child=modal_child(s,i);if(child>=0)i=child;
    if(s->scene.launcher_open || s->scene.calendar_open || s->scene.notifications.opened) {
        /* New admissions still change dock counts while the overlay owns focus. */
        if(s->scene.apps)desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
        if(s->switching)return;
        s->overlay_return=i;s->overlay_identity=i>=0?session_client(s,i)->identity:0;
        if(s->scene.launcher_open)launcher_catalog(s);
        return;
    }
    int previous=s->scene.focus;
    if (previous!=i) {
        pointer_cancel(s);
        if((s->scene.apps || s->ram_text) && previous>=0 && session_client(s,previous)->pid) {
            struct desk_packet p=desk_packet_new(DESK_FOCUS);p.value=0;queue_input(s,previous,p);
        }
    }
    if (previous>=0) damage_window(s,previous);
    s->scene.focus=i;
    if (i>=0) {
        desktop_window_at(&s->scene,i)->minimized=0;
        if(session_client(s,i)->parent_window)for(int j=0;j<session_capacity(s);j++)
            if(session_client(s,j)->pid==session_client(s,i)->pid && session_client(s,j)->window==session_client(s,i)->parent_window) {
                desktop_window_at(&s->scene,j)->minimized=0;damage_window(s,j);
                desktop_window_at(&s->scene,j)->z=++s->scene.stack_serial;
            }
        desktop_window_at(&s->scene,i)->z=++s->scene.stack_serial;
        damage_window(s,i);
        if((s->scene.apps || s->ram_text) && previous!=i) {
            struct desk_packet p=desk_packet_new(DESK_FOCUS);p.value=1;queue_input(s,i,p);
        }
    }
    if(s->trace && previous!=i && i>=0)desk_log("deskd: focused pid=%llu window=%llu identity=%llu\n",(unsigned long long)session_client(s,i)->pid,(unsigned long long)session_client(s,i)->window,(unsigned long long)session_client(s,i)->identity);
    if(s->scene.apps) {
        global_menu_rebuild(s);
        desk_damage_add(&s->damage,(struct desk_rect){0,0,s->damage.width,44});
        desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
    }
}
/* Roots represent task entries; focus() redirects to an owned modal. */
static int window_root(const struct session *s,int i) {
    if(i>=0 && session_client(s,i)->parent_window)for(int j=0;j<session_capacity(s);j++)
        if(session_client(s,j)->pid==session_client(s,i)->pid && session_client(s,j)->window==session_client(s,i)->parent_window)return j;
    return i;
}
static int focus_candidate(const struct session *s,int i,int hidden) {
    const struct client *c=session_client(s,i);const struct desktop_window *w=desktop_window_at(&s->scene,i);
    return c->pid && !c->closing && !c->parent_window && w->visible && (hidden || !w->minimized);
}
static void focus_best(struct session *s) {
    int best=-1;
    for(int j=0;j<session_capacity(s);j++)if(focus_candidate(s,j,0) &&
        (best<0 || desktop_window_at(&s->scene,j)->z>desktop_window_at(&s->scene,best)->z))best=j;
    focus(s,best);
}
static void forget(struct session *s,int i) {
    uint64_t parent=session_client(s,i)->parent_window,pid=session_client(s,i)->pid;
    if(session_client(s,i)->open_request) {
        strcpy(s->scene.notice,"App closed before confirming Open. Check its windows before retrying.");
        desk_log("deskd: document-route owner retired before confirmation pid=%llu\n",(unsigned long long)pid);
        desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-112,s->damage.width,112});
    }
    if((s->scene.launcher_open || s->scene.calendar_open || s->scene.notifications.opened) && s->overlay_return==i) s->overlay_return=-1;
    desk_clipboard_forget(&clipboard,&session_client(s,i)->clipboard);
    if (pointer_owned(s,i)) {
        s->pointer_pid=0; s->pointer_blocked=1;
    }
    struct desktop_window *w=desktop_window_at(&s->scene,i);
    damage_window(s,i);
    desk_log("deskd: retired pid=%llu\n",(unsigned long long)session_client(s,i)->pid);
    cache_retire(s,i);
    memset(w,0,sizeof(*w)); memset(session_client(s,i),0,sizeof((*session_client(s,i))));
    resource_trace(s,"retired");
    if(s->scene.apps) desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
    if (s->scene.focus==i) {
        focus(s,-1);
        int restore=-1;
        for(int j=0;j<session_capacity(s);j++)if(session_client(s,j)->pid==pid && session_client(s,j)->window==parent && !session_client(s,j)->closing)restore=j;
        if(restore>=0)focus(s,restore);
        else focus_best(s);
    }
    global_menu_rebuild(s);
}
static void memory_notice(struct session *s) {
    if(!s->scene.apps)return;
    uint64_t now=srv_ticks();
    if(s->memory_notice_serial && now-s->memory_notice_at<100)return;
    s->memory_notice_at=now;
    struct desk_notification text={0};strcpy(text.title,"Window memory");
    strcpy(text.message,"Memory is low. Close a window and try again.");
    uint64_t id;long us=srv_monotonic_us();
    if(desktop_notification_add(&s->scene.notifications.store,UINT64_MAX-2,DESK_APP_SYSTEM,
       ++s->memory_notice_serial,us<0?0:(uint64_t)us,DESK_NOTICE_WARNING,&text,&id)>0) {
        desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);notifications_damage(s);
    }
}
static int configure(struct session *s,int i,int width,int height) {
    if(!cache_prepare(s,i,width,height) && !s->cache_pending) {
        snprintf(s->scene.notice,sizeof(s->scene.notice),"Not enough memory to resize this window.");memory_notice(s);
        desk_log("deskd: resize refused pid=%llu reason=cache-memory bytes=%llu\n",(unsigned long long)session_client(s,i)->pid,(unsigned long long)s->cache_bytes);
        return 0;
    }
    if(!session_client(s,i)->awaiting) {
        session_client(s,i)->resize_origin=desktop_window_at(&s->scene,i)->frame;
        session_client(s,i)->resize_restore=desktop_window_at(&s->scene,i)->restore;
        session_client(s,i)->resize_maximized=desktop_window_at(&s->scene,i)->maximized;
    }
    if (pointer_owned(s,i)) pointer_cancel(s);
    struct client *c=session_client(s,i);
    struct desktop_window *w=desktop_window_at(&s->scene,i);
    damage_window(s,i);
    w->frame.w=width+2; w->frame.h=height+45;
    c->generation++; c->configure=1; c->awaiting=1; c->progress=srv_ticks();
    damage_window(s,i);return 1;
}
static void minimize(struct session *s,int i) {
    i=window_root(s,i);if(i<0 || !focus_candidate(s,i,1))return;
    int child=modal_child(s,i);
    damage_window(s,i);desktop_window_at(&s->scene,i)->minimized=1;
    if(child>=0) {damage_window(s,child);desktop_window_at(&s->scene,child)->minimized=1;}
    focus(s,-1);focus_best(s);
    if(s->trace)desk_log("deskd: minimized identity=%llu next=%llu\n",(unsigned long long)session_client(s,i)->identity,(unsigned long long)(s->scene.focus>=0?session_client(s,s->scene.focus)->identity:0));
}
static void maximize(struct session *s,int i) {
    struct desktop_window *w=desktop_window_at(&s->scene,i);damage_window(s,i);
    if(w->maximized) {
        struct desk_rect restore=w->restore;
        if(!configure(s,i,restore.w-2,restore.h-45))return;
        w->frame.x=restore.x;w->frame.y=restore.y;w->maximized=0;
    } else {
        struct desk_rect original=w->frame;
        int width=clamp(s->damage.width-18,280,DESK_MAX_WIDTH);
        int height=clamp(s->damage.height-177,240,DESK_MAX_HEIGHT);
        if(!configure(s,i,width,height))return;
        w->restore=original;w->frame.x=(s->damage.width-width-2)/2;w->frame.y=52;w->maximized=1;
    }
    global_menu_rebuild(s);
    if(s->trace) desk_log("deskd: zoom pid=%llu maximized=%d frame=%d,%d,%d,%d\n",(unsigned long long)session_client(s,i)->pid,w->maximized,w->frame.x,w->frame.y,w->frame.w,w->frame.h);
}
static void arrange_window(struct session *s,int i,unsigned placement) {
    struct desktop_window *w=desktop_window_at(&s->scene,i);struct desk_rect target;
    struct desk_rect original=w->maximized?w->restore:w->frame;
    if(!desktop_window_placement(s->damage.width,s->damage.height,original,placement,&target)) return;
    if(pointer_owned(s,i)) pointer_cancel(s);
    damage_window(s,i);
    struct desk_rect restore=w->maximized?w->restore:w->frame;
    int resize=w->frame.w!=target.w || w->frame.h!=target.h;
    if(resize && !configure(s,i,target.w-2,target.h-45))return;
    w->restore=restore;w->frame.x=target.x;w->frame.y=target.y;w->maximized=placement!=DESKTOP_CENTER;
    damage_window(s,i);
    global_menu_rebuild(s);
    if(s->trace) desk_log("deskd: arranged pid=%llu mode=%u frame=%d,%d,%d,%d generation=%llu\n",
        (unsigned long long)session_client(s,i)->pid,placement,w->frame.x,w->frame.y,w->frame.w,w->frame.h,(unsigned long long)session_client(s,i)->generation);
}
static void spawn_client(struct session *s) {
    int i=child_slot(s);if(i>=0) {
        long pid=spawn_bg("/fat/bin/deskclient");
        if (pid>0) (*session_children(s,i))=(uint64_t)pid;
        else srv_puts("deskd: launch failed\n");
        return;
    }
}
/* Diagnostic Text uses either RAM only or its normal asynchronous file worker. */
static void spawn_capture_text(struct session *s,int files) {
    char *argv[]={"/fat/bin/desktools","text",files==4?"--coexist-control-files-cancel":files==3?"--coexist-control-files":files==2?"--coexist-workspace":files?"--coexist-files":"--coexist-capture",NULL};
    char *env[]={"HOME=" DESK_RAM_HOME,"SRVROS_DESKTOP_HOME=" DESK_RAM_HOME,NULL};
    struct srv_exec_request request={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
        .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
    long pid=srv_exec(&request);
    if(pid>0) { (*session_children(s,0))=pid;(*session_child_apps(s,0))=DESK_APP_TEXT; }
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
static void launch_failed(struct session *s,unsigned application,long error) {
    const struct desk_application_info *info=desk_application_info(application);
    int resource=error==SRV_ERR_AGAIN || error==SRV_ERR_NOMEM;
    const char *reason=resource?"Resources are busy. Close an app and try again.":"Please try opening the application again.";
    snprintf(s->scene.notice,sizeof(s->scene.notice),"%s could not open. %s",info?info->title:"Application",reason);
    desk_log("deskd: launch-failed application=%u error=%lld resource=%u\n",application,(long long)error,(unsigned)resource);
    uint64_t now=srv_ticks();
    if(s->launch_notice_serial && now-s->launch_notice_at<100)return;
    s->launch_notice_at=now;
    struct desk_notification text={0};
    snprintf(text.title,sizeof(text.title),"Cannot open %s",info?info->title:"application");
    snprintf(text.message,sizeof(text.message),"%s",reason);
    uint64_t id;long us=srv_monotonic_us();
    if(desktop_notification_add(&s->scene.notifications.store,UINT64_MAX-3,DESK_APP_SYSTEM,
       ++s->launch_notice_serial,us<0?0:(uint64_t)us,DESK_NOTICE_WARNING,&text,&id)>0) {
        desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);notifications_damage(s);
    }
}
static void spawn_app(struct session *s,int application,const char *path) {
    if(s->exiting)return;
    const struct desk_application_info *info=desk_application_info(application);
    if(!info) return;
    s->last_launch_pid=0;
    int i=child_slot(s);if(i>=0) {
        if(s->worker) {
            for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++) {
                struct life_launch *j=&s->worker->launch[n];
                if(life_state(&j->state)!=LIFE_IDLE)continue;
                if(strlen(path?path:"")>=sizeof(j->path) || strlen(s->home?s->home:DESK_RAM_HOME)>=sizeof(j->home))break;
                j->application=application;j->child_slot=i;
                strcpy(j->path,path?path:"");strcpy(j->home,s->home?s->home:DESK_RAM_HOME);
                s->last_launch_pid=0;life_publish(&j->state,LIFE_QUEUED);return;
            }
            launch_failed(s,application,SRV_ERR_AGAIN);return;
        }
        char *argv[]={(char *)info->program,(char *)info->role,(char *)(path?path:""),NULL};
        char home[192],desktop_home[192];
        snprintf(home,sizeof(home),"HOME=%s",s->home?s->home:DESK_RAM_HOME);
        snprintf(desktop_home,sizeof(desktop_home),"SRVROS_DESKTOP_HOME=%s",s->home?s->home:DESK_RAM_HOME);
        char *env[]={"PATH=/fat/bin:/:/fat",home,desktop_home,NULL};
        struct srv_exec_request request={.path=argv[0],.argv=argv,.envp=env,.flags=SRV_EXEC_BACKGROUND,
            .stdin_fd=-1,.stdout_fd=-1,.stderr_fd=-1};
        uint64_t began=(uint64_t)srv_monotonic_us();
        long pid=srv_exec(&request);
        s->last_launch_pid=pid>0?(uint64_t)pid:0;
        desk_log("desktop-launch: app=%d pid=%lld exec_us=%llu\n",application,(long long)pid,(unsigned long long)((uint64_t)srv_monotonic_us()-began));
        if(pid>0) { (*session_children(s,i))=pid;(*session_child_apps(s,i))=application;s->scene.notice[0]=0; }
        else { launch_failed(s,application,pid);resource_trace(s,"process-launch"); }
        desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
        return;
    }
    resource_trace(s,"launch-capacity");
    launch_failed(s,application,SRV_ERR_NOMEM);
    desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-96,s->damage.width,96});
}
static void lifecycle_poll(struct session *s) {
    if(!s->worker)return;
    for(unsigned n=0;n<LIFE_MEMORY_SLOTS;n++) {
        struct life_memory *m=&s->worker->memory[n];
        if(life_state(&m->state)==LIFE_RETIRED) {
            s->cache_bytes-=(uint64_t)m->width*m->height*4;life_publish(&m->state,LIFE_IDLE);
        }
    }
    for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++) {
        struct life_launch *j=&s->worker->launch[n];
        if(life_state(&j->state)!=LIFE_DONE)continue;
        if(j->pid>0) {
            *session_children(s,j->child_slot)=(uint64_t)j->pid;
            *session_child_apps(s,j->child_slot)=j->application;s->last_launch_pid=j->pid;s->scene.notice[0]=0;
        } else launch_failed(s,j->application,j->pid);
        desk_log("desktop-launch: app=%d pid=%lld exec_us=%llu async=yes\n",j->application,(long long)j->pid,(unsigned long long)j->elapsed);
        life_publish(&j->state,LIFE_IDLE);
    }
}
static int route_document(struct session *s,unsigned application,const char *path) {
    size_t length=strlen(path)+1;
    if(path[0]!='/' || length>160 || s->activation_serial==UINT64_MAX)return 0;
    unsigned chunks=(unsigned)((length+87)/88);
    for(int i=0;i<session_capacity(s);i++) {
        struct client *c=session_client(s,i);
        if(!c->pid || c->closing || c->parent_window || c->open_request ||
           !(c->service_bits&DESK_SERVICE_OPEN_DOCUMENT) || (unsigned)desktop_window_at(&s->scene,i)->application!=application ||
           c->input_count+chunks+4>DESK_INPUT_QUEUE)continue;
        c->open_request=++s->activation_serial;c->open_since=srv_ticks();strcpy(c->open_path,path);
        focus(s,i);
        for(size_t offset=0;offset<length;) {
            struct desk_packet p=desk_packet_new(DESK_OPEN_DOCUMENT);
            unsigned n=length-offset>sizeof(p.text)?sizeof(p.text):(unsigned)(length-offset);
            p.serial=offset;p.height=n;p.width=offset+n==length;p.input_serial=c->open_request;
            memcpy(p.text,path+offset,n);queue_input(s,i,p);offset+=n;
        }
        desk_log("deskd: document-route request=%llu pid=%llu application=%u\n",
            (unsigned long long)c->open_request,(unsigned long long)c->pid,application);
        return 1;
    }
    return 0;
}
static void request_close(struct session *s,int i) {
    if(!s->scene.apps) { close_client(s,i);return; }
    if(session_client(s,i)->close_requested || !session_client(s,i)->pid) return;
    desk_log("deskd: close-request pid=%llu tick=%llu\n",(unsigned long long)session_client(s,i)->pid,(unsigned long long)srv_ticks());
    struct desk_packet p=desk_packet_new(DESK_CLOSE_REQUEST);
    queue_input(s,i,p);session_client(s,i)->close_requested=1;
}
/* Creation identity ordering makes repeated immediate commands visit every
 * root, even though focus changes stacking order. Alt-Tab uses an MRU snapshot. */
static void cycle_windows(struct session *s,int reverse,unsigned application) {
    int origin=window_root(s,s->scene.focus),best=-1,wrap=-1;
    uint64_t current=origin>=0?session_client(s,origin)->identity:0;
    for(int i=0;i<session_capacity(s);i++) {
        if(!focus_candidate(s,i,1) || (application && desktop_window_at(&s->scene,i)->application!=(int)application))continue;
        uint64_t id=session_client(s,i)->identity;
        if(wrap<0 || (reverse?id>session_client(s,wrap)->identity:id<session_client(s,wrap)->identity))wrap=i;
        if((reverse?id<current:id>current) && (best<0 || (reverse?id>session_client(s,best)->identity:id<session_client(s,best)->identity)))best=i;
    }
    if(best<0)best=wrap;
    if(best>=0) {focus(s,best);if(s->trace)desk_log("deskd: window-cycle application=%u identity=%llu window=%llu\n",application,(unsigned long long)session_client(s,best)->identity,(unsigned long long)session_client(s,best)->window);}
}
static void cycle_focus(struct session *s,int reverse) {cycle_windows(s,reverse,0);}
enum global_action { GLOBAL_ABOUT=65536,GLOBAL_SETTINGS,GLOBAL_TEXT,GLOBAL_TERMINAL,GLOBAL_EXIT,
    GLOBAL_MINIMIZE,GLOBAL_ZOOM,GLOBAL_NEXT,GLOBAL_LAUNCHER,GLOBAL_CALENDAR,GLOBAL_HELP,GLOBAL_NOTIFICATIONS,GLOBAL_FILES,GLOBAL_SYSTEM,GLOBAL_CAPTURE,GLOBAL_FIND,GLOBAL_TILE_LEFT,GLOBAL_TILE_RIGHT,GLOBAL_CENTER,GLOBAL_PREVIOUS,GLOBAL_SAME_APP,GLOBAL_WINDOW=65792 };
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
    s->battery_status=value;s->battery_result=(int)result;
    struct desk_battery_view view=desk_battery_view(result?NULL:&value);
    if(view.fresh) {
        s->battery_reads++;
        if(!s->battery_first)s->battery_first=value.generation;
        s->battery_last=value.generation;
    }
    unsigned warning=desk_battery_alert_update(&s->battery_alert,result?NULL:&value);
    if(warning && s->scene.apps && !s->exiting) {
        struct desk_notification text={0};
        strcpy(text.title,warning==2?"Battery very low":"Battery low");
        snprintf(text.message,sizeof(text.message),"%u%% remaining. %s",value.soc,
            warning==2?"Connect power now and save your work.":"Connect power soon.");
        uint64_t id;long us=srv_monotonic_us();
        if(desktop_notification_add(&s->scene.notifications.store,UINT64_MAX-1,DESK_APP_SYSTEM,
            ++s->battery_notice_serial,us<0?0:(uint64_t)us,DESK_NOTICE_WARNING,&text,&id)>0) {
            desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);
            notifications_damage(s);
            desk_log("deskd: battery notice level=%u soc=%u\n",warning,value.soc);
        }
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
    for(int n=0;n<session_capacity(s);n++) if(session_client(s,n)->pid) session_client(s,n)->preferences_dirty=1;
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
    struct desk_menu_item inline_items[DESK_MENU_ITEMS]={0};
    struct desk_menu_item *items=s->menu_build?s->menu_build:inline_items;
    memset(items,0,(s->menu_build?session_capacity(s)+DESK_MENU_ITEMS:DESK_MENU_ITEMS)*sizeof(*items));
    unsigned count=sizeof(system_items)/sizeof(*system_items);
    memcpy(items,system_items,sizeof(system_items));
    if(s->scene.focus>=0) {
        struct desk_menu_store *m=&session_client(s,s->scene.focus)->menus;
        memcpy(items+count,m->items,m->count*sizeof(items[0]));count+=m->count;
    }
    unsigned disabled=s->scene.focus<0?DESK_MENU_DISABLED:0;
    struct desk_rect unused,original={0};if(s->scene.focus>=0) original=desktop_window_at(&s->scene,s->scene.focus)->frame;
    unsigned tile_disabled=disabled|(!desktop_window_placement(s->damage.width,s->damage.height,original,DESKTOP_TILE_LEFT,&unused)?DESK_MENU_DISABLED:0);
    items[count++]=(struct desk_menu_item){0,DESK_MENU_HEADING,"Arrange",""};
    items[count++]=(struct desk_menu_item){GLOBAL_TILE_LEFT,tile_disabled,"Tile left","Cmd-Alt-Left"};
    items[count++]=(struct desk_menu_item){GLOBAL_TILE_RIGHT,tile_disabled,"Tile right","Cmd-Alt-Right"};
    items[count++]=(struct desk_menu_item){GLOBAL_CENTER,disabled,"Center","Cmd-Alt-Up"};
    items[count++]=(struct desk_menu_item){0,DESK_MENU_HEADING,"Window",""};
    items[count++]=(struct desk_menu_item){GLOBAL_MINIMIZE,disabled,"Minimize","Cmd-M"};
    items[count]=(struct desk_menu_item){GLOBAL_ZOOM,disabled,"Maximize","Cmd-Alt-Down"};
    if(s->scene.focus>=0 && desktop_window_at(&s->scene,s->scene.focus)->maximized) strcpy(items[count].label,"Restore size");
    count++;
    items[count++]=(struct desk_menu_item){GLOBAL_NEXT,0,"Next window","Alt-Tab"};
    items[count++]=(struct desk_menu_item){GLOBAL_PREVIOUS,0,"Previous window","Alt-Shift-Tab"};
    items[count++]=(struct desk_menu_item){GLOBAL_SAME_APP,disabled,"Next app window","Cmd-`"};
    int have=0;
    for(int i=0;i<session_capacity(s);i++) if(focus_candidate(s,i,1)) {
        if(!have++) items[count++]=(struct desk_menu_item){0,DESK_MENU_SEPARATOR,"",""};
        struct desk_menu_item *item=&items[count++];item->id=GLOBAL_WINDOW+session_client(s,i)->identity;
        item->flags=s->scene.focus==i?DESK_MENU_CHECKED:0;
        size_t n=strlen(desktop_window_at(&s->scene,i)->title);if(n>=sizeof(item->label)) n=sizeof(item->label)-1;
        while(n && !desk_text_utf8_valid(desktop_window_at(&s->scene,i)->title,n)) n--;
        if(n) memcpy(item->label,desktop_window_at(&s->scene,i)->title,n);else strcpy(item->label,"Window");
    }
    uint64_t owner=s->scene.focus>=0?session_client(s,s->scene.focus)->pid:0;
    uint64_t window=s->scene.focus>=0?session_client(s,s->scene.focus)->window:0;
    if(s->scene.menu.font && owner==s->menu_owner && window==s->menu_window) desk_menu_replace(&s->scene.menu,items,count);
    else desk_menu_configure(&s->scene.menu,items,count,(struct desk_rect){10,0,s->damage.width-(s->damage.width<1000?428:578),44},
        (struct desk_rect){0,0,s->damage.width,s->damage.height},&desk_font_16);
    s->menu_owner=owner;s->menu_window=window;
    global_menu_damage(s);
    if(s->scene.launcher_open) launcher_catalog(s);
}
static void global_menu_action(struct session *s,uint32_t id) {
    if(!id) return;
    if(id<65536) {
        int i=s->scene.focus;if(i<0 || session_client(s,i)->closing) return;
        struct desk_packet p=desk_packet_new(DESK_MENU_ACTION);p.value=id;p.serial=session_client(s,i)->menus.revision;
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
        const struct desk_application_info *info=s->scene.focus>=0?desk_application_info(desktop_window_at(&s->scene,s->scene.focus)->application):NULL;
        spawn_app(s,DESK_APP_HELP,info?info->role:"welcome");
    }
    else if(id==GLOBAL_SETTINGS) spawn_app(s,DESK_APP_SETTINGS,"");
    else if(id==GLOBAL_TEXT) spawn_app(s,DESK_APP_TEXT,"");
    else if(id==GLOBAL_TERMINAL) spawn_app(s,DESK_APP_TERMINAL,"");
    else if(id==GLOBAL_EXIT) {
        /* Owned dialogs resolve their document's close request. Closing them as
         * independent roots cancels or recreates the very decision we await. */
        s->exiting=1;s->exit_wait=0;
        for(int i=0;i<session_capacity(s);i++)
            if(session_client(s,i)->pid && !session_client(s,i)->parent_window)request_close(s,i);
    } else if(id==GLOBAL_MINIMIZE && s->scene.focus>=0) minimize(s,s->scene.focus);
    else if(id==GLOBAL_ZOOM && s->scene.focus>=0) maximize(s,s->scene.focus);
    else if(s->scene.focus>=0 && (id==GLOBAL_TILE_LEFT || id==GLOBAL_TILE_RIGHT || id==GLOBAL_CENTER))
        arrange_window(s,s->scene.focus,id==GLOBAL_TILE_LEFT?DESKTOP_TILE_LEFT:id==GLOBAL_TILE_RIGHT?DESKTOP_TILE_RIGHT:DESKTOP_CENTER);
    else if(id==GLOBAL_NEXT) cycle_focus(s,0);
    else if(id==GLOBAL_PREVIOUS) cycle_focus(s,1);
    else if(id==GLOBAL_SAME_APP && s->scene.focus>=0)cycle_windows(s,0,desktop_window_at(&s->scene,s->scene.focus)->application);
    else if(id>=GLOBAL_WINDOW)for(int i=0;i<session_capacity(s);i++)
        if(session_client(s,i)->pid && !session_client(s,i)->closing && session_client(s,i)->identity==id-GLOBAL_WINDOW) { focus(s,i);break; }
}
static void global_menu_result(struct session *s,struct desk_menu_result result) {
    if(result.redraw) global_menu_damage(s);
    global_menu_action(s,result.action);
}
static void launcher_damage(struct session *s) {
    struct desk_rect r=s->scene.launcher.frame;r.w+=5;r.h+=6;desk_damage_add(&s->damage,r);
}
static void launcher_catalog(struct session *s) {
    if(s->switching)return; /* Do not reorder the held-key selection under the user. */
    struct desk_palette_item inline_items[DESKD_WINDOWS+DESK_APP_COUNT]={0};
    struct desk_palette_item *items=s->palette_build?s->palette_build:inline_items;unsigned count=0;
    memset(items,0,(session_capacity(s)+DESK_APP_COUNT)*sizeof(*items));
    int inline_order[DESKD_WINDOWS];int *order=s->scene.order?s->scene.order:inline_order;desktop_stack(&s->scene,order);
    for(int n=session_capacity(s)-1;n>=0;n--) {
        int i=order[n];uint64_t pid=session_client(s,i)->pid;
        if(!pid || !session_client(s,i)->identity || session_client(s,i)->closing) continue;
        if(s->launcher_application && (desktop_window_at(&s->scene,i)->application!=(int)s->launcher_application || session_client(s,i)->parent_window))continue;
        struct desk_palette_item *item=&items[count++];item->id=session_client(s,i)->identity+DESK_APP_COUNT;
        snprintf(item->label,sizeof(item->label),"%s",desktop_window_at(&s->scene,i)->title[0]?desktop_window_at(&s->scene,i)->title:"Window");
        strcpy(item->detail,desktop_window_at(&s->scene,i)->minimized?"Hidden window":"Open window");
    }
    for(unsigned app=1;app<=DESK_APP_COUNT;app++) {
        if(s->launcher_application && app!=s->launcher_application)continue;
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
    s->launcher_application=0;
    if(s->scene.calendar_open) calendar_close(s);
    s->overlay_return=s->scene.focus;s->overlay_identity=s->scene.focus>=0?session_client(s,s->scene.focus)->identity:0;s->overlay_down=0;
    desk_menu_cancel(&s->scene.menu);global_menu_damage(s);focus(s,-1);
    launcher_catalog(s);struct desk_palette *p=&s->scene.launcher;
    desk_palette_open(p,"Open application or window",p->items,p->count);
    desk_palette_configure(p,(struct desk_rect){0,44,s->damage.width,s->damage.height-120});
    s->scene.launcher_open=1;launcher_damage(s);
    if(s->trace) desk_log("deskd: launcher opened items=%u\n",p->count);
}
static void dock_windows(struct session *s,unsigned app) {
    launcher_open(s);if(!s->scene.launcher_open)return;
    s->launcher_application=app;launcher_catalog(s);
    char title[96];snprintf(title,sizeof(title),"%s windows",desk_application_info(app)->title);
    struct desk_palette *p=&s->scene.launcher;desk_palette_open(p,title,p->items,p->count);
    desk_palette_configure(p,(struct desk_rect){0,44,s->damage.width,s->damage.height-120});launcher_damage(s);
    if(s->trace)desk_log("deskd: dock windows application=%u items=%u\n",app,p->count);
}
static void launcher_close(struct session *s) {
    if(!s->scene.launcher_open) return;
    launcher_damage(s);desk_palette_focus(&s->scene.launcher,0);s->scene.launcher_open=0;s->switching=0;
    if(s->overlay_down) s->pointer_blocked=1;
    int i=s->overlay_return;if(!window_lifetime_valid(s,i,s->overlay_identity))i=-1;
    if(i>=0)focus(s,i);else focus_best(s);if(s->trace) srv_puts("deskd: launcher closed\n");
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
        uint64_t identity=result.id-DESK_APP_COUNT;
        for(int i=0;i<session_capacity(s);i++) if(session_client(s,i)->pid && session_client(s,i)->identity==identity && !session_client(s,i)->closing) {
            focus(s,i);if(s->trace) desk_log("deskd: launcher window=%llu id=%llu\n",(unsigned long long)session_client(s,i)->pid,(unsigned long long)session_client(s,i)->window);break;
        }
    }
}
static int switcher_target(const struct session *s,uint64_t identity) {
    for(int i=0;i<session_capacity(s);i++)if(focus_candidate(s,i,1) && session_client(s,i)->identity+DESK_APP_COUNT==identity)return i;
    return -1;
}
static void switcher_step(struct session *s,int reverse) {
    struct desk_palette *p=&s->scene.launcher;
    if(!s->switching) {
        if(s->scene.launcher_open)launcher_close(s);
        launcher_open(s);if(!s->scene.launcher_open)return;
        struct desk_palette_item inline_items[DESKD_WINDOWS+DESK_APP_COUNT]={0};
        struct desk_palette_item *items=s->palette_build?s->palette_build:inline_items;unsigned count=0;
        int inline_order[DESKD_WINDOWS];int *order=s->scene.order?s->scene.order:inline_order;desktop_stack(&s->scene,order);
        int origin=window_root(s,s->overlay_return),selected=-1;
        for(int n=session_capacity(s)-1;n>=0;n--) {
            int i=order[n];if(!focus_candidate(s,i,1))continue;
            if(i==origin)selected=count;
            struct desk_palette_item *item=&items[count++];item->id=session_client(s,i)->identity+DESK_APP_COUNT;
            snprintf(item->label,sizeof(item->label),"%s",desktop_window_at(&s->scene,i)->title[0]?desktop_window_at(&s->scene,i)->title:"Window");
            snprintf(item->detail,sizeof(item->detail),"%s%s",desk_application_info(desktop_window_at(&s->scene,i)->application)?desk_application_info(desktop_window_at(&s->scene,i)->application)->title:"Application",desktop_window_at(&s->scene,i)->minimized?" · Minimized":"");
        }
        desk_palette_choose(p,"Switch window",items,count);desk_palette_configure(p,(struct desk_rect){0,44,s->damage.width,s->damage.height-120});
        desk_palette_message(p,"Alt-Tab: next · Shift: reverse · release Alt: switch · Esc: cancel");
        desk_list_select(&p->list,selected);desk_ui_focus(&p->ui,2);s->switching=1;
    }
    int selected=p->list.selected;
    for(int n=0;n<p->list.count;n++) {
        selected=selected<0?(reverse?p->list.count-1:0):(selected+(reverse?p->list.count-1:1))%p->list.count;
        if(switcher_target(s,p->visible[selected].id)>=0) {desk_list_select(&p->list,selected);break;}
    }
    launcher_damage(s);
    if(s->trace)desk_log("deskd: switcher selected=%llu count=%d\n",(unsigned long long)(p->list.selected>=0?p->visible[p->list.selected].id:0),p->list.count);
}
static int switcher_key(struct session *s,const struct srv_key_event *key) {
    if(!s->switching)return 0;
    if((key->flags&SRV_KEY_CANCEL) || ((key->flags&SRV_KEY_DOWN_EVENT) && key->usage==SRV_KEY_ESCAPE)) {
        launcher_close(s);return 1;
    }
    if(!(key->modifiers&SRV_KEY_ALT) || ((key->flags&SRV_KEY_DOWN_EVENT) && key->usage==SRV_KEY_ENTER)) {
        struct desk_palette *p=&s->scene.launcher;
        uint64_t identity=p->list.selected>=0?p->visible[p->list.selected].id:0;
        int target=switcher_target(s,identity);launcher_close(s);
        if(target>=0) {focus(s,target);if(s->trace)desk_log("deskd: switcher activated identity=%llu pid=%llu window=%llu\n",(unsigned long long)session_client(s,target)->identity,(unsigned long long)session_client(s,target)->pid,(unsigned long long)session_client(s,target)->window);}
        return 1;
    }
    if((key->flags&SRV_KEY_DOWN_EVENT) && (key->usage==SRV_KEY_TAB || key->usage==SRV_KEY_DOWN || key->usage==SRV_KEY_UP))
        switcher_step(s,key->usage==SRV_KEY_UP || (key->modifiers&SRV_KEY_SHIFT));
    return 1; /* No held-key text/Tab leaks into the outgoing or incoming app. */
}
static int navigation_key(struct session *s,const struct srv_key_event *key) {
    if(switcher_key(s,key))return 1;
    if(!(key->flags&SRV_KEY_DOWN_EVENT) || (key->flags&SRV_KEY_CANCEL))return 0;
    unsigned modifiers=key->modifiers;
    if(key->usage==SRV_KEY_TAB && (modifiers&SRV_KEY_ALT) && !(modifiers&(SRV_KEY_CTRL|SRV_KEY_META))) {
        pointer_cancel(s);context_close(s);switcher_step(s,(modifiers&SRV_KEY_SHIFT)!=0);return 1;
    }
    if(s->scene.launcher_open || s->scene.calendar_open || s->scene.notifications.opened || desk_popup_active(&s->scene.context) || s->scene.menu.open>=0)return 0;
    if(key->usage==SRV_KEY_F2 && (modifiers&SRV_KEY_CTRL) && !(modifiers&(SRV_KEY_META|SRV_KEY_ALT|SRV_KEY_SHIFT))) { /* Ctrl-F2 */
        struct srv_key_event menu=*key;menu.usage=SRV_KEY_F10;menu.modifiers=0;
        pointer_cancel(s);global_menu_result(s,desk_menu_key(&s->scene.menu,&menu));return 1;
    }
    if((modifiers&SRV_KEY_META) && !(modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT))) {
        if(key->usage==0x35) { /* HID grave; Shift reverses same-app cycling. */
            if(s->scene.focus>=0)cycle_windows(s,(modifiers&SRV_KEY_SHIFT)!=0,desktop_window_at(&s->scene,s->scene.focus)->application);
            return 1;
        }
        if(key->usage==0x10 && !(modifiers&SRV_KEY_SHIFT)) { /* HID M */
            if(!(key->flags&SRV_KEY_REPEAT) && s->scene.focus>=0)minimize(s,s->scene.focus);
            return 1;
        }
    }
    return 0;
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
    s->overlay_return=s->scene.focus;s->overlay_identity=s->scene.focus>=0?session_client(s,s->scene.focus)->identity:0;s->overlay_down=0;pointer_cancel(s);
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
    int i=s->overlay_return;if(!window_lifetime_valid(s,i,s->overlay_identity))i=-1;
    if(i>=0)focus(s,i);else focus_best(s);if(s->trace) srv_puts("deskd: calendar closed\n");
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
    s->overlay_return=s->scene.focus;s->overlay_identity=s->scene.focus>=0?session_client(s,s->scene.focus)->identity:0;s->overlay_down=0;pointer_cancel(s);
    desk_menu_cancel(&s->scene.menu);global_menu_damage(s);focus(s,-1);
    desktop_notice_view_configure(&s->scene.notifications,s->damage.width,s->damage.height);
    desktop_notice_view_open(&s->scene.notifications);notifications_damage(s);
    if(s->trace) desk_log("deskd: notifications opened count=%u\n",s->scene.notifications.store.count);
}
static void notifications_close(struct session *s) {
    if(!s->scene.notifications.opened) return;
    notifications_damage(s);desktop_notice_view_close(&s->scene.notifications);
    if(s->overlay_down) s->pointer_blocked=1;
    int i=s->overlay_return;if(!window_lifetime_valid(s,i,s->overlay_identity))i=-1;
    if(i>=0)focus(s,i);else focus_best(s);if(s->trace) srv_puts("deskd: notifications closed\n");
}
static void notifications_apply(struct session *s,struct desktop_notice_action result) {
    if(result.close) notifications_close(s);
    if(result.application) {
        int owner=-1;for(int i=0;i<session_capacity(s);i++) if(session_client(s,i)->pid==result.owner && (!result.window || session_client(s,i)->window==result.window) && !session_client(s,i)->closing) owner=i;
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
    else desk_log("deskd: screenshot captured path=%s width=%d height=%d\n",path,s->damage.width,s->damage.height);
    global_menu_rebuild(s);
}
static void notification_received(struct session *s,int i,const struct desk_packet *p) {
    struct client *c=session_client(s,i);
    if(!s->scene.apps || c->closing || s->exiting || !p->serial) return;
    c->notice_ack_serial=p->serial;c->notice_ack_pending=1;
    if(p->serial<=c->notice_last) { c->notice_ack_result=p->serial==c->notice_last?c->notice_last_result:1;return; }
    c->notice_last=p->serial;c->notice_last_result=c->notice_ack_result=1;
    uint64_t now=srv_ticks();struct desk_notification text;memcpy(&text,p->text,sizeof(text));
    if(p->generation || p->surface || p->width || p->height || p->x || p->y || p->input_serial ||
        !desktop_notification_valid(&text,p->value) || (c->notice_has_time && (now<c->notice_at || now-c->notice_at<100))) return;
    long us=srv_monotonic_us();uint64_t id;
    int added=desktop_notification_add_window(&s->scene.notifications.store,c->pid,c->window,desktop_window_at(&s->scene,i)->application,p->serial,us<0?0:(uint64_t)us,p->value,&text,&id);
    if(added<0) return;
    c->notice_ack_result=c->notice_last_result=0;c->notice_at=now;c->notice_has_time=1;
    if(added) { desktop_notice_view_arrived(&s->scene.notifications,id,us<0?0:(uint64_t)us);notifications_damage(s); }
    if(s->trace) desk_log("deskd: notification accepted owner=%llu id=%llu count=%u\n",(unsigned long long)c->pid,(unsigned long long)id,s->scene.notifications.store.count);
}
static void close_client(struct session *s,int i) {
    if (pointer_owned(s,i)) {
        s->pointer_pid=0; s->pointer_blocked=1;
    }
    if (!session_client(s,i)->closing) {
        session_client(s,i)->closing=1;
        session_client(s,i)->close_sent=0;
        session_client(s,i)->progress=srv_ticks();
    }
}
static void incoming(struct session *s,uint64_t pid,struct desk_packet *p) {
    int child=child_index(s,pid);if(!p->window)return;
    if(s->startup_trace && p->type==DESK_OPEN)desk_log("desktop-open: pid=%llu child=%d window=%llu flags=%u generation=%llu mode=%u at_us=%llu\n",
        (unsigned long long)pid,child,(unsigned long long)p->window,p->value,(unsigned long long)p->generation,
        child<0?0:*session_child_mode(s,child),(unsigned long long)srv_monotonic_us());
    if(child<0 && p->type==DESK_OPEN && s->worker) {
        int launching=0;
        for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++)launching|=life_state(&s->worker->launch[n].state)!=LIFE_IDLE;
        if(launching && s->pending_opens<LIFE_LAUNCH_SLOTS) {
            s->pending_open[s->pending_opens].pid=pid;s->pending_open[s->pending_opens++].packet=*p;
        }
        return;
    }
    int i;
    for(i=0;i<session_capacity(s);i++)if(session_client(s,i)->pid==pid && session_client(s,i)->window==p->window)break;
    if(p->type==DESK_OPEN) {
        if(child<0)return;
        if(session_child_reply(s,child)->type)return; /* One creation credit per connection. */
        int managed=(p->value&DESK_OPEN_CONNECTION)!=0;
        if(!(*session_child_mode(s,child)))(*session_child_mode(s,child))=managed?2:1;
        if(managed) {
            (*session_child_reply(s,child))=desk_packet_new(DESK_OPEN_REPLY);
            session_child_reply(s,child)->window=p->window;session_child_reply(s,child)->value=DESK_OPEN_INVALID;
            (*session_child_reply_blocked(s,child))=0;
        }
        if((p->value&~(DESK_OPEN_CONNECTION|DESK_OPEN_MODAL)) || ((p->value&DESK_OPEN_MODAL) && !managed) || (*session_child_mode(s,child))!=(unsigned)(managed?2:1) ||
           (!managed && p->window!=DESK_DEFAULT_WINDOW) || i<session_capacity(s))return;
        if(managed && p->window<=(*session_child_last_window(s,child)))return;
        (*session_child_last_window(s,child))=p->window;
        int parent=-1;
        if(p->value&DESK_OPEN_MODAL) {
            for(int j=0;j<session_capacity(s);j++) {
                const struct client *candidate=session_client(s,j);
                if(candidate->pid==pid && candidate->window==p->generation && !candidate->parent_window && !candidate->closing)parent=j;
            }
            if(parent<0 || modal_child(s,parent)>=0)return;
            if(s->refuse_dialogs) {
                session_child_reply(s,child)->value=DESK_OPEN_MEMORY;
                desk_log("deskd: dialog admission refused parent=%llu\n",(unsigned long long)p->generation);return;
            }
        } else if(p->generation)return;
        for(i=0;i<session_capacity(s);i++)if(!session_client(s,i)->pid)break;
        if((i==session_capacity(s) && !windows_grow(s)) || s->window_serial>=UINT32_MAX-GLOBAL_WINDOW) {
            if(managed)session_child_reply(s,child)->value=DESK_OPEN_CAPACITY;
            resource_trace(s,"window-capacity");return;
        }
        struct desktop_window *w=desktop_window_at(&s->scene,i);
        session_client(s,i)->pid=pid;session_client(s,i)->window=p->window;session_client(s,i)->identity=++s->window_serial;session_client(s,i)->managed=managed;session_client(s,i)->clipboard.pid=pid;session_client(s,i)->clipboard.window=p->window;session_client(s,i)->preferences_dirty=s->scene.apps;
        w->visible=1;w->dialog=parent>=0;session_client(s,i)->parent_window=parent<0?0:p->generation;
        for(int n=0;n<children_capacity(s);n++) if((*session_children(s,n))==pid) w->application=(*session_child_apps(s,n));
        w->frame.x=32+24*(i%4); w->frame.y=64+24*(i%4);
        if(!s->scene.apps) { w->frame.x=32+40*i;w->frame.y=64+36*i; }
        memcpy(w->title,p->text,sizeof(p->text)); w->title[sizeof(p->text)]=0;
        if(!configure(s,i,clamp(p->width,280,s->damage.width-w->frame.x-8<DESK_MAX_WIDTH?
            s->damage.width-w->frame.x-8:DESK_MAX_WIDTH),
            clamp(p->height,240,s->damage.height-w->frame.y-(s->scene.apps?125:85)<DESK_MAX_HEIGHT?
            s->damage.height-w->frame.y-(s->scene.apps?125:85):DESK_MAX_HEIGHT))) {
            if(managed) { session_child_reply(s,child)->value=DESK_OPEN_MEMORY;forget(s,i); }
            else close_client(s,i);
            return;
        }
        if(parent>=0) {
            struct desk_rect origin=desktop_window_at(&s->scene,parent)->frame;
            w->frame.x=clamp(origin.x+(origin.w-w->frame.w)/2,0,s->damage.width-w->frame.w);
            w->frame.y=clamp(origin.y+44,44,s->damage.height-100-w->frame.h);
        }
        if(managed)session_child_reply(s,child)->value=DESK_OPEN_OK;
        focus(s,i);
        /* Save/Discard/Cancel and its picker must remain interactive while
         * exiting. A late root joins cooperative shutdown, never forced close. */
        if(s->exiting && parent<0)request_close(s,i);
        desk_log("deskd: opened pid=%llu window=%d id=%llu parent=%llu\n",(unsigned long long)pid,i,(unsigned long long)p->window,(unsigned long long)session_client(s,i)->parent_window);
        if(s->trace)desk_log("deskd: geometry pid=%llu window=%llu frame=%d,%d,%d,%d\n",(unsigned long long)pid,(unsigned long long)p->window,w->frame.x,w->frame.y,w->frame.w,w->frame.h);
        resource_trace(s,"opened");
        return;
    }
    if(i==session_capacity(s))return;
    struct client *c=session_client(s,i); struct desktop_window *w=desktop_window_at(&s->scene,i);
    if(p->type==DESK_RESIZE_REFUSED) {
        if(c->awaiting && p->generation==c->generation && w->pixels &&
           p->width==(unsigned)w->width && p->height==(unsigned)w->height) {
            struct desk_rect origin=c->resize_origin,restore=c->resize_restore;int maximized=c->resize_maximized;
            damage_window(s,i);
            if(configure(s,i,w->width,w->height)) {
                w->frame=origin;w->restore=restore;w->maximized=maximized;damage_window(s,i);memory_notice(s);
                desk_log("deskd: resize restored pid=%llu generation=%llu\n",(unsigned long long)pid,(unsigned long long)c->generation);
            }
        }
        return;
    }
    if (p->type==DESK_CLOSE) {
        if(c->managed) { close_client(s,i);c->retiring=1; }else forget(s,i);
        return;
    }
    if(p->type==DESK_APP_SERVICE) {
        if(!c->parent_window && !(p->value&~DESK_SERVICE_OPEN_DOCUMENT) &&
           (w->application==DESK_APP_TEXT || w->application==DESK_APP_PREVIEW))c->service_bits=p->value;
        return;
    }
    if(p->type==DESK_OPEN_DOCUMENT_REPLY) {
        if(!c->open_request || p->serial!=c->open_request || p->value>1)return;
        char path[160];strcpy(path,c->open_path);c->open_request=0;
        desk_log("deskd: document-route reply pid=%llu accepted=%u\n",(unsigned long long)pid,p->value==0);
        if(p->value && !s->exiting && !c->closing)spawn_app(s,w->application,path);
        return;
    }
    if(p->type==DESK_NOTIFY) { notification_received(s,i,p);return; }
    if(p->type==DESK_MENU_SET) {
        if(s->scene.apps && desk_menu_receive(&c->menus,p) && s->scene.focus==i) global_menu_rebuild(s);
        return;
    }
    if(p->type==DESK_CLIPBOARD_SET || p->type==DESK_CLIPBOARD_GET) {
        if(s->scene.apps || s->ram_text) desk_clipboard_request(&clipboard,&c->clipboard,s->scene.focus==i,p,srv_ticks());
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
    if(p->type==DESK_LAUNCH && s->scene.apps && !s->exiting) {
        if(!p->serial) { c->launch_length=0;c->launch_application=p->value; }
        if(p->serial!=c->launch_length || p->height>sizeof(p->text) || !p->height ||
           p->height>sizeof(c->launch_path)-c->launch_length || p->value!=c->launch_application || p->width>1) {
            c->launch_length=0;return;
        }
        memcpy(c->launch_path+c->launch_length,p->text,p->height);c->launch_length+=p->height;
        if(p->width) {
            if(c->launch_path[c->launch_length-1]==0 && strlen(c->launch_path)==c->launch_length-1) {
                if(!route_document(s,c->launch_application,c->launch_path))spawn_app(s,c->launch_application,c->launch_path);
            }
            c->launch_length=0;
        }
        return;
    }
    if (p->type!=DESK_COMMIT) return;
    /* A conforming client has at most one unreleased commit. */
    if (c->release.type) { desk_log("deskd: closing pid=%llu reason=unreleased-buffer\n",(unsigned long long)pid);close_client(s,i); return; }
    c->release=*p; c->release.type=DESK_RELEASE;c->release.value=0; c->progress=srv_ticks();
    if (p->generation!=c->generation || p->width!=(uint32_t)(w->frame.w-2) ||
        p->height!=(uint32_t)(w->frame.h-45) || c->closing) {
        desk_log("deskd: stale commit released pid=%llu\n",(unsigned long long)pid); return;
    }
    /* Keep displaying the front until a complete copy succeeds. */
    uint32_t *next=cache_prepare(s,i,p->width,p->height);
    if (!next) {
        c->release.value=DESK_RELEASE_RETRY;
        if(!s->cache_pending) {memory_notice(s);
            desk_log("deskd: deferred frame pid=%llu reason=cache-memory\n",(unsigned long long)pid);}
        return;
    }
    if (p->width>DESK_MAX_WIDTH || p->height>DESK_MAX_HEIGHT ||
        gui_surface_copy(p->surface,0,0,p->width,p->height,next,p->width)!=0) {
        desk_log("deskd: closing pid=%llu reason=surface-copy\n",(unsigned long long)pid);
        close_client(s,i); return;
    }
    c->present_input=p->input_serial;
    int first_frame=w->pixels==NULL || c->awaiting;
    w->pixels=next; w->width=p->width; w->height=p->height;
    c->awaiting=0;cache_trim(s,i,(uint64_t)p->width*p->height*4);
    damage_window(s,i);
    if (s->trace && (!s->scene.apps || first_frame)) desk_log("deskd: commit pid=%llu generation=%llu size=%ux%u\n",
        (unsigned long long)pid,(unsigned long long)p->generation,p->width,p->height);
}
static void lifecycle_deliver(struct session *s) {
    int launching=0;
    if(s->worker)for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++)launching|=life_state(&s->worker->launch[n].state)!=LIFE_IDLE;
    for(unsigned n=0;n<s->pending_opens;) {
        uint64_t pid=s->pending_open[n].pid;
        if(child_index(s,pid)<0 && launching){n++;continue;}
        struct desk_packet p=s->pending_open[n].packet;
        s->pending_open[n]=s->pending_open[--s->pending_opens];
        if(child_index(s,pid)>=0)incoming(s,pid,&p);
    }
}
/* Queue age also includes time the compositor could not run. RELEASE and
 * CONFIGURE must get a delivery attempt before timeout attribution, just like
 * input. Start the response deadline only once the client can see the request. */
static int control_send(struct session *s,int i,const struct desk_packet *p) {
    struct client *c=session_client(s,i);
    c->input_blocked=0; /* Control traffic interrupted input delivery attempts. */
    if(!client_send(c,p)) {
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
    for(int n=0;n<children_capacity(s);n++)if((*session_children(s,n)) && session_child_reply(s,n)->type) {
        if(!desk_send(*session_children(s,n),session_child_reply(s,n))) { session_child_reply(s,n)->type=0;*session_child_reply_blocked(s,n)=0; }
        else if(!(*session_child_reply_blocked(s,n))) { (*session_child_reply_blocked(s,n))=1;(*session_child_reply_since(s,n))=srv_ticks(); }
        else if((uint64_t)srv_ticks()-(*session_child_reply_since(s,n))>500)srv_kill((*session_children(s,n)));
    }

    for (int i=0;i<session_capacity(s);i++) {
        struct client *c=session_client(s,i);
        if (!c->pid) continue;
        if(c->open_request && (uint64_t)srv_ticks()-c->open_since>1000) {
            desk_log("deskd: document-route unconfirmed pid=%llu request=%llu\n",(unsigned long long)c->pid,(unsigned long long)c->open_request);
            c->open_request=0;c->service_bits=0;
            strcpy(s->scene.notice,"Open was not confirmed. Check app windows before retrying.");
            desk_damage_add(&s->damage,(struct desk_rect){0,s->damage.height-112,s->damage.width,112});
        }
        if (!desk_process_alive(c->pid)) { forget(s,i); continue; }
        int child=child_index(s,c->pid);
        if(child>=0 && session_child_reply(s,child)->type)continue;
        if (c->release.type) {
            if (!control_send(s,i,&c->release)) continue;
            c->release.type=0;
        }
        if(c->retiring) {
            struct desk_packet p=desk_packet_new(DESK_CLOSE_ACK);
            if(control_send(s,i,&p))forget(s,i);
            continue;
        }
        if (c->closing) {
            if(!c->close_sent) {
                struct desk_packet p=desk_packet_new(DESK_CLOSE);
                if(!control_send(s,i,&p)) continue;
                c->close_sent=1;
            }
        } else if (c->configure) {
            if(s->worker && !cache_prepare(s,i,desktop_window_at(&s->scene,i)->frame.w-2,desktop_window_at(&s->scene,i)->frame.h-45)) {
                if(!s->cache_pending) {
                    memory_notice(s);
                    if(!c->sent_generation)close_client(s,i);
                    else {
                        /* Nothing for this resize was delivered. Restore the
                         * last delivered geometry without invalidating its
                         * still-live surfaces or the published front. */
                        struct desktop_window *w=desktop_window_at(&s->scene,i);
                        damage_window(s,i);w->frame=c->sent_frame;w->restore=c->sent_restore;w->maximized=c->sent_maximized;
                        c->generation=c->sent_generation;c->configure=0;
                        c->awaiting=!w->pixels || w->width!=w->frame.w-2 || w->height!=w->frame.h-45;
                        damage_window(s,i);
                    }
                }continue;
            }
            struct desk_packet p=desk_packet_new(DESK_CONFIGURE);
            p.generation=c->generation;
            p.width=desktop_window_at(&s->scene,i)->frame.w-2; p.height=desktop_window_at(&s->scene,i)->frame.h-45;
            if(!control_send(s,i,&p)) continue;
            c->configure=0;c->sent_generation=c->generation;
            c->sent_frame=desktop_window_at(&s->scene,i)->frame;c->sent_restore=desktop_window_at(&s->scene,i)->restore;c->sent_maximized=desktop_window_at(&s->scene,i)->maximized;
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
            if(!client_send(c,&p)) c->notice_ack_pending=0;
        }
        if(c->audio_reply.type && !c->configure && !c->closing &&
           !client_send(c,&c->audio_reply)) c->audio_reply.type=0;
        if(c->brightness_reply.type && !c->configure && !c->closing &&
           !client_send(c,&c->brightness_reply)) c->brightness_reply.type=0;
        if(c->preferences_dirty && !c->configure && !c->closing) {
            struct desk_packet p=desk_packet_new(DESK_PREFERENCES);p.value=s->preferences.status;p.serial=s->preferences.revision;
            memcpy(p.text,&s->preferences.value,sizeof(s->preferences.value));
            if(!client_send(c,&p)) c->preferences_dirty=0;
        }
        /* Input throughput must not be limited to the render-loop rate. Keep
         * a per-client budget so one busy window cannot monopolize service. */
        for(unsigned delivered=0;delivered<DESK_INPUT_BATCH && c->input_count && !c->configure && !c->closing;delivered++) {
            if(client_send(c,&c->input[c->input_head])!=0) {
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
        if((s->scene.apps || s->ram_text) && !c->closing) desk_clipboard_service(&clipboard,&c->clipboard,srv_ticks());
    }
    for (int i=0;i<children_capacity(s);i++) if ((*session_children(s,i))) {
        uint64_t status=0;
        uint64_t began=(uint64_t)srv_monotonic_us();
        long waited=srv_wait((*session_children(s,i)),&status,SRV_WAIT_NOHANG);
        uint64_t elapsed=(uint64_t)srv_monotonic_us()-began;
        if(elapsed>50000)desk_log("desktop-reap: elapsed_us=%llu\n",(unsigned long long)elapsed);
        /* An interrupted wait is not proof of death. Init only adopts a
         * child after its parent exits; live clients retain their own results. */
        if (waited==(long)(*session_children(s,i))) { s->child_reaped++;if(status)s->child_errors++; }
        else if(waited<0 && !desk_process_alive((*session_children(s,i)))) s->child_errors++;
        if (waited==(long)(*session_children(s,i)))
            desk_log("deskd: child-exit pid=%llu status=%llu tick=%llu\n",(unsigned long long)(*session_children(s,i)),(unsigned long long)status,(unsigned long long)srv_ticks());
        if (waited==(long)(*session_children(s,i)) || (waited<0 && !desk_process_alive((*session_children(s,i)))))
            { (*session_children(s,i))=0;(*session_child_mode(s,i))=0;(*session_child_last_window(s,i))=0;(*session_child_reply(s,i))=(struct desk_packet){0}; }
    }
}
/* Only idle maintenance may move to the 100 ms cadence. Publication, retries,
 * transfers and input capture retain the old tick. Incoming work is separately
 * checked atomically by the kernel immediately before blocking. */
static int session_quiescent(const struct session *s) {
    if(s->exiting || s->damage.count || s->capture.busy || s->capture_pending ||
       s->preferences.loading || s->preferences.dirty || s->preferences.job.busy ||
       s->pointer_pid || s->pointer_blocked || clipboard.writer) return 0;
    for(int i=0;i<children_capacity(s);i++)if(*session_children(s,i)) {
        if(session_child_reply(s,i)->type)return 0;
        int owner=0;for(int j=0;j<session_capacity(s);j++)owner|=session_client(s,j)->pid==*session_children(s,i);
        if(!owner)return 0;
    }
    for(int i=0;i<session_capacity(s);i++) {
        const struct client *c=session_client(s,i);
        if(!c->pid) continue;
        if(c->clipboard.reading || c->clipboard.reply.type ||
           c->configure || c->awaiting || c->closing || c->close_requested ||
           c->preferences_dirty || c->release.type || c->brightness_reply.type ||
           c->audio_reply.type || c->input_count || c->input_blocked ||
           c->control_blocked || c->pointer_cancel_pending || c->notice_ack_pending) return 0;
    }
    return 1;
}
static int hit(struct session *s,int x,int y) {
    int inline_order[DESKD_WINDOWS];int *order=s->scene.order?s->scene.order:inline_order;desktop_stack(&s->scene,order);
    for(int n=session_capacity(s)-1;n>=0;n--) {
        int i=order[n];struct desktop_window *w=desktop_window_at(&s->scene,i);
        if(!w->visible || w->minimized) continue;
        struct desk_rect r=w->frame;
        if(x>=r.x && y>=r.y && x<r.x+r.w && y<r.y+r.h) return i;
    }
    return -1;
}
static int dock_activate(struct session *s,unsigned app) {
    int active=s->scene.focus>=0 && desktop_window_at(&s->scene,s->scene.focus)->application==(int)app;
    int found=-1,present=0;
    for(int i=0;i<session_capacity(s);i++) {
        const struct client *c=session_client(s,i);const struct desktop_window *w=desktop_window_at(&s->scene,i);
        if(!c->pid || !w->visible || w->application!=(int)app || c->parent_window)continue;
        present=1;if(c->closing)continue;
        if(found<0 || (active?w->z<desktop_window_at(&s->scene,found)->z:w->z>desktop_window_at(&s->scene,found)->z))found=i;
    }
    if(found>=0) {
        damage_window(s,found);desktop_window_at(&s->scene,found)->minimized=0;focus(s,found);
        if(s->trace)desk_log("deskd: dock activate application=%u identity=%llu cycle=%d\n",app,(unsigned long long)session_client(s,found)->identity,active);
    } else if(!present)spawn_app(s,app,"");
    return found;
}
static int global_click(struct session *s,int x,int y) {
    if(!s->scene.apps) return 0;
    if(s->scene.battery[0] && desk_widget_contains(desktop_battery_rect(s->damage.width,s->scene.clock),x,y)) {
        for(int i=0;i<session_capacity(s);i++) if(session_client(s,i)->pid && !session_client(s,i)->closing && desktop_window_at(&s->scene,i)->application==DESK_APP_SYSTEM) {
            focus(s,i);return 1;
        }
        spawn_app(s,DESK_APP_SYSTEM,"");return 1;
    }
    if(desk_widget_contains(desktop_alert_rect(s->damage.width,s->scene.clock),x,y)) { notifications_open(s);return 1; }
    if(desk_widget_contains(desktop_clock_rect(s->damage.width,s->scene.clock),x,y)) { calendar_open(s);return 1; }
    unsigned app=desktop_dock_hit(&s->scene,s->damage.width,s->damage.height,x,y);
    if(app) {dock_activate(s,app);return 1;}
    return desk_widget_contains(desktop_dock_frame(&s->scene,s->damage.width,s->damage.height),x,y);
}
static void scroll_at(struct session *s,int x,int y,int wx,int wy) {
    if(s->scene.apps && desk_widget_contains(desktop_dock_frame(&s->scene,s->damage.width,s->damage.height),x,y))return;
    int i=hit(s,x,y);
    if (i<0 || session_client(s,i)->closing || modal_child(s,i)>=0) return;
    struct desktop_window *w=desktop_window_at(&s->scene,i);
    if (y<w->frame.y+44 || x<=w->frame.x || x>=w->frame.x+w->frame.w-1 || y>=w->frame.y+w->frame.h-1) return;
    struct desk_packet p=desk_packet_new(DESK_SCROLL);
    p.generation=session_client(s,i)->generation;
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
    if(!keyboard && s->scene.apps) {
        unsigned app=desktop_dock_hit(&s->scene,s->damage.width,s->damage.height,x,y);
        if(app) {dock_windows(s,app);return;}
        if(desk_widget_contains(desktop_dock_frame(&s->scene,s->damage.width,s->damage.height),x,y))return;
    }
    int i=keyboard?s->scene.focus:hit(s,x,y);
    if(i>=0 && modal_child(s,i)>=0) {focus(s,i);return;}
    if(i<0) {
        if(keyboard || (y>=44 && y<s->damage.height-80)) desktop_context_open(s,keyboard?24:x,keyboard?64:y,keyboard);
        return;
    }
    if(session_client(s,i)->closing || session_client(s,i)->awaiting) return;
    struct desk_rect w=desktop_window_at(&s->scene,i)->frame;
    if(!keyboard && (x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1)) return;
    pointer_cancel(s);focus(s,i);
    struct desk_packet p=desk_packet_new(DESK_CONTEXT);p.value=keyboard!=0;p.generation=session_client(s,i)->generation;
    if(!keyboard) { p.x=x-w.x-1;p.y=y-w.y-44; }
    queue_input(s,i,p);
    if(s->trace) desk_log("deskd: context pid=%llu keyboard=%d\n",(unsigned long long)session_client(s,i)->pid,keyboard);
}
static void pointer_at(struct session *s,int x,int y,unsigned buttons,int pressed) {
    if (s->pointer_blocked) {
        if (!(buttons&MOUSE_LEFT)) s->pointer_blocked=0;
        return;
    }
    if(!s->pointer_pid && s->scene.apps && desk_widget_contains(desktop_dock_frame(&s->scene,s->damage.width,s->damage.height),x,y))return;
    if (pressed && !s->pointer_pid) {
        int i=hit(s,x,y);
        if(i>=0 && modal_child(s,i)>=0) {focus(s,i);return;}
        if (i<0 || session_client(s,i)->closing || session_client(s,i)->awaiting) return;
        struct desk_rect w=desktop_window_at(&s->scene,i)->frame;
        if (x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1) return;
        s->pointer_pid=session_client(s,i)->pid;s->pointer_window=session_client(s,i)->window;
    }
    if (!s->pointer_pid) {
        if(!s->scene.apps || buttons) return;
        int i=hit(s,x,y);if(i<0 || modal_child(s,i)>=0 || session_client(s,i)->closing || session_client(s,i)->awaiting) return;
        struct desk_rect w=desktop_window_at(&s->scene,i)->frame;
        if(x<=w.x || x>=w.x+w.w-1 || y<w.y+44 || y>=w.y+w.h-1) return;
        struct desk_packet p=desk_packet_new(DESK_POINTER);p.value=DESK_POINTER_HOVER;p.generation=session_client(s,i)->generation;p.x=x-w.x-1;p.y=y-w.y-44;
        queue_input(s,i,p);return;
    }
    for (int i=0;i<session_capacity(s);i++) if (pointer_owned(s,i)) {
        struct desk_packet p=desk_packet_new(DESK_POINTER);
        p.x=x-desktop_window_at(&s->scene,i)->frame.x-1; p.y=y-desktop_window_at(&s->scene,i)->frame.y-44;
        p.generation=session_client(s,i)->generation;
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
    int control_monitor=argc==2 && !strcmp(argv[1],"--battery-monitor");
    int control_manual=control_monitor || (argc==2 && !strcmp(argv[1],"--battery-controls-manual"));
    int control_capture=control_manual || (argc==2 && !strcmp(argv[1],"--battery-controls"));
    int control_files_cancel=argc==2 && !strcmp(argv[1],"--battery-control-files-cancel");
    int control_files=control_files_cancel || (argc==2 && !strcmp(argv[1],"--battery-control-files"));
    int coexist_workspace=control_files || (argc==2 && !strcmp(argv[1],"--coexist-workspace"));
    int coexist_files=coexist_workspace || (argc==2 && !strcmp(argv[1],"--coexist-files"));
    int coexist_text=coexist_files || (argc==2 && !strcmp(argv[1],"--coexist-text"));
    int coexist=coexist_text || (argc==2 && !strcmp(argv[1],"--coexist-capture"));
    int battery_work=argc==2 && !strcmp(argv[1],"--battery-work-capture");
    int battery_guided=argc==2 && !strcmp(argv[1],"--battery-transition-capture");
    int battery_idle=argc==2 && !strcmp(argv[1],"--battery-idle-capture");
    int idle_capture=battery_idle || (argc==2 && !strcmp(argv[1],"--idle-capture"));
    int network_trial=argc==2 && !strcmp(argv[1],"--network-trial");
    int hardware_trial=network_trial || (argc==2 && !strcmp(argv[1],"--hardware-trial"));
    int capacity_capture=argc==2 && !strcmp(argv[1],"--capacity-capture");
    int navigation_trial=argc==2 && !strcmp(argv[1],"--navigation-trial");
    int lifecycle_trial=argc==2 && !strcmp(argv[1],"--lifecycle-trial");
    int audio_io_smoke=argc==2 && !strcmp(argv[1],"--audio-io-smoke");
    int audio_io=audio_io_smoke || (argc==2 && !strcmp(argv[1],"--audio-io-stress"));
    int home_apps=navigation_trial || lifecycle_trial || audio_io || capacity_capture || battery_work || (argc==2 && !strcmp(argv[1],"--home-apps"));
    int pool_wait=argc==2 && !strcmp(argv[1],"--file-pool-wait-smoke");
    int growth=argc==2 && !strcmp(argv[1],"--window-growth-smoke");
    int multiwindow=growth || (argc==2 && !strcmp(argv[1],"--multiwindow-smoke"));
    int dialog_refusal=argc==2 && !strcmp(argv[1],"--dialog-refusal-test");
    int apps=dialog_refusal || pool_wait || multiwindow || battery_guided || home_apps || idle_capture || hardware_trial || argc==1 || (argc==2 && (!strcmp(argv[1],"--apps") || !strcmp(argv[1],"--apps-trace") || !strcmp(argv[1],"--registry-pressure-test")));
    int scroll_smoke=argc==2 && strcmp(argv[1],"--scroll-smoke")==0,scroll_phase=0;
    int smoke=scroll_smoke || (argc==2 && strcmp(argv[1],"--smoke")==0);
    int lifecycle=argc==2 && strcmp(argv[1],"--lifecycle-smoke")==0,phase=0;
    int tap_typing=argc==2 && strcmp(argv[1],"--tap-typing-test")==0;
    int io_mode=argc==3 && strcmp(argv[1],"--io-test")==0 && desk_trial_run_valid(argv[2]);
    struct gfx_info info={0};
    if (gfx_info(&info)!=0 || info.width<640 || info.height<480 || info.width>3840 || info.height>2160) return 1;
    if((battery_work || battery_guided || idle_capture || coexist || control_capture) && !explicit_scale) {
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
    struct session s={.dynamic=1,.refuse_dialogs=dialog_refusal,.registry_budget=argc==2 && !strcmp(argv[1],"--registry-pressure-test")?1:DESK_REGISTRY_BYTES,.ram_text=coexist_text || control_capture,.home=(battery_guided || hardware_trial || idle_capture || home_apps)?DESK_WORKSPACE_HOME:DESK_RAM_HOME,.scene={.focus=-1,.cursor_x=24,.cursor_y=56,.automatic_test=capacity_capture},
        .trace=dialog_refusal || capacity_capture || pool_wait || multiwindow || battery_guided || coexist || idle_capture || lifecycle || (argc==2 && (!strcmp(argv[1],"--trace") || !strcmp(argv[1],"--apps-trace") || !strcmp(argv[1],"--registry-pressure-test")))};
    if(s.registry_budget==1)srv_puts("deskd: diagnostic registry growth budget=1 byte\n");
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
    s.scene.trackpad_trial=tap_typing?4:io_mode?3:argc==2 && strcmp(argv[1],"--gesture-test")==0?2:
        (argc==2 && strcmp(argv[1],"--trackpad-test")==0);
    if(tap_typing) {
        if(gui_pointer_config(3,SRV_POINTER_TAPS|SRV_POINTER_DISABLE_WHILE_TYPING)!=0) {
            gfx_console_mute(0);desk_pixels_free(pixels,width,height);return 1;
        }
        desk_log("deskd: tap/typing options=14\n");
    }
    desk_damage_init(&s.damage,width,height);
    if(apps) { desktop_preferences_init_path(&s.preferences,(battery_guided || hardware_trial || idle_capture || home_apps)?DESKTOP_WORKSPACE_PREFERENCES_PATH:DESKTOP_PREFERENCES_PATH);
        pointer_preferences(&s);clock_update(&s,1); }
    battery_update(&s,1);
    global_menu_rebuild(&s);
    desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
    static struct lifecycle_worker worker;
    if(life_start(&worker)){gfx_console_mute(0);return 1;}s.worker=&worker;s.startup_trace=audio_io;
    if(control_capture)spawn_app(&s,DESK_APP_SYSTEM,control_manual?"--battery-controls":"--battery-control-capture");
    else if(coexist_text) spawn_capture_text(&s,control_files_cancel?4:control_files?3:coexist_workspace?2:coexist_files);
    else if(network_trial) spawn_app(&s,DESK_APP_TERMINAL,"network-trial");
    else if(pool_wait) spawn_app(&s,DESK_APP_TEXT,"--file-pool-wait-capture");
    else if(multiwindow) spawn_app(&s,DESK_APP_TEXT,growth?"--window-growth-capture":"--multi-window-capture");
    else if(audio_io || capacity_capture) { /* Launch serially after each rendered admission below. */ }
    else if(navigation_trial) spawn_app(&s,DESK_APP_HELP,"window-navigation-trial");
    else if(lifecycle_trial) spawn_app(&s,DESK_APP_HELP,"lifecycle-trial");
    else if(apps) spawn_app(&s,hardware_trial?DESK_APP_HELP:DESK_APP_TEXT,hardware_trial?"laptop-trial":"");else spawn_client(&s);
    if (io_mode) spawn_client(&s); /* Load both studies before storage starts. */
    if(battery_work)strcpy(s.scene.notice,"5-minute test: type, save NEW file, reopen, use Files. Keep AC/USB connected.");
    if(battery_guided)strcpy(s.scene.notice,"1/3 Keep AC and USB connected. Waiting for three fresh battery readings.");
    if(idle_capture) strcpy(s.scene.notice,"Automatic idle check: leave input alone. Logs and a screenshot will be saved before return.");
    /* Diagnostic: no app menus, preferences, Home or screenshot jobs.
     * The study client exercises independent IPC/surface publication. Generated
     * font-toggle events are not physical keyboard input. */
    if(control_capture)strcpy(s.scene.notice,control_manual?"Battery controls: use Start/Stop in Sensors. Close System to finish.":"Battery controls: two automatic start/stop cycles. No input needed.");
    if(coexist) strcpy(s.scene.notice,coexist_workspace?"VM study: Text saves to mounted workspace. No input needed.":coexist_files?"Battery overlap: Text save/rename/reopen in test image. No input needed.":"Battery overlap: RAM only. No input needed; returns automatically.");
    uint64_t coexist_begin=0,coexist_last=0,coexist_gap=0,coexist_loops=0;
    uint64_t coexist_next=0,coexist_sent=0,coexist_ack=0,coexist_frames=0;
    /* The control/file fixture acknowledges every twelfth generated edit only
     * after save/stat/rename/reopen and a concurrent service transition. These
     * are workload checkpoints, not ordinary key-delivery acknowledgements.
     * Keep the client/control/transport deadlines unchanged. */
    uint64_t coexist_limit=control_files?90000000:60000000;
    int coexist_complete=0,coexist_script_done=0;
    uint64_t coexist_keys=0,coexist_key_ack=0;
    struct desk_battery_trial battery_trial={.began=(uint64_t)srv_monotonic_us()};
    int battery_saving=0;unsigned battery_shot_serial=0;uint64_t battery_shot_due=0;
    uint64_t work_begin=(uint64_t)srv_monotonic_us(),work_keys=0,work_motion=0,work_due=0;
    unsigned work_shot=0;int work_phase=0;
    if(battery_work)desk_log("desktop-work: begin minimum_us=300000000 deadline_us=480000000\n");
    unsigned capacity_launched=0,capacity_phase=0,capacity_shot=0,capacity_sent=0;
    uint64_t capacity_pid=0;
    int capacity_child_slot=-1;
    uint64_t capacity_begin=(uint64_t)srv_monotonic_us(),capacity_due=0;
    if(capacity_capture) {
        strcpy(s.scene.notice,"Automatic capacity test: opening 24 apps, saving a screenshot, then returning. No input needed.");
        desk_log("desktop-capacity: begin target=24 deadline_us=180000000 input=none documents=5\n");
    }
    uint64_t audio_io_pid=0,audio_io_begin=(uint64_t)srv_monotonic_us(),audio_io_notice=0;
    unsigned audio_io_seconds=audio_io_smoke?60:1800;
    int audio_io_complete=0;
    uint64_t life_pid=0,life_due=0,life_begin=0,life_close=0,loop_last=0,loop_max=0,life_ready_max=0,life_close_max=0,life_first=0,life_last=0;
    unsigned life_cycles=0,life_phase=0;
    struct loop_trace loop_trace={0};uint64_t loop_report_at=0;
    int idle_phase=0;
    unsigned idle_capture_serial=0;
    uint64_t idle_deadline=0,idle_frames=0,idle_bytes=0;
    struct srv_scheduler_status idle_before={0};
    int io_started=0,io_ready=0,stop_warning=0;
    uint64_t start=srv_ticks(),frames=0,bytes=0;
    uint64_t drag_identity=0,resize_identity=0;
    int quit=0,failed=0,drag=-1,resize=-1,grabx=0,graby=0,pending_key=0,io_leaving=0;
    struct desktop_input_timing timing={0};
    uint64_t key_pid=0,key_since=0,key_serial=0;
    uint8_t buttons=0;
    srv_puts("deskd: ready; independent clients, sealed surfaces\n");
    while (!quit) {
        uint64_t loop_now=(uint64_t)srv_monotonic_us();
        if(audio_io && audio_io_pid) {
            uint64_t gap=loop_sample(&loop_last,&loop_max,loop_now);
            loop_trace_end(&loop_trace,loop_now,life_cycles,life_pid,life_phase);
            loop_trace_begin(&loop_trace,loop_now,life_cycles,life_pid,life_phase);
            if(loop_now-loop_report_at>=10000000) {loop_trace_report(&loop_trace,0);loop_report_at=loop_now;}
            if(gap>500000)desk_log("desktop-loop: gap_us=%llu phase=%u\n",(unsigned long long)gap,life_phase);
        }
        if(audio_io) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(!audio_io_pid) {
                struct srv_battery_status state={0};
                if(srv_battery_status(&state)==0 && state.state==SRV_BATTERY_ACTIVE && (state.flags&SRV_BATTERY_FRESH)) {
                    char duration[24];snprintf(duration,sizeof(duration),"%u",audio_io_seconds);
                    char *args[]={"/fat/bin/playwav","--io-stress",duration,NULL};
                    struct srv_exec_request request={.path=args[0],.argv=args,.flags=SRV_EXEC_BACKGROUND,.stdin_fd=0,.stdout_fd=1,.stderr_fd=2};
                    long child=srv_exec(&request);
                    if(child<=0){failed=1;break;}
                    audio_io_pid=(uint64_t)child;audio_io_begin=now;
                    desk_log("desktop-audio-io: started pid=%llu seconds=%u input=none\n",(unsigned long long)audio_io_pid,audio_io_seconds);
                    /* Lifecycle fixture launches actual apps after playback starts. */
                } else if(now-audio_io_begin>15000000) {failed=1;break;}
            } else {
                uint64_t status=1;long done=srv_wait(audio_io_pid,&status,SRV_WAIT_NOHANG);
                if(done==(long)audio_io_pid) {audio_io_complete=status==0;audio_io_pid=0;failed=!audio_io_complete;quit=1;break;}
                if(now-audio_io_begin>((uint64_t)audio_io_seconds+90)*1000000){failed=1;break;}
            }
            if(now-audio_io_notice>=1000000) {
                audio_io_notice=now;
                snprintf(s.scene.notice,sizeof(s.scene.notice),"Audio + USB + apps: %llu / %u sec. Listen only; automatic return.",(unsigned long long)((now-audio_io_begin)/1000000),audio_io_seconds);
                desk_damage_add(&s.damage,(struct desk_rect){0,0,s.damage.width,s.damage.height});
            }
        }
        io_trial_loop_begin(&s.io);
        loop_trace_stamp(&loop_trace,LOOP_MESSAGES);
        for (int n=0;n<32;n++) {
            struct desk_packet p; uint64_t pid;
            int r=desk_receive(&pid,&p);
            if (r<=0) break;
            if (r==1) incoming(&s,pid,&p);
        }
        loop_trace_stamp(&loop_trace,LOOP_LIFECYCLE);
        lifecycle_poll(&s);
        lifecycle_deliver(&s);
        loop_trace_stamp(&loop_trace,LOOP_SERVICE);
        service(&s);
        loop_trace_stamp(&loop_trace,LOOP_TRIAL);
        if(audio_io && audio_io_pid) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(!life_due)life_due=now+5000000;
            if(!life_phase && now>=life_due && now-audio_io_begin<((uint64_t)audio_io_seconds-12)*1000000) {
                static const int roles[]={DESK_APP_TEXT,DESK_APP_FILES,DESK_APP_SYSTEM};
                life_begin=now;if(!life_first)life_first=now;spawn_app(&s,roles[life_cycles%3],"");life_pid=0;life_phase=1;
            } else if(life_phase==1) {
                if(!life_pid)life_pid=s.last_launch_pid;
                for(int i=0;i<session_capacity(&s);i++)if(session_client(&s,i)->pid==life_pid && desktop_window_at(&s.scene,i)->pixels && !session_client(&s,i)->awaiting) {
                    desk_log("desktop-life: ready cycle=%u pid=%llu elapsed_us=%llu\n",life_cycles,(unsigned long long)life_pid,(unsigned long long)(now-life_begin));
                    if(now-life_begin>life_ready_max)life_ready_max=now-life_begin;
                    life_phase=2;life_due=now+500000;break;
                }
            } else if(life_phase==2 && now>=life_due) {
                for(int i=0;i<session_capacity(&s);i++)if(session_client(&s,i)->pid==life_pid)request_close(&s,i);
                life_close=now;life_phase=3;
            } else if(life_phase==3 && child_index(&s,life_pid)<0) {
                desk_log("desktop-life: closed cycle=%u pid=%llu elapsed_us=%llu errors=%u\n",life_cycles,(unsigned long long)life_pid,(unsigned long long)(now-life_close),s.child_errors);
                if(now-life_close>life_close_max)life_close_max=now-life_close;
                life_last=now;life_cycles++;life_phase=0;life_pid=0;life_due=now+500000;
            }
            if((life_phase && now-life_begin>10000000) || s.child_errors){
                desk_log("desktop-life: deadline phase=%u pid=%llu child=%d elapsed_us=%llu pending_opens=%u errors=%u\n",life_phase,
                    (unsigned long long)life_pid,child_index(&s,life_pid),(unsigned long long)(now-life_begin),s.pending_opens,s.child_errors);
                for(int i=0;i<session_capacity(&s);i++)if(life_pid && session_client(&s,i)->pid==life_pid) {
                    const struct client *c=session_client(&s,i);
                    desk_log("desktop-life: window=%llu configure=%d awaiting=%d closing=%d generation=%llu progress=%llu pixels=%u\n",
                        (unsigned long long)c->window,c->configure,c->awaiting,c->closing,(unsigned long long)c->generation,
                        (unsigned long long)c->progress,desktop_window_at(&s.scene,i)->pixels!=NULL);
                }
                failed=1;break;
            }
        }
        if(control_capture) {
            if(s.child_reaped) { if(s.child_reaped!=1 || s.child_errors)failed=1;break; }
            if(!control_monitor && (uint64_t)srv_ticks()-start>10000){failed=1;break;}
        }
        if(coexist) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(coexist_last && now-coexist_last>coexist_gap) coexist_gap=now-coexist_last;
            coexist_last=now;coexist_loops++;
            if(!coexist_begin) {
                if(desktop_window_at(&s.scene,0)->pixels && session_client(&s,0)->generation) {
                    coexist_begin=now;coexist_next=now;coexist_frames=frames;
                    desk_log("desktop-coexist: ready start_us=%llu storage=%s generated=%s\n",(unsigned long long)now,coexist_workspace?"workspace":coexist_files?"image":"ram",coexist_text?"text-edit":"font-toggle");
                } else if((uint64_t)srv_ticks()-start>=500) { failed=1;break; }
            } else if(!session_client(&s,0)->pid || !desktop_window_at(&s.scene,0)->pixels) { failed=1;break; }
            if(coexist_begin && now>=coexist_next && coexist_sent==coexist_ack && now-coexist_begin<coexist_limit && (!coexist_text || coexist_sent<60)) {
                struct desk_packet p=desk_packet_new(coexist_text?DESK_KEY:DESK_TEXT);p.value=12;p.input_serial=coexist_sent+1;
                if(coexist_text) { struct srv_key_event key=capture_key(coexist_sent);memcpy(p.text,&key,sizeof(key));
                    queue_input(&s,0,p);coexist_sent++;coexist_next=now+700000;
                } else if(desk_send(session_client(&s,0)->pid,&p)==0) { coexist_sent++;coexist_next=now+1000000; }
            }
            uint64_t checkpoint_budget=control_files && coexist_sent && !(coexist_sent%12)?15000000:4000000;
            if(coexist_begin && coexist_sent!=coexist_ack && now>coexist_next+checkpoint_budget) {
                desk_log("desktop-coexist: progress-timeout serial=%llu presented=%llu elapsed_us=%llu budget_us=%llu\n",
                    (unsigned long long)coexist_sent,(unsigned long long)coexist_ack,
                    (unsigned long long)(now-coexist_next),(unsigned long long)checkpoint_budget);
                failed=1;break;
            }
        }
        if(apps || coexist || control_capture)battery_update(&s,0);
        if(apps) { if(desktop_preferences_poll(&s.preferences,srv_ticks())) preferences_changed(&s);clock_update(&s,0);notifications_tick(&s); }
        if (io_mode && !io_ready && desktop_window_at(&s.scene,0)->pixels && desktop_window_at(&s.scene,1)->pixels) {
            io_ready=1; srv_puts("deskd: trial ready; Space starts USB work\n");
        }
        if (io_mode && !io_ready && (uint64_t)srv_ticks()-start>500) { failed=1; break; }
        io_trial_poll(&s.io);
        if (io_leaving && s.io.finished) { quit=1; break; }
        if (scroll_smoke && desktop_window_at(&s.scene,0)->pixels) {
            struct desk_rect r=desktop_window_at(&s.scene,0)->frame;
            if (!scroll_phase) { scroll_at(&s,r.x+50,r.y+150,0,120); scroll_phase=1; }
            else if (scroll_phase==1 && (uint64_t)srv_ticks()-start>75) {
                scroll_at(&s,r.x+50,r.y+150,0,-24); scroll_phase=2;
            }
        }
        if (lifecycle) {
            if (phase==0 && desktop_window_at(&s.scene,0)->pixels && (uint64_t)srv_ticks()-start>=100) {
                srv_puts("deskd: lifecycle killing client\n");
                srv_kill(session_client(&s,0)->pid); phase=1;
            } else if (phase==1 && !(*session_children(&s,0)) && !session_client(&s,0)->pid) {
                srv_puts("deskd: lifecycle replacing client\n"); spawn_client(&s); phase=2;
            } else if (phase==2 && desktop_window_at(&s.scene,0)->pixels && (uint64_t)srv_ticks()-start>=200) {
                srv_puts("deskd: lifecycle abrupt server exit\n");
                srv_exit(0); /* Deliberately exercise kernel cleanup, not the normal exit path. */
            }
            if ((uint64_t)srv_ticks()-start>600) { failed=1; break; }
        }
        loop_trace_stamp(&loop_trace,LOOP_INPUT);
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
                    if (other>=0 && other<session_capacity(&s) && session_client(&s,other)->pid) {
                        if (io_trial_active(&s.io)) s.io.focus_changes++;
                        focus(&s,other);
                    }
                    continue;
                }
                key_pid=s.scene.focus>=0?session_client(&s,s.scene.focus)->pid:0;
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
            if(battery_work && (key.flags&SRV_KEY_DOWN_EVENT) && !(key.flags&SRV_KEY_REPEAT))work_keys++;
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
            if(navigation_key(&s,&key)) {drag=resize=-1;continue;}
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
            if(s.scene.menu.open<0 && context_key(&key)) { drag=resize=-1;context_at(&s,0,0,1);continue; }
            struct desk_menu_result menu_result=desk_menu_key(&s.scene.menu,&key);
            if(menu_result.consumed) { drag=resize=-1;pointer_cancel(&s);global_menu_result(&s,menu_result);if(!(key.flags&SRV_KEY_CANCEL)) continue; }
            if(s.scene.focus>=0) {
                struct desk_packet p=desk_packet_new(DESK_KEY);memcpy(p.text,&key,sizeof(key));
                queue_input(&s,s.scene.focus,p);
            }
        }
        if(apps && s.exiting) {
            int live=0;for(unsigned n=0;n<LIFE_LAUNCH_SLOTS;n++)live|=life_state(&worker.launch[n].state)!=LIFE_IDLE;
            for(int i=0;i<session_capacity(&s);i++) live|=session_client(&s,i)->pid!=0;
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
                if(battery_work)work_motion++;
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
                /* The overlay owns this release. Do not leave the opener's
                 * right-click block latched onto the next application click. */
                if(!e.buttons) {s.pointer_blocked=0;s.context_down=0;}
                buttons=e.buttons;desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});continue;
            }
            if (e.flags&MOUSE_CANCEL) {
                drag=resize=-1; buttons=e.buttons;
                cancel_input(&s,e.buttons);
                if(apps) global_menu_result(&s,desk_menu_pointer(&s.scene.menu,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL}));
            }
            if(apps && drag<0 && resize<0 && !s.pointer_pid && !(e.flags&MOUSE_CANCEL)) {
                if(e.wheel_y)global_menu_result(&s,desk_menu_scroll(&s.scene.menu,e.wheel_y));
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
                    if(modal_child(&s,i)>=0) {focus(&s,i);buttons=e.buttons;continue;}
                    focus(&s,i); struct desk_rect w=desktop_window_at(&s.scene,i)->frame;
                    unsigned control=desktop_titlebar_hit(w,apps,x,y);
                    if (control==DESKTOP_CLOSE) request_close(&s,i);
                    else if (control==DESKTOP_MAXIMIZE) { maximize(&s,i);s.pointer_blocked=1; }
                    else if (control==DESKTOP_MINIMIZE) { minimize(&s,i);s.pointer_blocked=1; }
                    else if (!desktop_window_at(&s.scene,i)->maximized && x>=w.x+w.w-24 && y>=w.y+w.h-24) { resize=i;resize_identity=session_client(&s,i)->identity; }
                    else if (!desktop_window_at(&s.scene,i)->maximized && y<w.y+43) { drag=i;drag_identity=session_client(&s,i)->identity; grabx=x-w.x; graby=y-w.y; }
                }
            }
            if (!(e.buttons&MOUSE_LEFT)) {
                if(drag>=0 && s.trace && window_lifetime_valid(&s,drag,drag_identity)) {
                    struct desk_rect r=desktop_window_at(&s.scene,drag)->frame;
                    desk_log("deskd: moved identity=%llu frame=%d,%d,%d,%d\n",(unsigned long long)drag_identity,r.x,r.y,r.w,r.h);
                }
                drag=resize=-1;
            }
            if(drag>=0 && !window_lifetime_valid(&s,drag,drag_identity)) { drag=-1;s.pointer_blocked=1; }
            if(resize>=0 && !window_lifetime_valid(&s,resize,resize_identity)) { resize=-1;s.pointer_blocked=1; }
            if (drag>=0 && session_client(&s,drag)->pid) {
                struct desktop_window *w=desktop_window_at(&s.scene,drag); damage_window(&s,drag);
                if(s.scene.apps)desktop_window_drag(canvas.width,canvas.height,w->frame,(int64_t)x-grabx,(int64_t)y-graby,&w->frame);
                else {
                    w->frame.x=clamp(x-grabx,0,canvas.width-w->frame.w-6);
                    w->frame.y=clamp(y-graby,44,canvas.height-w->frame.h-36);
                }
                damage_window(&s,drag);
            }
            if (resize>=0 && session_client(&s,resize)->pid) {
                struct desk_rect w=desktop_window_at(&s.scene,resize)->frame;
                int width=clamp(x-w.x-1,280,s.scene.apps?DESK_MAX_WIDTH:clamp(canvas.width-w.x-8,280,DESK_MAX_WIDTH));
                int height=clamp(y-w.y-44,240,s.scene.apps?DESK_MAX_HEIGHT:clamp(canvas.height-w.y-85,240,DESK_MAX_HEIGHT));
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
        loop_trace_stamp(&loop_trace,LOOP_RENDER);
        long render_start=(io_mode || hardware_trial) && s.damage.count?srv_monotonic_us():0;
        /* No scene mutation/dispatch during this frame. Sort once, then borrow
         * the registry scratch order for all damage rectangles. */
        int inline_order[DESKD_WINDOWS];int *frame_order=s.scene.order?s.scene.order:inline_order;
        if(s.damage.count) desktop_stack(&s.scene,frame_order);
        for (unsigned n=0;n<s.damage.count;n++) {
            struct desk_rect r=s.damage.rects[n]; desk_clip(&canvas,r); desktop_draw_ordered(&canvas,&s.scene,frame_order,1);
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
        if (!failed && s.damage.count) for (int i=0;i<session_capacity(&s);i++)
            io_trial_text_present(&s.io,session_client(&s,i)->pid,session_client(&s,i)->present_input);
        io_trial_loop_end(&s.io,render_start>0?(uint64_t)render_start:0);
        int painted=s.damage.count!=0;
        if (painted) frames++;
        if(coexist && !failed && s.damage.count && session_client(&s,0)->present_input==coexist_sent) {
            if(control_files && coexist_sent!=coexist_ack && !(coexist_sent%12))
                desk_log("desktop-coexist: file-checkpoint serial=%llu elapsed_us=%llu\n",
                    (unsigned long long)coexist_sent,(unsigned long long)((uint64_t)srv_monotonic_us()-(coexist_next-700000)));
            coexist_ack=coexist_sent;
        }
        if(coexist_text && !failed && s.damage.count && coexist_keys && session_client(&s,0)->present_input==1000+coexist_keys)
            coexist_key_ack=coexist_keys;
        if(coexist_text && !coexist_script_done && coexist_ack==60) {
            coexist_script_done=1;srv_puts("desktop-text: script presented; keyboard ready\n");
        }
        s.damage.count=0;
        loop_trace_stamp(&loop_trace,LOOP_POST);
        if(apps && !failed) capture_tick(&s,pixels);
        if(coexist && coexist_begin && !failed && (uint64_t)srv_monotonic_us()-coexist_begin>=60000000 && coexist_sent==coexist_ack &&
           (!control_files || coexist_ack==60 || (uint64_t)srv_monotonic_us()-coexist_begin>=coexist_limit)) {
            coexist_complete=coexist_ack>=55 && frames-coexist_frames>=55 &&
                (!coexist_text || (coexist_ack==60 && coexist_keys==coexist_key_ack));
            if(!coexist_complete) failed=1;
            quit=1;
        }
        if(capacity_capture && !failed) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            unsigned live=0,ready=0;
            for(int i=0;i<session_capacity(&s);i++)if(session_client(&s,i)->pid) {
                live++;
                if(desktop_window_at(&s.scene,i)->pixels && !session_client(&s,i)->configure)ready++;
            }
            if(now-capacity_begin>=180000000 || s.child_errors || s.launch_notice_serial ||
               (capacity_phase!=3 && s.child_reaped)) {
                desk_log("desktop-capacity: failed phase=%u launched=%u live=%u ready=%u reaped=%u errors=%u\n",
                    capacity_phase,capacity_launched,live,ready,s.child_reaped,s.child_errors);
                failed=1;
            } else if(!capacity_phase && live==capacity_launched && ready==live) {
                if(capacity_launched<24) {
                    static const unsigned roles[]={DESK_APP_TEXT,DESK_APP_FILES,DESK_APP_CALCULATOR,
                        DESK_APP_CLOCK,DESK_APP_HELP,DESK_APP_PREVIEW,DESK_APP_SKETCH};
                    int slot=child_slot(&s);
                    spawn_app(&s,capacity_launched==23?DESK_APP_TEXT:roles[capacity_launched%7],capacity_launched==23?"--capacity-files":"");
                    if(capacity_launched==23)capacity_child_slot=slot;
                    capacity_launched++;
                } else {
                    /* spawn_app queues work; read the reserved slot only
                     * after every launched child has published a ready frame. */
                    if(capacity_child_slot<0 || !(capacity_pid=*session_children(&s,capacity_child_slot)))failed=1;
                    else {
                        capacity_phase=4;capacity_due=now;
                        desk_log("desktop-capacity: rendered=24 processes=24; editing documents\n");
                    }
                }
            } else if(capacity_phase==4) {
                int target=-1;
                if(capacity_pid)for(int i=0;i<session_capacity(&s);i++)if(session_client(&s,i)->pid==capacity_pid)target=i;
                if(target<0 || live!=24 || ready!=24)failed=1;
                else if(session_client(&s,target)->present_input==capacity_sent && now>=capacity_due) {
                    if(capacity_sent==60) {
                        capacity_phase=1;capacity_due=now+5000000;
                        desk_log("desktop-capacity: document-inputs=60 acknowledged=yes\n");
                    } else {
                        focus(&s,target);
                        struct desk_packet p=desk_packet_new(DESK_KEY);p.input_serial=capacity_sent+1;
                        struct srv_key_event key=capture_key(capacity_sent);memcpy(p.text,&key,sizeof(key));
                        queue_input(&s,target,p);capacity_sent++;capacity_due=now+50000;
                    }
                }
            } else if(capacity_phase==1 && now>=capacity_due) {
                if(live!=24 || ready!=24)failed=1;
                else {capacity_phase=2;capacity_shot=s.capture_finished;capture_request(&s);}
            } else if(capacity_phase==2 && s.capture_finished>capacity_shot) {
                if(!s.capture_success)failed=1;
                else {capacity_phase=3;global_menu_action(&s,GLOBAL_EXIT);}
            }
        }
        if(capacity_capture) {
            char progress[sizeof(s.scene.notice)];
            snprintf(progress,sizeof(progress),"Automatic test: %u/24 apps; edits %u/60. %s",capacity_launched,capacity_sent,
                capacity_phase==3?"Closing; returning to Ubuntu.":capacity_phase==1 || capacity_phase==2?"Saving evidence.":"No input needed.");
            if(strcmp(progress,s.scene.notice)) {
                strcpy(s.scene.notice,progress);
                desk_damage_add(&s.damage,(struct desk_rect){0,canvas.height-112,canvas.width,36});
            }
        }
        if(battery_work && !failed) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(!work_phase && now-work_begin>=240000000) {
                work_phase=1;strcpy(s.scene.notice,"One minute left: SAVE your new file and reopen it. Wait here for the final prompt.");
                desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
            }
            if(work_phase==1 && now-work_begin>=300000000 && !s.exiting) {
                strcpy(s.scene.notice,"Five minutes complete: saving screenshot. Please wait for the exit prompt.");
                desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
                work_phase=2;work_shot=s.capture_finished;work_due=now+30000000;capture_request(&s);
            } else if(work_phase==2) {
                if(s.capture_finished>work_shot) {
                    if(!s.capture_success)failed=1;
                    else {
                        work_phase=3;desk_log("desktop-work: ready-to-exit screenshot=yes\n");
                        strcpy(s.scene.notice,"Complete: SAVE all work; SrvrOS > Return to console within two minutes.");
                        desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
                    }
                } else if(now>=work_due)failed=1;
            }
        }
        if(battery_guided && !failed) {
            uint64_t now=(uint64_t)srv_monotonic_us();
            if(battery_saving) {
                if(s.capture_finished>battery_shot_serial) {
                    if(!s.capture_success)failed=1;
                    else {
                        desk_log("desktop-ac: screenshot phase=%u saved=yes\n",battery_trial.phase);
                        desk_battery_trial_advance(&battery_trial,now);battery_saving=0;
                        if(battery_trial.phase==3)quit=1;
                        else {
                            strcpy(s.scene.notice,battery_trial.phase==1?
                                "2/3 Unplug AC now. Wait for battery readings and a saved screenshot. Keep USB connected.":
                                "3/3 Reconnect AC now. Wait for AC readings; the test will save and return to Ubuntu.");
                            desk_log("desktop-ac: prompt phase=%u\n",battery_trial.phase);
                            desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
                        }
                    }
                } else if(now>=battery_shot_due)failed=1;
            } else {
                int ready=desk_battery_trial_step(&battery_trial,s.battery_result?NULL:&s.battery_status,now);
                if(ready<0) {desk_log("desktop-ac: timeout-or-invalid phase=%u\n",battery_trial.phase);failed=1;}
                else if(ready) {
                    const struct srv_battery_status *v=&s.battery_status;
                    desk_log("desktop-ac: accepted=%u stable=3 epoch=%llu first_gen=%llu last_gen=%llu ac=%u soc=%u current_ua=%lld voltage_uv=%llu remaining=%llu full=%llu unit=%u ac_us=%llu sampled_us=%llu age_us=%llu label=%s\n",
                        battery_trial.phase,(unsigned long long)v->epoch,(unsigned long long)battery_trial.first_generation,
                        (unsigned long long)v->generation,!!(v->flags&SRV_BATTERY_AC_PRESENT),v->soc,
                        (long long)v->current_ua,(unsigned long long)v->voltage_uv,(unsigned long long)v->remaining_capacity,
                        (unsigned long long)v->full_capacity,v->capacity_unit,(unsigned long long)v->ac_sampled_us,
                        (unsigned long long)v->sampled_us,(unsigned long long)v->age_us,s.scene.battery);
                    strcpy(s.scene.notice,battery_trial.phase==1?"Battery confirmed. Saving screenshot; keep AC unplugged until the next instruction.":
                        "AC confirmed. Saving screenshot; keep AC connected until the next instruction.");
                    desk_damage_add(&s.damage,(struct desk_rect){0,0,width,height});
                    battery_saving=1;battery_shot_serial=s.capture_finished;battery_shot_due=now+30000000;
                    capture_request(&s);
                }
            }
        }
        if(idle_capture && !failed) {
            uint64_t now=srv_ticks();
            if(idle_phase && !session_client(&s,0)->generation) failed=1;
            if(!idle_phase && now-start>=500) {
                if(!session_client(&s,0)->generation) { if(now-start>=3000) failed=1; }
                else if(srv_scheduler_status(&idle_before) || idle_before.version!=SRV_SCHEDULER_VERSION) failed=1;
                else {
                    idle_phase=1;idle_deadline=now+(battery_idle?18000:6000);idle_frames=frames;idle_bytes=bytes;
                    desk_log("desktop-idle: measuring %u seconds with Text open; no user input required\n",battery_idle?180:60);
                }
            } else if(idle_phase==1 && now>=idle_deadline) {
                struct srv_scheduler_status after={0};
                if(srv_scheduler_status(&after) || after.version!=SRV_SCHEDULER_VERSION ||
                   after.monotonic_us<idle_before.monotonic_us+(battery_idle?179000000:59000000) ||
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
        loop_trace_stamp(&loop_trace,LOOP_WAIT);
        struct loop_wait_record *wait=loop_trace.active?&loop_trace.current.wait:NULL;
        if(wait) {
            wait->begin=(uint64_t)srv_monotonic_us();
            if(srv_sched_trace(SRV_SCHED_TRACE_BEGIN,&wait->scheduler))loop_trace.invalid++;
            else wait->valid=1;
        }
        int sleep=!apps || coexist || control_capture || battery_guided || hardware_trial ||
            quit || painted || buttons || drag>=0 || resize>=0 || pending_key || !session_quiescent(&s);
        if(!sleep) {
            if(wait)wait->gui_begin=(uint64_t)srv_monotonic_us();
            int result=gui_server_wait(10);
            if(wait) {wait->gui_end=(uint64_t)srv_monotonic_us();wait->gui_result=result;}
            sleep=result<0;
        }
        if(sleep) {
            if(wait)wait->sleep_begin=(uint64_t)srv_monotonic_us();
            long result=srv_sleep_ticks(1);
            if(wait) {wait->sleep_end=(uint64_t)srv_monotonic_us();wait->sleep_result=result;}
        }
        if(wait) {
            if(wait->valid && srv_sched_trace(SRV_SCHED_TRACE_END,&wait->scheduler)) {
                wait->valid=0;loop_trace.invalid++;
            }
            wait->end=(uint64_t)srv_monotonic_us();
            if(wait->scheduler.invalid)loop_trace.invalid++;
        }
    }
    uint64_t coexist_end=(uint64_t)srv_monotonic_us();
    if(audio_io && loop_last) {
        uint64_t gap=loop_sample(&loop_last,&loop_max,coexist_end);
        loop_trace_end(&loop_trace,coexist_end,life_cycles,life_pid,life_phase);
        if(gap>500000)desk_log("desktop-loop: gap_us=%llu phase=%u final=1\n",(unsigned long long)gap,life_phase);
    }
    if(battery_work) {
        uint64_t elapsed=coexist_end-work_begin;
        if(work_phase!=3 || elapsed<300000000 || !work_keys || !work_motion || !s.exiting || s.child_errors)failed=1;
        desk_log("desktop-work: completed=%s elapsed_us=%llu keys=%llu pointer_events=%llu screenshot=%s normal_exit=%u\n",
            failed?"NO":"yes",(unsigned long long)elapsed,(unsigned long long)work_keys,(unsigned long long)work_motion,
            work_phase==3?"yes":"NO",s.exiting?1u:0u);
    }
    if(battery_guided) {
        if(battery_trial.phase!=3)failed=1;
        desk_log("desktop-ac: completed=%s phases=%u screenshots=%u\n",failed?"NO":"yes",battery_trial.phase,battery_trial.phase);
    }
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
    if(audio_io) {
        if(audio_io_pid){srv_kill(audio_io_pid);uint64_t status;srv_wait(audio_io_pid,&status,0);}
        loop_trace_report(&loop_trace,1);
        if(loop_trace.invalid || loop_trace.worst.gap!=loop_max)failed=1;
        if(!audio_io_complete || life_cycles<6 || life_phase || s.child_errors || !loop_last || loop_max>500000 || !life_first || life_last-life_first<((uint64_t)audio_io_seconds-20)*1000000)failed=1;
        desk_log("desktop-life: result=%s cycles=%u max_loop_us=%llu max_ready_us=%llu max_close_us=%llu errors=%u limit_us=500000 first_us=%llu last_us=%llu\n",failed?"FAIL":"PASS",life_cycles,(unsigned long long)loop_max,(unsigned long long)life_ready_max,(unsigned long long)life_close_max,s.child_errors,(unsigned long long)life_first,(unsigned long long)life_last);
        desk_log("desktop-audio-io: completed=%s\n",audio_io_complete?"yes":"NO");
    }
    if(life_stop(&worker)){srv_puts("deskd: lifecycle worker join failed\n");return 1;}
    lifecycle_poll(&s);
    /* Bound shutdown even when a client stopped reading. Only kill children
     * launched by this session; all queues/surfaces also have kernel cleanup. */
    for (int i=0;i<session_capacity(&s);i++) if (session_client(&s,i)->pid) close_client(&s,i);
    uint64_t until=(uint64_t)srv_ticks()+30;
    while ((uint64_t)srv_ticks()<until) { service(&s); srv_sleep_ticks(1); }
    for (int i=0;i<children_capacity(&s);i++) {
        if ((*session_children(&s,i))) {
            /* A storage wait can cross the grace deadline after the client
             * exits. Reap that completion before treating it as a forced kill. */
            uint64_t status=0,pid=(*session_children(&s,i));
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
            (*session_children(&s,i))=0;
        }
    }
    for(int i=0;i<session_capacity(&s);i++)if(session_client(&s,i)->pid)forget(&s,i);
    if(capacity_capture) {
        if(capacity_phase!=3 || capacity_sent!=60 || capacity_launched!=24 || s.child_reaped!=24 || s.child_errors || !s.exiting)failed=1;
        desk_log("desktop-capacity: result=%s launched=%u reaped=%u errors=%u screenshot=%s normal_exit=%u\n",
            failed?"FAIL":"PASS",capacity_launched,s.child_reaped,s.child_errors,
            capacity_phase==3?"yes":"NO",s.exiting?1u:0u);
    }
    if(control_capture)desk_log("desktop-controls: completed=%s reaped=%u errors=%u cached_reads=%llu\n",!failed && s.child_reaped==1 && !s.child_errors?"yes":"NO",s.child_reaped,s.child_errors,(unsigned long long)s.battery_reads);
    if(coexist && (s.child_reaped!=1 || s.child_errors)) failed=1;
    if(coexist)desk_log("desktop-battery: reads=%llu first_gen=%llu last_gen=%llu cached=yes\n",
        (unsigned long long)s.battery_reads,(unsigned long long)s.battery_first,(unsigned long long)s.battery_last);
    if(coexist_text) desk_log("desktop-text: generated=%llu presented=%llu keyboard_sent=%llu keyboard_presented=%llu storage=%s\n",
        (unsigned long long)coexist_sent,(unsigned long long)coexist_ack,
        (unsigned long long)coexist_keys,(unsigned long long)coexist_key_ack,coexist_workspace?"workspace":coexist_files?"image":"ram");
    if(coexist) {
        if(!coexist_complete) failed=1;
        desk_log("desktop-coexist: completed=%s start_us=%llu end_us=%llu loops=%llu max_gap_us=%llu generated=%llu presented=%llu frames=%llu storage=%s\n",
            failed?"NO":"yes",(unsigned long long)coexist_begin,(unsigned long long)coexist_end,
            (unsigned long long)coexist_loops,(unsigned long long)coexist_gap,
            (unsigned long long)coexist_sent,(unsigned long long)coexist_ack,(unsigned long long)(frames-coexist_frames),coexist_workspace?"workspace":coexist_files?"image":"ram");
    }
    if(apps) {
        if(s.preferences.dirty) desk_log("deskd: preferences were not fully saved (status=%u)\n",s.preferences.status);
        uint64_t retire_start=srv_ticks();int preferences_retired=!s.preferences.initialized;
        while(!preferences_retired && (uint64_t)srv_ticks()-retire_start<600) {
            if(s.preferences.job.busy)(void)desk_file_job_poll(&s.preferences.job);
            else preferences_retired=desk_file_job_retire(&s.preferences.job)==1;
            if(!preferences_retired)srv_sleep_ticks(1);
        }
        if(!preferences_retired)failed=1;
        desktop_preferences_close(&s.preferences);
        desk_capture_job_close(&s.capture);
    }
    cache_free(&s);life_work(&worker);lifecycle_poll(&s);s.worker=NULL;
    if(s.trace)desk_log("deskd: cache peak_bytes=%llu retired_bytes=%llu budget_bytes=%llu windows=%u\n",(unsigned long long)s.cache_peak,(unsigned long long)s.cache_bytes,(unsigned long long)DESKD_CACHE_BUDGET,session_capacity(&s));
    registry_destroy(&s);
    if(s.trace)desk_log("deskd: registry peak_bytes=%llu retired_bytes=%llu budget_bytes=%llu\n",(unsigned long long)s.registry_peak,(unsigned long long)s.registry_bytes,(unsigned long long)s.registry_budget);
    desk_pixels_free(pixels,width,height); gfx_console_mute(0);
    desk_log("deskd: %s frames=%llu fb_bytes=%llu\n",failed?"FAIL":"exited",
        (unsigned long long)frames,(unsigned long long)bytes);
    return failed?1:0;
}
