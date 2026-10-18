#include <srvros/desk_time_model.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
int main(void) {
    const char *valid[]={"1","90","1:30","00:01:30","24:00:00","86400","1440:00","000001"};
    const uint64_t seconds[]={1,90,90,90,86400,86400,86400,1};
    for(unsigned i=0;i<sizeof(valid)/sizeof(*valid);i++) { uint64_t us=0;assert(!desk_duration_parse(valid[i],&us) && us==seconds[i]*1000000); }
    const char *invalid[]={"","0","00:00","-1","+2","1.5","1:",":1","1:60","1:00:60","24:00:01","86401","9999999999999999","1:2:3:4"," 1","1 ","0000001"};
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) { uint64_t us=42;assert(desk_duration_parse(invalid[i],&us)<0 && us==42); }
    struct desk_countdown t={0};assert(!desk_countdown_set(&t,1000000));assert(desk_countdown_start(&t,100));assert(desk_countdown_set(&t,2000000)<0);
    assert(desk_countdown_value(&t,50)==1000000 && desk_countdown_value(&t,500100)==500000);assert(desk_countdown_pause(&t,500100)==1);
    assert(desk_countdown_value(&t,9000000)==500000);assert(desk_countdown_start(&t,9000000));assert(!desk_countdown_update(&t,9499999));
    assert(desk_countdown_update(&t,9500000)==1 && t.finished && !t.running && !t.remaining);assert(!desk_countdown_update(&t,99999999));
    assert(desk_countdown_start(&t,0) && t.remaining==1000000 && !t.finished);assert(desk_countdown_pause(&t,1000000)==2 && t.finished);
    desk_countdown_reset(&t);assert(!t.running && !t.finished && t.remaining==1000000);
    assert(desk_countdown_set(&t,0)<0 && desk_countdown_set(&t,DESK_COUNTDOWN_MAX_US+1)<0);
    struct desk_stopwatch w={0};assert(desk_stopwatch_toggle(&w,100));assert(desk_stopwatch_value(&w,50)==0 && desk_stopwatch_value(&w,110)==10);
    desk_stopwatch_toggle(&w,110);assert(desk_stopwatch_value(&w,999)==10);desk_stopwatch_toggle(&w,1000);assert(desk_stopwatch_value(&w,1100)==110);
    assert(desk_stopwatch_update(&w,UINT64_MAX)==1 && !w.running && w.elapsed==DESK_STOPWATCH_MAX_US && !desk_stopwatch_toggle(&w,0));
    desk_stopwatch_reset(&w);assert(!w.elapsed && !w.running);desk_stopwatch_toggle(&w,UINT64_MAX-10);assert(desk_stopwatch_value(&w,UINT64_MAX)==10);
    char text[32];desk_duration_format(text,sizeof(text),0,1);assert(!strcmp(text,"00:00:00.00"));
    desk_duration_format(text,sizeof(text),UINT64_C(3661999999),1);assert(!strcmp(text,"01:01:01.99"));
    desk_duration_format(text,sizeof(text),DESK_STOPWATCH_MAX_US,0);assert(!strcmp(text,"100:00:00"));
    desk_duration_format(text,4,1000000,0);assert(!text[0]);
    for(unsigned i=1;i<=86400;i++) { desk_duration_format(text,sizeof(text),(uint64_t)i*1000000,0);uint64_t us=0;assert(!desk_duration_parse(text,&us) && us==(uint64_t)i*1000000); }
    puts("time-model: PASS duration syntax/range/roundtrip, exact expiry and one-shot completion, pause/resume/reset, rollback, upper bounds and formatting");
}
