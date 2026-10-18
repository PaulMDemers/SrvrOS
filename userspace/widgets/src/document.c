#include <srvros/desk_document.h>
#include <srvros/desk_flow.h>
#include <srvros/desk_text.h>
#include <string.h>

static const struct desk_font *font(const struct desk_document *d,enum desk_document_kind kind) {
    return kind==DESK_DOCUMENT_TITLE || kind==DESK_DOCUMENT_HEADING?&desk_font_20:kind==DESK_DOCUMENT_CODE?&desk_font_mono_16:d->body;
}
static int content_width(const struct desk_document *d) { int width=d->frame.w-42;return width>0?width:0; }
static void layout(struct desk_document *d) {
    int y=12,width=content_width(d);
    for(unsigned i=0;i<d->count;i++) {
        const struct desk_document_block *b=&d->blocks[i];int inset=b->kind==DESK_DOCUMENT_CODE || b->kind==DESK_DOCUMENT_NOTE?8:0;
        int height=desk_text_block_height(font(d,b->kind),b->text,width>inset*2?width-inset*2:0,DESK_DOCUMENT_BLOCK_BYTES);
        if(!height) height=font(d,b->kind)->line_height;
        d->layout[i].y=y;d->layout[i].height=height+inset*2;y+=height+inset*2+12;
    }
    struct desk_rect r=desk_widget_inset(d->frame,1);
    desk_scrollbar_configure(&d->scroll,(struct desk_rect){r.x+(r.w>12?r.w-12:0),r.y,r.w>12?12:r.w,r.h},y,r.h);
}
void desk_document_init(struct desk_document *d) { *d=(struct desk_document){.body=&desk_font_16}; }
int desk_document_set(struct desk_document *d,const struct desk_document_block *blocks,unsigned count) {
    if(count>DESK_DOCUMENT_BLOCKS || (count && !blocks)) return -1;
    size_t total=0;
    for(unsigned i=0;i<count;i++) {
        const struct desk_document_block *b=&blocks[i];if(!b->id || !b->text || b->kind>DESK_DOCUMENT_NOTE || b->kind<DESK_DOCUMENT_TITLE) return -1;
        for(unsigned j=0;j<i;j++) if(blocks[j].id==b->id) return -1;
        size_t length=0;while(length<DESK_DOCUMENT_BLOCK_BYTES && b->text[length]) length++;
        if(length==DESK_DOCUMENT_BLOCK_BYTES || !desk_text_utf8_valid(b->text,length) || total>DESK_DOCUMENT_BYTES-length) return -1;
        for(size_t j=0;j<length;j++) if(((unsigned char)b->text[j]<32 && b->text[j]!='\n' && b->text[j]!='\t') || (unsigned char)b->text[j]==127) return -1;
        total+=length;
    }
    if(count) memmove(d->blocks,blocks,count*sizeof(*blocks));
    d->count=count;
    d->scroll.offset=d->scroll.dragging=0;layout(d);return 0;
}
void desk_document_configure(struct desk_document *d,struct desk_rect frame,const struct desk_font *body,int enabled) {
    if(frame.x<-16384 || frame.x>16384 || frame.y<-16384 || frame.y>16384 || frame.w<0 || frame.h<0 || frame.w>16384 || frame.h>16384) return;
    if(!body || !body->line_height || body->line_height>128) body=&desk_font_16;
    if(memcmp(&d->frame,&frame,sizeof(frame)) || d->body!=body || !enabled) d->scroll.dragging=0;
    d->frame=frame;d->body=body;d->enabled=enabled && frame.w>=64 && frame.h>=32;if(!d->enabled) d->focused=0;layout(d);
}
unsigned desk_document_scroll(struct desk_document *d,int32_t delta) {
    if(!d->enabled) return 0;
    return desk_scrollbar_scroll(&d->scroll,delta);
}
unsigned desk_document_jump(struct desk_document *d,unsigned id) {
    for(unsigned i=0;i<d->count;i++) if(d->blocks[i].id==id) return desk_scrollbar_set(&d->scroll,d->layout[i].y);
    return 0;
}
unsigned desk_document_event(struct desk_document *d,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        d->scroll.dragging=0;if(e.type==DESK_WIDGET_FOCUS_OUT) { d->focused=0;return DESK_WIDGET_REDRAW; }return 0;
    }
    if(!d->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { d->focused=1;return DESK_WIDGET_REDRAW; }
    return desk_scrollbar_event(&d->scroll,e);
}
unsigned desk_document_key(struct desk_document *d,const struct srv_key_event *k) {
    if(k->flags&SRV_KEY_CANCEL) return desk_document_event(d,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!d->enabled || !d->focused || !(k->flags&SRV_KEY_DOWN_EVENT) || (k->modifiers&(SRV_KEY_CTRL|SRV_KEY_ALT|SRV_KEY_META))) return 0;
    int line=d->body->line_height,page=d->scroll.viewport-line;if(page<line) page=line;d->scroll.dragging=0;
    switch(k->usage) {
    case SRV_KEY_UP: return desk_document_scroll(d,-line);
    case SRV_KEY_DOWN: return desk_document_scroll(d,line);
    case SRV_KEY_PAGEUP: return desk_document_scroll(d,-page);
    case SRV_KEY_PAGEDOWN: return desk_document_scroll(d,page);
    case SRV_KEY_SPACE: return desk_document_scroll(d,(k->modifiers&SRV_KEY_SHIFT)?-page:page);
    case SRV_KEY_HOME: return desk_scrollbar_set(&d->scroll,0);
    case SRV_KEY_END: return desk_scrollbar_set(&d->scroll,d->scroll.content);
    default:return 0;
    }
}
void desk_document_draw(struct desk_canvas *c,const struct desk_document *d,const struct desk_theme *t) {
    struct desk_rect old=c->clip;desk_clip(c,desk_intersect(old,d->frame));desk_fill(c,d->frame,d->focused?t->accent:t->border);
    struct desk_rect r=desk_widget_inset(d->frame,1);desk_fill(c,r,t->surface);int width=content_width(d);
    struct desk_rect viewport=r;viewport.w=viewport.w>14?viewport.w-14:0;desk_clip(c,desk_intersect(c->clip,viewport));
    for(unsigned i=0;i<d->count;i++) {
        const struct desk_document_block *b=&d->blocks[i];struct desk_rect block={r.x+12,r.y+d->layout[i].y-d->scroll.offset,width,d->layout[i].height};
        struct desk_rect visible=desk_intersect(c->clip,block);if(!visible.w || !visible.h) continue;
        uint32_t color=t->text;
        if(b->kind==DESK_DOCUMENT_CODE) { desk_fill(c,block,t->track);block=desk_widget_inset(block,8); }
        else if(b->kind==DESK_DOCUMENT_NOTE) { desk_fill(c,(struct desk_rect){block.x,block.y,2,block.h},t->border);block=desk_widget_inset(block,8);color=t->muted; }
        desk_text_block_draw(c,block,font(d,b->kind),b->text,color);
    }
    desk_clip(c,desk_intersect(old,d->frame));if(d->scroll.content>d->scroll.viewport) desk_scrollbar_draw(c,&d->scroll,t);
    c->clip=old;
}
long desk_document_text(const struct desk_document *d,char *output,size_t capacity) {
    size_t size=0;for(unsigned i=0;i<d->count;i++) size+=strlen(d->blocks[i].text)+(i?2:0);
    if(!output) return (long)size;
    if(capacity<=size) return -1;
    size_t at=0;for(unsigned i=0;i<d->count;i++) {
        if(i) { output[at++]='\n';output[at++]='\n'; }
        size_t length=strlen(d->blocks[i].text);memcpy(output+at,d->blocks[i].text,length);at+=length;
    }
    output[at]=0;return (long)size;
}
