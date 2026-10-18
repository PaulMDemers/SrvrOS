#include <srvros/desk_tabs.h>
#include <srvros/desk_text.h>
#include <string.h>
void desk_tabs_init(struct desk_tabs *t) { *t=(struct desk_tabs){.selected=-1,.pressed=-1}; }
static void enabled(struct desk_tabs *t) {
    t->enabled=t->requested && t->selected>=0 && t->frame.w>=(int)t->count*24 && t->frame.h>=24;
    if(!t->enabled) { t->focused=0;t->pressed=-1; }
}
int desk_tabs_items(struct desk_tabs *t,const struct desk_tab *items,unsigned count) {
    if(count>DESK_TABS_LIMIT || (count && !items)) return -1;
    /* Snapshot first so callers may pass the current item array or a slice. */
    struct desk_tab next[DESK_TABS_LIMIT]={0};
    for(unsigned i=0;i<count;i++) {
        size_t n=0;while(n<DESK_TAB_LABEL && items[i].label[n]) n++;
        if(!items[i].id || !n || n==DESK_TAB_LABEL || !desk_text_utf8_valid(items[i].label,n)) return -1;
        for(size_t j=0;j<n;j++) if((unsigned char)items[i].label[j]<32 || items[i].label[j]==127) return -1;
        for(unsigned j=0;j<i;j++) if(items[j].id==items[i].id) return -1;
        next[i]=items[i];next[i].disabled=items[i].disabled!=0;
    }
    uint64_t selected=desk_tabs_selected(t);int chosen=-1;
    for(unsigned i=0;i<count;i++) if(!next[i].disabled && (chosen<0 || next[i].id==selected)) chosen=(int)i;
    memcpy(t->items,next,sizeof(next));t->count=count;t->selected=chosen;t->pressed=-1;t->inside=0;enabled(t);return 0;
}
void desk_tabs_configure(struct desk_tabs *t,struct desk_rect r,int active) {
    if(r.x<-16384 || r.y<-16384 || r.x>16384 || r.y>16384 || r.w<0 || r.h<0 || r.w>16384 || r.h>16384) return;
    if(memcmp(&t->frame,&r,sizeof(r)) || !active) t->pressed=-1;
    t->frame=r;t->requested=active!=0;enabled(t);
}
uint64_t desk_tabs_selected(const struct desk_tabs *t) { return t->selected>=0 && (unsigned)t->selected<t->count?t->items[t->selected].id:0; }
int desk_tabs_select(struct desk_tabs *t,uint64_t id) {
    for(unsigned i=0;i<t->count;i++) if(t->items[i].id==id && !t->items[i].disabled) {
        if(t->selected==(int)i) return 0;
        t->selected=(int)i;t->pressed=-1;enabled(t);return 1;
    }
    return -1;
}
struct desk_rect desk_tabs_item_rect(const struct desk_tabs *t,unsigned i) {
    if(i>=t->count || !t->count) return (struct desk_rect){0};
    int left=(int)((int64_t)t->frame.w*i/t->count),right=(int)((int64_t)t->frame.w*(i+1)/t->count);
    return (struct desk_rect){t->frame.x+left,t->frame.y,right-left,t->frame.h};
}
static int hit(const struct desk_tabs *t,int x,int y) {
    for(unsigned i=0;i<t->count;i++) if(!t->items[i].disabled && desk_widget_contains(desk_tabs_item_rect(t,i),x,y)) return (int)i;
    return -1;
}
static unsigned choose(struct desk_tabs *t,int i) {
    if(i<0) return 0;
    t->pressed=-1;t->inside=0;
    int changed=desk_tabs_select(t,t->items[i].id);
    return DESK_WIDGET_ACTIVATED|DESK_WIDGET_REDRAW|(changed>0?DESK_WIDGET_CHANGED:0);
}
unsigned desk_tabs_event(struct desk_tabs *t,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        int redraw=t->pressed>=0 || (e.type==DESK_WIDGET_FOCUS_OUT && t->focused);
        t->pressed=-1;t->inside=0;if(e.type==DESK_WIDGET_FOCUS_OUT) t->focused=0;
        return redraw?DESK_WIDGET_REDRAW:0;
    }
    if(!t->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { t->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type==DESK_WIDGET_ACTIVATE) return t->focused?choose(t,t->selected):0;
    if(e.type!=DESK_WIDGET_POINTER) return 0;
    int target=hit(t,e.x,e.y);
    if(e.pressed) { t->pressed=e.down?target:-1;t->inside=t->pressed>=0;return DESK_WIDGET_REDRAW; }
    if(t->pressed<0) return 0;
    int inside=t->pressed==target,changed=inside!=t->inside;t->inside=inside;
    if(e.down) return changed?DESK_WIDGET_REDRAW:0;
    int pressed=t->pressed;t->pressed=-1;t->inside=0;
    return DESK_WIDGET_REDRAW|(inside?choose(t,pressed):0);
}
unsigned desk_tabs_key(struct desk_tabs *t,const struct srv_key_event *k) {
    if(k->flags&SRV_KEY_CANCEL) return desk_tabs_event(t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!t->enabled || !t->focused || !(k->flags&SRV_KEY_DOWN_EVENT) || (k->modifiers&(SRV_KEY_ALT|SRV_KEY_CTRL|SRV_KEY_META))) return 0;
    int direction=0,index=t->selected;
    if(k->usage==SRV_KEY_LEFT) direction=-1;else if(k->usage==SRV_KEY_RIGHT) direction=1;
    else if(k->usage==SRV_KEY_HOME) { direction=1;index=-1; }
    else if(k->usage==SRV_KEY_END) { direction=-1;index=(int)t->count; }
    else if((k->usage==SRV_KEY_ENTER || k->usage==SRV_KEY_SPACE) && !(k->flags&SRV_KEY_REPEAT)) return choose(t,index);
    else return 0;
    t->pressed=-1;
    for(unsigned n=0;n<t->count;n++) {
        index=(index+direction+(int)t->count)%(int)t->count;
        if(!t->items[index].disabled) return choose(t,index);
    }
    return 0;
}
void desk_tabs_draw(struct desk_canvas *c,const struct desk_tabs *t,const struct desk_theme *theme) {
    if(t->frame.w<=0 || t->frame.h<=0) return;
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,t->frame));
    desk_fill(c,t->frame,theme->track);
    for(unsigned i=0;i<t->count;i++) {
        struct desk_rect r=desk_tabs_item_rect(t,i);int selected=(int)i==t->selected;
        uint32_t fill=selected || ((int)i==t->pressed && t->inside)?theme->selected:theme->surface;
        desk_fill(c,desk_widget_inset(r,1),fill);
        int active=t->enabled && !t->items[i].disabled;
        if(selected) desk_fill(c,(struct desk_rect){r.x+1,r.y+r.h-3,r.w>2?r.w-2:0,2},active?theme->accent:theme->muted);
        if(t->focused && selected) {
            desk_fill(c,(struct desk_rect){r.x+1,r.y+1,r.w>2?r.w-2:0,1},theme->accent);
            desk_fill(c,(struct desk_rect){r.x+1,r.y+1,1,r.h>2?r.h-2:0},theme->accent);
            desk_fill(c,(struct desk_rect){r.x+r.w-2,r.y+1,1,r.h>2?r.h-2:0},theme->accent);
        }
        int width=desk_text_width(theme->body,t->items[i].label),space=r.w-16;
        if(space>0 && r.h>=theme->body->line_height+4) {
            int x=r.x+8+(width<space?(space-width)/2:0);
            desk_label_ellipsis(c,(struct desk_rect){x,r.y+(r.h-theme->body->line_height)/2,r.x+r.w-8-x,theme->body->line_height},theme->body,t->items[i].label,active?theme->text:theme->muted);
        }
    }
    c->clip=old;
}
