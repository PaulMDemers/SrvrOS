#include <srvros/desk_terminal_view.h>
#include <string.h>
#include <limits.h>
static unsigned encode(uint32_t cp,char out[5]) {
    unsigned n;
    if(cp<128) { out[0]=cp;n=1; }
    else if(cp<0x800) { out[0]=0xc0|(cp>>6);out[1]=0x80|(cp&63);n=2; }
    else if(cp<0x10000) { out[0]=0xe0|(cp>>12);out[1]=0x80|((cp>>6)&63);out[2]=0x80|(cp&63);n=3; }
    else { out[0]=0xf0|(cp>>18);out[1]=0x80|((cp>>12)&63);out[2]=0x80|((cp>>6)&63);out[3]=0x80|(cp&63);n=4; }
    out[n]=0;return n;
}
void desk_terminal_view_init(struct desk_terminal_view *v,struct desk_terminal *t) {
    *v=(struct desk_terminal_view){.terminal=t,.cell_width=10,.cell_height=22};
    v->cell_width=desk_text_width(&desk_font_mono_16,"M");v->cell_height=desk_font_mono_16.line_height;
}
void desk_terminal_view_configure(struct desk_terminal_view *v,struct desk_rect frame,int enabled) {
    int changed=memcmp(&v->frame,&frame,sizeof(frame))!=0;
    if(changed || !enabled) { v->dragging=v->scrollbar_capture=0;v->anchor=v->caret=0; }
    v->frame=frame;v->enabled=enabled!=0;
    if(!enabled) v->focused=0;
    v->content=desk_widget_inset(frame,6);
    if(v->content.w>=18) v->content.w-=18;else v->content.w=0;
    unsigned columns=v->content.w/(int)v->cell_width,rows=v->content.h/(int)v->cell_height;
    if(!columns) columns=1;
    if(!rows) rows=1;
    if(columns>DESK_TERM_COLUMNS) columns=DESK_TERM_COLUMNS;
    if(rows>DESK_TERM_ROWS) rows=DESK_TERM_ROWS;
    v->columns=columns;v->rows=rows;desk_terminal_resize(v->terminal,columns,rows);
    struct desk_rect inner=desk_widget_inset(frame,1);
    struct desk_rect track=inner.w>=18?(struct desk_rect){inner.x+inner.w-18,inner.y,18,inner.h}:(struct desk_rect){0};
    unsigned history=desk_terminal_history(v->terminal);
    desk_scrollbar_configure(&v->scroll,track,(history+rows)*v->cell_height,rows*v->cell_height);
    desk_terminal_view_sync(v);
}
unsigned desk_terminal_view_sync(struct desk_terminal_view *v) {
    struct desk_terminal *t=v->terminal;
    if(v->seen_revision==t->revision) return 0;
    unsigned history=desk_terminal_history(t);
    int follow=v->scroll.offset==(int)(v->seen_history*v->cell_height);
    uint64_t lost=t->discarded-v->seen_discarded;
    int64_t offset=v->scroll.offset;
    if(v->seen_alt!=t->alt_active) follow=1;
    else if(lost) offset=lost>4096?0:offset-(int64_t)lost*v->cell_height;
    desk_scrollbar_configure(&v->scroll,v->scroll.track,(history+t->rows)*v->cell_height,t->rows*v->cell_height);
    desk_scrollbar_set(&v->scroll,follow?(int64_t)history*v->cell_height:offset);
    v->anchor=v->caret=0;v->dragging=0;
    v->seen_history=history;v->seen_alt=t->alt_active;v->seen_discarded=t->discarded;v->seen_revision=t->revision;
    return DESK_WIDGET_REDRAW;
}
void desk_terminal_view_follow(struct desk_terminal_view *v) {
    desk_scrollbar_set(&v->scroll,(int64_t)desk_terminal_history(v->terminal)*v->cell_height);
    v->anchor=v->caret=0;v->dragging=0;
}
void desk_terminal_view_select_all(struct desk_terminal_view *v) {
    v->anchor=0;v->caret=(desk_terminal_history(v->terminal)+v->terminal->rows)*v->terminal->columns;
}
unsigned desk_terminal_view_scroll(struct desk_terminal_view *v,int64_t pixels) {
    return desk_scrollbar_scroll(&v->scroll,pixels)&DESK_WIDGET_REDRAW;
}
static unsigned hit(const struct desk_terminal_view *v,int x,int y) {
    int64_t row=((int64_t)y-v->content.y+v->scroll.offset)/(int)v->cell_height;
    int64_t column=((int64_t)x-v->content.x+v->cell_width/2)/(int)v->cell_width;
    unsigned count=desk_terminal_history(v->terminal)+v->rows;
    if(row<0) row=0;
    if(row>=(int64_t)count) return count*v->columns;
    if(column<0) column=0;
    if(column>(int)v->columns) column=v->columns;
    return row*v->columns+column;
}
unsigned desk_terminal_view_event(struct desk_terminal_view *v,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        v->dragging=v->scrollbar_capture=0;desk_scrollbar_event(&v->scroll,e);
        if(e.type==DESK_WIDGET_FOCUS_OUT) v->focused=0;
        return DESK_WIDGET_REDRAW;
    }
    if(!v->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { v->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type!=DESK_WIDGET_POINTER) return 0;
    if(e.pressed) v->scrollbar_capture=desk_widget_contains(v->scroll.track,e.x,e.y);
    if(v->scrollbar_capture) {
        unsigned result=desk_scrollbar_event(&v->scroll,e)&DESK_WIDGET_REDRAW;
        if(!e.down) v->scrollbar_capture=0;
        return result;
    }
    if(e.pressed && e.down && desk_widget_contains(v->content,e.x,e.y)) {
        v->anchor=v->caret=hit(v,e.x,e.y);v->dragging=1;return DESK_WIDGET_REDRAW;
    }
    if(v->dragging) {
        if(e.y<v->content.y) desk_terminal_view_scroll(v,-(int)v->cell_height);
        else if(e.y>=v->content.y+v->content.h) desk_terminal_view_scroll(v,v->cell_height);
        v->caret=hit(v,e.x,e.y);if(!e.down) v->dragging=0;return DESK_WIDGET_REDRAW;
    }
    return 0;
}
static size_t selection(const struct desk_terminal_view *v,char *output) {
    unsigned start=v->anchor<v->caret?v->anchor:v->caret,end=v->anchor>v->caret?v->anchor:v->caret;
    unsigned columns=v->terminal->columns,total=(desk_terminal_history(v->terminal)+v->terminal->rows)*columns;
    if(end>total) end=total;
    size_t size=0;
    while(start<end) {
        unsigned row=start/columns,first=start%columns,last=end<(row+1)*columns?end%columns:columns;
        const struct desk_term_line *l=desk_terminal_line(v->terminal,row);
        unsigned trimmed=last;
        if(last==columns && !l->wrapped) while(trimmed>first && l->cells[trimmed-1].codepoint==' ') trimmed--;
        for(unsigned x=first;x<trimmed;x++) { char bytes[5];unsigned n=encode(l->cells[x].codepoint,bytes);if(output) memcpy(output+size,bytes,n);size+=n; }
        start=(row+1)*columns;
        if(start<end && !l->wrapped) { if(output) output[size]='\n';size++; }
    }
    return size;
}
int64_t desk_terminal_view_copy(const struct desk_terminal_view *v,char *output,size_t capacity) {
    size_t size=selection(v,NULL);if(!output || capacity<=size) return -1;
    selection(v,output);output[size]=0;return size;
}
void desk_terminal_view_draw(struct desk_canvas *c,const struct desk_terminal_view *v,const struct desk_theme *theme,int cursor_phase) {
    struct desk_rect saved=c->clip;desk_clip(c,desk_intersect(saved,v->frame));
    desk_fill(c,v->frame,theme->border);desk_fill(c,desk_widget_inset(v->frame,1),0xf9f7f1);
    struct desk_rect content=desk_intersect(c->clip,v->content);desk_clip(c,content);
    unsigned first=v->scroll.offset/v->cell_height,part=v->scroll.offset%v->cell_height;
    unsigned start=v->anchor<v->caret?v->anchor:v->caret,end=v->anchor>v->caret?v->anchor:v->caret;
    for(unsigned y=0;y<=v->rows;y++) {
        const struct desk_term_line *l=desk_terminal_line(v->terminal,first+y);if(!l) break;
        for(unsigned x=0;x<v->columns;x++) {
            struct desk_term_cell cell=l->cells[x];uint32_t fg=cell.foreground,bg=cell.background;
            if(cell.style&DESK_TERM_REVERSE) { uint32_t swap=fg;fg=bg;bg=swap; }
            unsigned position=(first+y)*v->columns+x;
            if(position>=start && position<end) { fg=theme->text;bg=theme->selected; }
            struct desk_rect r={v->content.x+x*v->cell_width,v->content.y+(int)(y*v->cell_height)-(int)part,v->cell_width,v->cell_height};
            desk_clip(c,desk_intersect(content,r));desk_fill(c,r,bg);
            char bytes[5];encode(cell.codepoint,bytes);desk_text(c,&desk_font_mono_16,r.x,r.y,bytes,fg);
            if(cell.style&DESK_TERM_BOLD) desk_text(c,&desk_font_mono_16,r.x+1,r.y,bytes,fg);
            if(cell.style&DESK_TERM_UNDERLINE) desk_fill(c,(struct desk_rect){r.x,r.y+r.h-3,r.w,1},fg);
        }
    }
    desk_clip(c,content);
    if(v->terminal->cursor_visible && v->scroll.offset==(int)(desk_terminal_history(v->terminal)*v->cell_height) &&
       (!v->focused || cursor_phase)) {
        struct desk_rect r={v->content.x+v->terminal->x*v->cell_width,v->content.y+v->terminal->y*v->cell_height,v->cell_width,v->cell_height};
        if(v->focused) desk_fill(c,(struct desk_rect){r.x,r.y,2,r.h},theme->accent);
        else { desk_fill(c,(struct desk_rect){r.x,r.y,r.w,1},theme->muted);desk_fill(c,(struct desk_rect){r.x,r.y+r.h-1,r.w,1},theme->muted);
            desk_fill(c,(struct desk_rect){r.x,r.y,1,r.h},theme->muted);desk_fill(c,(struct desk_rect){r.x+r.w-1,r.y,1,r.h},theme->muted); }
    }
    desk_clip(c,desk_intersect(saved,v->frame));desk_scrollbar_draw(c,&v->scroll,theme);c->clip=saved;
}
size_t desk_terminal_key(const struct desk_terminal *t,const struct srv_key_event *k,void *output,size_t capacity) {
    if(!output || !(k->flags&SRV_KEY_DOWN_EVENT) || (k->flags&SRV_KEY_CANCEL)) return 0;
    char bytes[32];unsigned length=0,ctrl=(k->modifiers&SRV_KEY_CTRL)!=0,alt=(k->modifiers&SRV_KEY_ALT)!=0;
    unsigned shift=(k->modifiers&SRV_KEY_SHIFT)!=0,modifier=1+shift+2*alt+4*ctrl;
    if(k->modifiers&SRV_KEY_META) return 0;
    char final=0;unsigned number=0;
    switch(k->usage) {
    case SRV_KEY_UP:final='A';break;case SRV_KEY_DOWN:final='B';break;
    case SRV_KEY_RIGHT:final='C';break;case SRV_KEY_LEFT:final='D';break;
    case SRV_KEY_HOME:final='H';break;case SRV_KEY_END:final='F';break;
    case SRV_KEY_INSERT:number=2;break;case SRV_KEY_DELETE:number=3;break;
    case SRV_KEY_PAGEUP:number=5;break;case SRV_KEY_PAGEDOWN:number=6;break;
    default:break;
    }
    if(final || number || (k->usage>=SRV_KEY_F1 && k->usage<=SRV_KEY_F12)) {
        if(k->usage>=SRV_KEY_F1 && k->usage<=SRV_KEY_F4) final='P'+k->usage-SRV_KEY_F1;
        else if(k->usage>=SRV_KEY_F5 && k->usage<=SRV_KEY_F12) {
            static const unsigned function[]={15,17,18,19,20,21,23,24};number=function[k->usage-SRV_KEY_F5];
        }
        bytes[length++]=27;
        bytes[length++]=modifier==1 && final && (t->application_cursor || final>='P')?'O':'[';
        if(number) { if(number>=10) bytes[length++]='0'+number/10;bytes[length++]='0'+number%10; }
        else if(modifier!=1) bytes[length++]='1';
        if(modifier!=1) { bytes[length++]=';';bytes[length++]='0'+modifier; }
        bytes[length++]=number?'~':final;
    } else {
        if(alt) bytes[length++]=27;
        if(k->usage==SRV_KEY_ENTER) bytes[length++]='\r';
        else if(k->usage==SRV_KEY_BACKSPACE) bytes[length++]=127;
        else if(k->usage==SRV_KEY_ESCAPE) bytes[length++]=27;
        else if(k->usage==SRV_KEY_TAB) { if(shift) { bytes[length++]=27;bytes[length++]='[';bytes[length++]='Z'; }else bytes[length++]='\t'; }
        else if(ctrl) {
            if(k->usage>=4 && k->usage<=29) bytes[length++]=k->usage-3;
            else if(k->usage==SRV_KEY_SPACE || k->usage==0x1f) bytes[length++]=0;
            else if(k->usage==0x2f) bytes[length++]=27;
            else if(k->usage==0x31) bytes[length++]=28;
            else if(k->usage==0x30) bytes[length++]=29;
            else if(k->usage==0x23) bytes[length++]=30;
            else if(k->usage==0x2d) bytes[length++]=31;
            else return 0;
        } else if(k->codepoint>=32 && k->codepoint<=0x10ffff && !(k->codepoint>=0xd800 && k->codepoint<=0xdfff))
            length+=encode(k->codepoint,bytes+length);
        else return 0;
    }
    if(length>capacity) return 0;
    memcpy(output,bytes,length);return length;
}
