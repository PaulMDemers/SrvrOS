#define _POSIX_C_SOURCE 200809L
#include <srvros/desk_preferences.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
struct desk_preferences desk_preferences_default(void) {
    return (struct desk_preferences){.text_size=16,.clock_flags=DESK_CLOCK_24H|DESK_CLOCK_DATE,.pointer_speed=3,.pointer_acceleration=1};
}
int desk_preferences_valid(const struct desk_preferences *p) {
    return p && (p->text_size==16 || p->text_size==20) && p->utc_offset_minutes>=-720 &&
        p->utc_offset_minutes<=840 && p->utc_offset_minutes%15==0 &&
        !(p->clock_flags&~(DESK_CLOCK_24H|DESK_CLOCK_DATE)) && p->wallpaper<3 && p->pointer_speed>=1 && p->pointer_speed<=5 && p->pointer_acceleration<=1;
}
int desk_preferences_merge(struct desk_preferences *d,const struct desk_preferences *s,unsigned mask) {
    if(!d || !s || !mask || (mask&~DESK_PREF_ALL)) return -1;
    struct desk_preferences p=*d;
    if(mask&DESK_PREF_TEXT) p.text_size=s->text_size;
    if(mask&DESK_PREF_OFFSET) p.utc_offset_minutes=s->utc_offset_minutes;
    if(mask&DESK_PREF_FORMAT) p.clock_flags=(p.clock_flags&~DESK_CLOCK_24H)|(s->clock_flags&DESK_CLOCK_24H);
    if(mask&DESK_PREF_DATE) p.clock_flags=(p.clock_flags&~DESK_CLOCK_DATE)|(s->clock_flags&DESK_CLOCK_DATE);
    if(mask&DESK_PREF_WALLPAPER) p.wallpaper=s->wallpaper;
    if(mask&DESK_PREF_POINTER_SPEED) p.pointer_speed=s->pointer_speed;
    if(mask&DESK_PREF_POINTER_ACCELERATION) p.pointer_acceleration=s->pointer_acceleration;
    if(!desk_preferences_valid(&p)) return -1;
    *d=p;return 0;
}
int desk_preferences_encode(const struct desk_preferences *p,char *out,size_t capacity) {
    if(!desk_preferences_valid(p) || !out || !capacity) return -1;
    int n=snprintf(out,capacity,"srvros-desktop-v2\ntext_size=%u\nutc_offset_minutes=%d\nclock_flags=%u\nwallpaper=%u\npointer_speed=%u\npointer_acceleration=%u\n",
        p->text_size,p->utc_offset_minutes,p->clock_flags,p->wallpaper,p->pointer_speed,p->pointer_acceleration);
    return n<0 || (size_t)n>=capacity?-1:n;
}
int desk_preferences_decode(struct desk_preferences *out,const void *data,size_t length) {
    static const char header[]="srvros-desktop-v";
    if(!out || !data || length<sizeof(header)+1 || length>256 || memcmp(data,header,sizeof(header)-1)) return -1;
    const char *text=data;size_t at=sizeof(header)-1;unsigned seen=0;
    char version=text[at++];if((version!='1' && version!='2') || text[at++]!='\n') return -1;
    struct desk_preferences p=desk_preferences_default();
    while(at<length) {
        size_t begin=at;while(at<length && text[at]!='=') { if(text[at]=='\n' || !text[at]) return -1;at++; }
        size_t name=at-begin;if(at==length) return -1;at++;
        int negative=at<length && text[at]=='-';if(negative) at++;
        int number=0,digits=0;
        while(at<length && text[at]>='0' && text[at]<='9') {
            if(++digits>4) return -1;
            number=number*10+text[at++]-'0';
        }
        if(!digits || at==length || text[at++]!='\n') return -1;
        if(negative) number=-number;
        unsigned bit=0;
        if(name==9 && !memcmp(text+begin,"text_size",9)) { bit=1;p.text_size=number; }
        else if(name==18 && !memcmp(text+begin,"utc_offset_minutes",18)) { bit=2;p.utc_offset_minutes=number; }
        else if(name==11 && !memcmp(text+begin,"clock_flags",11)) { bit=4;p.clock_flags=number; }
        else if(name==9 && !memcmp(text+begin,"wallpaper",9)) { bit=8;p.wallpaper=number; }
        else if(version=='2' && name==13 && !memcmp(text+begin,"pointer_speed",13)) { bit=16;p.pointer_speed=number; }
        else if(version=='2' && name==20 && !memcmp(text+begin,"pointer_acceleration",20)) { bit=32;p.pointer_acceleration=number; }
        else return -1;
        if(seen&bit) return -1;
        seen|=bit;
    }
    if(seen!=(version=='1'?15u:63u) || !desk_preferences_valid(&p)) return -1;
    *out=p;return 0;
}
int desk_clock_format(char *out,size_t capacity,long utc,const struct desk_preferences *p) {
    if(!out || !capacity || !desk_preferences_valid(p)) return -1;
    if(utc<0 || utc>253402300799L) { snprintf(out,capacity,"Clock unavailable");return -1; }
    time_t local=utc+(long)p->utc_offset_minutes*60;
    if(local<0 || local>253402300799L) { snprintf(out,capacity,"Clock unavailable");return -1; }
    struct tm t;if(!gmtime_r(&local,&t)) return -1;
    char zone[16],clock[24],date[32]="";
    int offset=p->utc_offset_minutes,magnitude=offset<0?-offset:offset;
    if(offset) snprintf(zone,sizeof(zone),"UTC%c%02d:%02d",offset<0?'-':'+',magnitude/60,magnitude%60);
    else strcpy(zone,"UTC");
    if(p->clock_flags&DESK_CLOCK_24H) snprintf(clock,sizeof(clock),"%02d:%02d",t.tm_hour,t.tm_min);
    else snprintf(clock,sizeof(clock),"%d:%02d %s",t.tm_hour%12?t.tm_hour%12:12,t.tm_min,t.tm_hour>=12?"PM":"AM");
    if(p->clock_flags&DESK_CLOCK_DATE) {
        static const char *days[]={"Sun","Mon","Tue","Wed","Thu","Fri","Sat"};
        static const char *months[]={"Jan","Feb","Mar","Apr","May","Jun","Jul","Aug","Sep","Oct","Nov","Dec"};
        snprintf(date,sizeof(date),"%s %s %d  ",days[t.tm_wday],months[t.tm_mon],t.tm_mday);
    }
    int n=snprintf(out,capacity,"%s%s %s",date,clock,zone);
    return n<0 || (size_t)n>=capacity?-1:n;
}
