#ifndef DESKD_DISPLAY_SETTINGS_H
#define DESKD_DISPLAY_SETTINGS_H
/* Included by deskd.c: display transactions belong to the compositor, never to
 * the Settings process. All rectangles and packet sizes remain logical. */
static int display_ready(const struct session *s) {
    if(s->capture.busy || s->capture_pending || s->exiting)return 0;
    for(int i=0;i<session_capacity(s);i++) {
        const struct client *c=session_client(s,i);
        if(c->pid && (c->configure || c->awaiting || c->closing))return 0;
    }
    return 1;
}
static int display_mode_index(unsigned width,unsigned height) {
    int count=gfx_mode_count();
    if(count<0 || count>GFX_MODE_MAX)return -1;
    for(int i=0;i<count;i++) {struct gfx_mode m={0};if(!gfx_mode_get(i,&m) && m.width==width && m.height==height)return i;}
    return -1;
}
static void display_reply(struct session *s,int i,uint64_t serial,unsigned result) {
    struct desk_display_state state={0};struct display_runtime *d=&s->display;
    if(!d->info)return;
    state.width=d->info->width;state.height=d->info->height;state.scale=session_scale(s);
    state.capabilities=DESK_DISPLAY_CAP_SCALE;
    if(d->info->flags&GFX_FLAG_MODE_SET)state.capabilities|=DESK_DISPLAY_CAP_MODE_SET;
    int count=gfx_mode_count();if(count>GFX_MODE_MAX)count=GFX_MODE_MAX;
    for(int n=0;n<count;n++) {struct gfx_mode m={0};if(gfx_mode_get(n,&m))break;
        state.modes[state.mode_count++]=(struct desk_display_mode){m.width,m.height};}
    if(!state.mode_count) {state.mode_count=1;state.modes[0]=(struct desk_display_mode){state.width,state.height};}
    state.phase=d->trial && !d->startup?DESK_DISPLAY_TRIAL:DESK_DISPLAY_IDLE;
    uint64_t now=srv_monotonic_us();
    if(d->trial && d->deadline>now)state.remaining_seconds=(d->deadline-now+999999)/1000000;
    struct desk_packet p=desk_packet_new(DESK_DISPLAY_STATE);p.serial=serial;p.generation=d->trial?d->token:0;p.value=result;
    memcpy(p.text,&state,sizeof(state));session_client(s,i)->display_reply=p;
}
static void display_notify(struct session *s,unsigned result) {
    for(int i=0;i<session_capacity(s);i++)if(session_client(s,i)->pid && desktop_window_at(&s->scene,i)->application==DESK_APP_SETTINGS)
        display_reply(s,i,session_client(s,i)->display_reply.type?session_client(s,i)->display_reply.serial:0,result);
}
static struct desk_rect display_fit(struct desk_rect r,int width,int height) {
    if(r.w>width-16)r.w=width-16;if(r.h>height-132)r.h=height-132;
    if(r.w>DESK_MAX_WIDTH+2)r.w=DESK_MAX_WIDTH+2;if(r.h>DESK_MAX_HEIGHT+45)r.h=DESK_MAX_HEIGHT+45;
    r.x=clamp(r.x,8,width-8-r.w);r.y=clamp(r.y,52,height-80-r.h);
    return r;
}
static void display_layout(struct session *s,unsigned old_scale,int old_width,int old_height,int restore) {
    struct display_runtime *d=&s->display;struct desk_canvas *canvas=d->canvas;
    cancel_input(s,s->pointer_buttons);s->scene.snap_preview=0;
    s->scene.launcher_open=0;s->scene.calendar_open=0;s->scene.notifications.opened=0;
    desk_popup_cancel(&s->scene.context);s->scene.menu.open=-1;
    desk_damage_init(&s->damage,canvas->width,canvas->height);
    for(int i=0;i<session_capacity(s);i++) {
        struct client *c=session_client(s,i);struct desktop_window *w=desktop_window_at(&s->scene,i);if(!c->pid)continue;
        struct desk_rect previous=w->frame,target=display_fit(w->frame,canvas->width,canvas->height);
        if(restore && i<d->window_count && d->windows[i].identity==c->identity) {
            target=d->windows[i].frame;w->restore=d->windows[i].restore;w->maximized=d->windows[i].maximized;
        } else if(w->maximized) {
            unsigned mode=DESKTOP_MAXIMIZE_PLACEMENT;struct desk_rect placed;
            for(unsigned side=DESKTOP_TILE_LEFT;side<=DESKTOP_TILE_RIGHT;side++)if(desktop_window_placement(old_width,old_height,w->restore,side,&placed) && !memcmp(&placed,&previous,sizeof(placed)))mode=side;
            (void)desktop_window_placement(canvas->width,canvas->height,w->restore,mode,&target);
        }
        w->frame=target;
        if(old_scale!=session_scale(s) || previous.w!=target.w || previous.h!=target.h || c->awaiting) {
            c->resize_origin=target;c->resize_restore=w->restore;c->resize_maximized=w->maximized;
            c->generation++;c->configure=1;c->awaiting=1;c->progress=srv_ticks();
        }
    }
    s->scene.cursor_x=clamp(s->scene.cursor_x,0,canvas->width-1);s->scene.cursor_y=clamp(s->scene.cursor_y,0,canvas->height-1);
    desktop_notice_view_configure(&s->scene.notifications,canvas->width,canvas->height);
    global_menu_rebuild(s);desk_damage_add(&s->damage,(struct desk_rect){0,0,canvas->width,canvas->height});d->changed=1;
}
static void display_cleanup(struct session *s) {
    struct display_runtime *d=&s->display;
    if(d->previous_canvas.pixels)desk_pixels_free(d->previous_canvas.pixels,desk_canvas_pixel_width(&d->previous_canvas),desk_canvas_pixel_height(&d->previous_canvas));
    if(d->windows)munmap(d->windows,d->windows_bytes);
    d->previous_canvas=(struct desk_canvas){0};d->windows=NULL;d->windows_bytes=0;d->window_count=0;
}
/* Prepare the CPU backing before touching scanout. Kernel mode changes either
 * succeed with fenced presentation or report their actual safe recovery state. */
static unsigned display_change(struct session *s,unsigned pw,unsigned ph,unsigned scale,int restore) {
    struct display_runtime *d=&s->display;struct desk_canvas next={0};
    if(!d->canvas || !d->info || (scale!=1 && scale!=2) || pw>3840 || ph>2160 || pw<640 || ph<480)return DESK_DISPLAY_INVALID;
    unsigned width=(pw+scale-1)/scale,height=(ph+scale-1)/scale;
    if(width<640 || height<480)return DESK_DISPLAY_INVALID;
    int mode=display_mode_index(pw,ph);
    int mode_change=pw!=d->info->width || ph!=d->info->height;
    if(mode_change && (!(d->info->flags&GFX_FLAG_MODE_SET) || mode<0))return DESK_DISPLAY_UNSUPPORTED;
    uint64_t needed=s->cache_bytes;
    if(!restore)for(int i=0;i<session_capacity(s);i++)if(session_client(s,i)->pid) {
        struct desk_rect r=display_fit(desktop_window_at(&s->scene,i)->frame,width,height);
        needed+=(uint64_t)(r.w-2)*(r.h-45)*4*scale*scale;
        if(needed>DESKD_CACHE_BUDGET)return DESK_DISPLAY_FAILED;
    }
    int reuse=restore && d->previous_canvas.pixels && d->previous_canvas.width==(int)width && d->previous_canvas.height==(int)height && desk_canvas_scale(&d->previous_canvas)==scale;
    if(reuse)next=d->previous_canvas;
    else {
        uint32_t *pixels=desk_pixels_alloc(width*scale,height*scale);if(!pixels)return DESK_DISPLAY_FAILED;
        if(desk_canvas_init_scaled(&next,pixels,width,height,width*scale,scale)) {desk_pixels_free(pixels,width*scale,height*scale);return DESK_DISPLAY_FAILED;}
    }
    if(mode_change && gfx_mode_set(mode)) {
        if(!reuse)desk_pixels_free(next.pixels,width*scale,height*scale);
        /* Try restoring the prior presentation once. A successful recovery is
         * still a failed request, with the existing desktop untouched. */
        int prior=display_mode_index(d->info->width,d->info->height);
        if(prior>=0 && !gfx_mode_set(prior))return DESK_DISPLAY_FAILED;
        struct gfx_info actual={0};
        if(gfx_info(&actual)) {d->fatal=1;return DESK_DISPLAY_FAILED;}
        /* An inherited VGA recovery remains usable without a fenced presenter.
         * Reconcile it through the same bounded canvas path on the next turn. */
        *d->info=actual;d->abort=d->trial;
        unsigned recovery_scale=session_scale(s);
        if((actual.width+recovery_scale-1)/recovery_scale<640 || (actual.height+recovery_scale-1)/recovery_scale<480)recovery_scale=1;
        if(display_change(s,actual.width,actual.height,recovery_scale,restore)!=DESK_DISPLAY_OK)d->fatal=1;
        return DESK_DISPLAY_FAILED;
    }
    struct gfx_info actual={0};if(gfx_info(&actual) || actual.width!=pw || actual.height!=ph) {
        if(!reuse)desk_pixels_free(next.pixels,width*scale,height*scale);d->fatal=1;return DESK_DISPLAY_FAILED;
    }
    unsigned previous_scale=session_scale(s);int old_width=d->canvas->width,old_height=d->canvas->height;
    struct desk_canvas previous=*d->canvas;*d->canvas=next;*d->info=actual;s->scale=scale;
    if(reuse)d->previous_canvas=(struct desk_canvas){0};
    if(!restore && !d->previous_canvas.pixels)d->previous_canvas=previous;
    else desk_pixels_free(previous.pixels,desk_canvas_pixel_width(&previous),desk_canvas_pixel_height(&previous));
    display_layout(s,previous_scale,old_width,old_height,restore);
    desk_log("deskd: display-applied physical=%ux%u logical=%ux%u scale=%u restore=%d\n",pw,ph,width,height,scale,restore);
    return DESK_DISPLAY_OK;
}
static unsigned display_revert(struct session *s) {
    struct display_runtime *d=&s->display;
    unsigned result=display_change(s,d->previous_info.width,d->previous_info.height,d->previous_scale,1);
    if(result==DESK_DISPLAY_OK) {
        d->trial=0;d->startup=0;d->abort=0;display_cleanup(s);display_notify(s,DESK_DISPLAY_OK);
        desk_log("deskd: display-reverted\n");
    }
    return result;
}
static void display_received(struct session *s,int i,const struct desk_packet *p) {
    struct client *c=session_client(s,i);struct display_runtime *d=&s->display;
    unsigned result=DESK_DISPLAY_OK;
    if(!p->serial || c->closing || !d->canvas || !d->info)return;
    if(p->value==DESK_DISPLAY_QUERY) {display_reply(s,i,p->serial,result);return;}
    if(!s->scene.apps || desktop_window_at(&s->scene,i)->application!=DESK_APP_SETTINGS || s->scene.focus!=i || s->exiting)result=DESK_DISPLAY_INVALID;
    else if(p->value==DESK_DISPLAY_APPLY) {
        if(d->trial || !display_ready(s) || s->preferences.loading)result=DESK_DISPLAY_BUSY;
        else if(p->width==d->info->width && p->height==d->info->height && p->x==(int)session_scale(s))result=DESK_DISPLAY_INVALID;
        else {
            d->window_count=session_capacity(s);d->windows_bytes=(size_t)d->window_count*sizeof(*d->windows);
            d->windows=mmap(NULL,d->windows_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
            if(d->windows==MAP_FAILED) {d->windows=NULL;result=DESK_DISPLAY_FAILED;}
            else {
                for(int n=0;n<d->window_count;n++) {const struct desktop_window *w=desktop_window_at(&s->scene,n);
                    d->windows[n]=(struct display_window_backup){session_client(s,n)->identity,w->frame,w->restore,w->maximized};}
                d->previous_info=*d->info;d->previous_scale=session_scale(s);d->owner=c->identity;
                result=display_change(s,p->width,p->height,p->x,0);
                if(result==DESK_DISPLAY_OK) {d->trial=1;d->token++;if(!d->token)d->token++;d->deadline=(uint64_t)srv_monotonic_us()+15000000;}
                else display_cleanup(s);
            }
        }
    } else if(p->value==DESK_DISPLAY_KEEP || p->value==DESK_DISPLAY_REVERT) {
        if(!d->trial || d->owner!=c->identity || p->generation!=d->token)result=DESK_DISPLAY_INVALID;
        else if(p->value==DESK_DISPLAY_REVERT)result=display_revert(s);
        else if(!display_ready(s) || d->abort || (uint64_t)srv_monotonic_us()>=d->deadline)result=DESK_DISPLAY_BUSY;
        else {
            struct desk_preferences value=s->preferences.value;
            value.display_width=d->info->width;value.display_height=d->info->height;value.display_scale=session_scale(s);
            if(desktop_preferences_set(&s->preferences,&value,DESK_PREF_DISPLAY,srv_ticks())<0)result=DESK_DISPLAY_FAILED;
            else {d->trial=0;display_cleanup(s);preferences_changed(s);desk_log("deskd: display-kept scale=%u resolution=%llux%llu\n",value.display_scale,(unsigned long long)d->info->width,(unsigned long long)d->info->height);}
        }
    } else result=DESK_DISPLAY_INVALID;
    display_reply(s,i,p->serial,result);
}
static void display_tick(struct session *s) {
    struct display_runtime *d=&s->display;if(!d->info || !d->canvas)return;
    if(d->trial) {
        if(d->startup && !d->abort && display_ready(s)) {d->trial=0;d->startup=0;display_cleanup(s);display_notify(s,DESK_DISPLAY_OK);return;}
        int owner=d->startup;for(int i=0;i<session_capacity(s);i++)if(session_client(s,i)->pid && session_client(s,i)->identity==d->owner && !session_client(s,i)->closing)owner=1;
        if(!owner || d->abort || s->exiting || (uint64_t)srv_monotonic_us()>=d->deadline) {
            if(display_revert(s)!=DESK_DISPLAY_OK) {d->trial=0;d->startup=0;d->abort=0;display_cleanup(s);display_notify(s,DESK_DISPLAY_FAILED);snprintf(s->scene.notice,sizeof(s->scene.notice),"Display recovery failed. Current settings were not saved.");}
        }
    } else if(!d->restore_done && !s->preferences.loading && display_ready(s)) {
        d->restore_done=1;const struct desk_preferences *p=&s->preferences.value;
        unsigned pw=p->display_width?p->display_width:d->info->width,ph=p->display_height?p->display_height:d->info->height;
        unsigned scale=d->explicit_scale?session_scale(s):p->display_scale;
        if(pw!=d->info->width || ph!=d->info->height || scale!=session_scale(s)) {
            d->window_count=session_capacity(s);d->windows_bytes=(size_t)d->window_count*sizeof(*d->windows);
            d->windows=mmap(NULL,d->windows_bytes,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);
            if(d->windows==MAP_FAILED) {d->windows=NULL;return;}
            for(int n=0;n<d->window_count;n++) {const struct desktop_window *w=desktop_window_at(&s->scene,n);
                d->windows[n]=(struct display_window_backup){session_client(s,n)->identity,w->frame,w->restore,w->maximized};}
            d->previous_info=*d->info;d->previous_scale=session_scale(s);
            unsigned result=display_change(s,pw,ph,scale,0);
            if(result) {display_cleanup(s);desk_log("deskd: saved display unavailable result=%u\n",result);}
            else {d->trial=1;d->startup=1;d->deadline=(uint64_t)srv_monotonic_us()+15000000;}
        }
    }
}
/* Captures keep their existing bounded logical dimensions. Average native
 * samples instead of reading a physical-stride buffer as tightly packed 1x. */
static void capture_tick_scaled(struct session *s,const struct desk_canvas *canvas) {
    unsigned scale=desk_canvas_scale(canvas);uint32_t *logical=NULL;
    int snapshot=s->capture_pending || (s->worker && life_state(&s->worker->prepare.state)==LIFE_DONE);
    if(scale==1 || !snapshot) {capture_tick(s,scale==1?canvas->pixels:NULL);return;}
    logical=desk_pixels_alloc(canvas->width,canvas->height);
    if(logical)for(int y=0;y<canvas->height;y++)for(int x=0;x<canvas->width;x++) {
        unsigned red=0,green=0,blue=0;
        for(unsigned dy=0;dy<scale;dy++)for(unsigned dx=0;dx<scale;dx++) {
            uint32_t pixel=canvas->pixels[(size_t)(y*scale+dy)*canvas->stride+x*scale+dx];
            red+=(pixel>>16)&255;green+=(pixel>>8)&255;blue+=pixel&255;
        }
        unsigned samples=scale*scale;logical[(size_t)y*canvas->width+x]=((red/samples)<<16)|((green/samples)<<8)|(blue/samples);
    }
    capture_tick(s,logical);desk_pixels_free(logical,canvas->width,canvas->height);
}
#endif
