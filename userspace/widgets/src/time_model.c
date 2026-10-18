#include <srvros/desk_time_model.h>
/* Formatting is kept local so the model remains independent of libc I/O. */
int desk_duration_parse(const char *text,uint64_t *duration_us) {
    if(!text || !*text || !duration_us) return -1;
    uint64_t values[3]={0};unsigned count=0,digits=0;
    for(const char *p=text;;p++) {
        if(*p>='0' && *p<='9') {
            if(values[count]>86400/10 || values[count]*10+(unsigned)(*p-'0')>86400) return -1;
            values[count]=values[count]*10+(unsigned)(*p-'0');if(++digits>6) return -1;
        } else if(*p==':' || !*p) {
            if(!digits || (count && values[count]>=60)) return -1;
            if(!*p) break;
            if(count==2) return -1;
            count++;digits=0;
        } else return -1;
    }
    uint64_t seconds=values[0];for(unsigned i=1;i<=count;i++) seconds=seconds*60+values[i];
    if(!seconds || seconds>86400) return -1;
    *duration_us=seconds*UINT64_C(1000000);return 0;
}
void desk_duration_format(char *text,size_t capacity,uint64_t duration_us,int centiseconds) {
    if(!text || !capacity) return;
    if(duration_us>DESK_STOPWATCH_MAX_US) duration_us=DESK_STOPWATCH_MAX_US;
    uint64_t hundredths=duration_us/10000,seconds=hundredths/100,hours=seconds/3600;unsigned minute=seconds/60%60,second=seconds%60;
    char result[16];unsigned at=0;
    if(hours>=100) result[at++]='1';
    result[at++]='0'+hours/10%10;result[at++]='0'+hours%10;result[at++]=':';
    result[at++]='0'+minute/10;result[at++]='0'+minute%10;result[at++]=':';
    result[at++]='0'+second/10;result[at++]='0'+second%10;
    if(centiseconds) { result[at++]='.';result[at++]='0'+hundredths/10%10;result[at++]='0'+hundredths%10; }
    if(at>=capacity) { text[0]=0;return; }
    for(unsigned i=0;i<at;i++) text[i]=result[i];
    text[at]=0;
}
static uint64_t elapsed(uint64_t now,uint64_t anchor) { return now>=anchor?now-anchor:0; }
int desk_countdown_set(struct desk_countdown *t,uint64_t duration) {
    if(!duration || duration>DESK_COUNTDOWN_MAX_US || t->running) return -1;
    *t=(struct desk_countdown){.duration=duration,.remaining=duration};return 0;
}
uint64_t desk_countdown_value(const struct desk_countdown *t,uint64_t now) {
    uint64_t passed=t->running?elapsed(now,t->anchor):0;return passed>=t->remaining?0:t->remaining-passed;
}
int desk_countdown_start(struct desk_countdown *t,uint64_t now) {
    if(t->running || !t->duration || t->duration>DESK_COUNTDOWN_MAX_US) return 0;
    if(!t->remaining) t->remaining=t->duration;
    t->anchor=now;t->running=1;t->finished=0;return 1;
}
int desk_countdown_update(struct desk_countdown *t,uint64_t now) {
    if(!t->running || desk_countdown_value(t,now)) return 0;
    t->remaining=0;t->running=0;t->finished=1;return 1;
}
int desk_countdown_pause(struct desk_countdown *t,uint64_t now) {
    if(desk_countdown_update(t,now)) return 2;
    if(!t->running) return 0;
    t->remaining=desk_countdown_value(t,now);t->running=0;return 1;
}
void desk_countdown_reset(struct desk_countdown *t) { t->remaining=t->duration;t->running=t->finished=0;t->anchor=0; }
uint64_t desk_stopwatch_value(const struct desk_stopwatch *w,uint64_t now) {
    uint64_t delta=w->running?elapsed(now,w->anchor):0;
    if(w->elapsed>=DESK_STOPWATCH_MAX_US || delta>=DESK_STOPWATCH_MAX_US-w->elapsed) return DESK_STOPWATCH_MAX_US;
    return w->elapsed+delta;
}
int desk_stopwatch_toggle(struct desk_stopwatch *w,uint64_t now) {
    if(w->running) { w->elapsed=desk_stopwatch_value(w,now);w->running=0;return 1; }
    if(w->elapsed>=DESK_STOPWATCH_MAX_US) return 0;
    w->anchor=now;w->running=1;return 1;
}
void desk_stopwatch_reset(struct desk_stopwatch *w) { *w=(struct desk_stopwatch){0}; }
int desk_stopwatch_update(struct desk_stopwatch *w,uint64_t now) {
    if(!w->running || desk_stopwatch_value(w,now)<DESK_STOPWATCH_MAX_US) return 0;
    w->elapsed=DESK_STOPWATCH_MAX_US;w->running=0;return 1;
}
