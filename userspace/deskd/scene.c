#include "scene.h"
#include <srvros/desktop_protocol.h>

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
    if(application<1 || application>DESK_APP_COUNT) return (struct desk_rect){0};
    int total=DESK_APP_COUNT*100;
    return (struct desk_rect){(width-total)/2+(application-1)*100,height-68,100,60};
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
    static const char *names[]={"Files","Text","Calculator","System","Settings","Terminal"};
    int width=DESK_APP_COUNT*100;
    desk_fill(c,(struct desk_rect){(c->width-width)/2-8,c->height-74,width+16,70},0xf1f0eb);
    for(int app=1;app<=DESK_APP_COUNT;app++) {
        struct desk_rect r=desktop_dock_item(c->width,c->height,app);
        int running=0,active=0;
        for(int i=0;i<DESKD_WINDOWS;i++) if(s->windows[i].visible && s->windows[i].application==app) {
            running=1;active|=s->focus==i;
        }
        if(active) desk_fill(c,(struct desk_rect){r.x+2,r.y+1,r.w-4,r.h-3},0xe0eade);
        icon(c,app,r.x+34,r.y+2);
        int text=desk_text_width(&desk_font_16,names[app-1]);
        desk_text(c,&desk_font_16,r.x+(r.w-text)/2,r.y+34,names[app-1],0x292e2b);
        if(running) desk_fill(c,(struct desk_rect){r.x+48,r.y+56,4,2},0x38644f);
    }
}

static void draw_window(struct desk_canvas *c,const struct desktop_window *w,int focused) {
    if (!w->visible || w->minimized) return;
    struct desk_rect r=w->frame;
    desk_fill(c,(struct desk_rect){r.x+5,r.y+7,r.w,r.h},0xb9c2b4);
    desk_fill(c,r,focused?0x748b79:0xb2baaf);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,r.h-2},0xfaf9f6);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,42},focused?0xe7ebe2:0xf1f0eb);
    struct desk_rect clip=c->clip;
    desk_clip(c,desk_intersect(clip,(struct desk_rect){r.x+16,r.y+1,r.w-(w->application?126:62),41}));
    desk_text(c,&desk_font_16,r.x+18,r.y+12,w->title,0x292e2b);
    c->clip=clip;
    desk_text(c,&desk_font_16,r.x+r.w-30,r.y+12,"×",0x646b65);
    if(w->application) {
        desk_fill(c,(struct desk_rect){r.x+r.w-91,r.y+24,12,1},0x646b65);
        desk_fill(c,(struct desk_rect){r.x+r.w-59,r.y+16,12,10},0x646b65);
        desk_fill(c,(struct desk_rect){r.x+r.w-58,r.y+18,10,7},focused?0xe7ebe2:0xf1f0eb);
        if(w->maximized) desk_fill(c,(struct desk_rect){r.x+r.w-56,r.y+13,12,1},0x646b65);
    }
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
void desktop_draw(struct desk_canvas *c,const struct desktop_scene *s) {
    desk_fill(c,(struct desk_rect){0,0,c->width,c->height},0xd6dcd2);
    desk_fill(c,(struct desk_rect){0,0,c->width,44},0xf1f0eb);
    desk_text(c,&desk_font_16,24,12,s->apps?"SrvrOS":s->trackpad_trial?"SrvrOS   /   Trackpad trial":"SrvrOS   /   Desktop",0x292e2b);
    if(s->apps) {
        for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){93+i,20+i,7-i*2,1},0x646b65);
        struct desk_rect clip=c->clip;
        desk_clip(c,desk_intersect(clip,(struct desk_rect){160,0,c->width-380,44}));
        if(s->focus>=0) desk_text(c,&desk_font_16,160,12,s->windows[s->focus].title,0x292e2b);
        c->clip=clip;
        desk_text(c,&desk_font_16,c->width-198,12,"Alt-Tab: switch window",0x646b65);
    }
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
    for(int n=0;n<DESKD_WINDOWS;n++) draw_window(c,&s->windows[order[n]],order[n]==s->focus);
    if(s->apps) {
        dock(c,s);
        if(s->notice[0]) desk_text(c,&desk_font_16,24,c->height-98,s->notice,0x38644f);
        if(s->menu) {
            desk_fill(c,(struct desk_rect){12,44,240,122},0x748b79);
            desk_fill(c,(struct desk_rect){13,45,238,120},0xfaf9f6);
            desk_text(c,&desk_font_16,28,58,"About SrvrOS",0x292e2b);
            desk_text(c,&desk_font_16,28,94,"New Text document",0x292e2b);
            desk_fill(c,(struct desk_rect){24,125,216,1},0xd9ddd5);
            desk_text(c,&desk_font_16,28,136,"Return to console",0x292e2b);
        }
    }
    for (int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+y,1+y/2,1},0x292e2b);
        if (y>1 && y<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+y,y/2,1},0xfaf9f6);
    }
}
