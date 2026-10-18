#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#define strftime srv_test_strftime
#define strftime_l srv_test_strftime_l
#include "../userspace/lib/src/posix_strftime.c"
#undef strftime
#undef strftime_l
int main(void) {
    const char *formats[]={"%Y-%m-%d %H:%M:%S","%a %A %b %B %c %x %X","%C %d %D %e %F %h %I %j %m %M %p %r %R %S %T %u %U %V %w %W %y %Y %%","%G-%V-%u %g %U %W","%_d %-m %0H %04Y"};
    char expected[512],actual[512];unsigned cases=0;
    /* Every calendar day across non-leap centuries, leap years and ISO week-year boundaries. */
    for(time_t date=-2208988800LL;date<4133980800LL;date+=86400) {
        struct tm tm;assert(gmtime_r(&date,&tm));tm.tm_hour=cases%24;tm.tm_min=cases%60;tm.tm_sec=cases%61;
        for(unsigned i=0;i<sizeof(formats)/sizeof(*formats);i++) {
            size_t n=strftime(expected,sizeof(expected),formats[i],&tm);
            size_t m=srv_test_strftime(actual,sizeof(actual),formats[i],&tm);
            if(n!=m || strcmp(expected,actual)) { fprintf(stderr,"format mismatch date=%lld format=%s expected=%s actual=%s\n",(long long)date,formats[i],expected,actual);return 1; }
            cases++;
        }
    }
    struct tm tm={.tm_year=126,.tm_mon=9,.tm_mday=18,.tm_hour=12,.tm_min=34,.tm_isdst=0};
    assert(srv_test_strftime(actual,sizeof(actual),"%z %Z",&tm)==9 && !strcmp(actual,"+0000 UTC"));
    tm.tm_isdst=-1;assert(!srv_test_strftime(actual,sizeof(actual),"%z%Z",&tm) && !actual[0]);
    char guard[18];memset(guard,'!',sizeof(guard));assert(srv_test_strftime(guard+1,17,"%F %R",&tm)==16 && guard[0]=='!' && guard[17]==0);
    memset(guard,'!',sizeof(guard));assert(!srv_test_strftime(guard+1,16,"%F %R",&tm) && guard[0]=='!' && guard[1]==0 && guard[17]=='!');
    assert(!srv_test_strftime(actual,sizeof(actual),"%Q",&tm));assert(!srv_test_strftime(actual,sizeof(actual),"%20F",&tm));assert(!srv_test_strftime(actual,sizeof(actual),"%999999Y",&tm));
    tm.tm_mon=12;assert(!srv_test_strftime(actual,sizeof(actual),"%B",&tm));
    printf("strftime: PASS %u C-locale comparisons, ISO week boundaries, padding, UTC, bounds and invalid formats\n",cases);
}
