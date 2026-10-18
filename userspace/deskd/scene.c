#include "scene.h"
#include <srvros/desktop_protocol.h>
#include <srvros/desktop_apps.h>
#include <stdio.h>
#include <string.h>

int desktop_window_placement(int screen_width,int screen_height,struct desk_rect original,
    unsigned placement,struct desk_rect *result) {
    if(!result || screen_width<1 || screen_height<1 || screen_width>16384 || screen_height>16384 ||
       placement<DESKTOP_TILE_LEFT || placement>DESKTOP_CENTER) return 0;
    int available_w=screen_width-16,available_h=screen_height-132;
    if(available_w<282 || available_h<285) return 0;
    int width,height;
    if(placement==DESKTOP_CENTER) {
        width=original.w;height=original.h;
        if(width<282) width=282;
        if(height<285) height=285;
        if(width>available_w) width=available_w;
        if(height>available_h) height=available_h;
    } else {
        width=(available_w-8)/2;height=available_h;
        if(width<282) return 0;
    }
    if(width>DESK_MAX_WIDTH+2) width=DESK_MAX_WIDTH+2;
    if(height>DESK_MAX_HEIGHT+45) height=DESK_MAX_HEIGHT+45;
    int x=placement==DESKTOP_TILE_LEFT?8:placement==DESKTOP_TILE_RIGHT?screen_width-width-8:(screen_width-width)/2;
    int y=52+(placement==DESKTOP_CENTER?(available_h-height)/2:0);
    *result=(struct desk_rect){x,y,width,height};return 1;
}

void desktop_stack(const struct desktop_scene *s,int order[DESKD_WINDOWS]) {
    for(int i=0;i<DESKD_WINDOWS;i++) order[i]=i;
    for(int i=1;i<DESKD_WINDOWS;i++) {
        int value=order[i],j=i;
        while(j>0) {
            int previous=order[j-1];
            int higher=value==s->focus || (previous!=s->focus &&
                (s->windows[value].z>s->windows[previous].z ||
                (s->windows[value].z==s->windows[previous].z && value>previous)));
            if(higher) break;
            order[j]=previous;j--;
        }
        order[j]=value;
    }
}
struct desk_rect desktop_dock_item(int width,int height,int application) {
    const struct desk_application_info *info=desk_application_info(application);
    if(!info || !info->dock) return (struct desk_rect){0};
    int total=(int)desk_dock_count()*100;
    return (struct desk_rect){(width-total)/2+(int)(info->dock-1)*100,height-68,100,60};
}
static void icon(struct desk_canvas *c,int app,int x,int y) {
    if(app==DESK_APP_FILES) {
        desk_fill(c,(struct desk_rect){x,y+2,12,6},0x748b79);
        desk_fill(c,(struct desk_rect){x,y+7,30,19},0x748b79);
        desk_fill(c,(struct desk_rect){x+1,y+10,28,15},0xa7b69e);
    } else if(app==DESK_APP_TEXT) {
        desk_fill(c,(struct desk_rect){x+4,y,23,29},0x748b79);
        desk_fill(c,(struct desk_rect){x+5,y+1,21,27},0xfaf9f6);
        for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){x+9,y+7+i*5,i==3?8:13,1},0x748b79);
    } else if(app==DESK_APP_CALCULATOR) {
        desk_fill(c,(struct desk_rect){x+3,y,25,29},0x748b79);
        desk_fill(c,(struct desk_rect){x+6,y+3,19,7},0xe0eade);
        for(int i=0;i<6;i++) desk_fill(c,(struct desk_rect){x+6+(i%3)*7,y+14+(i/3)*7,5,5},0xfaf9f6);
    } else if(app==DESK_APP_SYSTEM) {
        desk_fill(c,(struct desk_rect){x,y+1,31,23},0x748b79);
        desk_fill(c,(struct desk_rect){x+2,y+3,27,19},0xfaf9f6);
        for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){x+5+i*6,y+18-i*3,3,3+i*3},0x748b79);
        desk_fill(c,(struct desk_rect){x+11,y+24,9,4},0x748b79);
    } else if(app==DESK_APP_TERMINAL) {
        desk_fill(c,(struct desk_rect){x,y+1,31,26},0x748b79);
        desk_fill(c,(struct desk_rect){x+2,y+3,27,22},0xfaf9f6);
        desk_text(c,&desk_font_16,x+5,y+3,">_",0x38644f);
    } else {
        for(int i=0;i<3;i++) {
            desk_fill(c,(struct desk_rect){x+2,y+5+i*9,27,2},0x748b79);
            desk_fill(c,(struct desk_rect){x+7+(i%2)*12,y+2+i*9,5,8},0x38644f);
        }
    }
}
static void dock(struct desk_canvas *c,const struct desktop_scene *s) {
    int width=(int)desk_dock_count()*100;
    desk_fill(c,(struct desk_rect){(c->width-width)/2-8,c->height-74,width+16,70},0xf1f0eb);
    for(int app=1;app<=DESK_APP_COUNT;app++) {
        struct desk_rect r=desktop_dock_item(c->width,c->height,app);if(!r.w) continue;
        const char *name=desk_application_info(app)->title;
        int running=0,active=0;
        for(int i=0;i<DESKD_WINDOWS;i++) if(s->windows[i].visible && s->windows[i].application==app) {
            running=1;active|=s->focus==i;
        }
        if(active) desk_fill(c,(struct desk_rect){r.x+2,r.y+1,r.w-4,r.h-3},0xe0eade);
        icon(c,app,r.x+34,r.y+2);
        int text=desk_text_width(&desk_font_16,name);
        desk_text(c,&desk_font_16,r.x+(r.w-text)/2,r.y+34,name,0x292e2b);
        if(running) desk_fill(c,(struct desk_rect){r.x+48,r.y+56,4,2},0x38644f);
    }
}

struct desk_rect desktop_window_control_rect(struct desk_rect frame,unsigned control) {
    if(control<DESKTOP_CLOSE || control>DESKTOP_MINIMIZE) return (struct desk_rect){0};
    return (struct desk_rect){frame.x+frame.w-8-32*(int)control,frame.y+1,32,42};
}
unsigned desktop_titlebar_hit(struct desk_rect frame,int standard,int x,int y) {
    for(unsigned control=DESKTOP_CLOSE;control<=(standard?DESKTOP_MINIMIZE:DESKTOP_CLOSE);control++) {
        struct desk_rect r=desktop_window_control_rect(frame,control);
        if(x>=r.x && x<r.x+r.w && y>=r.y && y<r.y+r.h) return control;
    }
    return 0;
}
static void window_control(struct desk_canvas *c,struct desk_rect frame,unsigned control,int restored,int focused) {
    struct desk_rect hit=desktop_window_control_rect(frame,control);
    struct desk_rect box={hit.x+2,hit.y+5,28,30};
    uint32_t ink=focused?0x46574b:0x646b65,bg=focused?0xfaf9f6:0xf1f0eb;
    desk_fill(c,box,focused?0x9ca99b:0xb2baaf);
    desk_fill(c,desk_widget_inset(box,1),bg);
    int x=hit.x+9,y=hit.y+14;
    if(control==DESKTOP_MINIMIZE) desk_fill(c,(struct desk_rect){x,y+10,14,2},ink);
    else if(control==DESKTOP_MAXIMIZE) {
        if(restored) {
            desk_fill(c,(struct desk_rect){x+3,y-3,12,11},ink);
            desk_fill(c,(struct desk_rect){x+5,y-1,8,7},bg);
        }
        desk_fill(c,(struct desk_rect){x,y,14,12},ink);
        desk_fill(c,(struct desk_rect){x+2,y+3,10,7},bg);
    } else for(int n=0;n<10;n++) {
        desk_fill(c,(struct desk_rect){x+2+n,y+1+n,2,2},ink);
        desk_fill(c,(struct desk_rect){x+11-n,y+1+n,2,2},ink);
    }
}
static void draw_window(struct desk_canvas *c,const struct desktop_window *w,int focused,int standard) {
    if (!w->visible || w->minimized) return;
    struct desk_rect r=w->frame;
    desk_fill(c,(struct desk_rect){r.x+5,r.y+7,r.w,r.h},0xb9c2b4);
    desk_fill(c,r,focused?0x748b79:0xb2baaf);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,r.h-2},0xfaf9f6);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,42},focused?0xe7ebe2:0xf1f0eb);
    struct desk_rect clip=c->clip;
    desk_clip(c,desk_intersect(clip,(struct desk_rect){r.x+16,r.y+1,r.w-(standard?126:62),41}));
    desk_label_ellipsis(c,(struct desk_rect){r.x+18,r.y+12,r.w-(standard?128:64),24},&desk_font_16,w->title,0x292e2b);
    c->clip=clip;
    for(unsigned control=DESKTOP_CLOSE;control<=(standard?DESKTOP_MINIMIZE:DESKTOP_CLOSE);control++)
        window_control(c,r,control,w->maximized,focused);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+43,r.w-2,1},0xd9ddd5);
    if (w->pixels) {
        struct desk_rect content={r.x+1,r.y+44,w->width,w->height};
        struct desk_rect visible=desk_intersect(c->clip,
            desk_intersect(content,(struct desk_rect){r.x+1,r.y+44,r.w-2,r.h-45}));
        for (int y=visible.y;y<visible.y+visible.h;y++)
            for (int x=visible.x;x<visible.x+visible.w;x++)
                c->pixels[(size_t)y*c->stride+x]=w->pixels[(size_t)(y-content.y)*w->width+x-content.x];
        c->pixel_writes+=(uint64_t)visible.w*visible.h;
    }
    for (int n=0;n<3;n++)
        desk_fill(c,(struct desk_rect){r.x+r.w-5-4*n,r.y+r.h-5,2,2},0x748b79);
}
struct desk_rect desktop_clock_rect(int width,const char *clock) {
    int text=desk_text_width(&desk_font_16,clock);return (struct desk_rect){width-text-24,0,text+24,44};
}
struct desk_rect desktop_alert_rect(int width,const char *clock) {
    struct desk_rect r=desktop_clock_rect(width,clock);return (struct desk_rect){r.x-94,0,90,44};
}
static void calendar_panel(struct desk_canvas *c,const struct desktop_scene *s) {
    struct desk_rect r=s->calendar_frame,clip=c->clip;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);
    desk_fill(c,r,desk_theme_quiet.border);desk_fill(c,desk_widget_inset(r,1),desk_theme_quiet.surface);
    desk_clip(c,desk_intersect(clip,r));
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+12,140,28},&desk_font_20,"Calendar",desk_theme_quiet.text);
    desk_label_draw(c,(struct desk_rect){r.x+r.w-148,r.y+16,132,24},&desk_font_16,s->calendar_zone,desk_theme_quiet.muted);
    desk_ui_draw(c,&s->calendar_ui,&desk_theme_quiet);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+r.h-82,r.w-32,24},&desk_font_16,s->calendar_date,desk_theme_quiet.text);
    desk_clip(c,clip);
}
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s) {
    static const uint32_t backgrounds[]={0xd6dcd2,0xe2dace,0xcbd5da};
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},backgrounds[s->wallpaper>=0 && s->wallpaper<3?s->wallpaper:0]);
    desk_fill(c,(struct desk_rect){0,0,c->width,44},0xf1f0eb);
    if(s->apps) {
        desk_menu_bar_draw(c,&s->menu,&desk_theme_quiet);
        if(s->calendar_open) desk_fill(c,desktop_clock_rect(c->width,s->clock),desk_theme_quiet.selected);
        struct desk_rect alerts=desktop_alert_rect(c->width,s->clock);char label[24];unsigned unread=desktop_notifications_unread(&s->notifications.store);
        if(s->notifications.opened || unread) desk_fill(c,alerts,desk_theme_quiet.selected);
        if(unread) snprintf(label,sizeof(label),"Alerts %u",unread);else strcpy(label,"Alerts");
        desk_label_draw(c,(struct desk_rect){alerts.x+6,12,alerts.w-12,24},&desk_font_16,label,desk_theme_quiet.text);
        int clock_width=desk_text_width(&desk_font_16,s->clock);
        desk_label_draw(c,(struct desk_rect){c->width-clock_width-16,12,clock_width,24},&desk_font_16,s->clock,0x646b65);
    } else desk_text(c,&desk_font_16,24,12,s->trackpad_trial?"SrvrOS   /   Trackpad trial":"SrvrOS   /   Desktop",0x292e2b);
    if(!s->apps) desk_text(c,&desk_font_16,24,c->height-30,s->trackpad_trial==3?s->io_status:
        s->trackpad_trial==2?"Two fingers: scroll   Move / press-drag   Type to test   Esc: save checks":
        s->trackpad_trial?"One finger: move / press-drag   Type to test   Esc: save checks":
        "Tab: switch window   Ctrl-N: open study   Esc: console",0x465348);
    if (s->trackpad_trial>=2 && c->height>=700) {
        int y=c->height-269;
        desk_fill(c,(struct desk_rect){24,y,c->width-48,218},0xf1f0eb);
        const char *steps[]={
            "Trackpad checks - take your time",
            "1. Drag the scrollbar thumb; move outside the window while held, then release.",
            "   Click the pale track above/below it. Also check two-finger scrolling.",
            "2. Hold a letter key while brushing the pad lightly. Do not press the pad.",
            "   The pointer should stay still. Release the key AND lift your hand; try moving again.",
            "3. Without typing, rest a palm edge on the pad. Note any unwanted movement.",
            "   Lift completely, then check normal movement, clicking, dragging and resizing.",
            "4. Press Escape for live filter counters and the final save instructions."
        };
        const char *io_steps[]={
            s->io_next[0]?s->io_next:"Input + USB file work - keep testing while the status below says RUNNING",
            "1. Press Space to start USB work, then type, scroll, drag and resize.",
            "2. Two studies are ready. Tab switches focus; type 32+ characters in both.",
            "3. Click AND HOLD the scrollbar thumb, press Tab while held, then release.",
            "   Moving afterward must NOT continue the old drag. Try a fresh drag.",
            "4. Test until 16+ rounds, 100+ active samples and 32+ text samples; all checks OK.",
            "5. Escape stops file work after its current round, then shows save checks.",
            s->io_check[0]?s->io_check:"   Complete all coverage counters below before Escape."
        };
        for (unsigned i=0;i<sizeof(steps)/sizeof(*steps);i++)
            desk_text(c,&desk_font_16,40,y+12+24*i,s->trackpad_trial==3?io_steps[i]:steps[i],i?0x465348:0x292e2b);
    }
    int order[DESKD_WINDOWS];desktop_stack(s,order);
    for(int n=0;n<DESKD_WINDOWS;n++) draw_window(c,&s->windows[order[n]],order[n]==s->focus,s->apps);
    if(s->apps) {
        dock(c,s);
        if(s->notice[0]) desk_text(c,&desk_font_16,24,c->height-98,s->notice,0x38644f);
        desk_menu_popup_draw(c,&s->menu,&desk_theme_quiet);
        if(s->calendar_open) calendar_panel(c,s);
        if(s->launcher_open) desk_palette_draw(c,&s->launcher,&desk_theme_quiet);
        if(s->notifications.opened || (!s->launcher_open && !s->calendar_open && s->menu.open<0 && !desk_popup_active(&s->context))) desktop_notice_view_draw(c,&s->notifications,&desk_theme_quiet);
    }
    if(s->apps) desk_popup_draw(c,&s->context,&desk_theme_quiet);
    for (int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+y,1+y/2,1},0x292e2b);
        if (y>1 && y<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+y,y/2,1},0xfaf9f6);
    }
}
