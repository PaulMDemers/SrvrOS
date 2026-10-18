#ifndef SRVROS_DESK_CALENDAR_H
#define SRVROS_DESK_CALENDAR_H
#include <srvros/desk_widgets.h>
#include <srvros/key_event.h>
struct desk_date { int year;unsigned month,day; };
struct desk_calendar {
    struct desk_rect frame;
    struct desk_date selected,today;
    int enabled,focused,pressed;
    unsigned first_weekday; /* 0 Sunday through 6 Saturday. */
};
int desk_date_valid(struct desk_date date); /* Gregorian years 1..9999. */
void desk_calendar_init(struct desk_calendar *calendar);
void desk_calendar_configure(struct desk_calendar *calendar,struct desk_rect frame,int enabled,unsigned first_weekday);
int desk_calendar_select(struct desk_calendar *calendar,struct desk_date date);
/* A zero year clears the today indicator. Neither operation reads a clock. */
int desk_calendar_today(struct desk_calendar *calendar,struct desk_date date);
struct desk_rect desk_calendar_cell_rect(const struct desk_calendar *calendar,unsigned cell);
int desk_calendar_cell_date(const struct desk_calendar *calendar,unsigned cell,struct desk_date *date);
unsigned desk_calendar_event(struct desk_calendar *calendar,struct desk_widget_event event);
unsigned desk_calendar_key(struct desk_calendar *calendar,const struct srv_key_event *key);
void desk_calendar_draw(struct desk_canvas *canvas,const struct desk_calendar *calendar,const struct desk_theme *theme);
#endif
