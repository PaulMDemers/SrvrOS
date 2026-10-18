#define main network_main
#include "../userspace/desknet/desknet.c"
#undef main
#include <assert.h>
static struct srv_net_status_info net;
static int started,poll_result,spawn_fail,inject;
static char child_path[160],child_target[256];
long srv_ticks(void) { return 42; }
void srv_puts(const char *s) { (void)s; }
long srv_net_status_info(struct srv_net_status_info *s) { *s=net;return 0; }
int desk_command_job_init(struct desk_command_job *j,char *o,size_t n) { *j=(struct desk_command_job){.reader=-1,.output=o,.capacity=n};o[0]=0;return 0; }
int desk_command_job_start(struct desk_command_job *j,const char *path,char *const argv[],uint64_t now,uint64_t timeout) {
    assert(timeout==15000000 && argv[0] && argv[1] && !argv[2]);
    if(spawn_fail) return -1;
    started++;strcpy(child_path,path);strcpy(child_target,argv[1]);j->busy=1;j->started_us=now;j->length=0;j->output[0]=0;return 0;
}
void desk_command_job_cancel(struct desk_command_job *j) { j->busy=0;j->canceled=1;j->retiring_pid=123; }
int desk_command_job_poll(struct desk_command_job *j,uint64_t now) { (void)now;if(inject) { memcpy(j->output,"hello\n\x1b\x80\0end",12);j->length=12;inject=0; }if(poll_result) j->busy=0;return poll_result; }
static void target(const char *s) { assert(desk_text_buffer_set(&app.target_text,s,strlen(s))>=0); }
int main(void) {
    const char *valid[]={"10.0.2.2","example.test","a-b.c.","localhost","A0"};
    const char *invalid[]={"","-n","a..b","a-.b","-a.b","a b","x/y","x;echo","é.test",".abc"};
    for(unsigned i=0;i<sizeof(valid)/sizeof(*valid);i++) assert(target_valid(valid[i]));
    for(unsigned i=0;i<sizeof(invalid)/sizeof(*invalid);i++) assert(!target_valid(invalid[i]));
    char longlabel[65];memset(longlabel,'a',64);longlabel[64]=0;assert(!target_valid(longlabel));longlabel[63]=0;assert(target_valid(longlabel));
    net=(struct srv_net_status_info){.initialized=1,.worker_started=1,.router_ip=0x0a000202,.local_ip=0x0a00020f,.link_flags=SRV_NET_LINK_KNOWN|SRV_NET_LINK_UP};
    initialize();app.window.width=680;app.window.height=460;app.window.focused=1;app.window.server=42;layout();desk_ui_focus(&app.ui,NET_TARGET);
    assert(!strcmp(app.target_text.data,"10.0.2.2") && page()==NET_PING);
    target("bad target");run();assert(!started && strstr(app.status,"without spaces"));target("10.0.2.2");
    net.initialized=0;run();assert(!started && strstr(app.status,"No supported"));net.initialized=1;
    net.link_flags=SRV_NET_LINK_KNOWN;run();assert(!started && strstr(app.status,"disconnected"));net.link_flags|=SRV_NET_LINK_UP;
    net.local_ip=0;run();assert(!started && strstr(app.status,"IPv4"));net.local_ip=0x0a00020f;
    spawn_fail=1;run();assert(!started && strstr(app.status,"could not start"));spawn_fail=0;
    run();assert(started==1 && app.job.busy && !app.run.enabled && app.stop.enabled && !strcmp(child_path,"/fat/bin/ping"));
    target("changed.test");run();assert(started==1 && !strcmp(app.run_target,"10.0.2.2"));
    action((struct desk_action){NET_LOOKUP,DESK_WIDGET_ACTIVATED});assert(page()==NET_PING);
    inject=1;poll_job();assert(!memcmp(app.output,"hello\n???end",12));
    app.job.exit_status=0;poll_result=1;poll_job();poll_result=0;assert(!app.job.busy && strstr(app.result_status,"completed") && !app.stop.enabled);
    copy_report();assert(app.clipboard.mode && strstr((char *)app.clipboard_data,"Target: 10.0.2.2\n") && strstr((char *)app.clipboard_data,"hello\n???end"));app.clipboard.mode=0;clipboard_finish(1);
    action((struct desk_action){NET_LOOKUP,DESK_WIDGET_ACTIVATED});run();assert(started==2 && !strcmp(child_path,"/fat/bin/host") && !strcmp(child_target,"changed.test"));
    stop();assert(!app.job.busy && app.job.retiring_pid && strstr(app.result_status,"Stopped"));run();assert(started==2);app.job.retiring_pid=0;
    run();app.job.exit_status=1;poll_result=1;poll_job();poll_result=0;assert(strstr(app.result_status,"did not succeed"));
    run();app.job.timed_out=1;poll_result=-1;poll_job();poll_result=0;assert(strstr(app.result_status,"15 seconds"));app.job.timed_out=0;
    desk_ui_focus(&app.ui,NET_TARGET);target("kept");desk_text_buffer_select(&app.target_text,0,4);clipboard_action(DESK_EDIT_PASTE);target("newer");strcpy((char *)app.clipboard_data,"stale");app.clipboard.length=5;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.target_text.data,"newer"));
    desk_text_buffer_select(&app.target_text,0,5);clipboard_action(DESK_EDIT_PASTE);strcpy((char *)app.clipboard_data,"two\nlines");app.clipboard.length=9;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.target_text.data,"newer"));
    clipboard_action(DESK_EDIT_PASTE);strcpy((char *)app.clipboard_data,"example.test");app.clipboard.length=12;app.clipboard.mode=0;clipboard_finish(1);assert(!strcmp(app.target_text.data,"example.test"));
    context(0,0,1);assert(desk_popup_active(&app.context));layout();assert(!desk_popup_active(&app.context));
    static uint32_t full[980*660],tiled[980*660];const int sizes[][2]={{680,460},{280,240},{400,320},{960,640}};
    for(unsigned size=0;size<4;size++) {
        app.window.width=sizes[size][0];app.window.height=sizes[size][1];layout();
        assert(app.copy.frame.x+app.copy.frame.w<=(int)app.window.width-12 && app.document.frame.h>0);
        struct desk_canvas a,b;memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,app.window.width,app.window.height,980);desk_canvas_init(&b,tiled,app.window.width,app.window.height,980);draw(&a);
        for(int y=0;y<(int)app.window.height;y+=53) for(int x=0;x<(int)app.window.width;x+=73) { desk_clip(&b,(struct desk_rect){x,y,73,53});draw(&b); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<660;y++) for(unsigned x=0;x<980;x++) if(x>=(unsigned)app.window.width || y>=(unsigned)app.window.height) assert(!full[y*980+x]);
    }
    action((struct desk_action){NET_CLEAR,DESK_WIDGET_ACTIVATED});assert(!app.run_mode && !app.output[0]);
    puts("network-app: PASS target validation, network prerequisites, immutable run targets, success/failure/timeout/cancel, output sanitization, report/field clipboard, stale-transfer guards and compact/tiled rendering");
}
