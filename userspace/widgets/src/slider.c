#include <srvros/desk_slider.h>
#include <string.h>
void desk_slider_init(struct desk_slider *s) { *s=(struct desk_slider){.maximum=100,.step=1}; }
static int32_t snapped(const struct desk_slider *s,int64_t value) {
    if(value<=s->minimum) return s->minimum;
    if(value>=s->maximum) return s->maximum;
    uint64_t units=(uint64_t)(value-s->minimum),step=s->step?s->step:1;
    int64_t result=(int64_t)((units+step/2)/step*step)+s->minimum;
    return result>s->maximum?s->maximum:(int32_t)result;
}
unsigned desk_slider_set(struct desk_slider *s,int64_t value) {
    int32_t next=snapped(s,value);if(next==s->value) return 0;
    s->value=next;return DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW;
}
int desk_slider_range(struct desk_slider *s,int32_t minimum,int32_t maximum,uint32_t step) {
    if(maximum<minimum || !step) return -1;
    s->minimum=minimum;s->maximum=maximum;s->step=step;s->dragging=0;
    s->value=snapped(s,s->value);if(minimum==maximum) s->enabled=s->focused=0;return 0;
}
void desk_slider_configure(struct desk_slider *s,struct desk_rect r,int enabled) {
    if(r.x<-16384 || r.y<-16384 || r.x>16384 || r.y>16384 || r.w<0 || r.h<0 || r.w>16384 || r.h>16384) return;
    if(memcmp(&s->frame,&r,sizeof(r)) || !enabled) s->dragging=0;
    s->frame=r;s->enabled=enabled && r.w>=24 && r.h>=20 && s->maximum>s->minimum && s->step;
    if(!s->enabled) s->focused=s->dragging=0;
}
struct desk_rect desk_slider_thumb(const struct desk_slider *s) {
    struct desk_rect r=s->frame;if(r.w<24 || r.h<20) return (struct desk_rect){0};
    uint64_t range=(uint64_t)((int64_t)s->maximum-s->minimum);
    int travel=r.w-18;
    int offset=range?(int)(((uint64_t)((int64_t)s->value-s->minimum)*travel+range/2)/range):0;
    return (struct desk_rect){r.x+2+offset,r.y+(r.h-18)/2,14,18};
}
static unsigned pointer_value(struct desk_slider *s,int32_t x) {
    int travel=s->frame.w-18;if(travel<=0) return 0;
    int64_t position=(int64_t)x-s->grab-s->frame.x-2;
    if(position<0) position=0;
    if(position>travel) position=travel;
    uint64_t range=(uint64_t)((int64_t)s->maximum-s->minimum);
    return desk_slider_set(s,(int64_t)((position*range+travel/2)/travel)+s->minimum);
}
unsigned desk_slider_event(struct desk_slider *s,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        int changed=s->dragging || (e.type==DESK_WIDGET_FOCUS_OUT && s->focused);
        s->dragging=0;if(e.type==DESK_WIDGET_FOCUS_OUT) s->focused=0;return changed?DESK_WIDGET_REDRAW:0;
    }
    if(!s->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { s->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type!=DESK_WIDGET_POINTER) return 0;
    if(e.pressed) {
        s->dragging=e.down && desk_widget_contains(s->frame,e.x,e.y);if(!s->dragging) return 0;
        struct desk_rect thumb=desk_slider_thumb(s);int inside=desk_widget_contains(thumb,e.x,e.y);
        s->grab=inside?e.x-thumb.x:thumb.w/2;
        return (inside?0:pointer_value(s,e.x))|DESK_WIDGET_REDRAW;
    }
    if(!s->dragging) return 0;
    unsigned result=pointer_value(s,e.x);if(!e.down) { s->dragging=0;result|=DESK_WIDGET_ACTIVATED|DESK_WIDGET_REDRAW; }return result;
}
unsigned desk_slider_key(struct desk_slider *s,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) return desk_slider_event(s,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!s->enabled || !s->focused || !(key->flags&SRV_KEY_DOWN_EVENT) || (key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) return 0;
    int64_t step=s->step;if(key->modifiers&SRV_KEY_SHIFT) step*=10;
    int64_t next=s->value;
    switch(key->usage) {
    case SRV_KEY_LEFT: case SRV_KEY_DOWN: next-=step;break;
    case SRV_KEY_RIGHT: case SRV_KEY_UP: next+=step;break;
    case SRV_KEY_PAGEDOWN: next-=(int64_t)s->step*10;break;
    case SRV_KEY_PAGEUP: next+=(int64_t)s->step*10;break;
    case SRV_KEY_HOME: next=s->minimum;break;
    case SRV_KEY_END: next=s->maximum;break;
    default: return 0;
    }
    unsigned redraw=s->dragging?DESK_WIDGET_REDRAW:0;s->dragging=0;return desk_slider_set(s,next)|redraw;
}
void desk_slider_draw(struct desk_canvas *c,const struct desk_slider *s,const struct desk_theme *t) {
    struct desk_rect r=s->frame,thumb=desk_slider_thumb(s),old=c->clip;if(!thumb.w) return;
    desk_clip(c,desk_intersect(old,r));
    int x=r.x+9,y=r.y+r.h/2;
    desk_fill(c,(struct desk_rect){x,y-2,r.w-18,4},t->track);
    desk_fill(c,(struct desk_rect){x,y-2,thumb.x+7-x,4},s->enabled?t->accent:t->muted);
    desk_fill(c,thumb,s->focused?t->accent:t->border);
    desk_fill(c,desk_widget_inset(thumb,2),s->dragging?t->selected:t->surface);
    if(s->focused) {
        desk_fill(c,(struct desk_rect){r.x,r.y,r.w,1},t->accent);desk_fill(c,(struct desk_rect){r.x,r.y+r.h-1,r.w,1},t->accent);
        desk_fill(c,(struct desk_rect){r.x,r.y,1,r.h},t->accent);desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y,1,r.h},t->accent);
    }
    c->clip=old;
}
