#include <srvros/desk_popup.h>
#include <srvros/desk_text.h>
#include <string.h>
static int valid_text(const char *text,size_t capacity) {
    size_t n=0;while(n<capacity && text[n]) { unsigned c=(unsigned char)text[n];if(c<32 || c==127) return 0;n++; }
    return n<capacity && desk_text_utf8_valid(text,n);
}
static int valid_items(const struct desk_menu_item *items,unsigned count) {
    if(!items || !count || count>DESK_POPUP_ITEMS) return 0;
    for(unsigned i=0;i<count;i++) {
        const struct desk_menu_item *item=items+i;
        if(item->flags&~(DESK_MENU_SEPARATOR|DESK_MENU_DISABLED|DESK_MENU_CHECKED) ||
            !valid_text(item->label,sizeof(item->label)) || !valid_text(item->shortcut,sizeof(item->shortcut))) return 0;
        if(item->flags&DESK_MENU_SEPARATOR) {
            if(item->flags!=DESK_MENU_SEPARATOR || item->id || item->label[0] || item->shortcut[0]) return 0;
        } else {
            if(!item->id || !item->label[0]) return 0;
            for(unsigned j=0;j<i;j++) if(items[j].id==item->id) return 0;
        }
    }
    return 1;
}
void desk_popup_cancel(struct desk_popup *p) { p->opened=p->armed=p->scroll_capture=p->swallow_release=0;p->selected=-1;p->scroll.dragging=0; }
void desk_popup_init(struct desk_popup *p) { *p=(struct desk_popup){0};desk_popup_cancel(p); }
int desk_popup_active(const struct desk_popup *p) { return p->opened || p->swallow_release; }
static int enabled(const struct desk_popup *p,int index) { return index>=0 && index<(int)p->count && !(p->items[index].flags&(DESK_MENU_SEPARATOR|DESK_MENU_DISABLED)); }
static int row_height(const struct desk_popup *p,unsigned index) { return p->items[index].flags&DESK_MENU_SEPARATOR?9:p->font->line_height+10; }
static int row_top(const struct desk_popup *p,unsigned index) { int y=0;for(unsigned i=0;i<index;i++) y+=row_height(p,i);return y; }
static int step(const struct desk_popup *p,int from,int direction) {
    int index=from;for(unsigned n=0;n<p->count;n++) { index+=direction;if(index<0) index=p->count-1;if(index>=(int)p->count) index=0;if(enabled(p,index)) return index; }return -1;
}
static void reveal(struct desk_popup *p) {
    if(p->selected<0) return;
    int top=row_top(p,p->selected),bottom=top+row_height(p,p->selected),offset=p->scroll.offset;
    if(bottom>offset+p->scroll.viewport) offset=bottom-p->scroll.viewport;
    if(top<offset) offset=top;
    desk_scrollbar_set(&p->scroll,offset);
}
int desk_popup_open(struct desk_popup *p,const struct desk_menu_item *items,unsigned count,
    int x,int y,struct desk_rect bounds,const struct desk_font *font,int keyboard) {
    if(!valid_items(items,count) || !font || !font->line_height || font->line_height>64 ||
        bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384 || bounds.w<16 || bounds.h<16 || bounds.w>16384 || bounds.h>16384) return -1;
    struct desk_popup next={.count=count,.font=font,.bounds=bounds,.opened=1,.selected=-1};
    memcpy(next.items,items,count*sizeof(*items));int label_width=0,shortcut_width=0,height=8;
    for(unsigned i=0;i<count;i++) {
        int label=desk_text_width(font,items[i].label),shortcut=desk_text_width(font,items[i].shortcut);
        if(label>label_width) label_width=label;
        if(shortcut>shortcut_width) shortcut_width=shortcut;
        height+=row_height(&next,i);
    }
    int width=label_width+shortcut_width+72;if(width<200) width=200;
    int content=height-8;if(width>bounds.w) width=bounds.w;if(height>bounds.h) height=bounds.h;
    if(x>bounds.x+bounds.w-width) x=bounds.x+bounds.w-width;
    if(x<bounds.x) x=bounds.x;
    if(y>bounds.y+bounds.h-height) y=bounds.y+bounds.h-height;
    if(y<bounds.y) y=bounds.y;
    next.frame=(struct desk_rect){x,y,width,height};
    desk_scrollbar_configure(&next.scroll,(struct desk_rect){x+width-14,y+4,10,height-8},content,height-8);
    if(keyboard) { next.selected=step(&next,-1,1);reveal(&next); }
    *p=next;return 0;
}
struct desk_rect desk_popup_row(const struct desk_popup *p,unsigned index) {
    if(!p->opened || index>=p->count) return (struct desk_rect){0};
    struct desk_rect r=p->frame;int scrollbar=desk_scrollbar_limit(&p->scroll)?14:0;
    return desk_intersect((struct desk_rect){r.x+4,r.y+4,r.w-8-scrollbar,r.h-8},
        (struct desk_rect){r.x+4,r.y+4+row_top(p,index)-p->scroll.offset,r.w-8-scrollbar,row_height(p,index)});
}
static void close_pressed(struct desk_popup *p,int down) { desk_popup_cancel(p);p->swallow_release=down!=0; }
struct desk_menu_result desk_popup_pointer(struct desk_popup *p,struct desk_widget_event e) {
    struct desk_menu_result result={.consumed=desk_popup_active(p)};
    if(!result.consumed) return result;
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) { desk_popup_cancel(p);result.redraw=1;return result; }
    if(e.type!=DESK_WIDGET_POINTER) return result;
    if(p->swallow_release) { if(!e.down) p->swallow_release=0;return result; }
    if(e.pressed && !desk_widget_contains(p->frame,e.x,e.y)) { close_pressed(p,e.down);result.redraw=1;return result; }
    if(p->scroll_capture || (e.pressed && desk_scrollbar_limit(&p->scroll) && desk_widget_contains(p->scroll.track,e.x,e.y))) {
        if(e.pressed) { p->scroll_capture=1;p->armed=0;p->selected=-1;result.redraw=1; }
        if(desk_scrollbar_event(&p->scroll,e)) result.redraw=1;
        if(!e.down) p->scroll_capture=0;
        return result;
    }
    int selected=-1;for(unsigned i=0;i<p->count;i++) if(enabled(p,i) && desk_widget_contains(desk_popup_row(p,i),e.x,e.y)) { selected=i;break; }
    if(selected!=p->selected) { p->selected=selected;result.redraw=1; }
    if(e.pressed) p->armed=1;
    else if(!e.down && p->armed) {
        if(selected>=0) { result.action=p->items[selected].id;desk_popup_cancel(p);result.redraw=1; }
        else p->armed=0;
    }
    return result;
}
struct desk_menu_result desk_popup_scroll(struct desk_popup *p,int64_t delta) {
    struct desk_menu_result result={.consumed=desk_popup_active(p)};
    if(p->opened && !p->armed && !p->scroll_capture && desk_scrollbar_scroll(&p->scroll,delta)) { p->selected=-1;result.redraw=1; }
    return result;
}
struct desk_menu_result desk_popup_key(struct desk_popup *p,const struct srv_key_event *key) {
    struct desk_menu_result result={.consumed=desk_popup_active(p)};
    if(!result.consumed) return result;
    if(key->flags&SRV_KEY_CANCEL) { desk_popup_cancel(p);result.redraw=1;return result; }
    if(!p->opened || !(key->flags&SRV_KEY_DOWN_EVENT)) return result;
    if(key->usage==SRV_KEY_ESCAPE || key->usage==SRV_KEY_TAB || key->usage==SRV_KEY_F10) { close_pressed(p,p->armed || p->scroll_capture);result.redraw=1;return result; }
    if(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) return result;
    int old=p->selected;
    if(key->usage==SRV_KEY_UP || key->usage==SRV_KEY_DOWN) p->selected=step(p,p->selected,key->usage==SRV_KEY_UP?-1:1);
    else if(key->usage==SRV_KEY_HOME) p->selected=step(p,-1,1);
    else if(key->usage==SRV_KEY_END) p->selected=step(p,p->count,-1);
    else if(key->usage==SRV_KEY_ENTER || key->usage==SRV_KEY_SPACE) {
        if(enabled(p,p->selected) && !(key->flags&SRV_KEY_REPEAT)) { result.action=p->items[p->selected].id;close_pressed(p,p->armed || p->scroll_capture);result.redraw=1; }
        return result;
    } else if(key->codepoint>=32 && key->codepoint<127) {
        int index=p->selected;unsigned query=key->codepoint;if(query>='A' && query<='Z') query+=32;
        for(unsigned i=0;i<p->count;i++) { index=step(p,index,1);if(index<0) break;unsigned first=(unsigned char)p->items[index].label[0];if(first>='A' && first<='Z') first+=32;if(first==query) { p->selected=index;break; } }
    }
    if(old!=p->selected) { p->armed=p->scroll_capture=0;p->scroll.dragging=0;reveal(p);result.redraw=1; }
    return result;
}
void desk_popup_draw(struct desk_canvas *c,const struct desk_popup *p,const struct desk_theme *t) {
    if(!p->opened) return;
    struct desk_rect old=c->clip,r=p->frame;desk_fill(c,(struct desk_rect){r.x+3,r.y+4,r.w,r.h},0xb9c2b4);desk_fill(c,r,t->border);desk_fill(c,desk_widget_inset(r,1),t->surface);
    struct desk_rect content={r.x+4,r.y+4,r.w-8-(desk_scrollbar_limit(&p->scroll)?14:0),r.h-8};desk_clip(c,desk_intersect(old,content));
    for(unsigned i=0;i<p->count;i++) {
        struct desk_rect row={content.x,r.y+4+row_top(p,i)-p->scroll.offset,content.w,row_height(p,i)};const struct desk_menu_item *item=p->items+i;
        if(item->flags&DESK_MENU_SEPARATOR) { desk_fill(c,(struct desk_rect){row.x+12,row.y+4,row.w>24?row.w-24:0,1},t->border);continue; }
        if((int)i==p->selected) desk_fill(c,row,t->selected);
        uint32_t color=item->flags&DESK_MENU_DISABLED?t->muted:t->text;int sw=desk_text_width(p->font,item->shortcut),available=row.w>48?row.w-48:0;
        int shortcut_limit=available>48?available-48:0;if(sw>shortcut_limit) sw=shortcut_limit;
        desk_label_ellipsis(c,(struct desk_rect){row.x+24,row.y+5,available-sw,p->font->line_height},p->font,item->label,color);
        desk_label_ellipsis(c,(struct desk_rect){row.x+row.w-sw-12,row.y+5,sw,p->font->line_height},p->font,item->shortcut,t->muted);
        if(item->flags&DESK_MENU_CHECKED) desk_text(c,p->font,row.x+5,row.y+5,"✓",color);
    }
    desk_clip(c,desk_intersect(old,r));if(desk_scrollbar_limit(&p->scroll)) desk_scrollbar_draw(c,&p->scroll,t);c->clip=old;
}
