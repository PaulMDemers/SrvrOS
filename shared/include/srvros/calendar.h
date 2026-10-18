#ifndef SRVROS_CALENDAR_H
#define SRVROS_CALENDAR_H
#include <stdint.h>
/* Gregorian civil-date algorithms adapted from Howard Hinnant's public-domain
 * formulas: https://howardhinnant.github.io/date_algorithms.html
 * Epoch day zero is 1970-01-01. Months/days are one-based. Callers validate input
 * dates; arithmetic supports the full int32 year range and time_t/86400 days. */
static inline int64_t srv_civil_days(int64_t year,unsigned month,unsigned day) {
    year-=month<=2;
    int64_t era=(year>=0?year:year-399)/400;
    unsigned within=(unsigned)(year-era*400);
    unsigned ordinal=(153*(month>2?month-3:month+9)+2)/5+day-1;
    return era*146097+(int64_t)(within*365+within/4-within/100+ordinal)-719468;
}
static inline void srv_civil_date(int64_t days,int64_t *year,unsigned *month,unsigned *day) {
    days+=719468;
    int64_t era=(days>=0?days:days-146096)/146097;
    unsigned ordinal=(unsigned)(days-era*146097);
    unsigned within=(ordinal-ordinal/1460+ordinal/36524-ordinal/146096)/365;
    int64_t y=(int64_t)within+era*400;
    unsigned day_of_year=ordinal-(365*within+within/4-within/100);
    unsigned march_month=(5*day_of_year+2)/153;
    *day=day_of_year-(153*march_month+2)/5+1;
    *month=march_month<10?march_month+3:march_month-9;
    *year=y+(*month<=2);
}
static inline int srv_civil_leap(int64_t year) { return year%4==0 && (year%100!=0 || year%400==0); }
static inline unsigned srv_civil_month_days(int64_t year,unsigned month) {
    static const unsigned char days[]={31,28,31,30,31,30,31,31,30,31,30,31};
    return month>=1 && month<=12?days[month-1]+(month==2 && srv_civil_leap(year)):0;
}
static inline unsigned srv_civil_weekday(int64_t days) { int n=(int)(days%7)+4;return (unsigned)((n+7)%7); }
#endif
