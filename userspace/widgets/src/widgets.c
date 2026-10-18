#include <srvros/desk_widgets.h>
#include <limits.h>
#include <string.h>

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

void desk_label_ellipsis(struct desk_canvas *c,struct desk_rect r,
                         const struct desk_font *font,const char *text,uint32_t color) {
    if(!valid(r) || !font || !text) return;
    const char *p=text,*end=text;int64_t width=0,prefix_width=0;
    int marker=desk_text_width(font,"…"),truncated=0;
    while(*p) {
        const char *start=p;uint32_t cp=desk_utf8_next(&p);
        if(cp=='\n') { truncated=1;break; }
        char character[5]={0};size_t bytes=p-start;if(bytes>4) bytes=4;memcpy(character,start,bytes);
        width+=desk_text_width(font,character);
        if(width>r.w) { truncated=1;break; }
        if(width+marker<=r.w) { end=p;prefix_width=width; }
    }
    if(!truncated) { desk_label_draw(c,r,font,text,color);return; }
    if(marker>r.w) return;
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,r));
    p=text;int64_t x=r.x;
    while(p<end) {
        const char *start=p;desk_utf8_next(&p);char character[5]={0};size_t bytes=p-start;if(bytes>4) bytes=4;memcpy(character,start,bytes);
        desk_text(c,font,(int32_t)x,r.y,character,color);x+=desk_text_width(font,character);
    }
    desk_text(c,font,r.x+(int32_t)prefix_width,r.y,"…",color);c->clip=old;
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
static void configure_scrollbar(struct desk_scrollbar *b,struct desk_rect track,int32_t content,int32_t viewport,int horizontal) {
    if (!valid(track)) track=(struct desk_rect){0};
    if (content<0) content=0;
    if (viewport<0) viewport=0;
    if (!same(track,b->track) || content!=b->content || viewport!=b->viewport || b->horizontal!=horizontal)
        b->dragging=0;
    b->track=track; b->content=content; b->viewport=viewport;b->horizontal=horizontal;
    desk_scrollbar_set(b,b->offset);
}
void desk_scrollbar_configure(struct desk_scrollbar *b,struct desk_rect track,int32_t content,int32_t viewport) {
    configure_scrollbar(b,track,content,viewport,0);
}
void desk_scrollbar_configure_horizontal(struct desk_scrollbar *b,struct desk_rect track,int32_t content,int32_t viewport) {
    configure_scrollbar(b,track,content,viewport,1);
}
struct desk_scrollbar_geometry desk_scrollbar_geometry(const struct desk_scrollbar *b) {
    struct desk_scrollbar_geometry g={.limit=desk_scrollbar_limit(b)};
    if (!valid(b->track)) return g;
    int32_t extent=b->horizontal?b->track.w:b->track.h,span=extent;
    if (g.limit) {
        span=(int32_t)((int64_t)extent*b->viewport/b->content);
        if(span<16) span=16;
        if(span>extent) span=extent;
    }
    g.travel=extent-span;
    int32_t offset=g.limit?(int32_t)((int64_t)b->offset*g.travel/g.limit):0;
    int32_t cross=b->horizontal?b->track.h:b->track.w,inset=cross<8?0:4;
    g.thumb=b->horizontal?(struct desk_rect){b->track.x+offset,b->track.y+inset,span,b->track.h-2*inset}:
        (struct desk_rect){b->track.x+inset,b->track.y+offset,b->track.w-2*inset,span};
    return g;
}
unsigned desk_scrollbar_event(struct desk_scrollbar *b, struct desk_widget_event e) {
    if (e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        b->dragging=0; return 0;
    }
    if (e.type!=DESK_WIDGET_POINTER) return 0;
    struct desk_scrollbar_geometry g=desk_scrollbar_geometry(b);
    if (!g.limit || !valid(b->track)) { b->dragging=0; return 0; }
    int32_t point=b->horizontal?e.x:e.y,thumb_start=b->horizontal?g.thumb.x:g.thumb.y;
    int32_t thumb_span=b->horizontal?g.thumb.w:g.thumb.h,track_start=b->horizontal?b->track.x:b->track.y;
    if (e.pressed) {
        b->dragging=0;
        if (!e.down || !desk_widget_contains(b->track,e.x,e.y)) return 0;
        if(point>=thumb_start && (int64_t)point<(int64_t)thumb_start+thumb_span) {
            b->dragging=1; b->grab=point-thumb_start;
            return 0;
        }
        return desk_scrollbar_scroll(b,point<thumb_start?-(int64_t)b->viewport:b->viewport);
    }
    unsigned result=0;
    if (b->dragging && g.travel>0) {
        int64_t top=(int64_t)point-b->grab-track_start;
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
void desk_button_centered(struct desk_button *b,int centered) { b->centered=centered!=0; }
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
    if(b->centered && text.w>0) {
        int width=desk_text_width(theme->body,label);
        if(width<text.w) { int shift=(text.w-width)/2;text.x+=shift;text.w-=shift; }
    }
    desk_label_draw(c,text,theme->body,label,b->enabled?theme->text:theme->muted);
}

void desk_checkmark_draw(struct desk_canvas *c,int32_t x,int32_t y,uint32_t color) {
    for(int i=0;i<4;i++) desk_fill(c,(struct desk_rect){x+i,y+3+i,2,2},color);
    for(int i=0;i<7;i++) desk_fill(c,(struct desk_rect){x+3+i,y+6-i,2,2},color);
}
