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
    desk_text(c,&desk_font_16,24,12,"SrvrOS   /   Desktop",0x292e2b);
    desk_text(c,&desk_font_16,24,c->height-30,"Tab: switch window   Ctrl-N: open study   Esc: console",0x465348);
    for (int i=0;i<DESKD_WINDOWS;i++) if (i!=s->focus) draw_window(c,&s->windows[i],0);
    if (s->focus>=0 && s->focus<DESKD_WINDOWS) draw_window(c,&s->windows[s->focus],1);
    for (int y=0;y<16;y++) {
        desk_fill(c,(struct desk_rect){s->cursor_x,s->cursor_y+y,1+y/2,1},0x292e2b);
        if (y>1 && y<13) desk_fill(c,(struct desk_rect){s->cursor_x+1,s->cursor_y+y,y/2,1},0xfaf9f6);
    }
}
