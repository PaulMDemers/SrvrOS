#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../userspace/desktools/system.c"
static uint64_t ticks=500;
static int phase,reads;
long srv_ticks(void) { return ticks; }
void srv_puts(const char *s) { (void)s; }
long srv_proc_list(uint64_t index,struct srv_process_info *p) {
    reads++;
    if(phase==2) return -1;
    if(index>=(phase?2u:3u)) return 0;
    unsigned ids[]={9,2,33};const char *names[]={"zeta","Alpha","beta"};
    *p=(struct srv_process_info){.pid=ids[index],.parent_pid=1,.user_mapped_bytes=(3-index)*4096};
    strcpy(p->name,names[index]);strcpy(p->state,index?"background":"foreground");return index+1;
}
long srv_meminfo(struct srv_meminfo *m) { *m=(struct srv_meminfo){.total_bytes=512*1048576,.free_bytes=300*1048576,.used_bytes=212*1048576};return 0; }
void tool_status(struct tool *t,const char *text) { snprintf(t->status,sizeof(t->status),"%s",text);t->window.dirty=1; }
static struct tool t;
int main(void) {
    t.window.width=760;t.window.height=500;desk_ui_init(&t.ui);tool_system_init(&t);tool_system_layout(&t);desk_ui_layout_changed(&t.ui);
    assert(t.process_count==3 && t.processes[0].pid==2 && t.processes[2].pid==33);
    desk_list_select(&t.table.body,1);assert(t.table.navigation[1].id==9);
    desk_table_sort_indicator(&t.table,4,1);tool_system_action(&t,(struct desk_action){SYSTEM_TABLE,DESK_TABLE_SORT});
    assert(t.processes[0].pid==9 && t.table.body.selected==0);
    int before=reads;tool_system_refresh(&t,0);assert(reads==before);
    ticks+=100;phase=1;tool_system_refresh(&t,0);assert(t.process_count==2 && t.table.body.selected==0);
    tool_system_action(&t,(struct desk_action){SYSTEM_PAUSE,DESK_WIDGET_ACTIVATED});assert(t.system_paused);
    before=reads;ticks+=200;tool_system_refresh(&t,0);assert(reads==before);
    tool_system_action(&t,(struct desk_action){SYSTEM_REFRESH,DESK_WIDGET_ACTIVATED});assert(reads>before);
    phase=2;tool_system_refresh(&t,1);assert(strstr(t.status,"unavailable") && t.process_count==0 && t.table.body.selected==-1);
    phase=0;tool_system_action(&t,(struct desk_action){SYSTEM_PAUSE,DESK_WIDGET_ACTIVATED});assert(!t.system_paused && t.process_count==3);
    static uint32_t pixels[760*500];struct desk_canvas c;assert(!desk_canvas_init(&c,pixels,760,500,760));
    struct desk_rect clip={20,50,700,400};desk_clip(&c,clip);desk_ui_draw(&c,&t.ui,&desk_theme_quiet);tool_system_draw(&t,&c);
    assert(!memcmp(&clip,&c.clip,sizeof(clip)));
    for(int y=0;y<500;y++) for(int x=0;x<760;x++) if(x<20 || x>=720 || y<50 || y>=450) assert(!pixels[y*760+x]);
    puts("system: PASS process sorting, stable selection, refresh cadence, pause/manual refresh, errors and clipped rendering");
}
