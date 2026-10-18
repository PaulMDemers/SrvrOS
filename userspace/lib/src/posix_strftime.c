/* C-locale calendar formatting. SrvrOS currently exposes UTC localtime; this
 * formatter does not pretend to load timezone or locale databases. */
#include <time.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
struct time_writer { char *text;size_t capacity,used;int failed; };
static void append_time(struct time_writer *w,const char *text,size_t length) {
    if(w->failed) return;
    if(length>=w->capacity-w->used) { w->failed=1;return; }
    memcpy(w->text+w->used,text,length);w->used+=length;w->text[w->used]=0;
}
static int leap(int64_t year) { return year%4==0 && (year%100!=0 || year%400==0); }
static int64_t floor_century(int64_t year) { return year>=0?year/100:(year-99)/100; }
static int iso_week(const struct tm *tm,int64_t *year) {
    int day=tm->tm_wday?tm->tm_wday:7;
    int week=(tm->tm_yday+11-day)/7;
    int jan1=(tm->tm_wday-tm->tm_yday%7+7)%7;
    if(week<1) {
        (*year)--;jan1=(jan1-1-leap(*year)+7)%7;
        return jan1==4 || (jan1==3 && leap(*year))?53:52;
    }
    int weeks=jan1==4 || (jan1==3 && leap(*year))?53:52;
    if(week>weeks) { (*year)++;return 1; }
    return week;
}
static void format_time(struct time_writer *w,const char *format,const struct tm *tm,unsigned depth) {
    static const char *const weekdays[]={"Sunday","Monday","Tuesday","Wednesday","Thursday","Friday","Saturday"};
    static const char *const months[]={"January","February","March","April","May","June","July","August","September","October","November","December"};
    if(depth>4) { w->failed=1;return; }
    for(const char *p=format;*p && !w->failed;p++) {
        if(*p!='%') { append_time(w,p,1);continue; }
        p++;int width=0,pad=-1;
        if(*p=='_' || *p=='-' || *p=='0') { pad=*p=='_'?' ':*p=='-'?0:'0';p++; }
        while(*p>='0' && *p<='9') { width=width*10+*p++-'0';if(width>32) { w->failed=1;return; } }
        if(*p=='E' || *p=='O') p++;
        if(!*p) { w->failed=1;return; }
        const char *text=NULL,*compound=NULL;size_t length=0;int numeric=0,default_width=2;
        int64_t value=0,year=(int64_t)tm->tm_year+1900;
#define NUMBER(v) do { numeric=1;value=(v); } while(0)
#define REQUIRE(c) do { if(!(c)) { w->failed=1;return; } } while(0)
        switch(*p) {
        case '%': text="%";break;
        case 'n': text="\n";break;
        case 't': text="\t";break;
        case 'a': case 'A': REQUIRE(tm->tm_wday>=0 && tm->tm_wday<7);text=weekdays[tm->tm_wday];if(*p=='a') length=3;break;
        case 'b': case 'B': case 'h': REQUIRE(tm->tm_mon>=0 && tm->tm_mon<12);text=months[tm->tm_mon];if(*p!='B') length=3;break;
        case 'Y': NUMBER(year);default_width=4;break;
        case 'y': NUMBER(year-floor_century(year)*100);break;
        case 'C': NUMBER(floor_century(year));break;
        case 'm': REQUIRE(tm->tm_mon>=0 && tm->tm_mon<12);NUMBER(tm->tm_mon+1);break;
        case 'd': case 'e': REQUIRE(tm->tm_mday>=1 && tm->tm_mday<=31);NUMBER(tm->tm_mday);if(*p=='e' && pad<0) pad=' ';break;
        case 'H': case 'k': REQUIRE(tm->tm_hour>=0 && tm->tm_hour<24);NUMBER(tm->tm_hour);if(*p=='k' && pad<0) pad=' ';break;
        case 'I': case 'l': REQUIRE(tm->tm_hour>=0 && tm->tm_hour<24);NUMBER(tm->tm_hour%12?tm->tm_hour%12:12);if(*p=='l' && pad<0) pad=' ';break;
        case 'M': REQUIRE(tm->tm_min>=0 && tm->tm_min<60);NUMBER(tm->tm_min);break;
        case 'S': REQUIRE(tm->tm_sec>=0 && tm->tm_sec<=60);NUMBER(tm->tm_sec);break;
        case 'p': case 'P': REQUIRE(tm->tm_hour>=0 && tm->tm_hour<24);text=*p=='p'?(tm->tm_hour<12?"AM":"PM"):(tm->tm_hour<12?"am":"pm");break;
        case 'j': REQUIRE(tm->tm_yday>=0 && tm->tm_yday<=365);NUMBER(tm->tm_yday+1);default_width=3;break;
        case 'w': case 'u': REQUIRE(tm->tm_wday>=0 && tm->tm_wday<7);NUMBER(*p=='w'?tm->tm_wday:tm->tm_wday?tm->tm_wday:7);default_width=1;break;
        case 'U': case 'W': REQUIRE(tm->tm_yday>=0 && tm->tm_yday<=365 && tm->tm_wday>=0 && tm->tm_wday<7);
            NUMBER((tm->tm_yday+7-(*p=='U'?tm->tm_wday:(tm->tm_wday+6)%7))/7);break;
        case 'g': case 'G': case 'V': {
            REQUIRE(tm->tm_yday>=0 && tm->tm_yday<=365 && tm->tm_wday>=0 && tm->tm_wday<7);
            int week=iso_week(tm,&year);NUMBER(*p=='V'?week:*p=='g'?year-floor_century(year)*100:year);if(*p=='G') default_width=4;break;
        }
        case 'z': text=tm->tm_isdst<0?"":"+0000";break;
        case 'Z': text=tm->tm_isdst<0?"":"UTC";break;
        case 'F': compound="%Y-%m-%d";break;
        case 'D': case 'x': compound="%m/%d/%y";break;
        case 'T': case 'X': compound="%H:%M:%S";break;
        case 'R': compound="%H:%M";break;
        case 'r': compound="%I:%M:%S %p";break;
        case 'c': compound="%a %b %e %H:%M:%S %Y";break;
        default: w->failed=1;return;
        }
#undef NUMBER
#undef REQUIRE
        if(!numeric && (width || pad!=-1)) { w->failed=1;return; }
        if(compound) format_time(w,compound,tm,depth+1);
        else if(numeric) {
            if(!width) width=default_width;
            if(pad<0) pad='0';
            if(!pad) width=0;
            char buffer[64];int n=snprintf(buffer,sizeof(buffer),pad=='0'?"%0*lld":"%*lld",width,(long long)value);
            if(n<0 || (size_t)n>=sizeof(buffer)) { w->failed=1;return; }append_time(w,buffer,n);
        } else if(text) append_time(w,text,length?length:strlen(text));
    }
}
size_t strftime(char *s,size_t max,const char *format,const struct tm *tm) {
    if(!s || !format || !tm || !max) return 0;
    struct time_writer writer={s,max,0,0};s[0]=0;format_time(&writer,format,tm,0);
    if(writer.failed) { s[0]=0;return 0; }return writer.used;
}
size_t strftime_l(char *s,size_t max,const char *format,const struct tm *tm,locale_t locale) {
    (void)locale;return strftime(s,max,format,tm);
}
