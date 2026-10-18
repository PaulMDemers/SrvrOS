#include <srvros/desk_properties.h>
#include <srvros/desk_flow.h>
#include <srvros/desk_text.h>
#include <string.h>
static int valid(const char *text,size_t capacity,int multiline) {
    if(!text) return 0;
    size_t n=0;while(n<capacity && text[n]) { unsigned c=(unsigned char)text[n];if((c<32 && !(multiline && (c=='\n' || c=='\t'))) || c==127) return 0;n++; }
    return n<capacity && desk_text_utf8_valid(text,n);
}
static void layout(struct desk_properties *p) {
    int width=p->frame.w>42?p->frame.w-42:0;p->stacked=p->frame.w<360;
    p->label_width=p->stacked?width:width/3;if(!p->stacked && p->label_width>160) p->label_width=160;
    p->value_width=p->stacked?width:width-p->label_width-16;if(p->value_width<0) p->value_width=0;
    int y=12;
    for(unsigned i=0;i<p->count;i++) {
        p->fields[i].label_height=desk_text_block_height(p->body,p->fields[i].label,p->label_width,DESK_PROPERTY_LABEL);
        p->fields[i].value_height=desk_text_block_height(p->body,p->fields[i].value[0]?p->fields[i].value:"—",p->value_width,DESK_PROPERTY_VALUE);
        int a=p->fields[i].label_height,b=p->fields[i].value_height;
        p->fields[i].height=p->stacked?a+4+b:(a>b?a:b);p->fields[i].y=y;y+=p->fields[i].height+20;
    }
    struct desk_rect r=desk_widget_inset(p->frame,1);
    desk_scrollbar_configure(&p->scroll,(struct desk_rect){r.x+(r.w>12?r.w-12:0),r.y,r.w>12?12:r.w,r.h},y,r.h);
}
void desk_properties_init(struct desk_properties *p) { *p=(struct desk_properties){.body=&desk_font_16}; }
int desk_properties_set(struct desk_properties *p,const struct desk_property *fields,unsigned count) {
    if(count>DESK_PROPERTIES_FIELDS || (count && !fields)) return -1;
    struct desk_properties next=*p;
    memset(next.fields,0,sizeof(next.fields));next.count=count;
    for(unsigned i=0;i<count;i++) {
        if(!fields[i].id || !valid(fields[i].label,DESK_PROPERTY_LABEL,0) || !fields[i].label[0] || !valid(fields[i].value,DESK_PROPERTY_VALUE,1)) return -1;
        for(unsigned j=0;j<i;j++) if(fields[j].id==fields[i].id) return -1;
        next.fields[i].id=fields[i].id;strcpy(next.fields[i].label,fields[i].label);strcpy(next.fields[i].value,fields[i].value);
    }
    next.scroll.offset=next.scroll.dragging=0;layout(&next);*p=next;return 0;
}
void desk_properties_configure(struct desk_properties *p,struct desk_rect frame,const struct desk_font *body,int enabled) {
    if(frame.x<-16384 || frame.x>16384 || frame.y<-16384 || frame.y>16384 || frame.w<0 || frame.h<0 || frame.w>16384 || frame.h>16384) return;
    if(!body || !body->line_height || body->line_height>128) body=&desk_font_16;
    if(memcmp(&p->frame,&frame,sizeof(frame)) || p->body!=body || !enabled) p->scroll.dragging=0;
    p->frame=frame;p->body=body;p->enabled=enabled && frame.w>=64 && frame.h>=32;if(!p->enabled) p->focused=0;layout(p);
}
unsigned desk_properties_scroll(struct desk_properties *p,int32_t delta) { return p->enabled?desk_scrollbar_scroll(&p->scroll,delta):0; }
unsigned desk_properties_event(struct desk_properties *p,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        p->scroll.dragging=0;if(e.type==DESK_WIDGET_FOCUS_OUT) { p->focused=0;return DESK_WIDGET_REDRAW; }return 0;
    }
    if(!p->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { p->focused=1;return DESK_WIDGET_REDRAW; }
    return desk_scrollbar_event(&p->scroll,e);
}
unsigned desk_properties_key(struct desk_properties *p,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) return desk_properties_event(p,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!p->enabled || !p->focused || !(key->flags&SRV_KEY_DOWN_EVENT) || (key->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) return 0;
    int line=p->body->line_height,page=p->scroll.viewport-line;if(page<line) page=line;p->scroll.dragging=0;
    switch(key->usage) {
    case SRV_KEY_UP:return desk_properties_scroll(p,-line);case SRV_KEY_DOWN:return desk_properties_scroll(p,line);
    case SRV_KEY_PAGEUP:return desk_properties_scroll(p,-page);case SRV_KEY_PAGEDOWN:return desk_properties_scroll(p,page);
    case SRV_KEY_SPACE:return desk_properties_scroll(p,key->modifiers&SRV_KEY_SHIFT?-page:page);
    case SRV_KEY_HOME:return desk_scrollbar_set(&p->scroll,0);case SRV_KEY_END:return desk_scrollbar_set(&p->scroll,p->scroll.content);
    default:return 0;
    }
}
void desk_properties_draw(struct desk_canvas *c,const struct desk_properties *p,const struct desk_theme *t) {
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,p->frame));desk_fill(c,p->frame,p->focused?t->accent:t->border);
    struct desk_rect r=desk_widget_inset(p->frame,1);desk_fill(c,r,t->surface);
    struct desk_rect viewport=r;viewport.w=viewport.w>14?viewport.w-14:0;desk_clip(c,desk_intersect(c->clip,viewport));
    for(unsigned i=0;i<p->count;i++) {
        struct desk_rect label={r.x+12,r.y+p->fields[i].y-p->scroll.offset,p->label_width,p->fields[i].label_height};
        struct desk_rect block={label.x,label.y,p->frame.w>42?p->frame.w-42:0,p->fields[i].height+11};
        struct desk_rect visible=desk_intersect(c->clip,block);if(!visible.w || !visible.h) continue;
        struct desk_rect value={p->stacked?label.x:label.x+p->label_width+16,p->stacked?label.y+p->fields[i].label_height+4:label.y,p->value_width,p->fields[i].value_height};
        desk_text_block_draw(c,label,p->body,p->fields[i].label,t->muted);desk_text_block_draw(c,value,p->body,p->fields[i].value[0]?p->fields[i].value:"—",t->text);
        if(i+1<p->count) desk_fill(c,(struct desk_rect){label.x,label.y+p->fields[i].height+10,p->frame.w>42?p->frame.w-42:0,1},t->border);
    }
    desk_clip(c,desk_intersect(old,p->frame));if(p->scroll.content>p->scroll.viewport) desk_scrollbar_draw(c,&p->scroll,t);c->clip=old;
}
long desk_properties_text(const struct desk_properties *p,char *output,size_t capacity) {
    size_t length=0;for(unsigned i=0;i<p->count;i++) length+=strlen(p->fields[i].label)+strlen(p->fields[i].value)+3;
    if(!output) return length;
    if(capacity<=length) return -1;
    size_t at=0;for(unsigned i=0;i<p->count;i++) {
        size_t n=strlen(p->fields[i].label);memcpy(output+at,p->fields[i].label,n);at+=n;output[at++]=':';output[at++]=' ';
        n=strlen(p->fields[i].value);memcpy(output+at,p->fields[i].value,n);at+=n;output[at++]='\n';
    }
    output[at]=0;return at;
}
