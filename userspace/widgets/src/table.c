#include <srvros/desk_table.h>
#include <string.h>
#define HEADER 30
#define ROW 28
static struct desk_rect header(const struct desk_table *t) {
    struct desk_rect r=desk_widget_inset(t->frame,1);
    if(r.h>HEADER) r.h=HEADER;
    if(r.w>18) r.w-=18;else r.w=0;
    return r;
}
static struct desk_rect column(const struct desk_table *t,int index) {
    struct desk_rect h=header(t);int fixed=0,flex=0;
    for(int i=0;i<t->column_count;i++) { if(t->columns[i].width) fixed+=t->columns[i].width;else flex++; }
    int extra=flex?(h.w-fixed)/flex:0;if(extra<64) extra=64;
    int64_t x=h.x;
    for(int i=0;i<index;i++) x+=t->columns[i].width?t->columns[i].width:extra;
    if(x>=(int64_t)h.x+h.w) return (struct desk_rect){0};
    int width=t->columns[index].width?t->columns[index].width:extra;
    if(x+width>(int64_t)h.x+h.w) width=(int)((int64_t)h.x+h.w-x);
    return (struct desk_rect){(int)x,h.y,width,h.h};
}
static int hit_header(const struct desk_table *t,int x,int y) {
    for(int i=0;i<t->column_count;i++) if(desk_widget_contains(column(t,i),x,y)) return i;
    return -1;
}
void desk_table_init(struct desk_table *t) {
    *t=(struct desk_table){.sort_column=-1,.pressed_column=-1};desk_list_init(&t->body);
}
int desk_table_columns(struct desk_table *t,const struct desk_table_column *columns,int count) {
    if(!columns || count<1 || count>DESK_TABLE_MAX_COLUMNS) return -1;
    for(int i=0;i<count;i++) if(!columns[i].label || columns[i].width<0 || columns[i].width>4096) return -1;
    t->columns=columns;t->column_count=count;t->pressed_column=-1;t->header_column=0;t->sort_column=-1;
    return 0;
}
int desk_table_rows(struct desk_table *t,const struct desk_table_row *rows,int count) {
    if(count<0 || count>DESK_TABLE_MAX_ROWS || (!rows && count)) return -1;
    for(int i=0;i<count;i++) {
        if(!rows[i].cells[0]) return -1;
        for(int j=0;j<i;j++) if(rows[i].id==rows[j].id) return -1;
    }
    int selected=-1,had=t->body.selected>=0;
    uint64_t id=had?t->navigation[t->body.selected].id:0;
    int64_t offset=t->body.scroll.offset;
    for(int i=0;i<count;i++) {
        t->navigation[i]=(struct desk_list_item){.label=rows[i].cells[0],.id=rows[i].id};
        if(had && rows[i].id==id) selected=i;
    }
    t->rows=rows;desk_list_set_items(&t->body,t->navigation,count);
    t->body.selected=selected;desk_scrollbar_set(&t->body.scroll,offset);
    return 0;
}
void desk_table_configure(struct desk_table *t,struct desk_rect frame,int enabled) {
    if(memcmp(&frame,&t->frame,sizeof(frame)) || !enabled) t->pressed_column=-1;
    t->frame=frame;
    struct desk_rect body=frame;int cut=body.h<HEADER?body.h:HEADER;if(cut<0) cut=0;
    body.y+=cut;body.h-=cut;
    desk_list_configure(&t->body,body,enabled);
}
void desk_table_sort_indicator(struct desk_table *t,int col,int descending) {
    if(col<-1 || col>=t->column_count) return;
    t->sort_column=col;t->descending=descending!=0;
}
static unsigned sort(struct desk_table *t,int col) {
    if(col<0 || col>=t->column_count || !t->columns[col].sortable) return 0;
    t->descending=t->sort_column==col?!t->descending:0;t->sort_column=col;
    return DESK_TABLE_SORT|DESK_WIDGET_REDRAW;
}
unsigned desk_table_event(struct desk_table *t,struct desk_widget_event e,uint64_t time_us) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        t->pressed_column=-1;return desk_list_event(&t->body,e,time_us)|DESK_WIDGET_REDRAW;
    }
    if(!t->body.enabled) return 0;
    if(e.type==DESK_WIDGET_ACTIVATE && t->header_focus) return sort(t,t->header_column);
    if(e.type!=DESK_WIDGET_POINTER) return desk_list_event(&t->body,e,time_us);
    int col=hit_header(t,e.x,e.y);unsigned redraw=0;
    if(e.pressed && e.down) {
        t->pressed_column=col;redraw=t->header_focus!=(col>=0)?DESK_WIDGET_REDRAW:0;t->header_focus=col>=0;
        if(col>=0) { t->header_column=col;desk_list_event(&t->body,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);return DESK_WIDGET_REDRAW; }
    }
    if(t->pressed_column>=0) {
        if(!e.down) { int pressed=t->pressed_column;t->pressed_column=-1;return col==pressed?sort(t,col):0; }
        return 0;
    }
    return desk_list_event(&t->body,e,time_us)|redraw;
}
unsigned desk_table_key(struct desk_table *t,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) return desk_table_event(t,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL},0);
    if(!t->body.enabled || !t->body.focused || !(key->flags&SRV_KEY_DOWN_EVENT)) return 0;
    if(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META)) return 0;
    if(key->usage==0x3f && !(key->flags&SRV_KEY_REPEAT)) { t->header_focus=!t->header_focus;return DESK_WIDGET_REDRAW; }
    if(!t->header_focus) return desk_list_key(&t->body,key);
    if(key->usage==SRV_KEY_LEFT && t->header_column>0) { t->header_column--;return DESK_WIDGET_REDRAW; }
    if(key->usage==SRV_KEY_RIGHT && t->header_column+1<t->column_count) { t->header_column++;return DESK_WIDGET_REDRAW; }
    if((key->usage==SRV_KEY_ENTER || key->usage==SRV_KEY_SPACE) && !(key->flags&SRV_KEY_REPEAT)) return sort(t,t->header_column);
    return 0;
}
unsigned desk_table_scroll(struct desk_table *t,int64_t delta) { return t->body.enabled?desk_list_scroll(&t->body,delta):0; }
static void cell(struct desk_canvas *c,struct desk_rect r,const struct desk_font *font,const char *text,uint32_t color,int right) {
    if(!text || r.w<=0 || r.h<=0) return;
    struct desk_rect clip=c->clip;desk_clip(c,desk_intersect(clip,r));
    if(right) { int width=desk_text_width(font,text);if(width<r.w) { r.x+=r.w-width;r.w=width; } }
    desk_label_ellipsis(c,r,font,text,color);c->clip=clip;
}
void desk_table_draw(struct desk_canvas *c,const struct desk_table *t,const struct desk_theme *theme) {
    struct desk_rect clip=c->clip,inner=desk_widget_inset(t->frame,1);
    desk_fill(c,t->frame,t->body.focused?theme->accent:theme->border);desk_fill(c,inner,theme->surface);
    struct desk_rect h=inner;if(h.h>HEADER) h.h=HEADER;desk_fill(c,h,theme->selected);
    for(int i=0;i<t->column_count;i++) {
        struct desk_rect r=column(t,i);if(r.w<=0) continue;
        if(t->header_focus && t->body.focused && t->header_column==i) desk_fill(c,r,theme->border);
        struct desk_rect label={r.x+8,r.y+4,r.w-16-(t->sort_column==i?14:0),r.h-4};
        cell(c,label,theme->body,t->columns[i].label,theme->text,0);
        if(t->sort_column==i && r.w>=24) {
            int x=r.x+r.w-15,y=r.y+12;
            for(int j=0;j<4;j++) desk_fill(c,(struct desk_rect){x+j,y+(t->descending?j:3-j),7-2*j,1},theme->muted);
        }
        desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y+5,1,r.h-10},theme->border);
    }
    struct desk_rect body=desk_widget_inset(t->body.frame,1);if(body.w>18) body.w-=18;else body.w=0;
    desk_clip(c,desk_intersect(clip,body));
    if(!t->body.count) desk_label_draw(c,desk_widget_inset(body,12),theme->body,"No items",theme->muted);
    for(int i=t->body.scroll.offset/ROW;i<t->body.count;i++) {
        int y=body.y+i*ROW-t->body.scroll.offset;if(y>=body.y+body.h) break;
        if(i==t->body.selected) desk_fill(c,(struct desk_rect){body.x,y,body.w,ROW},theme->selected);
        for(int j=0;j<t->column_count;j++) {
            struct desk_rect r=column(t,j);r.y=y+4;r.h=ROW-4;r.x+=8;r.w-=16;
            cell(c,r,theme->body,t->rows[i].cells[j],t->body.enabled?theme->text:theme->muted,t->columns[j].right);
        }
    }
    c->clip=clip;desk_scrollbar_draw(c,&t->body.scroll,theme);
}
