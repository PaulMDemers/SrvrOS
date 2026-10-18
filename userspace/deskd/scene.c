#include "scene.h"

static void draw_window(struct desk_canvas *c,const struct desktop_window *w,int focused) {
    if (!w->visible) return;
    struct desk_rect r=w->frame;
    desk_fill(c,(struct desk_rect){r.x+5,r.y+7,r.w,r.h},0xb9c2b4);
    desk_fill(c,r,focused?0x748b79:0xb2baaf);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,r.h-2},0xfaf9f6);
    desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w-2,42},focused?0xe7ebe2:0xf1f0eb);
    struct desk_rect clip=c->clip;
    desk_clip(c,desk_intersect(clip,(struct desk_rect){r.x+16,r.y+1,r.w-62,41}));
    desk_text(c,&desk_font_16,r.x+18,r.y+12,w->title,0x292e2b);
    c->clip=clip;
    desk_text(c,&desk_font_16,r.x+r.w-30,r.y+12,"×",0x646b65);
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
    desk_text(c,&desk_font_16,24,12,s->trackpad_trial?"SrvrOS   /   Trackpad trial":"SrvrOS   /   Desktop",0x292e2b);
    desk_text(c,&desk_font_16,24,c->height-30,s->trackpad_trial==3?s->io_status:
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
            "1. Type, scroll with two fingers, drag the thumb and resize the window.",
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
    for (int i=0;i<DESKD_WINDOWS;i++) if (i!=s->focus) draw_window(c,&s->windows[i],0);
    if (s->focus>=0 && s->focus<DESKD_WINDOWS) draw_window(c,&s->windows[s->focus],1);
    for (int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+y,1+y/2,1},0x292e2b);
        if (y>1 && y<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+y,y/2,1},0xfaf9f6);
    }
}
