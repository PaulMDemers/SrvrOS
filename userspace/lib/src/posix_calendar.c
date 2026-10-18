#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <time.h>
#include <srvros/calendar.h>
/* SrvrOS libc's local zone is UTC. Desktop fixed offsets are presentation state,
 * not a process-global TZ database. Conversion does no allocation or year loop. */
struct tm *gmtime_r(const time_t *timer,struct tm *result) {
    if(!timer || !result) { errno=EINVAL;return NULL; }
    int64_t seconds=*timer,days=seconds/86400,remaining=seconds%86400;
    if(remaining<0) { days--;remaining+=86400; }
    int64_t year;unsigned month,day;srv_civil_date(days,&year,&month,&day);
    if(year-1900<INT_MIN || year-1900>INT_MAX) { errno=EOVERFLOW;return NULL; }
    struct tm converted={.tm_year=(int)(year-1900),.tm_mon=(int)month-1,.tm_mday=(int)day,
        .tm_hour=(int)(remaining/3600),.tm_min=(int)(remaining/60%60),.tm_sec=(int)(remaining%60),
        .tm_wday=(int)srv_civil_weekday(days),.tm_yday=(int)(days-srv_civil_days(year,1,1)),.tm_isdst=0};
    *result=converted;return result;
}
struct tm *localtime_r(const time_t *timer,struct tm *result) { return gmtime_r(timer,result); }
static struct tm calendar_result;
struct tm *gmtime(const time_t *timer) { return gmtime_r(timer,&calendar_result); }
struct tm *localtime(const time_t *timer) { return localtime_r(timer,&calendar_result); }
void tzset(void) { }
time_t mktime(struct tm *value) {
    if(!value) { errno=EINVAL;return (time_t)-1; }
    int64_t year=(int64_t)value->tm_year+1900,month=value->tm_mon;
    int64_t extra=month/12,remainder=month%12;if(remainder<0) { extra--;remainder+=12; }
    year+=extra;
    int64_t days=srv_civil_days(year,(unsigned)remainder+1,1)+(int64_t)value->tm_mday-1;
    /* All inputs are int, so these widened products and their sum fit int64. */
    time_t seconds=days*86400+(int64_t)value->tm_hour*3600+(int64_t)value->tm_min*60+value->tm_sec;
    if(value->tm_isdst>0) seconds-=3600; /* Normalize a supplied DST hint into UTC. */
    struct tm normalized;if(!gmtime_r(&seconds,&normalized)) return (time_t)-1;
    *value=normalized;return seconds;
}
