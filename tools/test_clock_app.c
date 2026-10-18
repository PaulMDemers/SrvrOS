#define main clock_main
#include "../userspace/deskclock/deskclock.c"
#undef main
#include <assert.h>
static uint64_t ticks=10;static int canceled;
long srv_ticks(void) { return ticks; }
void srv_puts(const char *text) { (void)text; }
int desk_app_cancel_close(struct desk_app *window) { (void)window;canceled++;return 0; }
int main(void) {
    initialize();app.window.width=560;app.window.height=440;app.window.focused=1;app.window.server=42;layout();
    assert(timer_page() && app.duration.enabled && !app.reset.enabled && !app.lap.enabled);
    desk_text_buffer_set(&app.duration_text,"invalid",7);toggle();assert(!app.timer.running && strstr(app.status,"1–86400"));
    desk_text_buffer_set(&app.duration_text,"2",1);update(100);toggle();assert(app.timer.running && !app.duration.enabled && app.timer.duration==2000000);
    update(1000100);toggle();assert(!app.timer.running && app.timer.remaining==1000000 && app.duration.enabled);
    update(9999999);toggle();assert(app.timer.running);update(10999998);assert(app.timer.running && !app.notify.busy);
    update(10999999);assert(app.timer.finished && !app.timer.running && app.notify.busy && !strcmp(app.notify.text.title,"Timer finished"));
    uint64_t serial=app.notify.serial;update(99999999);assert(app.notify.serial==serial);app.notify.busy=0;
    reset();assert(!app.timer.finished && app.timer.remaining==2000000);copy_time(0);assert(app.clipboard.mode && !strcmp((char *)app.clipboard_data,"00:00:02"));app.clipboard.mode=0;clipboard_finish(1);
    page(CLOCK_STOPWATCH);assert(!timer_page() && !app.duration.enabled && !app.reset.enabled);
    update(100000000);toggle();assert(app.watch.running);update(101234567);lap();assert(app.lap_count==1 && app.laps[0].total==1234567 && app.laps[0].split==1234567);
    update(102345678);lap();assert(app.lap_count==2 && app.laps[1].split==1111111);
    copy_time(1);assert(strstr((char *)app.clipboard_data,"Lap\tElapsed\tSplit\n1\t00:01.23\t00:01.23\n2\t00:02.34\t00:01.11\n"));app.clipboard.mode=0;clipboard_finish(1);
    reset();assert(app.watch.running && app.lap_count==2);request_close();assert(app.modal && !app.ui.active && !app.quit);
    dialog_action((struct desk_action){CLOCK_KEEP_OPEN,DESK_WIDGET_ACTIVATED});assert(!app.modal && canceled==1 && app.watch.running);
    for(unsigned i=0;i<70;i++) { update(103000000+i*100000);lap(); }
    assert(app.lap_count==64 && app.lap_serial==72 && app.laps[0].id==9 && app.laps[63].id==72);
    toggle();uint64_t paused=app.watch.elapsed;update(500000000);assert(desk_stopwatch_value(&app.watch,app.now)==paused && app.reset.enabled);
    page(CLOCK_TIMER);desk_ui_focus(&app.ui,CLOCK_DURATION);desk_text_buffer_set(&app.duration_text,"1",1);edit_clipboard(DESK_EDIT_PASTE);assert(app.clip_action==DESK_EDIT_PASTE);
    page(CLOCK_STOPWATCH);page(CLOCK_TIMER);strcpy((char *)app.clipboard_data,"9");app.clipboard.length=1;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.duration_text.data,"1"));
    static uint32_t full[980*660],tiled[980*660];const int sizes[][2]={{560,440},{280,240},{400,320},{960,640}};
    for(unsigned mode=0;mode<4;mode++) for(unsigned p=0;p<2;p++) {
        app.window.width=sizes[mode][0];app.window.height=sizes[mode][1];page(p?CLOCK_STOPWATCH:CLOCK_TIMER);layout();
        assert(app.toggle.frame.x>=12 && app.copy.frame.x+app.copy.frame.w<=sizes[mode][0]-12);
        struct desk_canvas a,b;memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,sizes[mode][0],sizes[mode][1],980);desk_canvas_init(&b,tiled,sizes[mode][0],sizes[mode][1],980);draw(&a);
        for(int y=0;y<sizes[mode][1];y+=53) for(int x=0;x<sizes[mode][0];x+=73) { desk_clip(&b,(struct desk_rect){x,y,73,53});draw(&b); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=(unsigned)sizes[mode][0] || y>=(unsigned)sizes[mode][1]) assert(!full[y*980+x]);
    }
    page(CLOCK_STOPWATCH);reset();assert(!app.lap_count && !app.watch.elapsed);request_close();assert(app.quit);
    puts("clock-app: PASS duration errors, pause/resume/exact finish/one notice, clipboard snapshots/laps, 64-lap eviction, close cancellation, stale clipboard isolation, compact/tiled layouts");
}
