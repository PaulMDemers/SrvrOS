#define _GNU_SOURCE
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <srvros/desk_ui.h>
#include <srvros/calendar.h>
static struct srv_key_event key(unsigned usage,unsigned mods) { return (struct srv_key_event){.usage=usage,.modifiers=mods,.flags=SRV_KEY_DOWN_EVENT}; }
static void date_is(const struct desk_calendar *c,int year,unsigned month,unsigned day) { assert(c->selected.year==year && c->selected.month==month && c->selected.day==day); }
int main(void) {
    struct desk_calendar c;desk_calendar_init(&c);desk_calendar_configure(&c,(struct desk_rect){10,10,328,288},1,0);
    assert(desk_date_valid((struct desk_date){2000,2,29}) && !desk_date_valid((struct desk_date){1900,2,29}));
    assert(!desk_date_valid((struct desk_date){0,1,1}) && !desk_date_valid((struct desk_date){10000,1,1}));
    unsigned count=0;
    for(int year=1600;year<=2400;year++) for(unsigned month=1;month<=12;month++) for(unsigned first=0;first<7;first++) {
        desk_calendar_select(&c,(struct desk_date){year,month,15});desk_calendar_configure(&c,c.frame,1,first);
        struct tm first_date={.tm_year=year-1900,.tm_mon=(int)month-1,.tm_mday=1};time_t seconds=timegm(&first_date);
        seconds-=(first_date.tm_wday+7-first)%7*86400;
        for(unsigned cell=0;cell<42;cell++,seconds+=86400) {
            struct tm expected;gmtime_r(&seconds,&expected);struct desk_date actual;assert(!desk_calendar_cell_date(&c,cell,&actual));
            assert(actual.year==expected.tm_year+1900 && actual.month==(unsigned)expected.tm_mon+1 && actual.day==(unsigned)expected.tm_mday);count++;
        }
    }
    desk_calendar_configure(&c,c.frame,1,0);desk_calendar_select(&c,(struct desk_date){2024,1,31});desk_calendar_event(&c,(struct desk_widget_event){.type=DESK_WIDGET_FOCUS_IN});
    struct srv_key_event down=key(SRV_KEY_PAGEDOWN,0);assert(desk_calendar_key(&c,&down)&DESK_WIDGET_CHANGED);date_is(&c,2024,2,29);
    down.modifiers=SRV_KEY_SHIFT;desk_calendar_key(&c,&down);date_is(&c,2025,2,28);
    struct srv_key_event right=key(SRV_KEY_RIGHT,0);desk_calendar_key(&c,&right);date_is(&c,2025,3,1);
    struct srv_key_event left=key(SRV_KEY_LEFT,0);desk_calendar_key(&c,&left);date_is(&c,2025,2,28);
    desk_calendar_today(&c,(struct desk_date){2026,10,18});struct srv_key_event home=key(SRV_KEY_HOME,SRV_KEY_CTRL);desk_calendar_key(&c,&home);date_is(&c,2026,10,18);
    desk_calendar_select(&c,(struct desk_date){1,1,1});desk_calendar_key(&c,&left);date_is(&c,1,1,1);struct desk_date invalid;assert(desk_calendar_cell_date(&c,0,&invalid)<0);
    desk_calendar_select(&c,(struct desk_date){9999,12,31});desk_calendar_key(&c,&right);date_is(&c,9999,12,31);
    desk_calendar_select(&c,(struct desk_date){2026,10,18});unsigned cell=0;struct desk_date selected;
    for(;cell<42;cell++) { desk_calendar_cell_date(&c,cell,&selected);if(selected.month==10 && selected.day==21) break; }assert(cell<42);
    struct desk_rect r=desk_calendar_cell_rect(&c,cell);struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=r.x+r.w/2,.y=r.y+r.h/2,.down=1,.pressed=1};
    desk_calendar_event(&c,e);e.pressed=e.down=0;assert(desk_calendar_event(&c,e)&DESK_WIDGET_ACTIVATED);date_is(&c,2026,10,21);
    e.pressed=e.down=1;desk_calendar_event(&c,e);desk_calendar_event(&c,(struct desk_widget_event){.type=DESK_WIDGET_CANCEL});e.pressed=e.down=0;assert(!(desk_calendar_event(&c,e)&DESK_WIDGET_ACTIVATED));
    e.pressed=e.down=1;desk_calendar_event(&c,e);desk_calendar_configure(&c,(struct desk_rect){10,10,320,280},1,0);e.pressed=e.down=0;assert(!(desk_calendar_event(&c,e)&DESK_WIDGET_ACTIVATED));
    struct desk_ui ui;struct desk_button button={0};desk_ui_init(&ui);assert(!desk_ui_add(&ui,1,DESK_CONTROL_CALENDAR,&c,NULL));assert(!desk_ui_add(&ui,2,DESK_CONTROL_BUTTON,&button,"Today"));
    desk_button_configure(&button,(struct desk_rect){10,310,100,28},1);desk_ui_focus(&ui,1);
    struct srv_key_event tab=key(SRV_KEY_TAB,0);desk_ui_key(&ui,&tab);assert(button.focused && !c.focused);tab.modifiers=SRV_KEY_SHIFT;desk_ui_key(&ui,&tab);assert(c.focused && !button.focused);
    desk_ui_window_focus(&ui,0);assert(!c.focused);desk_ui_window_focus(&ui,1);assert(c.focused);
    static uint32_t a[360*360],b[360*360];struct desk_canvas full,tiles;desk_canvas_init(&full,a,360,360,360);desk_canvas_init(&tiles,b,360,360,360);
    desk_calendar_draw(&full,&c,&desk_theme_quiet);
    for(int y=0;y<360;y+=17) for(int x=0;x<360;x+=19) { desk_clip(&tiles,(struct desk_rect){x,y,19,17});desk_calendar_draw(&tiles,&c,&desk_theme_quiet); }
    assert(!memcmp(a,b,sizeof(a)));
    for(int y=0;y<360;y++) for(int x=0;x<360;x++) if(x<10 || x>=330 || y<10 || y>=290) assert(!a[y*360+x]);
    desk_calendar_configure(&c,(struct desk_rect){0,0,1,1},1,0);assert(!c.enabled);desk_calendar_draw(&full,&c,&desk_theme_quiet);
    printf("calendar-widget: PASS %u grid/reference dates, leap/boundary navigation, today, activation/cancel/resize, focus routing and tiled/clipped drawing\n",count);
}
