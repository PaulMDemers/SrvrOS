#include <srvros/desk_palette.h>
#include <string.h>
enum { QUERY=1,RESULTS };
static int valid(const char *text,size_t capacity) {
    if(!text) return 0;
    size_t n=0;
    while(n<capacity && text[n]) { if((unsigned char)text[n]<32 || (unsigned char)text[n]==127) return 0;n++; }
    return n<capacity && desk_text_utf8_valid(text,n);
}
static unsigned fold(unsigned c) { return c>='A' && c<='Z'?c+32:c; }
static int contains(const char *text,const char *token,size_t length) {
    for(;*text;text++) { size_t i=0;while(i<length && text[i] && fold((unsigned char)text[i])==fold((unsigned char)token[i])) i++;if(i==length) return 1; }
    return 0;
}
static int matches(const struct desk_palette_item *item,const char *query) {
    while(*query) {
        while(*query==' ') query++;
        const char *start=query;while(*query && *query!=' ') query++;
        size_t length=query-start;
        if(length && !contains(item->label,start,length) && !contains(item->detail,start,length)) return 0;
    }
    return 1;
}
static void filter(struct desk_palette *p,uint64_t selected) {
    int count=0,select=-1;
    for(unsigned i=0;i<p->count;i++) if(matches(&p->items[i],p->text.data)) {
        p->visible[count]=(struct desk_list_item){.label=p->items[i].label,.detail=p->items[i].detail,.id=p->items[i].id};
        if(p->items[i].id==selected) select=count;
        count++;
    }
    desk_list_set_items(&p->list,p->visible,count);desk_list_select(&p->list,select>=0?select:count?0:-1);
}
void desk_palette_filter(struct desk_palette *p) { p->notice[0]=0;filter(p,0); }
int desk_palette_message(struct desk_palette *p,const char *message) {
    if(!valid(message,sizeof(p->notice))) return -1;
    strcpy(p->notice,message);return 0;
}
void desk_palette_init(struct desk_palette *p) {
    *p=(struct desk_palette){0};desk_ui_init(&p->ui);desk_list_init(&p->list);
    desk_text_buffer_init(&p->text,p->storage[0],p->storage[1],DESK_PALETTE_QUERY);desk_edit_init(&p->query,&p->text,0);
    desk_ui_add(&p->ui,QUERY,DESK_CONTROL_EDIT,&p->query,NULL);desk_ui_add(&p->ui,RESULTS,DESK_CONTROL_LIST,&p->list,NULL);
}
int desk_palette_items(struct desk_palette *p,const struct desk_palette_item *items,unsigned count) {
    if(count>DESK_PALETTE_ITEMS || (!items && count)) return -1;
    for(unsigned i=0;i<count;i++) {
        if(!items[i].id || !items[i].label[0] || !valid(items[i].label,sizeof(items[i].label)) || !valid(items[i].detail,sizeof(items[i].detail))) return -1;
        for(unsigned j=0;j<i;j++) if(items[i].id==items[j].id) return -1;
    }
    uint64_t selected=p->list.selected>=0?p->visible[p->list.selected].id:0;
    if(count) memmove(p->items,items,count*sizeof(*items));
    p->count=count;filter(p,selected);return 0;
}
int desk_palette_open(struct desk_palette *p,const char *title,const struct desk_palette_item *items,unsigned count) {
    if(!valid(title,sizeof(p->title))) return -1;
    char copy[96];strcpy(copy,title);
    if(desk_palette_items(p,items,count)) return -1;
    strcpy(p->title,copy);desk_text_buffer_set(&p->text,"",0);desk_palette_filter(p);
    p->query.enabled=1;p->list.enabled=1;desk_ui_window_focus(&p->ui,1);desk_ui_focus(&p->ui,QUERY);return 0;
}
void desk_palette_configure(struct desk_palette *p,struct desk_rect bounds) {
    if(bounds.w<0 || bounds.h<0 || bounds.w>16384 || bounds.h>16384 || bounds.x<0 || bounds.y<0 || bounds.x>16384 || bounds.y>16384) return;
    int w=bounds.w>32?bounds.w-32:bounds.w,h=bounds.h>32?bounds.h-32:bounds.h;if(w>560) w=560;if(h>400) h=400;
    struct desk_rect r={bounds.x+(bounds.w-w)/2,bounds.y+(bounds.h-h)/2,w,h};p->frame=r;
    desk_edit_configure(&p->query,desk_intersect(r,(struct desk_rect){r.x+16,r.y+52,w>32?w-32:0,36}),&desk_font_16,1);
    desk_list_configure(&p->list,desk_intersect(r,(struct desk_rect){r.x+16,r.y+100,w>32?w-32:0,h>140?h-140:0}),1);
    desk_ui_layout_changed(&p->ui);
}
unsigned desk_palette_focus(struct desk_palette *p,int active) { return desk_ui_window_focus(&p->ui,active); }
static struct desk_palette_result result(struct desk_palette *p,struct desk_action a) {
    struct desk_palette_result r={.flags=a.flags};
    if(a.id==QUERY && (a.flags&DESK_WIDGET_CHANGED)) desk_palette_filter(p);
    if((a.id==QUERY && (a.flags&DESK_EDIT_SUBMIT)) || (a.id==RESULTS && (a.flags&DESK_WIDGET_ACTIVATED))) {
        if(p->list.selected>=0) { r.id=p->visible[p->list.selected].id;r.flags|=DESK_WIDGET_ACTIVATED; }
    }
    return r;
}
struct desk_palette_result desk_palette_key(struct desk_palette *p,const struct srv_key_event *key) {
    if(p->ui.active && (key->flags&SRV_KEY_DOWN_EVENT)) {
        if(key->usage==SRV_KEY_ESCAPE) return (struct desk_palette_result){.cancel=1,.flags=DESK_WIDGET_REDRAW};
        if(p->ui.focus==0 && !(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) &&
            (key->usage==SRV_KEY_DOWN || key->usage==SRV_KEY_UP)) {
            int index=p->list.selected+(key->usage==SRV_KEY_DOWN?1:-1);if(index<0) index=0;if(index>=p->list.count) index=p->list.count-1;
            return (struct desk_palette_result){.flags=desk_list_select(&p->list,index)};
        }
    }
    return result(p,desk_ui_key(&p->ui,key));
}
struct desk_palette_result desk_palette_pointer(struct desk_palette *p,struct desk_widget_event event,uint64_t time_us) {
    if(p->ui.active && event.type==DESK_WIDGET_POINTER && event.pressed && !desk_widget_contains(p->frame,event.x,event.y)) {
        desk_ui_pointer(&p->ui,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},time_us);
        return (struct desk_palette_result){.cancel=1,.flags=DESK_WIDGET_REDRAW};
    }
    return result(p,desk_ui_pointer(&p->ui,event,time_us));
}
struct desk_palette_result desk_palette_scroll(struct desk_palette *p,int x,int y,int delta) { return result(p,desk_ui_scroll(&p->ui,x,y,0,delta)); }
void desk_palette_draw(struct desk_canvas *c,const struct desk_palette *p,const struct desk_theme *theme) {
    struct desk_rect r=p->frame;if(r.w<=0 || r.h<=0) return;
    desk_fill(c,(struct desk_rect){r.x+4,r.y+5,r.w,r.h},0xb9c2b4);desk_fill(c,r,theme->border);desk_fill(c,desk_widget_inset(r,1),theme->surface);
    struct desk_rect clip=c->clip;desk_clip(c,desk_intersect(clip,r));
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+16,r.w>32?r.w-32:0,28},theme->heading,p->title,theme->text);
    desk_ui_draw(c,&p->ui,theme);
    desk_label_draw(c,(struct desk_rect){r.x+16,r.y+r.h-28,r.w>32?r.w-32:0,24},theme->body,p->notice[0]?p->notice:r.w<420?"Enter: open · Esc: close":"Type to search · Enter to open · Esc to close",theme->muted);
    desk_clip(c,clip);
}
