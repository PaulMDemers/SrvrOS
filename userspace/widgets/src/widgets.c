#include <srvros/desk_widgets.h>
#include <limits.h>

const struct desk_theme desk_theme_quiet = {
    .surface=0xfaf9f6, .text=0x292e2b, .muted=0x646b65, .accent=0x38644f,
    .selected=0xe0eade, .track=0xe7ebe2, .thumb=0x748b79, .border=0x748b79,
    .body=&desk_font_16, .heading=&desk_font_20,
    .spacing=4, .padding=8, .control_height=28
};

static int valid(struct desk_rect r) {
    return r.w>0 && r.h>0 && (int64_t)r.x+r.w<=INT32_MAX &&
        (int64_t)r.y+r.h<=INT32_MAX;
}
static int same(struct desk_rect a, struct desk_rect b) {
    return a.x==b.x && a.y==b.y && a.w==b.w && a.h==b.h;
}
int desk_widget_contains(struct desk_rect r, int32_t x, int32_t y) {
    return valid(r) && x>=r.x && y>=r.y &&
        (int64_t)x<(int64_t)r.x+r.w && (int64_t)y<(int64_t)r.y+r.h;
}
struct desk_rect desk_widget_inset(struct desk_rect r, int32_t inset) {
    if (!valid(r) || inset<0 || (int64_t)inset*2>=r.w || (int64_t)inset*2>=r.h)
        return (struct desk_rect){0};
    return (struct desk_rect){r.x+inset,r.y+inset,r.w-2*inset,r.h-2*inset};
}
void desk_label_draw(struct desk_canvas *c, struct desk_rect r,
                     const struct desk_font *font, const char *text, uint32_t color) {
    if (!valid(r)) return;
    struct desk_rect clip=c->clip;
    desk_clip(c,desk_intersect(clip,r));
    desk_text(c,font,r.x,r.y,text,color);
    c->clip=clip;
}

int32_t desk_scrollbar_limit(const struct desk_scrollbar *b) {
    return b->content>b->viewport && b->viewport>0 ? b->content-b->viewport : 0;
}
unsigned desk_scrollbar_set(struct desk_scrollbar *b, int64_t offset) {
    int32_t limit=desk_scrollbar_limit(b);
    int32_t next=offset<0?0:offset>limit?limit:(int32_t)offset;
    if (b->offset==next) return 0;
    b->offset=next;
    return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
unsigned desk_scrollbar_scroll(struct desk_scrollbar *b, int64_t delta) {
    int32_t limit=desk_scrollbar_limit(b);
    if (delta>limit-b->offset) return desk_scrollbar_set(b,limit);
    if (delta<-(int64_t)b->offset) return desk_scrollbar_set(b,0);
    return desk_scrollbar_set(b,b->offset+delta);
}
void desk_scrollbar_configure(struct desk_scrollbar *b, struct desk_rect track,
                             int32_t content, int32_t viewport) {
    if (!valid(track)) track=(struct desk_rect){0};
    if (content<0) content=0;
    if (viewport<0) viewport=0;
    if (!same(track,b->track) || content!=b->content || viewport!=b->viewport)
        b->dragging=0;
    b->track=track; b->content=content; b->viewport=viewport;
    desk_scrollbar_set(b,b->offset);
}
struct desk_scrollbar_geometry desk_scrollbar_geometry(const struct desk_scrollbar *b) {
    struct desk_scrollbar_geometry g={.limit=desk_scrollbar_limit(b)};
    if (!valid(b->track)) return g;
    int32_t height=b->track.h;
    if (g.limit) {
        height=(int32_t)((int64_t)b->track.h*b->viewport/b->content);
        if (height<16) height=16;
        if (height>b->track.h) height=b->track.h;
    }
    g.travel=b->track.h-height;
    int32_t top=b->track.y+(g.limit?(int32_t)((int64_t)b->offset*g.travel/g.limit):0);
    int32_t inset=b->track.w<8?0:4;
    g.thumb=(struct desk_rect){b->track.x+inset,top,b->track.w-2*inset,height};
    return g;
}
unsigned desk_scrollbar_event(struct desk_scrollbar *b, struct desk_widget_event e) {
    if (e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        b->dragging=0; return 0;
    }
    if (e.type!=DESK_WIDGET_POINTER) return 0;
    struct desk_scrollbar_geometry g=desk_scrollbar_geometry(b);
    if (!g.limit || !valid(b->track)) { b->dragging=0; return 0; }
    if (e.pressed) {
        b->dragging=0;
        if (!e.down || !desk_widget_contains(b->track,e.x,e.y)) return 0;
        if (e.y>=g.thumb.y && (int64_t)e.y<(int64_t)g.thumb.y+g.thumb.h) {
            b->dragging=1; b->grab=e.y-g.thumb.y;
            return 0;
        }
        return desk_scrollbar_scroll(b,e.y<g.thumb.y?-(int64_t)b->viewport:b->viewport);
    }
    unsigned result=0;
    if (b->dragging && g.travel>0) {
        int64_t top=(int64_t)e.y-b->grab-b->track.y;
        if (top<0) top=0;
        if (top>g.travel) top=g.travel;
        result=desk_scrollbar_set(b,(top*g.limit+g.travel/2)/g.travel);
    }
    if (!e.down) b->dragging=0;
    return result;
}
void desk_scrollbar_draw(struct desk_canvas *c, const struct desk_scrollbar *b,
                        const struct desk_theme *theme) {
    desk_fill(c,b->track,theme->track);
    desk_fill(c,desk_scrollbar_geometry(b).thumb,theme->thumb);
}

void desk_button_configure(struct desk_button *b, struct desk_rect r, int enabled) {
    if (!valid(r)) r=(struct desk_rect){0};
    enabled=enabled && valid(r);
    if (!enabled || !same(r,b->frame)) { b->armed=0; b->inside=0; }
    if (!enabled) b->focused=0;
    b->frame=r; b->enabled=enabled;
}
unsigned desk_button_event(struct desk_button *b, struct desk_widget_event e) {
    int old_visual=b->armed && b->inside, old_focus=b->focused;
    unsigned result=0;
    if (e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        b->armed=0; b->inside=0;
        if (e.type==DESK_WIDGET_FOCUS_OUT) b->focused=0;
    } else if (b->enabled) {
        if (e.type==DESK_WIDGET_FOCUS_IN) b->focused=1;
        else if (e.type==DESK_WIDGET_ACTIVATE && b->focused && !b->armed)
            result=DESK_WIDGET_ACTIVATED;
        else if (e.type==DESK_WIDGET_POINTER) {
            b->inside=desk_widget_contains(b->frame,e.x,e.y);
            if (e.pressed) b->armed=e.down && b->inside;
            else if (!e.down) {
                if (b->armed && b->inside) result=DESK_WIDGET_ACTIVATED;
                b->armed=0;
            }
        }
    }
    if (old_visual!=(b->armed && b->inside) || old_focus!=b->focused)
        result|=DESK_WIDGET_REDRAW;
    return result;
}
void desk_button_draw(struct desk_canvas *c, const struct desk_button *b,
                      const char *label, const struct desk_theme *theme) {
    if (!valid(b->frame)) return;
    desk_fill(c,b->frame,b->focused?theme->accent:theme->border);
    desk_fill(c,desk_widget_inset(b->frame,b->focused?2:1),
              b->enabled && b->armed && b->inside?theme->selected:theme->surface);
    struct desk_rect text=desk_widget_inset(b->frame,4);
    if (text.h>theme->body->line_height) text.y+=(text.h-theme->body->line_height)/2;
    desk_label_draw(c,text,theme->body,label,b->enabled?theme->text:theme->muted);
}
