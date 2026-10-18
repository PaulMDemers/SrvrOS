#include <srvros/desk_widgets.h>

void desk_check_configure(struct desk_check *c,struct desk_rect frame,int enabled,int checked,int radio) {
    if(c->checked!=(checked!=0) || c->radio!=(radio!=0))
        desk_button_event(&c->button,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    desk_button_configure(&c->button,frame,enabled);
    c->checked=checked!=0;c->radio=radio!=0;
}
unsigned desk_check_event(struct desk_check *c,struct desk_widget_event event) {
    unsigned flags=desk_button_event(&c->button,event);
    if(flags&DESK_WIDGET_ACTIVATED) {
        int next=c->radio?1:!c->checked;
        if(next!=c->checked) { c->checked=next;flags|=DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW; }
    }
    return flags;
}
static void circle(struct desk_canvas *c,int x,int y,int radius,uint32_t color) {
    for(int dy=-radius;dy<=radius;dy++) for(int dx=-radius;dx<=radius;dx++)
        if(dx*dx+dy*dy<=radius*radius) desk_fill(c,(struct desk_rect){x+dx,y+dy,1,1},color);
}
void desk_check_draw(struct desk_canvas *c,const struct desk_check *check,const char *label,const struct desk_theme *t) {
    const struct desk_button *b=&check->button;struct desk_rect r=b->frame,old=c->clip;
    if(r.w<30 || r.h<20) return;
    desk_clip(c,desk_intersect(old,r));
    uint32_t ink=b->enabled?t->text:t->muted;
    uint32_t edge=b->focused?t->accent:t->border;
    int x=r.x+3,y=r.y+(r.h-18)/2;
    if(b->focused) {
        desk_fill(c,(struct desk_rect){r.x,r.y,r.w,1},t->accent);
        desk_fill(c,(struct desk_rect){r.x,r.y+r.h-1,r.w,1},t->accent);
        desk_fill(c,(struct desk_rect){r.x,r.y,1,r.h},t->accent);
        desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y,1,r.h},t->accent);
    }
    uint32_t fill=b->armed && b->inside?t->selected:t->surface;
    if(check->radio) {
        circle(c,x+8,y+8,8,edge);circle(c,x+8,y+8,6,fill);
        if(check->checked) circle(c,x+8,y+8,4,b->enabled?t->accent:t->muted);
    } else {
        desk_fill(c,(struct desk_rect){x,y,17,17},edge);
        desk_fill(c,(struct desk_rect){x+1,y+1,15,15},fill);
        if(check->checked) {
            uint32_t color=b->enabled?t->accent:t->muted;
            desk_checkmark_draw(c,x+3,y+5,color);
        }
    }
    if(r.w>30 && r.h>=t->body->line_height) desk_label_draw(c,(struct desk_rect){r.x+28,r.y+(r.h-t->body->line_height)/2,r.w-30,t->body->line_height},t->body,label,ink);
    c->clip=old;
}
