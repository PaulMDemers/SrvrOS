#define _GNU_SOURCE
#include <assert.h>
#include <errno.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define gmtime_r test_gmtime_r
#define localtime_r test_localtime_r
#define gmtime test_gmtime
#define localtime test_localtime
#define mktime test_mktime
#define tzset test_tzset
#include "../userspace/lib/src/posix_calendar.c"
#undef gmtime_r
#undef localtime_r
#undef gmtime
#undef localtime
#undef mktime
#undef tzset
static unsigned long comparisons;
static uint64_t random_state=715;
static uint64_t random_value(void) { random_state^=random_state<<13;random_state^=random_state>>7;random_state^=random_state<<17;return random_state; }
static void equal(const struct tm *a,const struct tm *b) {
    assert(a->tm_year==b->tm_year && a->tm_mon==b->tm_mon && a->tm_mday==b->tm_mday && a->tm_hour==b->tm_hour && a->tm_min==b->tm_min && a->tm_sec==b->tm_sec && a->tm_wday==b->tm_wday && a->tm_yday==b->tm_yday && a->tm_isdst==b->tm_isdst);comparisons++;
}
static void stamp(time_t seconds) {
    struct tm actual,expected;memset(&actual,0x5a,sizeof(actual));struct tm before=actual;
    errno=0;struct tm *reference=gmtime_r(&seconds,&expected);int reference_error=errno;
    errno=0;struct tm *result=test_gmtime_r(&seconds,&actual);
    assert((result!=NULL)==(reference!=NULL));
    if(result) { equal(&actual,&expected);struct tm roundtrip=actual;assert(test_mktime(&roundtrip)==seconds);equal(&actual,&roundtrip); }
    else { assert(errno==reference_error && errno==EOVERFLOW);assert(!memcmp(&actual,&before,sizeof(actual))); }
}
int main(void) {
    setenv("TZ","UTC",1);tzset();
    int64_t first=srv_civil_days(1600,1,1),last=srv_civil_days(2401,1,1);
    for(int64_t day=first;day<last;day++) stamp(day*86400+45296);
    const time_t edges[]={INT64_MIN,INT64_MAX,-1,0,1,-86400,-86401,253402300799LL,253402300800LL};
    for(unsigned i=0;i<sizeof(edges)/sizeof(*edges);i++) stamp(edges[i]);
    for(int i=0;i<30000;i++) stamp((time_t)(random_value()%150000000000000000ULL)-75000000000000000LL);
    for(int i=0;i<30000;i++) {
        struct tm actual={.tm_year=(int)(random_value()%5000)-3000,.tm_mon=(int)(random_value()%400)-200,
            .tm_mday=(int)(random_value()%200)-100,.tm_hour=(int)(random_value()%200)-100,
            .tm_min=(int)(random_value()%10000)-5000,.tm_sec=(int)(random_value()%10000)-5000,.tm_isdst=(int)(random_value()%3)-1};
        struct tm expected=actual;errno=0;time_t want=mktime(&expected);int error=errno;errno=0;time_t got=test_mktime(&actual);
        assert(got==want);if(want!=(time_t)-1 || !error) equal(&actual,&expected);else assert(errno==error);
    }
    const int extremes[]={INT_MIN,INT_MAX,0};
    for(unsigned y=0;y<3;y++) for(unsigned m=0;m<3;m++) for(unsigned d=0;d<3;d++) {
        struct tm actual={.tm_year=extremes[y],.tm_mon=extremes[m],.tm_mday=extremes[d],.tm_hour=INT_MAX,.tm_min=INT_MIN,.tm_sec=INT_MAX};
        struct tm expected=actual,before=actual;errno=0;time_t want=mktime(&expected);int error=errno;errno=0;time_t got=test_mktime(&actual);
        assert(got==want);if(want!=(time_t)-1 || !error) equal(&actual,&expected);else { assert(errno==error && !memcmp(&actual,&before,sizeof(actual))); }
    }
    struct tm result;time_t zero=0;errno=0;assert(!test_gmtime_r(NULL,&result) && errno==EINVAL);assert(!test_localtime_r(&zero,NULL));assert(test_mktime(NULL)==-1);
    struct tm *shared=test_gmtime(&zero);assert(shared && shared==test_localtime(&zero));
    printf("calendar: PASS %lu UTC comparisons, full 1600–2400 daily range, negative epochs, normalization, DST hints, extreme fields, overflow and output preservation\n",comparisons);
}
