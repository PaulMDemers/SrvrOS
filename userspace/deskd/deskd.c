#include <srvros/desk_log.h>
#include "scene.h"
#include "io_trial.h"
#include <srvros/desktop.h>
#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>
#include <stdio.h>
#include <string.h>

struct client {
    uint64_t pid,generation,progress,present_input;
    int configure,awaiting,closing;
    struct desk_packet release;
    struct desk_packet input[16];
    unsigned input_head,input_count;
    uint64_t input_since;
};
struct session {
    struct desktop_scene scene;
    struct desk_damage damage;
    struct client clients[DESKD_WINDOWS];
    uint64_t children[DESKD_WINDOWS];
    int trace,pointer_blocked;
    uint64_t pointer_pid;
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
static void queue_input(struct session *s,int i,struct desk_packet p) {
    struct client *c=&s->clients[i];
    if (c->closing) return;
    if (c->input_count) {
        struct desk_packet *last=&c->input[(c->input_head+c->input_count-1)%16];
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
    if (c->input_count==16) { close_client(s,i); return; }
    if (!c->input_count) c->input_since=srv_ticks();
    c->input[(c->input_head+c->input_count++)%16]=p;
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
    if (s->scene.focus!=i) pointer_cancel(s);
    if (s->scene.focus>=0) damage_window(s,s->scene.focus);
    s->scene.focus=i;
    if (i>=0) damage_window(s,i);
}
static void forget(struct session *s,int i) {
    if (s->pointer_pid==s->clients[i].pid && s->pointer_pid) {
        s->pointer_pid=0; s->pointer_blocked=1;
    }
    struct desktop_window *w=&s->scene.windows[i];
    damage_window(s,i);
    if (s->trace) desk_log("deskd: retired pid=%llu\n",(unsigned long long)s->clients[i].pid);
    memset(w,0,sizeof(*w)); memset(&s->clients[i],0,sizeof(s->clients[i]));
    if (s->scene.focus==i) {
        focus(s,-1);
        for (int j=0;j<DESKD_WINDOWS;j++) if (s->clients[j].pid) focus(s,j);
    }
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
static void spawn_client(struct session *s) {
    for (int i=0;i<DESKD_WINDOWS;i++) if (!s->children[i]) {
        long pid=spawn_bg("/fat/bin/deskclient");
        if (pid>0) s->children[i]=(uint64_t)pid;
        else srv_puts("deskd: launch failed\n");
        return;
    }
}
static int known_child(struct session *s,uint64_t pid) {
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->children[i]==pid) return 1;
    return 0;
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
    /* This milestone deliberately accepts only the study processes it owns. */
    if (i==DESKD_WINDOWS) {
        if (p->type!=DESK_OPEN || !known_child(s,pid)) return;
        for (i=0;i<DESKD_WINDOWS;i++) if (!s->clients[i].pid) break;
        if (i==DESKD_WINDOWS) return;
        struct desktop_window *w=&s->scene.windows[i];
        s->clients[i].pid=pid;
        w->visible=1;
        w->frame.x=32+40*i; w->frame.y=64+36*i;
        memcpy(w->title,p->text,sizeof(p->text)); w->title[sizeof(p->text)]=0;
        configure(s,i,clamp(p->width,280,s->damage.width-w->frame.x-8<DESK_MAX_WIDTH?
            s->damage.width-w->frame.x-8:DESK_MAX_WIDTH),
            clamp(p->height,240,s->damage.height-w->frame.y-85<DESK_MAX_HEIGHT?
            s->damage.height-w->frame.y-85:DESK_MAX_HEIGHT));
        focus(s,i);
        desk_log("deskd: opened pid=%llu window=%d\n",(unsigned long long)pid,i);
        return;
    }
    struct client *c=&s->clients[i]; struct desktop_window *w=&s->scene.windows[i];
    if (p->type==DESK_CLOSE) { forget(s,i); return; }
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
    w->pixels=next; w->width=p->width; w->height=p->height;
    c->awaiting=0;
    damage_window(s,i);
    if (s->trace) desk_log("deskd: commit pid=%llu generation=%llu size=%ux%u\n",
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
        if (c->input_count && !c->configure && !c->closing &&
            desk_send(c->pid,&c->input[c->input_head])==0) {
            c->input_head=(c->input_head+1)%16; c->input_count--;
            c->input_since=srv_ticks();
        }
    }
    for (int i=0;i<DESKD_WINDOWS;i++) if (s->children[i]) {
        uint64_t status;
        long waited=srv_wait(s->children[i],&status,SRV_WAIT_NOHANG);
        /* Background children can be reaped by another system waiter. A
         * negative wait result alone isn't proof of death (e.g. interruption). */
        if (waited==(long)s->children[i] || (waited<0 && !desk_process_alive(s->children[i])))
            s->children[i]=0;
    }
}
static int hit(struct session *s,int x,int y) {
    int first=s->scene.focus;
    for (int pass=0;pass<DESKD_WINDOWS;pass++) {
        int i=pass==0?first:1-first;
        if (i<0 || i>=DESKD_WINDOWS || !s->scene.windows[i].visible) continue;
        struct desk_rect w=s->scene.windows[i].frame;
        if (x>=w.x && y>=w.y && x<w.x+w.w && y<w.y+w.h) return i;
    }
    return -1;
}
static void scroll_at(struct session *s,int x,int y,int wx,int wy) {
    int i=hit(s,x,y);
    if (i<0 || s->clients[i].closing) return;
    struct desktop_window *w=&s->scene.windows[i];
    if (y<w->frame.y+44 || x<=w->frame.x || x>=w->frame.x+w->frame.w-1 || y>=w->frame.y+w->frame.h-1) return;
    struct desk_packet p=desk_packet_new(DESK_SCROLL);
    p.x=clamp(wx,-16384,16384); p.y=clamp(wy,-16384,16384); queue_input(s,i,p);
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
        .trace=lifecycle || (argc==2 && strcmp(argv[1],"--trace")==0)};
    if(cache_init(&s)!=0) { gfx_console_mute(0); desk_pixels_free(pixels,info.width,info.height); return 1; }
    s.scene.trackpad_trial=io_mode?3:argc==2 && strcmp(argv[1],"--gesture-test")==0?2:
        (argc==2 && strcmp(argv[1],"--trackpad-test")==0);
    desk_damage_init(&s.damage,info.width,info.height);
    desk_damage_add(&s.damage,(struct desk_rect){0,0,info.width,info.height});
    spawn_client(&s);
    if (io_mode) spawn_client(&s); /* Load both studies before storage starts. */
    int io_started=0;
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
        if (io_mode && !io_started && s.scene.windows[0].pixels && s.scene.windows[1].pixels) {
            io_trial_start(&s.io,argv[2]); io_started=1;
        }
        if (io_mode && !io_started && (uint64_t)srv_ticks()-start>500) { failed=1; break; }
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
        for (int n=0;n<32;n++) {
            int key=pending_key?pending_key:kbhit();
            if (key<=0) break;
            if (!pending_key) {
                if (key==27) {
                    if (io_mode && io_started) { io_trial_stop(&s.io); io_leaving=1; continue; }
                    quit=1; break;
                }
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
        struct mouse_event e;
        for (int n=0;n<32 && mouse_scan(&e)>0;n++) {
            if (io_trial_active(&s.io)) {
                if (e.dx || e.dy) s.io.motions++;
                if (e.wheel_x || e.wheel_y) s.io.wheels++;
            }
            desk_damage_add(&s.damage,(struct desk_rect){s.scene.cursor_x,s.scene.cursor_y,10,17});
            int x=s.scene.cursor_x=clamp((int64_t)s.scene.cursor_x+e.dx,0,canvas.width-1);
            int y=s.scene.cursor_y=clamp((int64_t)s.scene.cursor_y+e.dy,0,canvas.height-1);
            if (s.trace) desk_log("deskd: pointer x=%d y=%d buttons=%u\n",x,y,(unsigned)e.buttons);
            if (e.flags&MOUSE_CANCEL) {
                drag=resize=-1; buttons=e.buttons;
                cancel_input(&s,e.buttons);
            }
            if ((e.wheel_x || e.wheel_y) && drag<0 && resize<0 && !e.buttons && !s.pointer_pid)
                scroll_at(&s,x,y,e.wheel_x,e.wheel_y);
            if ((e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT)) {
                int i=hit(&s,x,y);
                if (i>=0) {
                    if (s.scene.focus!=i && io_trial_active(&s.io)) s.io.focus_changes++;
                    focus(&s,i); struct desk_rect w=s.scene.windows[i].frame;
                    if (x>=w.x+w.w-40 && y<w.y+43) close_client(&s,i);
                    else if (x>=w.x+w.w-24 && y>=w.y+w.h-24) resize=i;
                    else if (y<w.y+43) { drag=i; grabx=x-w.x; graby=y-w.y; }
                }
            }
            if (!(e.buttons&MOUSE_LEFT)) drag=resize=-1;
            if (drag>=0 && s.clients[drag].pid) {
                struct desktop_window *w=&s.scene.windows[drag]; damage_window(&s,drag);
                w->frame.x=clamp(x-grabx,0,canvas.width-w->frame.w-6);
                w->frame.y=clamp(y-graby,44,canvas.height-w->frame.h-36); damage_window(&s,drag);
            }
            if (resize>=0 && s.clients[resize].pid) {
                struct desk_rect w=s.scene.windows[resize].frame;
                int width=clamp(x-w.x-1,280,clamp(canvas.width-w.x-8,280,DESK_MAX_WIDTH));
                int height=clamp(y-w.y-44,240,clamp(canvas.height-w.y-85,240,DESK_MAX_HEIGHT));
                if (w.w!=width+2 || w.h!=height+45) configure(&s,resize,width,height);
            }
            if (drag<0 && resize<0 && !(e.flags&MOUSE_CANCEL))
                pointer_at(&s,x,y,e.buttons,(e.buttons&MOUSE_LEFT) && !(buttons&MOUSE_LEFT));
            buttons=e.buttons;
            desk_damage_add(&s.damage,(struct desk_rect){x,y,10,17});
        }
        if (io_mode) {
            char status[160]; const char *states[]={"STARTING","RUNNING","COMPLETE","STOPPED","FAILED"};
            snprintf(status,sizeof(status),"USB %s%s | rounds %u/128 | keys %llu | moves %llu | scroll %llu | focus %llu | cancel %llu",
                states[s.io.progress.state],s.io.stopping && !s.io.finished?" (stopping)":"",
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
    cache_free(&s);
    desk_pixels_free(pixels,info.width,info.height); gfx_console_mute(0);
    desk_log("deskd: %s frames=%llu fb_bytes=%llu\n",failed?"FAIL":"exited",
        (unsigned long long)frames,(unsigned long long)bytes);
    return failed?1:0;
}
