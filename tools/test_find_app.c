#define main find_main
#include "../userspace/deskfind/deskfind.c"
#undef main
#include <assert.h>
static int started,poll_result,spawn_fail,launched;static unsigned launch_app;static char launch_path[160],child_root[160],child_query[96];
static struct { struct desk_search_header header;struct desk_search_entry entries[3]; } payload;
long srv_ticks(void) { return 42; }
void srv_puts(const char *s) { (void)s; }
int desk_app_launch(struct desk_app *window,unsigned application,const char *path) { assert(window==&app.window);launched++;launch_app=application;strcpy(launch_path,path);return 0; }
int desk_command_job_init(struct desk_command_job *j,char *o,size_t n) { *j=(struct desk_command_job){.reader=-1,.output=o,.capacity=n};o[0]=0;return 0; }
int desk_command_job_start(struct desk_command_job *j,const char *path,char *const argv[],uint64_t now,uint64_t timeout) {
    assert(timeout==60000000 && !strcmp(path,"/fat/bin/deskfind-worker") && argv[3] && !argv[4]);if(spawn_fail) return -1;
    started++;strcpy(child_root,argv[1]);strcpy(child_query,argv[2]);j->busy=1;j->started_us=now;j->length=0;j->output[0]=0;return 0;
}
void desk_command_job_cancel(struct desk_command_job *j) { j->busy=0;j->retiring_pid=42;j->canceled=1; }
int desk_command_job_poll(struct desk_command_job *j,uint64_t now) { (void)now;if(poll_result) { j->busy=0;j->length=sizeof(payload);memcpy(j->output,&payload,sizeof(payload)); }return poll_result; }
static void set(struct desk_text_buffer *t,const char *s) { assert(desk_text_buffer_set(t,s,strlen(s))>=0); }
static void finish(void) {
    payload.header=(struct desk_search_header){DESK_SEARCH_MAGIC,DESK_SEARCH_VERSION,3,0,10,3,0};
    payload.entries[0]=(struct desk_search_entry){.path="/fat/home/z-report.txt",.inode=10,.size=30};
    payload.entries[1]=(struct desk_search_entry){.path="/fat/home/a-report.png",.inode=11,.size=10};
    payload.entries[2]=(struct desk_search_entry){.path="/fat/home/reports",.inode=12,.directory=1};
    poll_result=1;poll_job();poll_result=0;
}
int main(void) {
    initialize(NULL);app.window.width=760;app.window.height=500;app.window.focused=1;app.window.server=42;layout();desk_ui_focus(&app.ui,FIND_QUERY);
    assert(!strcmp(app.root_text.data,"/fat/home"));run();assert(!started && strstr(app.status,"Enter part"));
    set(&app.query_text,"report");set(&app.root_text,"relative");run();assert(!started && strstr(app.status,"absolute"));set(&app.root_text,"/fat/../fat//home/");
    spawn_fail=1;run();assert(!started && strstr(app.status,"could not start"));spawn_fail=0;run();assert(started==1 && app.job.busy && !app.search.enabled && app.stop.enabled);
    set(&app.query_text,"newer");set(&app.root_text,"/elsewhere");run();assert(started==1 && !strcmp(child_query,"report") && !strcmp(child_root,"/fat/home"));
    action((struct desk_action){FIND_FILES,DESK_WIDGET_ACTIVATED});assert(desk_tabs_selected(&app.tabs)==FIND_ALL);
    finish();assert(app.header.count==3 && app.table.body.selected==0 && selected()->directory && strstr(app.status,"report") && app.table.body.focused);
    open_result(0);assert(launched==1 && launch_app==DESK_APP_FILES && !strcmp(launch_path,"/fat/home/reports"));
    desk_list_select(&app.table.body,1);assert(!strcmp(selected()->path,"/fat/home/a-report.png"));open_result(0);assert(launch_app==DESK_APP_PREVIEW);
    open_result(1);assert(launch_app==DESK_APP_FILES && !strcmp(launch_path,"/fat/home"));
    copy_path();assert(app.clipboard.mode && !strcmp((char *)app.clipboard_data,"/fat/home/a-report.png"));app.clipboard.mode=0;clipboard_finish(1);
    desk_table_sort_indicator(&app.table,2,1);rows();assert(app.table.body.selected==2 && !strcmp(selected()->path,"/fat/home/a-report.png"));
    desk_list_select(&app.table.body,1);open_result(0);assert(launch_app==DESK_APP_TEXT && strstr(launch_path,"z-report"));
    desk_ui_focus(&app.ui,FIND_QUERY);set(&app.query_text,"report");desk_text_buffer_select(&app.query_text,0,6);clipboard_action(FIND_QUERY,DESK_EDIT_PASTE);
    set(&app.query_text,"newer");strcpy((char *)app.clipboard_data,"stale");app.clipboard.length=5;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.query_text.data,"newer"));
    desk_text_buffer_select(&app.query_text,0,5);clipboard_action(FIND_QUERY,DESK_EDIT_PASTE);desk_ui_focus(&app.ui,FIND_ROOT);app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.query_text.data,"newer"));
    desk_ui_focus(&app.ui,FIND_QUERY);clipboard_action(FIND_QUERY,DESK_EDIT_PASTE);strcpy((char *)app.clipboard_data,"a\nb");app.clipboard.length=3;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.query_text.data,"newer"));
    clipboard_action(FIND_QUERY,DESK_EDIT_PASTE);strcpy((char *)app.clipboard_data,"report");app.clipboard.length=6;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.query_text.data,"report"));
    set(&app.root_text,"/fat/home");run();assert(app.header.count==0 && selected()==NULL);stop();assert(!app.job.busy && strstr(app.status,"stopped"));run();assert(started==2);app.job.retiring_pid=0;
    run();payload.header.magic=0;poll_result=1;poll_job();poll_result=0;assert(!app.header.count && strstr(app.status,"did not finish"));
    run();app.job.timed_out=1;poll_result=-1;poll_job();poll_result=0;assert(strstr(app.status,"60 seconds"));app.job.timed_out=0;
    run();finish();context(0,0,1);assert(desk_popup_active(&app.context));layout();assert(!desk_popup_active(&app.context));
    static uint32_t full[980*660],tiled[980*660];const int sizes[][2]={{760,500},{280,240},{400,320},{960,640}};
    for(unsigned size=0;size<4;size++) {
        app.window.width=sizes[size][0];app.window.height=sizes[size][1];layout();
        assert(app.table.body.focused);
        assert(app.open.frame.x+app.open.frame.w<=(int)app.window.width-12 && app.table.body.frame.h>=28);
        struct desk_canvas a,b;memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,app.window.width,app.window.height,980);desk_canvas_init(&b,tiled,app.window.width,app.window.height,980);draw(&a);
        for(int y=0;y<(int)app.window.height;y+=53) for(int x=0;x<(int)app.window.width;x+=73) { desk_clip(&b,(struct desk_rect){x,y,73,53});draw(&b); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=app.window.width || y>=app.window.height) assert(!full[y*980+x]);
    }
    puts("find-app: PASS search snapshots, validation/failure/cancel, stable sorting, open/reveal/copy, stale clipboard isolation, context and compact/tiled rendering");
}
