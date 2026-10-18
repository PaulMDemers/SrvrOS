#include <srvros/desk_edit.h>
#include <limits.h>
#include <string.h>

static int32_t clamp(int64_t value,int32_t lo,int32_t hi) {
    return value<lo?lo:value>hi?hi:(int32_t)value;
}
static int advance(const struct desk_edit *e,size_t p,char glyph[5]) {
    const struct desk_text_buffer *t=e->text;
    size_t n=desk_text_next(t,p)-p;
    if(t->data[p]=='\t') { glyph[0]=0; return desk_text_width(e->font,"    "); }
    if(n>4) n=4;
    memcpy(glyph,t->data+p,n); glyph[n]=0;
    return desk_text_width(e->font,glyph);
}
static void position(const struct desk_edit *e,size_t p,int *x,int *y) {
    *x=*y=0;const struct desk_text_buffer *t=e->text;size_t start=0;
    while(start<p) {
        const char *newline=memchr(t->data+start,'\n',p-start);if(!newline) break;
        start=(size_t)(newline-t->data)+1;*y=clamp((int64_t)*y+e->font->line_height,0,INT32_MAX);
    }
    for(size_t i=start;i<p;i=desk_text_next(t,i)) { char glyph[5];*x=clamp((int64_t)*x+advance(e,i,glyph),0,INT32_MAX); }
}
static size_t hit_text(const struct desk_edit *e,int64_t x,int64_t y) {
    const struct desk_text_buffer *t=e->text; size_t p=0;
    int64_t row=y<0?0:y/e->font->line_height;
    if(!e->multiline) row=0;
    while(row>0 && p<t->length) if(t->data[p++]=='\n') row--;
    int64_t pen=0;
    while(p<t->length && t->data[p]!='\n') {
        char glyph[5]; int width=advance(e,p,glyph);
        if(x<pen+(width+1)/2) break;
        pen+=width; p=desk_text_next(t,p);
    }
    return p;
}
struct desk_rect desk_edit_viewport(const struct desk_edit *e) {
    struct desk_rect r=desk_widget_inset(e->frame,6);
    if(e->show_vertical) r.w=r.w>18?r.w-18:0;
    if(e->show_horizontal) r.h=r.h>18?r.h-18:0;
    return r;
}
static void measure(struct desk_edit *e) {
    if(e->metrics_valid && e->measure_revision==e->text->revision && e->measure_font==e->font) return;
    int64_t widest=0,line=0,lines=1;
    for(size_t p=0;p<e->text->length;p=desk_text_next(e->text,p)) {
        if(e->text->data[p]=='\n') { line=0;lines++; }
        else { char glyph[5];line+=advance(e,p,glyph);if(line>widest) widest=line; }
    }
    e->content_width=clamp(widest+2,0,INT32_MAX/2);
    e->content_height=clamp(lines*e->font->line_height,0,INT32_MAX/2);
    e->measure_revision=e->text->revision;e->measure_font=e->font;e->metrics_valid=1;
    e->scroll_capture=0;e->vertical.dragging=e->horizontal.dragging=0;
}
static void update_scrollbars(struct desk_edit *e) {
    if(!e->scrollbars || !e->multiline) {
        e->show_vertical=e->show_horizontal=0;
        desk_scrollbar_configure(&e->vertical,(struct desk_rect){0},0,0);
        desk_scrollbar_configure_horizontal(&e->horizontal,(struct desk_rect){0},0,0);return;
    }
    measure(e);struct desk_rect base=desk_widget_inset(e->frame,6),inner=desk_widget_inset(e->frame,1);
    int v=base.w>18 && base.h>0 && e->content_height>base.h;
    int h=base.h>18 && base.w>0 && e->content_width>base.w-(v?18:0);
    v=base.w>18 && base.h>0 && e->content_height>base.h-(h?18:0);
    h=base.h>18 && base.w>0 && e->content_width>base.w-(v?18:0);
    if(v!=e->show_vertical || h!=e->show_horizontal) e->scroll_capture=0;
    e->show_vertical=v;e->show_horizontal=h;struct desk_rect view=desk_edit_viewport(e);
    desk_scrollbar_configure(&e->vertical,v?(struct desk_rect){inner.x+inner.w-18,inner.y,18,inner.h-(h?18:0)}:(struct desk_rect){0},v?e->content_height:0,v?view.h:0);
    desk_scrollbar_configure_horizontal(&e->horizontal,h?(struct desk_rect){inner.x,inner.y+inner.h-18,inner.w-(v?18:0),18}:(struct desk_rect){0},h?e->content_width:0,h?view.w:0);
    desk_scrollbar_set(&e->vertical,e->scroll_y);desk_scrollbar_set(&e->horizontal,e->scroll_x);
    e->scroll_y=e->vertical.offset;
    /* A very short viewport has no room for a horizontal bar, but caret reveal
     * and horizontal wheel motion still need to reach the end of a long line. */
    if(h || e->content_width<=view.w) e->scroll_x=e->horizontal.offset;
}
void desk_edit_scrollbars(struct desk_edit *e,int enabled) {
    e->scrollbars=!!enabled;e->scroll_capture=0;e->vertical.dragging=e->horizontal.dragging=0;desk_edit_reveal(e);
}
void desk_edit_init(struct desk_edit *e,struct desk_text_buffer *t,int multiline) {
    *e=(struct desk_edit){.text=t,.multiline=multiline!=0,.font=&desk_font_16,.preferred_x=-1};
}
void desk_edit_configure(struct desk_edit *e,struct desk_rect frame,const struct desk_font *font,int enabled) {
    if(memcmp(&e->frame,&frame,sizeof(frame)) || !enabled) { e->dragging=e->scroll_capture=0;e->vertical.dragging=e->horizontal.dragging=0; }
    e->frame=frame; e->font=font?font:&desk_font_16; e->enabled=enabled!=0;
    if(!enabled) e->focused=0;
    desk_edit_reveal(e);
}
void desk_edit_reveal(struct desk_edit *e) {
    update_scrollbars(e);struct desk_rect r=desk_edit_viewport(e);
    if(!r.w || !r.h) return;
    int x,y; position(e,e->text->caret,&x,&y);
    if(x<e->scroll_x) e->scroll_x=x;
    if((int64_t)x>=e->scroll_x+(int64_t)r.w-1) e->scroll_x=clamp((int64_t)x-r.w+2,0,INT32_MAX);
    if(!e->multiline) { e->scroll_y=0; return; }
    if(y<e->scroll_y) e->scroll_y=y;
    if((int64_t)y+e->font->line_height>e->scroll_y+(int64_t)r.h)
        e->scroll_y=clamp((int64_t)y+e->font->line_height-r.h,0,INT32_MAX);
    update_scrollbars(e);
}
unsigned desk_edit_scroll(struct desk_edit *e,int32_t x,int32_t y) {
    if(!e->enabled) return 0;
    int32_t oldx=e->scroll_x,oldy=e->scroll_y;measure(e);update_scrollbars(e);
    struct desk_rect view=desk_edit_viewport(e);
    int32_t maxx=clamp((int64_t)e->content_width-view.w,0,INT32_MAX/2),maxy=clamp((int64_t)e->content_height-view.h,0,INT32_MAX/2);
    e->scroll_x=clamp((int64_t)e->scroll_x+x,0,maxx);
    e->scroll_y=e->multiline?clamp((int64_t)e->scroll_y+y,0,maxy):0;
    update_scrollbars(e);
    return e->scroll_x!=oldx || e->scroll_y!=oldy?DESK_WIDGET_REDRAW:0;
}
unsigned desk_edit_event(struct desk_edit *e,struct desk_widget_event event) {
    if(event.type==DESK_WIDGET_CANCEL || event.type==DESK_WIDGET_FOCUS_OUT) {
        e->dragging=e->scroll_capture=0;desk_scrollbar_event(&e->vertical,event);desk_scrollbar_event(&e->horizontal,event);
        if(event.type==DESK_WIDGET_FOCUS_OUT && e->focused) { e->focused=0; return DESK_WIDGET_REDRAW; }
        return 0;
    }
    if(!e->enabled) return 0;
    if(event.type==DESK_WIDGET_FOCUS_IN) { e->focused=1; return DESK_WIDGET_REDRAW; }
    if(event.type!=DESK_WIDGET_POINTER) return 0;
    update_scrollbars(e);
    if(event.pressed && e->show_vertical && e->show_horizontal &&
       desk_widget_contains((struct desk_rect){e->vertical.track.x,e->horizontal.track.y,18,18},event.x,event.y)) {
        e->dragging=e->scroll_capture=0;return 0;
    }
    if(event.pressed) {
        e->scroll_capture=desk_widget_contains(e->vertical.track,event.x,event.y)?1:desk_widget_contains(e->horizontal.track,event.x,event.y)?2:0;
        if(e->scroll_capture) e->dragging=0;
    }
    if(e->scroll_capture) {
        struct desk_scrollbar *bar=e->scroll_capture==1?&e->vertical:&e->horizontal;
        unsigned result=desk_scrollbar_event(bar,event)&DESK_WIDGET_REDRAW;
        if(e->scroll_capture==1) e->scroll_y=bar->offset;else e->scroll_x=bar->offset;
        if(!event.down) e->scroll_capture=0;
        return result;
    }
    if(event.pressed) {
        e->dragging=0;
        if(!event.down || !desk_widget_contains(e->frame,event.x,event.y)) return 0;
        e->dragging=1;
    } else if(!e->dragging) return 0;
    struct desk_rect r=desk_edit_viewport(e);
    if(!r.w || !r.h) { e->dragging=0; return 0; }
    size_t p=hit_text(e,(int64_t)event.x-r.x+e->scroll_x,(int64_t)event.y-r.y+e->scroll_y);
    desk_text_buffer_select(e->text,event.pressed?p:e->text->anchor,p);
    e->preferred_x=-1; desk_edit_reveal(e);
    if(!event.down) e->dragging=0;
    return DESK_WIDGET_REDRAW;
}
static int word_space(char c) { return c==' ' || c=='\t' || c=='\n' || c=='\r'; }
static size_t word(const struct desk_text_buffer *t,size_t p,int backwards) {
    if(backwards) {
        while(p && word_space(t->data[desk_text_prev(t,p)])) p=desk_text_prev(t,p);
        while(p && !word_space(t->data[desk_text_prev(t,p)])) p=desk_text_prev(t,p);
    } else {
        while(p<t->length && !word_space(t->data[p])) p=desk_text_next(t,p);
        while(p<t->length && word_space(t->data[p])) p=desk_text_next(t,p);
    }
    return p;
}
unsigned desk_edit_command(struct desk_edit *e,enum desk_edit_command command) {
    if(!e->enabled || !e->focused) return 0;
    if(command==DESK_EDIT_SELECT_ALL) {
        desk_text_buffer_select(e->text,0,e->text->length);desk_edit_reveal(e);return DESK_WIDGET_REDRAW;
    }
    if(command==DESK_EDIT_COMMAND_COPY) return DESK_EDIT_COPY;
    if(command==DESK_EDIT_COMMAND_CUT) return DESK_EDIT_CUT;
    if(command==DESK_EDIT_COMMAND_PASTE) return DESK_EDIT_PASTE;
    if(command!=DESK_EDIT_UNDO && command!=DESK_EDIT_REDO) return 0;
    int changed=desk_text_buffer_undo(e->text,command==DESK_EDIT_REDO);e->preferred_x=-1;
    if(changed>0) desk_edit_reveal(e);
    return changed>0?DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW:0;
}
unsigned desk_edit_key(struct desk_edit *e,const struct srv_key_event *key) {
    if(key->flags&SRV_KEY_CANCEL) { e->dragging=e->scroll_capture=0;e->vertical.dragging=e->horizontal.dragging=0;return 0; }
    if(!e->enabled || !e->focused || !(key->flags&SRV_KEY_DOWN_EVENT)) return 0;
    struct desk_text_buffer *t=e->text;
    int ctrl=(key->modifiers&(SRV_KEY_CTRL|SRV_KEY_META))!=0;
    int shift=(key->modifiers&SRV_KEY_SHIFT)!=0;
    size_t p=t->caret; int changed=0,moved=0,vertical=0;
    if(ctrl) {
        switch(key->usage) {
        case 4: return desk_edit_command(e,DESK_EDIT_SELECT_ALL);
        case 6: return desk_edit_command(e,DESK_EDIT_COMMAND_COPY);
        case 0x1b: return desk_edit_command(e,DESK_EDIT_COMMAND_CUT);
        case 0x19: return desk_edit_command(e,DESK_EDIT_COMMAND_PASTE);
        case 0x1d: return desk_edit_command(e,shift?DESK_EDIT_REDO:DESK_EDIT_UNDO);
        case 0x1c: return desk_edit_command(e,DESK_EDIT_REDO);
        }
    }
    switch(key->usage) {
    case SRV_KEY_LEFT:
        p=ctrl?word(t,p,1):!shift && t->anchor!=p?(t->anchor<p?t->anchor:p):desk_text_prev(t,p);
        moved=1; break;
    case SRV_KEY_RIGHT:
        p=ctrl?word(t,p,0):!shift && t->anchor!=p?(t->anchor>p?t->anchor:p):desk_text_next(t,p);
        moved=1; break;
    case SRV_KEY_HOME: p=ctrl?0:desk_text_line_start(t,p); moved=1; break;
    case SRV_KEY_END: p=ctrl?t->length:desk_text_line_end(t,p); moved=1; break;
    case SRV_KEY_UP: case SRV_KEY_DOWN: case SRV_KEY_PAGEUP: case SRV_KEY_PAGEDOWN: {
        if(!e->multiline) return 0;
        int x,y; position(e,p,&x,&y); if(e->preferred_x<0) e->preferred_x=x;
        struct desk_rect view=desk_edit_viewport(e);
        int rows=(key->usage==SRV_KEY_PAGEUP || key->usage==SRV_KEY_PAGEDOWN)?view.h/e->font->line_height:1;
        if(rows<1) rows=1;
        int direction=(key->usage==SRV_KEY_UP || key->usage==SRV_KEY_PAGEUP)?-1:1;
        p=hit_text(e,e->preferred_x,(int64_t)y+direction*(int64_t)rows*e->font->line_height);
        moved=vertical=1; break;
    }
    case SRV_KEY_BACKSPACE: case SRV_KEY_DELETE:
        if(ctrl && t->anchor==t->caret) t->anchor=word(t,t->caret,key->usage==SRV_KEY_BACKSPACE);
        changed=desk_text_buffer_erase(t,key->usage==SRV_KEY_BACKSPACE); break;
    case SRV_KEY_ENTER:
        if(!e->multiline) return DESK_EDIT_SUBMIT;
        changed=desk_text_buffer_insert(t,"\n",1); break;
    default:
        if(ctrl || (key->modifiers&SRV_KEY_ALT) || key->codepoint<32 || key->codepoint==127) break;
        uint32_t cp=key->codepoint; char bytes[4]; size_t n;
        if(cp<0x80) { bytes[0]=cp; n=1; }
        else if(cp<0x800) { bytes[0]=0xc0|(cp>>6); bytes[1]=0x80|(cp&63); n=2; }
        else if(cp<0x10000) { bytes[0]=0xe0|(cp>>12); bytes[1]=0x80|((cp>>6)&63); bytes[2]=0x80|(cp&63); n=3; }
        else if(cp<=0x10ffff) { bytes[0]=0xf0|(cp>>18); bytes[1]=0x80|((cp>>12)&63); bytes[2]=0x80|((cp>>6)&63); bytes[3]=0x80|(cp&63); n=4; }
        else return 0;
        changed=desk_text_buffer_insert(t,bytes,n); break;
    }
    if(moved) desk_text_buffer_select(t,shift?t->anchor:p,p);
    if(!vertical) e->preferred_x=-1;
    if(changed>0 || moved) desk_edit_reveal(e);
    return (changed>0?DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW:0)|
        (moved?DESK_WIDGET_REDRAW:0)|(changed<0?DESK_EDIT_FULL:0);
}
void desk_edit_draw(struct desk_canvas *c,const struct desk_edit *e,const struct desk_theme *theme) {
    struct desk_edit shown=*e;update_scrollbars(&shown);e=&shown;
    struct desk_rect clip=c->clip,inner=desk_edit_viewport(e);
    desk_fill(c,e->frame,e->focused?theme->accent:theme->border);
    desk_fill(c,desk_widget_inset(e->frame,1),theme->surface);
    if(e->show_vertical) desk_scrollbar_draw(c,&e->vertical,theme);
    if(e->show_horizontal) desk_scrollbar_draw(c,&e->horizontal,theme);
    if(!inner.w || !inner.h) return;
    desk_clip(c,desk_intersect(clip,inner));
    const struct desk_text_buffer *t=e->text;
    size_t lo=t->caret<t->anchor?t->caret:t->anchor,hi=t->caret>t->anchor?t->caret:t->anchor;
    int64_t x=inner.x-(int64_t)e->scroll_x,y=inner.y-(int64_t)e->scroll_y;
    struct desk_rect caret={0};size_t start=0;
    /* Entire physical lines above the viewport need no glyph measurements. */
    while(y+e->font->line_height<=inner.y && start<t->length) {
        const char *newline=memchr(t->data+start,'\n',t->length-start);
        if(!newline) { c->clip=clip;return; }
        start=(size_t)(newline-t->data)+1;y+=e->font->line_height;
    }
    for(size_t p=start;p<=t->length;p=desk_text_next(t,p)) {
        if(y>=inner.y+(int64_t)inner.h) break;
        if(p==t->caret && e->focused && e->enabled && x>=INT32_MIN && x<=INT32_MAX && y>=INT32_MIN)
            caret=(struct desk_rect){x,y,1,e->font->line_height};
        if(p==t->length) break;
        char glyph[5]; int width=t->data[p]=='\n'?4:advance(e,p,glyph);
        if(y+e->font->line_height>inner.y && x+width>inner.x && x<inner.x+(int64_t)inner.w) {
            if(p>=lo && p<hi) desk_fill(c,(struct desk_rect){x,y,width,e->font->line_height},theme->selected);
            if(t->data[p]!='\n') desk_text(c,e->font,x,y,glyph,e->enabled?theme->text:theme->muted);
        }
        if(t->data[p]=='\n') { x=inner.x-(int64_t)e->scroll_x; y+=e->font->line_height; }
        else x+=width;
    }
    if(caret.w) desk_fill(c,caret,theme->accent);
    c->clip=clip;
}
