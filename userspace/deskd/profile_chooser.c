#include "profile_chooser.h"
#include "profile_catalog.h"
#include "profile_startup.h"
#include <srvros/desktop.h>
#include <srvros/desk_widgets.h>
#include <srvros/desk_log.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/mouse_position.h>
#include <pthread.h>
#include <stdio.h>
#include <string.h>

enum chooser_work { CHOOSER_RECOVER=1, CHOOSER_OPEN, CHOOSER_OPEN_ONCE };
struct chooser_job {
    pthread_t thread;
    unsigned done;
    int active, kind, result, truncated;
    uint64_t exit_status;
    struct profile_entry entry;
    struct profile_catalog catalog;
    struct srv_profile_session profile;
    char portable[160], output[4096];
    const struct desktop_profile_request *request;
};
struct chooser {
    struct profile_catalog catalog;
    struct chooser_job job;
    struct desk_canvas canvas;
    struct desk_rect panel, list;
    struct desk_button primary, back, stay;
    unsigned selected, first, visible;
    int focus, cursor_x, cursor_y, buttons, return_pending, save_failed;
    char status[160];
};

static const char *state_name(int state) {
    return state==PROFILE_READY?"ready":state==PROFILE_RECOVERABLE?"recoverable":"blocked";
}
static void trace_catalog(const struct profile_catalog *catalog) {
    for(unsigned i=0;i<catalog->count;i++) {
        const struct profile_entry *e=&catalog->entries[i];
        desk_log("profile-chooser: row index=%u name=%s status=%s home=%s\n",i,e->name,state_name(e->state),e->home);
    }
    desk_log("profile-chooser: ready rows=%u\n",catalog->count);
}
static int find_entry(const struct profile_catalog *catalog,const struct profile_entry *entry) {
    for(unsigned i=0;i<catalog->count;i++)
        if(!strcmp(catalog->entries[i].home,entry->home) && !strcmp(catalog->entries[i].id,entry->id))return (int)i;
    return -1;
}
/* Recovery is mutating: it has no timeout/kill path. The worker drains bounded
 * retained output through EOF, then reaps the child before publishing done.
 * Console return waits visibly for this ownership to end, never abandons it. */
static int recover_profile(struct chooser_job *job) {
    int input[2],output[2];
    if(srv_pipe(input))return -1;
    if(srv_pipe(output)) {srv_close(input[0]);srv_close(input[1]);return -1;}
    struct srv_profile_session session;struct srv_profile_environment environment;
    if(!srv_profile_session_from_env(&session) || !srv_profile_environment_init(&environment,&session,"/fat/bin")) {
        srv_close(input[0]);srv_close(input[1]);srv_close(output[0]);srv_close(output[1]);return -1;
    }
    char *argv[]={"profile","recover",job->entry.home,NULL};
    struct srv_exec_request request={.path="/fat/bin/profile",.argv=argv,.envp=environment.envp,
        .flags=SRV_EXEC_BACKGROUND,.stdin_fd=input[0],.stdout_fd=output[1],.stderr_fd=output[1],.process_group=UINT64_MAX};
    long pid=srv_exec(&request);
    srv_close(input[0]);srv_close(input[1]);srv_close(output[1]);
    if(pid<=0) {srv_close(output[0]);return -1;}
    size_t used=0;int failed=0;
    for(;;) {
        char buffer[512];long n=srv_read(output[0],buffer,sizeof(buffer));
        if(n==SRV_ERR_INTR)continue;
        if(n<0 || n>(long)sizeof(buffer)) {failed=1;break;}
        if(!n)break;
        size_t keep=(size_t)n;
        if(keep>sizeof(job->output)-1-used) {keep=sizeof(job->output)-1-used;job->truncated=1;}
        memcpy(job->output+used,buffer,keep);used+=keep;job->output[used]=0;
    }
    srv_close(output[0]);
    long waited;
    do {waited=srv_wait((uint64_t)pid,&job->exit_status,0);} while(waited==SRV_ERR_INTR);
    return failed || waited!=pid || job->exit_status?-1:0;
}
static void *chooser_worker(void *context) {
    struct chooser_job *job=context;
    job->result=-1;
    if(job->kind==CHOOSER_RECOVER) {
        /* Recheck eligibility immediately before the explicit mutation. */
        profile_catalog_scan(&job->catalog,job->request->root);
        int index=find_entry(&job->catalog,&job->entry);
        if(!job->catalog.error && !job->catalog.truncated && index>=0 && job->catalog.entries[index].state==PROFILE_RECOVERABLE)
            job->result=recover_profile(job);
        profile_catalog_scan(&job->catalog,job->request->root);
    } else {
        profile_catalog_scan(&job->catalog,job->request->root);
        int index=find_entry(&job->catalog,&job->entry);
        if(!job->catalog.error && !job->catalog.truncated && index>=0 && job->catalog.entries[index].state==PROFILE_READY &&
           !desktop_profile_load(&job->profile,job->portable,sizeof(job->portable),job->entry.home) &&
           !strcmp(job->profile.id,job->entry.id)) {
            job->result=0;
            if(job->kind==CHOOSER_OPEN && profile_choice_save(job->request->remember_path,&job->entry))job->result=-2;
        }
    }
    __atomic_store_n(&job->done,1,__ATOMIC_RELEASE);
    return NULL;
}
static int start_work(struct chooser *ui,const struct desktop_profile_request *request,int kind) {
    if(ui->job.active || ui->selected>=ui->catalog.count)return -1;
    ui->job=(struct chooser_job){.active=1,.kind=kind,.entry=ui->catalog.entries[ui->selected],.request=request};
    if(pthread_create(&ui->job.thread,NULL,chooser_worker,&ui->job)) {ui->job.active=0;return -1;}
    snprintf(ui->status,sizeof(ui->status),"%s",kind==CHOOSER_RECOVER?"Recovering profile… You can keep browsing.":"Checking profile and preparing your desktop…");
    if(kind==CHOOSER_RECOVER)desk_log("profile-chooser: recovery start home=%s\n",ui->job.entry.home);
    return 0;
}
static void keep_visible(struct chooser *ui) {
    if(ui->selected<ui->first)ui->first=ui->selected;
    if(ui->selected>=ui->first+ui->visible)ui->first=ui->selected-ui->visible+1;
    unsigned last=ui->catalog.count>ui->visible?ui->catalog.count-ui->visible:0;
    if(ui->first>last)ui->first=last;
}
static void select_row(struct chooser *ui,unsigned selected) {
    if(selected>=ui->catalog.count)return;
    ui->selected=selected;ui->save_failed=0;ui->focus=0;keep_visible(ui);
    const struct profile_entry *e=&ui->catalog.entries[selected];
    desk_log("profile-chooser: selected index=%u name=%s home=%s\n",selected,e->name,e->home);
}
static int selected_state(const struct chooser *ui) {
    if(ui->catalog.error || ui->catalog.truncated)return PROFILE_BLOCKED;
    return ui->selected<ui->catalog.count?ui->catalog.entries[ui->selected].state:PROFILE_BLOCKED;
}
static void button_focus(struct desk_button *button,int focused) {
    if(button->focused!=focused)desk_button_event(button,(struct desk_widget_event){
        .type=focused?DESK_WIDGET_FOCUS_IN:DESK_WIDGET_FOCUS_OUT});
}
static void configure(struct chooser *ui) {
    int w=ui->canvas.width,h=ui->canvas.height,pw=w-48,ph=h-88;
    if(pw>760)pw=760;
    if(ph>680)ph=680;
    ui->panel=(struct desk_rect){(w-pw)/2,(h-ph)/2+6,pw,ph};
    struct desk_rect p=ui->panel;
    ui->visible=(unsigned)(ph-236)/52;
    if(!ui->visible)ui->visible=1;
    ui->list=(struct desk_rect){p.x+20,p.y+76,p.w-40,(int)ui->visible*52};
    int enabled=!ui->job.active && selected_state(ui)!=PROFILE_BLOCKED;
    desk_button_configure(&ui->primary,(struct desk_rect){p.x+20,p.y+p.h-50,184,34},enabled);
    desk_button_configure(&ui->stay,(struct desk_rect){p.x+216,p.y+p.h-50,108,34},ui->save_failed && !ui->job.active);
    desk_button_configure(&ui->back,(struct desk_rect){p.x+p.w-172,p.y+p.h-50,152,34},1);
    button_focus(&ui->primary,ui->focus==1);button_focus(&ui->stay,ui->focus==2 && ui->save_failed);
    button_focus(&ui->back,ui->focus==(ui->save_failed?3:2));
    keep_visible(ui);
}
/* Two lines, UTF-8 aware: shared text metrics and clipping bound all long
 * names/details without ever drawing into the action controls. */
static void wrapped(struct desk_canvas *c,struct desk_rect frame,const char *text,uint32_t color) {
    char line[192];const char *next=text;
    for(int row=0;row<2 && *next;row++) {
        size_t used=0;const char *end=next,*space=NULL;
        while(*end && used<sizeof(line)-5) {
            const char *after=end;desk_utf8_next(&after);size_t n=(size_t)(after-end);
            if(used+n>=sizeof(line))break;
            memcpy(line+used,end,n);line[used+n]=0;
            if(desk_text_width(&desk_font_16,line)>frame.w)break;
            used+=n;if(*end==' ')space=end;end=after;
        }
        if(*end && space && space>next) {end=space;used=(size_t)(end-next);}
        if(!used && *end) {const char *after=end;desk_utf8_next(&after);used=(size_t)(after-end);end=after;}
        memcpy(line,next,used);line[used]=0;
        while(*end==' ')end++;
        if(row==1 && *end && used+4<sizeof(line))strcat(line,"…");
        desk_label_ellipsis(c,(struct desk_rect){frame.x,frame.y+row*20,frame.w,20},&desk_font_16,line,color);
        next=end;
    }
}
static void draw(struct chooser *ui) {
    struct desk_canvas *c=&ui->canvas;const struct desk_theme *t=&desk_theme_quiet;
    struct desk_rect all={0,0,c->width,c->height},p=ui->panel;
    desk_clip(c,all);
    desk_theme_gradient(c,all,0x587fa4,0xa4bfd3);
    desk_theme_glass(c,(struct desk_rect){0,0,c->width,44},t->chrome_top,t->chrome_bottom,t->glass_alpha,0);
    desk_fill(c,(struct desk_rect){0,43,c->width,1},t->chrome_border);
    desk_text(c,&desk_font_16,24,12,"SrvrOS   /   Profiles",t->text);
    desk_theme_panel(c,p,t,0,0);
    desk_text(c,&desk_font_20,p.x+20,p.y+16,"Choose your desktop",t->text);
    desk_label_ellipsis(c,(struct desk_rect){p.x+20,p.y+46,p.w-40,22},&desk_font_16,
        "Open a profile to use its documents and preferences.",t->muted);
    desk_theme_field_draw(c,ui->list,t,ui->focus==0);
    if(!ui->catalog.count)wrapped(c,desk_widget_inset(ui->list,12),"No profiles are available. Return to the console to create or import one.",t->muted);
    for(unsigned row=0;row<ui->visible && row+ui->first<ui->catalog.count;row++) {
        unsigned i=row+ui->first;const struct profile_entry *e=&ui->catalog.entries[i];
        struct desk_rect r={ui->list.x+2,ui->list.y+(int)row*52+2,ui->list.w-12,50};
        if(i==ui->selected)desk_theme_selection(c,r,t);
        desk_label_ellipsis(c,(struct desk_rect){r.x+10,r.y+4,r.w-178,22},&desk_font_16,e->name,t->text);
        const char *state=e->state==PROFILE_READY?"Ready":e->state==PROFILE_RECOVERABLE?"Recovery needed":"Unavailable";
        desk_label_draw(c,(struct desk_rect){r.x+r.w-164,r.y+4,156,22},&desk_font_16,state,t->muted);
        desk_label_ellipsis(c,(struct desk_rect){r.x+10,r.y+27,r.w-20,21},&desk_font_16,
            e->state==PROFILE_READY?e->home:e->detail,t->muted);
    }
    if(ui->catalog.count>ui->visible) {
        int track=ui->list.h-4,thumb=track*(int)ui->visible/(int)ui->catalog.count;
        int y=ui->list.y+2+(track-thumb)*(int)ui->first/(int)(ui->catalog.count-ui->visible);
        desk_fill(c,(struct desk_rect){ui->list.x+ui->list.w-7,ui->list.y+2,4,track},t->track);
        desk_fill(c,(struct desk_rect){ui->list.x+ui->list.w-7,y,4,thumb},t->thumb);
    }
    if(ui->selected<ui->catalog.count)wrapped(c,(struct desk_rect){p.x+20,ui->list.y+ui->list.h+10,p.w-40,40},ui->catalog.entries[ui->selected].detail,t->text);
    wrapped(c,(struct desk_rect){p.x+20,p.y+p.h-98,p.w-40,40},ui->status,t->muted);
    const char *label=ui->job.active?"Working…":ui->save_failed?"Continue anyway":selected_state(ui)==PROFILE_RECOVERABLE?"Recover profile":"Open desktop";
    desk_button_draw(c,&ui->primary,label,t);
    if(ui->save_failed)desk_button_draw(c,&ui->stay,"Stay here",t);
    desk_button_draw(c,&ui->back,"Back to console",t);
    desk_label_ellipsis(c,(struct desk_rect){24,c->height-24,c->width-48,22},&desk_font_16,
        "Up/Down Select   Tab Focus   Enter Continue   Esc Console",0x354352);
    for(int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){ui->cursor_x,ui->cursor_y+y,1+y/2,1},0x252a30);
        if(y>1 && y<13)desk_fill(c,(struct desk_rect){ui->cursor_x+1,ui->cursor_y+y,y/2,1},0xf0f1f3);
    }
}
static void back_to_console(struct chooser *ui) {
    ui->return_pending=1;
    if(ui->job.active)snprintf(ui->status,sizeof(ui->status),"Finishing the current operation safely, then returning to the console…");
}
static void activate(struct chooser *ui,const struct desktop_profile_request *request) {
    if(ui->focus==(ui->save_failed?3:2)) {back_to_console(ui);return;}
    if(ui->save_failed && ui->focus==2) {ui->save_failed=0;ui->focus=0;return;}
    if(ui->job.active || selected_state(ui)==PROFILE_BLOCKED)return;
    const struct profile_entry *e=&ui->catalog.entries[ui->selected];
    desk_log("profile-chooser: selected index=%u name=%s home=%s\n",ui->selected,e->name,e->home);
    int kind=ui->save_failed?CHOOSER_OPEN_ONCE:selected_state(ui)==PROFILE_RECOVERABLE?CHOOSER_RECOVER:CHOOSER_OPEN;
    if(start_work(ui,request,kind))snprintf(ui->status,sizeof(ui->status),"The operation could not start. Please try again or return to the console.");
}

int desktop_profile_choose(const struct desktop_profile_request *request,struct desktop_profile_result *result) {
    if(!request || !result)return DESKTOP_PROFILE_ERROR;
    *result=(struct desktop_profile_result){0};
    struct chooser ui={.cursor_x=24,.cursor_y=56};
    if(!request->root || !request->remember_path || (request->scale!=1 && request->scale!=2))return DESKTOP_PROFILE_ERROR;
    profile_catalog_scan(&ui.catalog,request->root);
    char reason[160]={0};unsigned remembered=0;
    int saved=profile_choice_load(request->remember_path,&ui.catalog,&remembered,reason);
    if(saved==1 && !request->force) {
        const struct profile_entry *e=&ui.catalog.entries[remembered];
        if(!desktop_profile_load(&result->profile,result->portable_path,sizeof(result->portable_path),e->home) &&
           !strcmp(result->profile.id,e->id)) {
            desk_log("profile-chooser: remembered restored home=%s\n",e->home);
            return DESKTOP_PROFILE_SELECTED;
        }
        saved=-1;snprintf(reason,sizeof(reason),"The remembered profile is no longer ready. Choose a profile to continue.");
    }
    if(!request->force && !ui.catalog.count && !ui.catalog.error && !ui.catalog.truncated && !saved)return DESKTOP_PROFILE_LEGACY;
    if(saved==1)ui.selected=remembered;
    snprintf(ui.status,sizeof(ui.status),"%s",saved<0?reason:ui.catalog.message[0]?ui.catalog.message:
        "Your selection will be remembered when you open the desktop.");
    struct gfx_info info={0};unsigned scale=request->scale;
    if(gfx_info(&info) || info.width<640*scale || info.height<480*scale || info.width>3840 || info.height>2160) {
        snprintf(result->error,sizeof(result->error),"The profile chooser needs a display of at least 640 × 480 logical pixels.");return DESKTOP_PROFILE_ERROR;
    }
    unsigned width=(unsigned)(info.width+scale-1)/scale,height=(unsigned)(info.height+scale-1)/scale;
    uint32_t *pixels=desk_pixels_alloc(width*scale,height*scale);
    if(!pixels) {snprintf(result->error,sizeof(result->error),"Not enough memory to open the profile chooser.");return DESKTOP_PROFILE_ERROR;}
    int answer=DESKTOP_PROFILE_ERROR,muted=0,keyboard=0,presenting=0;
    if(desk_canvas_init_scaled(&ui.canvas,pixels,width,height,width*scale,scale))goto cleanup;
    if(gui_register_server()) {snprintf(result->error,sizeof(result->error),"Another desktop is already open.");goto cleanup;}
    result->server_registered=1;
    if(gfx_console_mute(1))goto cleanup;
    muted=1;
    if(gui_keyboard_mode(1))goto cleanup;
    keyboard=1;
    if(info.flags&GFX_FLAG_FENCED_PRESENT) {
        if(gfx_present_begin())goto cleanup;
        presenting=1;
    }
    configure(&ui);
    int dirty=1,announced=0;
    for(;;) {
        if(ui.job.active && __atomic_load_n(&ui.job.done,__ATOMIC_ACQUIRE)) {
            pthread_join(ui.job.thread,NULL);ui.job.active=0;
            ui.catalog=ui.job.catalog;
            int index=find_entry(&ui.catalog,&ui.job.entry);
            ui.selected=index>=0?(unsigned)index:0;keep_visible(&ui);dirty=1;
            if(ui.job.kind==CHOOSER_RECOVER) {
                int recovered=!ui.job.result && index>=0 && ui.catalog.entries[index].state==PROFILE_READY;
                desk_log("profile-chooser: recovery result=%s home=%s status=%llu output_bytes=%u truncated=%u\n",
                    recovered?"success":"failed",ui.job.entry.home,(unsigned long long)ui.job.exit_status,
                    (unsigned)strlen(ui.job.output),(unsigned)ui.job.truncated);
                snprintf(ui.status,sizeof(ui.status),"%s",recovered?"Recovery finished. This profile is ready to open.":
                    "Recovery could not finish. Connect its recovery bundle and try again, or use the console.");
                trace_catalog(&ui.catalog);
            } else if(!ui.job.result) {
                if(ui.job.kind==CHOOSER_OPEN)desk_log("profile-chooser: remember saved home=%s\n",ui.job.entry.home);
                if(!ui.return_pending) {
                    result->profile=ui.job.profile;strcpy(result->portable_path,ui.job.portable);
                    answer=DESKTOP_PROFILE_SELECTED;break;
                }
            } else if(ui.job.result==-2) {
                ui.save_failed=1;ui.focus=1;
                snprintf(ui.status,sizeof(ui.status),"Could not confirm the saved choice. Continue anyway, or stay here to try again.");
                desk_log("profile-chooser: remember failed home=%s\n",ui.job.entry.home);
            } else {
                ui.save_failed=0;
                snprintf(ui.status,sizeof(ui.status),"%s",ui.catalog.message[0]?ui.catalog.message:
                    "This profile changed or is unavailable. Select a ready profile, or return to the console.");
            }
        }
        if(ui.return_pending && !ui.job.active) {answer=DESKTOP_PROFILE_CONSOLE;desk_log("profile-chooser: backconsole\n");break;}
        configure(&ui);
        for(unsigned n=0;n<16;n++) {
            struct srv_key_event key;int read=gui_keyboard_read(&key);
            if(read<=0)break;
            if(!(key.flags&SRV_KEY_DOWN_EVENT) || (key.flags&SRV_KEY_CANCEL))continue;
            dirty=1;
            if(key.usage==SRV_KEY_ESCAPE)back_to_console(&ui);
            else if(key.usage==SRV_KEY_TAB) {
                int count=ui.save_failed?4:3;
                ui.focus=(ui.focus+(key.modifiers&SRV_KEY_SHIFT?count-1:1))%count;
            } else if(key.usage==SRV_KEY_UP && ui.selected)select_row(&ui,ui.selected-1);
            else if(key.usage==SRV_KEY_DOWN && ui.selected+1<ui.catalog.count)select_row(&ui,ui.selected+1);
            else if(key.usage==SRV_KEY_HOME && ui.catalog.count)select_row(&ui,0);
            else if(key.usage==SRV_KEY_END && ui.catalog.count)select_row(&ui,ui.catalog.count-1);
            else if(key.usage==SRV_KEY_ENTER || key.usage==SRV_KEY_SPACE)activate(&ui,request);
            configure(&ui);
        }
        struct mouse_event event;
        for(unsigned n=0;n<32 && mouse_scan(&event)>0;n++) {
            ui.cursor_x=(int)mouse_position_axis(ui.cursor_x,event.dx,event.flags,ui.canvas.width);
            ui.cursor_y=(int)mouse_position_axis(ui.cursor_y,event.dy,event.flags,ui.canvas.height);
            int pressed=(event.buttons&MOUSE_LEFT) && !(ui.buttons&MOUSE_LEFT);
            int canceled=event.flags&MOUSE_CANCEL;
            struct desk_widget_event pointer={.type=canceled?DESK_WIDGET_CANCEL:DESK_WIDGET_POINTER,
                .x=ui.cursor_x,.y=ui.cursor_y,.down=!!(event.buttons&MOUSE_LEFT),.pressed=pressed};
            if(!canceled && pressed && desk_widget_contains(ui.list,ui.cursor_x,ui.cursor_y)) {
                unsigned row=(unsigned)(ui.cursor_y-ui.list.y)/52;
                if(row<ui.visible)select_row(&ui,ui.first+row);
            }
            if(!canceled && event.wheel_y && desk_widget_contains(ui.list,ui.cursor_x,ui.cursor_y) && ui.catalog.count) {
                int next=(int)ui.selected+(event.wheel_y>0?1:-1);
                if(next>=0 && next<(int)ui.catalog.count)select_row(&ui,(unsigned)next);
            }
            if(desk_button_event(&ui.primary,pointer)&DESK_WIDGET_ACTIVATED) {ui.focus=1;activate(&ui,request);}
            if(ui.save_failed && (desk_button_event(&ui.stay,pointer)&DESK_WIDGET_ACTIVATED)) {ui.focus=2;activate(&ui,request);}
            if(desk_button_event(&ui.back,pointer)&DESK_WIDGET_ACTIVATED)back_to_console(&ui);
            ui.buttons=canceled?0:event.buttons;dirty=1;configure(&ui);
        }
        if(dirty) {
            draw(&ui);
            if(blitrect(0,0,info.width,info.height,pixels,width*scale) || (presenting && gfx_present())) {
                snprintf(result->error,sizeof(result->error),"The profile chooser could not update the display.");break;
            }
            dirty=0;
            if(!announced) {trace_catalog(&ui.catalog);announced=1;}
        }
        gui_server_wait(1);
    }
cleanup:
    /* Also covers a presentation failure during recovery: retain worker memory
     * and child ownership until the mutating operation has completed. */
    if(ui.job.active)pthread_join(ui.job.thread,NULL);
    if(presenting && gfx_present_end())answer=DESKTOP_PROFILE_ERROR;
    if(keyboard)gui_keyboard_mode(0);
    if(muted)gfx_console_mute(0);
    desk_pixels_free(pixels,width*scale,height*scale);
    if(answer==DESKTOP_PROFILE_ERROR && !result->error[0])snprintf(result->error,sizeof(result->error),"The profile chooser could not start. Return to the console and try again.");
    return answer;
}
