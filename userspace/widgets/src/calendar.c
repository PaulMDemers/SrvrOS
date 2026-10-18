#include <srvros/desk_calendar.h>
#include <srvros/calendar.h>
#include <stdio.h>
#include <string.h>
static int same(struct desk_date a,struct desk_date b) { return a.year==b.year && a.month==b.month && a.day==b.day; }
int desk_date_valid(struct desk_date d) { return d.year>=1 && d.year<=9999 && d.month>=1 && d.month<=12 && d.day>=1 && d.day<=srv_civil_month_days(d.year,d.month); }
void desk_calendar_init(struct desk_calendar *c) { *c=(struct desk_calendar){.selected={1970,1,1},.pressed=-1}; }
void desk_calendar_configure(struct desk_calendar *c,struct desk_rect r,int enabled,unsigned first_weekday) {
    if(r.x<0 || r.y<0 || r.w<0 || r.h<0 || r.x>16384 || r.y>16384 || r.w>16384 || r.h>16384 || first_weekday>6) return;
    if(memcmp(&r,&c->frame,sizeof(r)) || first_weekday!=c->first_weekday || !enabled) c->pressed=-1;
    c->frame=r;c->first_weekday=first_weekday;c->enabled=enabled && r.w>=140 && r.h>=196;
    if(!c->enabled) c->focused=0;
}
int desk_calendar_select(struct desk_calendar *c,struct desk_date d) {
    if(!desk_date_valid(d)) return -1;
    if(same(c->selected,d)) return 0;
    c->selected=d;c->pressed=-1;return 1;
}
int desk_calendar_today(struct desk_calendar *c,struct desk_date d) {
    if(!d.year) d=(struct desk_date){0};else if(!desk_date_valid(d)) return -1;
    if(same(c->today,d)) return 0;
    c->today=d;return 1;
}
struct desk_rect desk_calendar_cell_rect(const struct desk_calendar *c,unsigned cell) {
    struct desk_rect r=c->frame;if(cell>=42 || r.w<14 || r.h<64) return (struct desk_rect){0};
    int x=cell%7,y=cell/7,width=r.w-2,height=r.h-58;
    return (struct desk_rect){r.x+1+width*x/7,r.y+57+height*y/6,width*(x+1)/7-width*x/7,height*(y+1)/6-height*y/6};
}
int desk_calendar_cell_date(const struct desk_calendar *c,unsigned cell,struct desk_date *date) {
    if(cell>=42 || !date || !desk_date_valid(c->selected)) return -1;
    int64_t first=srv_civil_days(c->selected.year,c->selected.month,1);
    unsigned offset=(srv_civil_weekday(first)+7-c->first_weekday)%7;
    int64_t year;unsigned month,day;srv_civil_date(first-(int)offset+cell,&year,&month,&day);
    if(year<1 || year>9999) return -1;
    *date=(struct desk_date){(int)year,month,day};return 0;
}
static struct desk_rect arrow(const struct desk_calendar *c,int next) {
    struct desk_rect r=c->frame;return desk_intersect(r,(struct desk_rect){next?r.x+r.w-31:r.x+3,r.y+3,28,28});
}
static unsigned choose(struct desk_calendar *c,struct desk_date date) {
    int changed=desk_calendar_select(c,date);return changed>0?DESK_WIDGET_CHANGED|DESK_WIDGET_REDRAW:0;
}
static unsigned month_move(struct desk_calendar *c,int delta) {
    int month=(c->selected.year-1)*12+(int)c->selected.month-1+delta;
    if(month<0 || month>=9999*12) return 0;
    struct desk_date d={month/12+1,(unsigned)(month%12+1),c->selected.day};
    unsigned limit=srv_civil_month_days(d.year,d.month);if(d.day>limit) d.day=limit;return choose(c,d);
}
static unsigned day_move(struct desk_calendar *c,int delta) {
    int64_t days=srv_civil_days(c->selected.year,c->selected.month,c->selected.day)+delta,year;
    unsigned month,day;srv_civil_date(days,&year,&month,&day);
    if(year<1 || year>9999) return 0;
    return choose(c,(struct desk_date){(int)year,month,day});
}
static int hit(const struct desk_calendar *c,int x,int y) {
    if(desk_widget_contains(arrow(c,0),x,y)) return 42;
    if(desk_widget_contains(arrow(c,1),x,y)) return 43;
    for(unsigned i=0;i<42;i++) if(desk_widget_contains(desk_calendar_cell_rect(c,i),x,y)) {
        struct desk_date date;return !desk_calendar_cell_date(c,i,&date)?(int)i:-1;
    }
    return -1;
}
unsigned desk_calendar_event(struct desk_calendar *c,struct desk_widget_event e) {
    if(e.type==DESK_WIDGET_CANCEL || e.type==DESK_WIDGET_FOCUS_OUT) {
        int changed=c->pressed>=0 || (e.type==DESK_WIDGET_FOCUS_OUT && c->focused);c->pressed=-1;
        if(e.type==DESK_WIDGET_FOCUS_OUT) c->focused=0;
        return changed?DESK_WIDGET_REDRAW:0;
    }
    if(!c->enabled) return 0;
    if(e.type==DESK_WIDGET_FOCUS_IN) { c->focused=1;return DESK_WIDGET_REDRAW; }
    if(e.type==DESK_WIDGET_ACTIVATE) return c->focused?DESK_WIDGET_ACTIVATED:0;
    if(e.type!=DESK_WIDGET_POINTER) return 0;
    int target=hit(c,e.x,e.y);
    if(e.pressed) { c->pressed=e.down?target:-1;return DESK_WIDGET_REDRAW; }
    if(!e.down && c->pressed>=0) {
        int pressed=c->pressed;c->pressed=-1;
        if(target!=pressed) return DESK_WIDGET_REDRAW;
        if(target>=42) return DESK_WIDGET_REDRAW|month_move(c,target==42?-1:1);
        struct desk_date date;if(desk_calendar_cell_date(c,target,&date)) return DESK_WIDGET_REDRAW;
        return DESK_WIDGET_REDRAW|DESK_WIDGET_ACTIVATED|choose(c,date);
    }
    return 0;
}
unsigned desk_calendar_key(struct desk_calendar *c,const struct srv_key_event *k) {
    if(k->flags&SRV_KEY_CANCEL) return desk_calendar_event(c,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});
    if(!c->enabled || !c->focused || !(k->flags&SRV_KEY_DOWN_EVENT) || (k->modifiers&(SRV_KEY_ALT|SRV_KEY_META))) return 0;
    unsigned flags=c->pressed>=0?DESK_WIDGET_REDRAW:0;c->pressed=-1;
    if(k->modifiers&SRV_KEY_CTRL) return flags|(k->usage==SRV_KEY_HOME && desk_date_valid(c->today)?choose(c,c->today):0);
    switch(k->usage) {
    case SRV_KEY_LEFT: return flags|day_move(c,-1);
    case SRV_KEY_RIGHT: return flags|day_move(c,1);
    case SRV_KEY_UP: return flags|day_move(c,-7);
    case SRV_KEY_DOWN: return flags|day_move(c,7);
    case SRV_KEY_PAGEUP: return flags|month_move(c,k->modifiers&SRV_KEY_SHIFT?-12:-1);
    case SRV_KEY_PAGEDOWN: return flags|month_move(c,k->modifiers&SRV_KEY_SHIFT?12:1);
    case SRV_KEY_HOME: { struct desk_date d=c->selected;d.day=1;return flags|choose(c,d); }
    case SRV_KEY_END: { struct desk_date d=c->selected;d.day=srv_civil_month_days(d.year,d.month);return flags|choose(c,d); }
    case SRV_KEY_ENTER: case SRV_KEY_SPACE: return flags|(!(k->flags&SRV_KEY_REPEAT)?DESK_WIDGET_ACTIVATED:0);
    default: return flags;
    }
}
void desk_calendar_draw(struct desk_canvas *canvas,const struct desk_calendar *c,const struct desk_theme *theme) {
    static const char *months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    static const char *week[]={"Su","Mo","Tu","We","Th","Fr","Sa"};
    struct desk_rect old=canvas->clip,r=c->frame;if(r.w<=0 || r.h<=0 || !desk_date_valid(c->selected)) return;
    desk_clip(canvas,desk_intersect(old,r));desk_fill(canvas,r,c->focused?theme->accent:theme->border);desk_fill(canvas,desk_widget_inset(r,1),theme->surface);
    char title[32];snprintf(title,sizeof(title),"%s %d",months[c->selected.month-1],c->selected.year);
    int width=desk_text_width(theme->body,title),available=r.w>68?r.w-68:0;if(width>available) width=available;
    desk_label_draw(canvas,(struct desk_rect){r.x+(r.w-width)/2,r.y+7,width,24},theme->body,title,theme->text);
    for(int next=0;next<2;next++) {
        struct desk_rect a=arrow(c,next);if(c->pressed==42+next) desk_fill(canvas,a,theme->selected);
        desk_label_draw(canvas,(struct desk_rect){a.x+8,a.y+3,16,24},theme->body,next?">":"<",c->enabled?theme->text:theme->muted);
    }
    for(unsigned col=0;col<7;col++) {
        struct desk_rect cell=desk_calendar_cell_rect(c,col);int width=desk_text_width(theme->body,week[(col+c->first_weekday)%7]);
        desk_label_draw(canvas,(struct desk_rect){cell.x+(cell.w-width)/2,r.y+34,width,22},theme->body,week[(col+c->first_weekday)%7],theme->muted);
    }
    for(unsigned i=0;i<42;i++) {
        struct desk_date date;if(desk_calendar_cell_date(c,i,&date)) continue;
        struct desk_rect cell=desk_calendar_cell_rect(c,i),inner=desk_widget_inset(cell,2);
        if(same(date,c->selected) || c->pressed==(int)i) desk_fill(canvas,inner,theme->selected);
        if(same(date,c->today)) {
            desk_fill(canvas,(struct desk_rect){inner.x,inner.y,inner.w,1},theme->accent);desk_fill(canvas,(struct desk_rect){inner.x,inner.y+inner.h-1,inner.w,1},theme->accent);
            desk_fill(canvas,(struct desk_rect){inner.x,inner.y,1,inner.h},theme->accent);desk_fill(canvas,(struct desk_rect){inner.x+inner.w-1,inner.y,1,inner.h},theme->accent);
        }
        char day[3];snprintf(day,sizeof(day),"%u",date.day);int width=desk_text_width(theme->body,day);
        desk_label_draw(canvas,(struct desk_rect){cell.x+(cell.w-width)/2,cell.y+(cell.h-theme->body->line_height)/2,width,theme->body->line_height},theme->body,day,c->enabled && date.month==c->selected.month?theme->text:theme->muted);
    }
    desk_clip(canvas,old);
}
