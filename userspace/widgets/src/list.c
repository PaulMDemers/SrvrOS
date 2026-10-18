#include <srvros/desk_list.h>
#include <string.h>
#define ROW 28
void desk_list_init(struct desk_list *l) {
    *l=(struct desk_list){.selected=-1,.pressed=-1,.last_click=-1};
}
void desk_list_configure(struct desk_list *l,struct desk_rect frame,int enabled) {
    if(memcmp(&frame,&l->frame,sizeof(frame)) || !enabled) {
        l->pressed=-1;l->scrollbar_capture=0;l->last_click=-1;l->last_click_us=0;
    }
    l->frame=frame;l->enabled=enabled!=0;
    if(!enabled) l->focused=0;
    struct desk_rect inner=desk_widget_inset(frame,1);
    struct desk_rect track=inner.w>=18?(struct desk_rect){inner.x+inner.w-18,inner.y,18,inner.h}:(struct desk_rect){0};
    desk_scrollbar_configure(&l->scroll,track,l->count*ROW,inner.h);
}
int desk_list_set_items(struct desk_list *l,const struct desk_list_item *items,int count) {
    if(count<0 || count>DESK_LIST_MAX_ITEMS || (!items && count)) return -1;
    for(int i=0;i<count;i++) if(!items[i].label) return -1;
    l->items=items;l->count=count;l->selected=-1;l->pressed=-1;l->last_click=-1;
    l->last_click_us=0;l->scrollbar_capture=0;
    l->scroll.dragging=0;
    desk_list_configure(l,l->frame,l->enabled);desk_scrollbar_set(&l->scroll,0);
    return 0;
}
unsigned desk_list_select(struct desk_list *l,int index) {
    if(index<-1 || index>=l->count) return 0;
    unsigned result=l->selected!=index?DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW:0;
    l->selected=index;
    if(index>=0) {
        if(index*ROW<l->scroll.offset) result|=desk_scrollbar_set(&l->scroll,index*ROW)&DESK_WIDGET_REDRAW;
        if((index+1)*ROW>l->scroll.offset+l->scroll.viewport)
            result|=desk_scrollbar_set(&l->scroll,(index+1)*ROW-l->scroll.viewport)&DESK_WIDGET_REDRAW;
    }
    return result;
}
unsigned desk_list_scroll(struct desk_list *l,int64_t delta) {
    return desk_scrollbar_scroll(&l->scroll,delta)&DESK_WIDGET_REDRAW;
}
static int hit(const struct desk_list *l,int x,int y) {
    struct desk_rect r=desk_widget_inset(l->frame,1);
    if(!desk_widget_contains(r,x,y) || x>=l->scroll.track.x) return -1;
    int64_t index=((int64_t)y-r.y+l->scroll.offset)/ROW;
    return index>=0 && index<l->count?(int)index:-1;
}
unsigned desk_list_event(struct desk_list *l,struct desk_widget_event e,uint64_t time_us) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        l->pressed=-1;l->scrollbar_capture=0;l->last_click=-1;l->last_click_us=0;
        desk_scrollbar_event(&l->scroll,e);
        if(e.type==DESK_WIDGET_FOCUS_OUT && l->focused) { l->focused=0;return DESK_WIDGET_REDRAW; }
        return 0;
    }
    if(!l->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { l->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type==DESK_WIDGET_ACTIVATE) return l->focused && l->selected>=0?DESK_WIDGET_ACTIVATED:0;
    if(e.type!=DESK_WIDGET_POINTER) return 0;
    if(e.pressed) l->scrollbar_capture=desk_widget_contains(l->scroll.track,e.x,e.y);
    if(l->scrollbar_capture) {
        unsigned result=desk_scrollbar_event(&l->scroll,e)&DESK_WIDGET_REDRAW;
        if(!e.down) l->scrollbar_capture=0;
        l->pressed=-1;return result;
    }
    int index=hit(l,e.x,e.y);
    if(e.pressed) {
        l->pressed=e.down?index:-1;
        return e.down?desk_list_select(l,index):0;
    }
    if(!e.down) {
        int previous=l->pressed;l->pressed=-1;
        if(previous>=0 && index==previous) {
            if(time_us && l->last_click_us && time_us>=l->last_click_us &&
               time_us-l->last_click_us<=400000 && l->last_click==index) {
                l->last_click=-1;l->last_click_us=0;return DESK_WIDGET_ACTIVATED;
            }
            l->last_click=index;l->last_click_us=time_us;
        } else { l->last_click=-1;l->last_click_us=0; }
    }
    return 0;
}
unsigned desk_list_key(struct desk_list *l,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) return desk_list_event(l,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);
    if(!l->enabled || !l->focused || !(key->flags&SRV_KEY_DOWN_EVENT) || !l->count) return 0;
    int index=l->selected,step=l->scroll.viewport/ROW;
    if(step<1) step=1;
    switch(key->usage) {
    case SRV_KEY_ENTER: return index>=0?DESK_WIDGET_ACTIVATED:0;
    case SRV_KEY_UP: index=index<=0?0:index-1;break;
    case SRV_KEY_DOWN: index=index+1>=l->count?l->count-1:index+1;break;
    case SRV_KEY_PAGEUP: index=index-step<0?0:index-step;break;
    case SRV_KEY_PAGEDOWN: index=index+step>=l->count?l->count-1:index+step;break;
    case SRV_KEY_HOME: index=0;break;
    case SRV_KEY_END: index=l->count-1;break;
    default: {
        if(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) return 0;
        unsigned c=key->codepoint; if(c>='A' && c<='Z') c+=32;
        if(c<32 || c>126) return 0;
        for(int n=1;n<=l->count;n++) {
            int i=(index+n)%l->count; unsigned first=(unsigned char)l->items[i].label[0];
            if(first>='A' && first<='Z') first+=32;
            if(first==c) return desk_list_select(l,i);
        }
        return 0;
    }
    }
    return desk_list_select(l,index);
}
void desk_list_draw(struct desk_canvas *c,const struct desk_list *l,const struct desk_theme *theme) {
    desk_fill(c,l->frame,l->focused?theme->accent:theme->border);
    struct desk_rect inner=desk_widget_inset(l->frame,1),clip=c->clip;
    desk_fill(c,inner,theme->surface);
    if(inner.w<=18 || !inner.h) return;
    struct desk_rect rows=inner;rows.w-=18;
    desk_clip(c,desk_intersect(clip,rows));
    if(!l->count) desk_label_draw(c,desk_widget_inset(rows,12),theme->body,"No items",theme->muted);
    for(int i=l->scroll.offset/ROW;i<l->count;i++) {
        int y=inner.y+i*ROW-l->scroll.offset;if(y>=inner.y+inner.h) break;
        if(i==l->selected) desk_fill(c,(struct desk_rect){inner.x,y,rows.w,ROW},theme->selected);
        if(l->items[i].folder) {
            desk_fill(c,(struct desk_rect){inner.x+8,y+7,8,4},theme->thumb);
            desk_fill(c,(struct desk_rect){inner.x+8,y+10,16,12},theme->thumb);
        } else {
            desk_fill(c,(struct desk_rect){inner.x+10,y+6,12,17},theme->border);
            desk_fill(c,(struct desk_rect){inner.x+11,y+7,10,15},theme->surface);
        }
        int detail=rows.w>380?130:0;
        desk_label_draw(c,(struct desk_rect){inner.x+32,y+4,rows.w-40-detail,ROW-4},theme->body,l->items[i].label,l->enabled?theme->text:theme->muted);
        if(detail && l->items[i].detail) desk_label_draw(c,(struct desk_rect){inner.x+rows.w-detail,y+4,detail-8,ROW-4},theme->body,l->items[i].detail,theme->muted);
    }
    c->clip=clip;desk_scrollbar_draw(c,&l->scroll,theme);
}
