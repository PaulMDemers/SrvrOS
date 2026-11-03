#include "scene.h"
#include <srvros/desktop_protocol.h>
#include <srvros/desktop_apps.h>
#include <stdio.h>
#include <string.h>

int desktop_window_drag(int width,int height,struct desk_rect original,
    int64_t x,int64_t y,struct desk_rect *result) {
    if(!result || width<160 || height<156 || width>16384 || height>16384 ||
       original.w<282 || original.h<285 || original.w>DESK_MAX_WIDTH+2 || original.h>DESK_MAX_HEIGHT+45)return 0;
    int left=160-original.w,right=width-160,bottom=height-80-32;
    original.x=x<left?left:x>right?right:(int)x;
    original.y=y<44?44:y>bottom?bottom:(int)y;
    *result=original;return 1;
}

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

void desktop_stack(const struct desktop_scene *s,int *order) {
    for(int i=0;i<desktop_capacity(s);i++) order[i]=i;
    for(int i=1;i<desktop_capacity(s);i++) {
        int value=order[i],j=i;
        while(j>0) {
            int previous=order[j-1];
            int higher=value==s->focus || (previous!=s->focus &&
                (desktop_window_at(s,value)->z>desktop_window_at(s,previous)->z ||
                (desktop_window_at(s,value)->z==desktop_window_at(s,previous)->z && value>previous)));
            if(higher) break;
            order[j]=previous;j--;
        }
        order[j]=value;
    }
}
unsigned desktop_dock_entries(const struct desktop_scene *s,int width,int height,
    struct desktop_dock_entry entries[DESK_APP_COUNT]) {
    if(!s || !entries || width<160 || height<156)return 0;
    struct desktop_dock_entry apps[DESK_APP_COUNT]={0};
    for(int i=0;i<desktop_capacity(s);i++) {
        const struct desktop_window *w=desktop_window_at(s,i);
        if(!w->visible || w->application<1 || w->application>DESK_APP_COUNT)continue;
        struct desktop_dock_entry *entry=&apps[w->application-1];
        if(!w->dialog) {entry->windows++;entry->minimized+=w->minimized!=0;}
        entry->active|=s->focus==i && !w->minimized;
    }
    unsigned count=0;
    for(unsigned position=1;position<=desk_dock_count();position++)for(unsigned app=1;app<=DESK_APP_COUNT;app++)
        if(desk_application_info(app)->dock==position) {entries[count]=apps[app-1];entries[count++].application=app;}
    for(unsigned app=1;app<=DESK_APP_COUNT;app++)if(!desk_application_info(app)->dock && apps[app-1].windows) {
        entries[count]=apps[app-1];entries[count++].application=app;
    }
    int cell=(width-24)/(int)count;if(cell>100)cell=100;
    int start=(width-cell*(int)count)/2;
    for(unsigned i=0;i<count;i++)entries[i].frame=(struct desk_rect){start+(int)i*cell,height-68,cell,60};
    return count;
}
struct desk_rect desktop_dock_frame(const struct desktop_scene *s,int width,int height) {
    struct desktop_dock_entry entries[DESK_APP_COUNT];unsigned count=desktop_dock_entries(s,width,height,entries);
    if(!count)return (struct desk_rect){0};
    return (struct desk_rect){entries[0].frame.x-8,height-74,entries[0].frame.w*(int)count+16,70};
}
unsigned desktop_dock_hit(const struct desktop_scene *s,int width,int height,int x,int y) {
    struct desktop_dock_entry entries[DESK_APP_COUNT];unsigned count=desktop_dock_entries(s,width,height,entries);
    for(unsigned i=0;i<count;i++)if(desk_widget_contains(entries[i].frame,x,y))return entries[i].application;
    return 0;
}
static void icon(struct desk_canvas *c,int app,int x,int y,const struct desk_theme *t) {
    if(app==DESK_APP_FILES) {
        desk_fill(c,(struct desk_rect){x,y+2,12,6},t->chrome_border);
        desk_fill(c,(struct desk_rect){x,y+7,30,19},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+1,y+10,28,15},t->selected);
    } else if(app==DESK_APP_TEXT) {
        desk_fill(c,(struct desk_rect){x+4,y,23,29},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+5,y+1,21,27},t->field?t->field:t->surface);
        for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){x+9,y+7+i*5,i==3?8:13,1},t->chrome_border);
    } else if(app==DESK_APP_CALCULATOR) {
        desk_fill(c,(struct desk_rect){x+3,y,25,29},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+6,y+3,19,7},t->selected);
        for(int i=0;i<6;i++) desk_fill(c,(struct desk_rect){x+6+(i%3)*7,y+14+(i/3)*7,5,5},t->field?t->field:t->surface);
    } else if(app==DESK_APP_SYSTEM) {
        desk_fill(c,(struct desk_rect){x,y+1,31,23},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+2,y+3,27,19},t->field?t->field:t->surface);
        for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){x+5+i*6,y+18-i*3,3,3+i*3},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+11,y+24,9,4},t->chrome_border);
    } else if(app==DESK_APP_TERMINAL) {
        desk_fill(c,(struct desk_rect){x,y+1,31,26},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+2,y+3,27,22},t->field?t->field:t->surface);
        desk_text(c,&desk_font_16,x+5,y+3,">_",t->accent);
    } else if(app==DESK_APP_SETTINGS) {
        for(int i=0;i<3;i++) {
            desk_fill(c,(struct desk_rect){x+2,y+5+i*9,27,2},t->chrome_border);
            desk_fill(c,(struct desk_rect){x+7+(i%2)*12,y+2+i*9,5,8},t->accent);
        }
    } else {
        char label[]={desk_application_info(app)->title[0],0};if(app==DESK_APP_HELP)label[0]='?';
        desk_fill(c,(struct desk_rect){x+1,y+1,29,27},t->chrome_border);
        desk_fill(c,(struct desk_rect){x+2,y+2,27,25},t->field?t->field:t->surface);
        desk_text(c,&desk_font_20,x+9,y+1,label,t->accent);
    }
}
static const struct desk_theme *scene_theme(const struct desktop_scene *s) { return s->theme.body?&s->theme:&desk_theme_quiet; }
static void dock(struct desk_canvas *c,const struct desktop_scene *s) {
    const struct desk_theme *t=scene_theme(s);
    struct desktop_dock_entry entries[DESK_APP_COUNT];unsigned count=desktop_dock_entries(s,c->width,c->height,entries);
    struct desk_rect frame=desktop_dock_frame(s,c->width,c->height);
    desk_theme_glass(c,frame,t->chrome_top,t->chrome_bottom,t->glass_alpha,t->corner_radius);
    desk_fill(c,(struct desk_rect){frame.x+8,frame.y+1,frame.w-16,1},0xffffff);
    for(unsigned i=0;i<count;i++) {
        struct desktop_dock_entry entry=entries[i];unsigned app=entry.application;struct desk_rect r=entry.frame;
        const char *name=desk_application_info(app)->title;
        if(entry.active) desk_fill(c,(struct desk_rect){r.x+2,r.y+1,r.w-4,r.h-3},t->selected);
        struct desk_rect clip=c->clip;desk_clip(c,desk_intersect(clip,r));
        int x=r.x+(r.w-32)/2;icon(c,app,x,r.y+2,t);
        int text=desk_text_width(&desk_font_16,name);
        desk_label_ellipsis(c,(struct desk_rect){text<=r.w-8?r.x+(r.w-text)/2:r.x+4,r.y+34,r.w-8,22},&desk_font_16,name,0x252a30);
        if(entry.windows) {
            struct desk_rect mark={r.x+(r.w-24)/2,r.y+55,24,4};desk_fill(c,mark,t->accent);
            if(entry.minimized==entry.windows)desk_fill(c,desk_widget_inset(mark,1),0xe8ebef);
        }
        if(entry.windows>1 && r.w>=40) {
            char text[16];if(entry.windows>99)strcpy(text,"99+");else snprintf(text,sizeof(text),"%u",entry.windows);
            int width=desk_text_width(&desk_font_16,text)+6;
            int badge=x+20;if(badge+width>r.x+r.w-2)badge=r.x+r.w-width-2;
            desk_fill(c,(struct desk_rect){badge,r.y,width,20},t->accent);
            desk_text(c,&desk_font_16,badge+3,r.y,text,0xffffff);
        }
        c->clip=clip;
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
static void window_control(struct desk_canvas *c,struct desk_rect frame,unsigned control,int restored,int focused,const struct desk_theme *t) {
    struct desk_rect hit=desktop_window_control_rect(frame,control);
    struct desk_rect box={hit.x+2,hit.y+5,28,30};
    uint32_t ink=focused?0x354352:0x596572,bg=focused?0xf0f1f3:0xe8ebef;
    desk_theme_panel(c,box,t,0,0);
    if(control==DESKTOP_CLOSE && focused) {
        desk_theme_gradient(c,desk_widget_inset(box,1),0xe59085,0xb94e43);
        ink=0xffffff;
    }
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
static void draw_window(struct desk_canvas *c,const struct desktop_window *w,int focused,int standard,const struct desk_theme *t,int skip_body,int skip_content) {
    if (!w->visible || w->minimized) return;
    struct desk_rect r=w->frame;
    /* Shadow stays inside the established right+5 / bottom+7 damage bounds.
       Do not tint beneath the window itself: title glass sees the scene below. */
    if(r.w>0 && r.h>0) {
        for(unsigned edge=0;edge<5;edge++)
            desk_theme_glass(c,(struct desk_rect){r.x+r.w+(int)edge,r.y+7,1,r.h},0x263647,0x263647,72-edge*12,0);
        for(unsigned edge=0;edge<7;edge++)
            desk_theme_glass(c,(struct desk_rect){r.x+5,r.y+r.h+(int)edge,r.w-5,1},0x263647,0x263647,72-edge*9,0);
    }
    if(r.w>=128 && r.h>=45) {
        desk_fill(c,(struct desk_rect){r.x,r.y+44,1,r.h-44},t->chrome_border);
        desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y+44,1,r.h-44},t->chrome_border);
        desk_fill(c,(struct desk_rect){r.x+1,r.y+r.h-1,r.w-2,1},t->chrome_border);
        if(!skip_body)desk_fill(c,(struct desk_rect){r.x+1,r.y+44,r.w-2,r.h-45},t->surface);
    } else {
        desk_fill(c,(struct desk_rect){r.x,r.y+44,r.w,r.h-44},t->chrome_border);
        desk_fill(c,(struct desk_rect){r.x+1,r.y+44,r.w-2,r.h-45},t->surface);
    }
    desk_theme_glass(c,(struct desk_rect){r.x,r.y,r.w,44},t->chrome_top,t->chrome_bottom,
        focused?t->glass_alpha:(unsigned)((t->glass_alpha+255)/2),t->corner_radius);
    desk_theme_glass(c,(struct desk_rect){r.x+3,r.y+2,r.w-6,18},0xffffff,t->chrome_top,32,1);
    desk_fill(c,(struct desk_rect){r.x+8,r.y+1,r.w-16,1},0xffffff);
    struct desk_rect clip=c->clip;
    desk_clip(c,desk_intersect(clip,(struct desk_rect){r.x+16,r.y+1,r.w-(standard?126:62),41}));
    /* Low-opacity, theme-colored layers soften the background under the text
       without an opaque label. All layers stay within the title clip. */
    int title_width=desk_text_width(&desk_font_16,w->title);
    int title_limit=r.w-(standard?128:64);
    if(title_width>title_limit)title_width=title_limit;
    if(title_width>0) {
        desk_theme_glass(c,(struct desk_rect){r.x+12,r.y+5,title_width+16,31},t->chrome_top,t->chrome_top,20,8);
        desk_theme_glass(c,(struct desk_rect){r.x+14,r.y+7,title_width+12,27},t->chrome_top,t->chrome_top,30,7);
        desk_theme_glass(c,(struct desk_rect){r.x+16,r.y+9,title_width+8,23},t->chrome_top,t->chrome_top,48,6);
        desk_theme_glass(c,(struct desk_rect){r.x+17,r.y+10,title_width+6,21},t->chrome_top,t->chrome_top,60,5);
    }
    desk_label_ellipsis(c,(struct desk_rect){r.x+18,r.y+12,r.w-(standard?128:64),24},&desk_font_16,w->title,t->text);
    c->clip=clip;
    for(unsigned control=DESKTOP_CLOSE;control<=(standard?DESKTOP_MINIMIZE:DESKTOP_CLOSE);control++)
        window_control(c,r,control,w->maximized,focused,t);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+43,r.w-2,1},t->chrome_border);
    if (w->pixels && !skip_body && !skip_content) {
        struct desk_rect content={r.x+1,r.y+44,w->width,w->height};
        struct desk_rect visible=desk_intersect(c->clip,
            desk_intersect(content,(struct desk_rect){r.x+1,r.y+44,r.w-2,r.h-45}));
        for (int y=visible.y;y<visible.y+visible.h;y++)
            for (int x=visible.x;x<visible.x+visible.w;x++)
                c->pixels[(size_t)y*c->stride+x]=w->pixels[(size_t)(y-content.y)*w->width+x-content.x];
        c->pixel_writes+=(uint64_t)visible.w*visible.h;
    }
    if(!skip_body || r.h<49) for (int n=0;n<3;n++)
        desk_fill(c,(struct desk_rect){r.x+r.w-5-4*n,r.y+r.h-5,2,2},t->chrome_border);
}
struct desk_rect desktop_clock_rect(int width,const char *clock) {
    int text=desk_text_width(&desk_font_16,clock);return (struct desk_rect){width-text-24,0,text+24,44};
}
struct desk_rect desktop_battery_rect(int width,const char *clock) {
    struct desk_rect r=desktop_clock_rect(width,clock);return (struct desk_rect){r.x-238,0,140,44};
}
struct desk_rect desktop_alert_rect(int width,const char *clock) {
    struct desk_rect r=desktop_clock_rect(width,clock);return (struct desk_rect){r.x-94,0,90,44};
}
static void calendar_panel(struct desk_canvas *c,const struct desktop_scene *s) {
    const struct desk_theme *t=scene_theme(s);
    struct desk_rect r=s->calendar_frame,clip=c->clip;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0x697b90);
    desk_fill(c,r,t->border);desk_fill(c,desk_widget_inset(r,1),t->surface);
    desk_clip(c,desk_intersect(clip,r));
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+12,140,28},&desk_font_20,"Calendar",t->text);
    desk_label_draw(c,(struct desk_rect){r.x+r.w-148,r.y+16,132,24},&desk_font_16,s->calendar_zone,t->muted);
    desk_ui_draw(c,&s->calendar_ui,t);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+r.h-82,r.w-32,24},&desk_font_16,s->calendar_date,t->text);
    desk_clip(c,clip);
}
/* Only the opaque client body of a higher window proves coverage. */
static int covered_by_body(const struct desktop_scene *s,const int *order,int n,struct desk_rect area,struct desk_rect clip) {
    area=desk_intersect(area,clip);
    if(area.w<=0 || area.h<=0)return 0;
    for(int higher=desktop_capacity(s)-1;higher>n;higher--) {
        const struct desktop_window *cover=desktop_window_at(s,order[higher]);
        struct desk_rect r=cover->frame;
        if(!cover->visible || cover->minimized || r.w<128 || r.h<45)continue;
        struct desk_rect opaque={r.x+1,r.y+44,r.w-2,r.h-45};
        if(area.x>=opaque.x && area.y>=opaque.y &&
           (int64_t)area.x+area.w<=(int64_t)opaque.x+opaque.w &&
           (int64_t)area.y+area.h<=(int64_t)opaque.y+opaque.h)return 1;
    }
    return 0;
}
static int content_covered(const struct desktop_scene *s,const int *order,int n,struct desk_rect clip) {
    const struct desktop_window *w=desktop_window_at(s,order[n]);
    if(!w->visible || w->minimized || !w->pixels || w->frame.w<128 || w->frame.h<45)return 0;
    struct desk_rect content={w->frame.x+1,w->frame.y+44,w->width,w->height};
    content=desk_intersect(content,(struct desk_rect){w->frame.x+1,w->frame.y+44,w->frame.w-2,w->frame.h-45});
    return covered_by_body(s,order,n,content,clip);
}
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s) {
    int inline_order[DESKD_WINDOWS];int *order=s->order?s->order:inline_order;
    desktop_stack(s,order);desktop_draw_ordered(c,s,order,1);
}
void desktop_draw_ordered(struct desk_canvas *c,const struct desktop_scene *s,
    const int *order,int occlusion) {
    const struct desk_theme *t=scene_theme(s);
    static const uint32_t tops[]={0x587fa4,0xc8baa6,0x697887};
    static const uint32_t bottoms[]={0xa4bfd3,0xeee7db,0xb5c1cc};
    unsigned wallpaper=s->wallpaper>=0 && s->wallpaper<3?(unsigned)s->wallpaper:0;
    desk_theme_gradient(c,(struct desk_rect){0,0,c->width,c->height},tops[wallpaper],bottoms[wallpaper]);
    desk_theme_glass(c,(struct desk_rect){0,0,c->width,44},t->chrome_top,t->chrome_bottom,t->glass_alpha,0);
    desk_fill(c,(struct desk_rect){0,43,c->width,1},t->chrome_border);
    if(s->apps) {
        desk_menu_bar_draw(c,&s->menu,t);
        if(s->calendar_open) desk_fill(c,desktop_clock_rect(c->width,s->clock),t->selected);
        struct desk_rect alerts=desktop_alert_rect(c->width,s->clock);char label[24];unsigned unread=desktop_notifications_unread(&s->notifications.store);
        if(s->notifications.opened || unread) desk_fill(c,alerts,t->selected);
        if(unread) snprintf(label,sizeof(label),"Alerts %u",unread);else strcpy(label,"Alerts");
        desk_label_draw(c,(struct desk_rect){alerts.x+6,12,alerts.w-12,24},&desk_font_16,label,t->text);
        if(s->battery[0]) desk_label_draw(c,(struct desk_rect){alerts.x-144,12,140,24},&desk_font_16,s->battery,t->muted);
        int clock_width=desk_text_width(&desk_font_16,s->clock);
        desk_label_draw(c,(struct desk_rect){c->width-clock_width-16,12,clock_width,24},&desk_font_16,s->clock,0x596572);
    } else desk_text(c,&desk_font_16,24,12,s->trackpad_trial?"SrvrOS   /   Trackpad trial":"SrvrOS   /   Desktop",0x252a30);
    if(!s->apps && s->battery[0])desk_label_draw(c,(struct desk_rect){c->width-180,12,164,24},&desk_font_16,s->battery,t->muted);
    if(!s->apps) desk_text(c,&desk_font_16,24,c->height-30,s->notice[0]?s->notice:s->trackpad_trial==3?s->io_status:
        s->trackpad_trial>=2?"Two fingers: scroll   Move / press-drag   Type to test   Esc: save checks":
        s->trackpad_trial?"One finger: move / press-drag   Type to test   Esc: save checks":
        "Tab: switch window   Ctrl-N: open study   Esc: console",0x354352);
    if (s->trackpad_trial>=2 && c->height>=700) {
        int y=c->height-269;
        desk_fill(c,(struct desk_rect){24,y,c->width-48,218},0xe8ebef);
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
        const char *tap_steps[]={
            "Tap-to-click and typing suppression - both enabled",
            "1. Lightly tap Text size with one finger; it should activate once. Do not press.",
            "2. Hold A and brush the pad without clicking: the pointer should stay still.",
            "   Release A AND lift fully, then touch again: pointer movement should resume.",
            "3. Repeat taps after typing. Scroll and physically click/drag: no extra click.",
            "4. Optional: short two-finger taps. Long holds and scrolling must not tap.",
            "5. Escape opens live tap/filter counters. Follow checks, clear, type AaZz09.",
            "   Enter saves. Wait for USB unmount; the test halts for manual restart."
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
            desk_text(c,&desk_font_16,40,y+12+24*i,s->trackpad_trial==4?tap_steps[i]:s->trackpad_trial==3?io_steps[i]:steps[i],i?0x354352:0x252a30);
    }
    for(int n=0;n<desktop_capacity(s);n++) {
        const struct desktop_window *w=desktop_window_at(s,order[n]);
        struct desk_rect r=w->frame;
        int bounded=w->visible && !w->minimized && r.w>=128 && r.h>=45;
        if(occlusion && bounded && covered_by_body(s,order,n,(struct desk_rect){r.x,r.y,r.w+5,r.h+7},c->clip))continue;
        int skip_body=occlusion && bounded && covered_by_body(s,order,n,(struct desk_rect){r.x+1,r.y+44,r.w-2,r.h-45},c->clip);
        int skip_content=occlusion && !skip_body && content_covered(s,order,n,c->clip);
        draw_window(c,w,order[n]==s->focus,s->apps,t,skip_body,skip_content);
    }
    if(s->apps) {
        dock(c,s);
        if(s->notice[0]) {
            if(s->automatic_test) {
                struct desk_rect banner={16,c->height-112,c->width-32,36};
                desk_fill(c,banner,t->border);
                desk_fill(c,desk_widget_inset(banner,1),t->surface);
            }
            desk_text(c,&desk_font_16,24,c->height-98,s->notice,0x2674b8);
        }
        desk_menu_popup_draw(c,&s->menu,t);
        if(s->calendar_open) calendar_panel(c,s);
        if(s->launcher_open) desk_palette_draw(c,&s->launcher,t);
        if(s->notifications.opened || (!s->launcher_open && !s->calendar_open && s->menu.open<0 && !desk_popup_active(&s->context))) desktop_notice_view_draw(c,&s->notifications,t);
    }
    if(s->apps) desk_popup_draw(c,&s->context,t);
    for (int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+y,1+y/2,1},0x252a30);
        if (y>1 && y<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+y,y/2,1},0xf0f1f3);
    }
}
