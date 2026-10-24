#include <srvros/desk_menu.h>
#include <string.h>
#include <srvros/desk_text.h>

static int text_valid(const char *s,unsigned capacity) {
    unsigned n=0;
    while(n<capacity && s[n]) { if((unsigned char)s[n]<32 || (unsigned char)s[n]==127) return 0;n++; }
    return n<capacity && desk_text_utf8_valid(s,n);
}
int desk_menu_valid(const struct desk_menu_item *items,unsigned count) {
    if(count>DESK_MENU_ITEMS || (count && !items)) return 0;
    unsigned groups=0,rows=0;
    for(unsigned i=0;i<count;i++) {
        const struct desk_menu_item *p=&items[i];
        if(p->flags&~(DESK_MENU_HEADING|DESK_MENU_SEPARATOR|DESK_MENU_DISABLED|DESK_MENU_CHECKED) ||
           !text_valid(p->label,sizeof(p->label)) || !text_valid(p->shortcut,sizeof(p->shortcut))) return 0;
        if(p->flags&DESK_MENU_HEADING) {
            if(p->flags!=DESK_MENU_HEADING || p->id || !p->label[0] || p->shortcut[0] ||
                (groups && !rows) || ++groups>DESK_MENU_GROUPS) return 0;
            for(unsigned j=0;j<i;j++) if((items[j].flags&DESK_MENU_HEADING) && !strcmp(items[j].label,p->label)) return 0;
            rows=0;
        } else {
            if(!groups || ++rows>32) return 0;
            if(p->flags&DESK_MENU_SEPARATOR) {
                if(p->flags!=DESK_MENU_SEPARATOR || p->id || p->label[0] || p->shortcut[0]) return 0;
            } else {
                if(!p->id || !p->label[0]) return 0;
                for(unsigned j=0;j<i;j++) if(items[j].id==p->id) return 0;
            }
        }
    }
    return !count || rows>0;
}
void desk_menu_cancel(struct desk_menu *m) { m->open=m->selected=-1;m->armed=0;m->scroll=0; }
void desk_menu_init(struct desk_menu *m) { *m=(struct desk_menu){0};desk_menu_cancel(m); }
int desk_menu_configure(struct desk_menu *m,const struct desk_menu_item *items,unsigned count,
    struct desk_rect bar,struct desk_rect bounds,const struct desk_font *font) {
    if(!desk_menu_valid(items,count) || !font || bar.w<0 || bar.h<0 || bounds.w<0 || bounds.h<0 ||
       bar.w>16384 || bar.h>128 || bounds.w>16384 || bounds.h>16384 ||
       bar.x<0 || bar.y<0 || bounds.x<0 || bounds.y<0 || bar.x>16384 || bar.y>16384 || bounds.x>16384 || bounds.y>16384) return -1;
    if(count) memmove(m->items,items,count*sizeof(*items));
    m->count=count;m->groups=0;m->bar=bar;m->bounds=bounds;m->font=font;
    for(unsigned i=0;i<count;i++) if(items[i].flags&DESK_MENU_HEADING) m->headings[m->groups++]=i;
    desk_menu_cancel(m);return 0;
}
static unsigned end(const struct desk_menu *m) {
    return m->open+1<(int)m->groups?m->headings[m->open+1]:m->count;
}
static void reveal(struct desk_menu *m);
int desk_menu_replace(struct desk_menu *m,const struct desk_menu_item *items,unsigned count) {
    if(!desk_menu_valid(items,count)) return -1;
    struct desk_menu_item old[33],selected={0};unsigned old_count=0;int armed=m->armed,scroll=m->scroll;
    if(m->open>=0 && m->open<(int)m->groups) {
        unsigned first=m->headings[m->open];old_count=end(m)-first;
        memcpy(old,m->items+first,old_count*sizeof(old[0]));
        if(m->selected>=0) selected=m->items[m->selected];
    }
    if(desk_menu_configure(m,items,count,m->bar,m->bounds,m->font)) return -1;
    if(!old_count) return 0;
    for(unsigned g=0;g<m->groups;g++) if(!strcmp(m->items[m->headings[g]].label,old[0].label)) {
        m->open=g;unsigned first=m->headings[g],last=end(m);
        if(last-first==old_count && !memcmp(m->items+first,old,old_count*sizeof(old[0]))) { m->armed=armed;m->scroll=scroll; }
        if(selected.id) for(unsigned i=first+1;i<last;i++)
            if(!(m->items[i].flags&DESK_MENU_DISABLED) && !memcmp(&m->items[i],&selected,sizeof(selected))) m->selected=i;
        break;
    }
    if(m->open<0 && m->groups) m->open=0;
    reveal(m);return 0;
}
static int enabled(const struct desk_menu *m,int i) {
    return i>=0 && i<(int)m->count && !(m->items[i].flags&(DESK_MENU_HEADING|DESK_MENU_SEPARATOR|DESK_MENU_DISABLED));
}
struct desk_rect desk_menu_heading(const struct desk_menu *m,unsigned group) {
    if(group>=m->groups) return (struct desk_rect){0};
    int x=m->bar.x;
    for(unsigned g=0;g<group;g++) x+=desk_text_width(m->font,m->items[m->headings[g]].label)+28;
    return desk_intersect(m->bar,(struct desk_rect){x,m->bar.y,desk_text_width(m->font,m->items[m->headings[group]].label)+28,m->bar.h});
}
struct desk_rect desk_menu_popup(const struct desk_menu *m) {
    if(m->open<0 || m->open>=(int)m->groups) return (struct desk_rect){0};
    int width=200,height=8;
    for(unsigned i=m->headings[m->open]+1;i<end(m);i++) {
        int w=desk_text_width(m->font,m->items[i].label)+desk_text_width(m->font,m->items[i].shortcut)+72;
        if(w>width) width=w;
        height+=m->items[i].flags&DESK_MENU_SEPARATOR?9:30;
    }
    if(width>m->bounds.w) width=m->bounds.w;
    if(height>m->bounds.h-m->bar.h) height=m->bounds.h-m->bar.h;
    if(height<0) height=0;
    struct desk_rect h=desk_menu_heading(m,m->open);int x=h.x,y=m->bar.y+m->bar.h;
    if(x+width>m->bounds.x+m->bounds.w) x=m->bounds.x+m->bounds.w-width;
    if(x<m->bounds.x) x=m->bounds.x;
    if(y+height>m->bounds.y+m->bounds.h) y=m->bounds.y+m->bounds.h-height;
    return (struct desk_rect){x,y,width,height};
}
struct desk_rect desk_menu_row(const struct desk_menu *m,unsigned index) {
    struct desk_rect p=desk_menu_popup(m);
    if(m->open<0 || index<=m->headings[m->open] || index>=end(m)) return (struct desk_rect){0};
    int y=p.y+4-m->scroll;
    for(unsigned i=m->headings[m->open]+1;i<index;i++) y+=m->items[i].flags&DESK_MENU_SEPARATOR?9:30;
    return desk_intersect(p,(struct desk_rect){p.x+4,y,p.w>8?p.w-8:0,m->items[index].flags&DESK_MENU_SEPARATOR?9:30});
}
struct desk_menu_result desk_menu_scroll(struct desk_menu *m,int pixels) {
    if(m->open<0)return (struct desk_menu_result){0};
    int height=0;for(unsigned i=m->headings[m->open]+1;i<end(m);i++)height+=m->items[i].flags&DESK_MENU_SEPARATOR?9:30;
    int limit=height-desk_menu_popup(m).h+8;if(limit<0)limit=0;
    if(pixels>4096)pixels=4096;
    if(pixels< -4096)pixels=-4096;
    int before=m->scroll;m->scroll+=pixels;
    if(m->scroll<0)m->scroll=0;
    if(m->scroll>limit)m->scroll=limit;
    m->armed=0;m->selected=-1;
    return (struct desk_menu_result){.consumed=1,.redraw=m->scroll!=before};
}
static void reveal(struct desk_menu *m) {
    if(m->open<0 || m->selected<0)return;
    int y=0;for(unsigned i=m->headings[m->open]+1;i<(unsigned)m->selected;i++)y+=m->items[i].flags&DESK_MENU_SEPARATOR?9:30;
    int visible=desk_menu_popup(m).h-8;
    if(y<m->scroll)m->scroll=y;
    if(y+30>m->scroll+visible)m->scroll=y+30-visible;
    if(m->scroll<0)m->scroll=0;
}
static int step(struct desk_menu *m,int from,int direction) {
    int first=m->headings[m->open]+1,last=end(m),i=from;
    for(int n=0;n<last-first;n++) {
        i+=direction;if(i<first) i=last-1;if(i>=last) i=first;
        if(enabled(m,i)) return i;
    }
    return -1;
}
struct desk_menu_result desk_menu_pointer(struct desk_menu *m,struct desk_widget_event e) {
    int previous=m->open,selection=m->selected;
    struct desk_menu_result r={0};
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        r.consumed=m->open>=0;desk_menu_cancel(m);r.redraw=r.consumed;return r;
    }
    if(e.type!=DESK_WIDGET_POINTER) return r;
    int heading=-1;
    for(unsigned g=0;g<m->groups;g++) if(desk_widget_contains(desk_menu_heading(m,g),e.x,e.y)) { heading=g;break; }
    if(heading>=0) {
        if(e.pressed) {
            if(m->open==heading) desk_menu_cancel(m);
            else { m->open=heading;m->selected=-1;m->scroll=0;m->armed=1; }
        } else if(m->open>=0 && heading!=m->open) { m->open=heading;m->selected=-1;m->scroll=0; }
        if(!e.down) m->armed=0;
        r.consumed=1;
    } else if(m->open>=0) {
        r.consumed=1;int row=-1;
        for(unsigned i=m->headings[m->open]+1;i<end(m);i++)
            if(enabled(m,i) && desk_widget_contains(desk_menu_row(m,i),e.x,e.y)) { row=i;break; }
        m->selected=row;
        if(e.pressed) {
            if(!desk_widget_contains(desk_menu_popup(m),e.x,e.y)) desk_menu_cancel(m);
            else m->armed=1;
        } else if(!e.down && m->armed) {
            if(row>=0) { r.action=m->items[row].id;desk_menu_cancel(m); }
            else m->armed=0;
        }
    }
    r.redraw=previous!=m->open || selection!=m->selected;return r;
}
struct desk_menu_result desk_menu_key(struct desk_menu *m,const struct srv_key_event *k) {
    struct desk_menu_result r={.consumed=m->open>=0};
    if(k->flags&SRV_KEY_CANCEL) { r.redraw=r.consumed;desk_menu_cancel(m);return r; }
    if(!(k->flags&SRV_KEY_DOWN_EVENT)) return r;
    if(m->open<0) {
        if(k->usage==SRV_KEY_F10 && m->groups) { m->open=0;m->selected=step(m,m->headings[0],1);r.consumed=r.redraw=1; }
        return r;
    }
    r.redraw=1;
    if(k->usage==SRV_KEY_ESCAPE || k->usage==SRV_KEY_F10) desk_menu_cancel(m);
    else if(k->usage==SRV_KEY_LEFT || k->usage==SRV_KEY_RIGHT) {
        m->open=(m->open+(k->usage==SRV_KEY_RIGHT?1:m->groups-1))%m->groups;m->scroll=0;
        m->selected=step(m,m->headings[m->open],1);m->armed=0;
    } else if(k->usage==SRV_KEY_UP || k->usage==SRV_KEY_DOWN) {
        m->selected=step(m,m->selected<0?(int)m->headings[m->open]:m->selected,k->usage==SRV_KEY_DOWN?1:-1);
    } else if(k->usage==SRV_KEY_HOME) m->selected=step(m,m->headings[m->open],1);
    else if(k->usage==SRV_KEY_END) m->selected=step(m,end(m),-1);
    else if(k->usage==SRV_KEY_ENTER || k->usage==SRV_KEY_SPACE) {
        if(enabled(m,m->selected)) { r.action=m->items[m->selected].id;desk_menu_cancel(m); }
    } else if(k->codepoint>=32 && k->codepoint<127) {
        int i=m->selected<0?(int)m->headings[m->open]:m->selected;
        for(unsigned n=0;n<end(m)-m->headings[m->open]-1;n++) {
            i=step(m,i,1);if(i<0) break;
            unsigned a=(unsigned char)m->items[i].label[0],b=k->codepoint;
            if(a>='A' && a<='Z') a+=32;
            if(b>='A' && b<='Z') b+=32;
            if(a==b) { m->selected=i;break; }
        }
    } else r.redraw=0;
    reveal(m);return r;
}
void desk_menu_bar_draw(struct desk_canvas *c,const struct desk_menu *m,const struct desk_theme *t) {
    for(unsigned g=0;g<m->groups;g++) {
        struct desk_rect h=desk_menu_heading(m,g);
        if((int)g==m->open) desk_fill(c,h,t->selected);
        desk_label_draw(c,(struct desk_rect){h.x+14,h.y+(h.h-20)/2,h.w>28?h.w-28:0,20},m->font,m->items[m->headings[g]].label,t->text);
    }
}
void desk_menu_popup_draw(struct desk_canvas *c,const struct desk_menu *m,const struct desk_theme *t) {
    if(m->open<0) return;
    struct desk_rect p=desk_menu_popup(m),clip=c->clip;
    desk_fill(c,(struct desk_rect){p.x+3,p.y+4,p.w,p.h},0xb9c2b4);
    desk_fill(c,p,t->border);desk_fill(c,desk_widget_inset(p,1),t->surface);
    desk_clip(c,desk_intersect(clip,p));
    for(unsigned i=m->headings[m->open]+1;i<end(m);i++) {
        struct desk_rect row=desk_menu_row(m,i);const struct desk_menu_item *item=&m->items[i];
        if(item->flags&DESK_MENU_SEPARATOR) { desk_fill(c,(struct desk_rect){row.x+12,row.y+4,row.w>24?row.w-24:0,1},t->border);continue; }
        if((int)i==m->selected) desk_fill(c,row,t->selected);
        uint32_t color=item->flags&DESK_MENU_DISABLED?t->muted:t->text;
        int sw=desk_text_width(m->font,item->shortcut);
        desk_label_draw(c,(struct desk_rect){row.x+24,row.y+5,row.w>sw+48?row.w-sw-48:0,20},m->font,item->label,color);
        desk_label_draw(c,(struct desk_rect){row.x+row.w-sw-12,row.y+5,sw,20},m->font,item->shortcut,t->muted);
        if(item->flags&DESK_MENU_CHECKED) desk_checkmark_draw(c,row.x+7,row.y+(row.h-8)/2,color);
    }
    c->clip=clip;
}
