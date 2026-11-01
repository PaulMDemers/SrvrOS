#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
long srv_futex_wake(uint32_t *word,uint64_t count) { (void)word;(void)count;return 0; }
static void *test_registry_alloc(size_t bytes);
static void test_registry_free(void *p,size_t bytes);
#define DESKD_STORAGE_ALLOC test_registry_alloc
#define DESKD_STORAGE_FREE test_registry_free
#define main desktop_guest_main
#include "../userspace/deskd/deskd.c"
#undef main

static unsigned capture_closes,capture_reap_pending;
int desk_capture_job_prepare(struct desk_capture_job *j,uint32_t width,uint32_t height,const char *path) {
    (void)j;(void)width;(void)height;(void)path;assert(!"unexpected capture preparation");return -1;
}
void desk_capture_job_close(struct desk_capture_job *j) {
    capture_closes++;assert(j->pixels && j->reader==10 && j->writer==11);
    j->pixels=NULL;j->reader=j->writer=-1;j->busy=0;j->retiring_pid=j->pid;j->pid=0;
    j->cleanup_us=660000;j->close_us=650000;j->unmap_us=10000;
}
int desk_capture_job_poll(struct desk_capture_job *j) {assert(!j->busy);if(!capture_reap_pending)j->retiring_pid=0;return 0;}
static void capture_cleanup_ownership(void) {
    struct lifecycle_worker w={0};struct desk_capture_job j={.pid=42,.reader=10,.writer=11,.busy=1,.pixels=(void *)1};
    j.request.id=9;j.response.status=DESK_CAPTURE_OK;j.finished_us=100;
    capture_closes=0;capture_reap_pending=1;
    assert(life_capture_submit(&w,&j,1));assert(j.busy && !j.pixels && j.reader==-1 && !capture_closes);
    assert(!life_capture_submit(&w,&j,-1) && !life_capture_take(&w,&j));
    life_capture_work(&w);assert(capture_closes==1 && j.busy && !life_capture_take(&w,&j));
    life_capture_work(&w);assert(capture_closes==1 && life_state(&w.capture.state)==LIFE_QUEUED);
    capture_reap_pending=0;life_capture_work(&w);assert(life_capture_take(&w,&j)==1);
    assert(!j.busy && !j.retiring_pid && j.request.id==9 && j.finished_us==100 && j.cleanup_us>=660000);
    assert(!life_capture_take(&w,&j) && life_state(&w.capture.state)==LIFE_IDLE);
    j=(struct desk_capture_job){.pid=43,.reader=10,.writer=11,.busy=1,.pixels=(void *)1};
    capture_reap_pending=1;assert(life_capture_submit(&w,&j,-1));
    life_capture_work(&w);assert(j.busy && !life_capture_take(&w,&j));
    capture_reap_pending=0;life_capture_work(&w);assert(life_capture_take(&w,&j)==-1);
    assert(!j.busy && !j.retiring_pid && capture_closes==2);
}
static unsigned registry_calls,registry_fail_at,registry_live;
static size_t registry_live_bytes;
static void *test_registry_alloc(size_t bytes) {
    if(++registry_calls==registry_fail_at)return NULL;
    void *p=malloc(bytes);assert(p);registry_live++;registry_live_bytes+=bytes;return p;
}
static void test_registry_free(void *p,size_t bytes) {
    assert(registry_live && registry_live_bytes>=bytes);registry_live--;registry_live_bytes-=bytes;free(p);
}
static struct srv_battery_status battery_mock;
static int battery_mock_error;
long srv_battery_status(struct srv_battery_status *out) { *out=battery_mock;return battery_mock_error; }
static unsigned audio_calls;
int gui_audio(unsigned op,unsigned value,struct srv_audio_status *out) {
    (void)value;if(op) audio_calls++;*out=(struct srv_audio_status){.version=SRV_AUDIO_VERSION,.state=SRV_AUDIO_READY,.volume=25};return 0;
}
static unsigned brightness_calls;
int gui_brightness(unsigned percent) { brightness_calls++;return percent?(int)percent:14; }
static unsigned pointer_options;
int gui_pointer_config(unsigned speed,unsigned options) { (void)speed;pointer_options=options;return 0; }
static int capture_loop_log;
static char captured_loop_log[4096];
void srv_puts(const char *text) {
    if(capture_loop_log) {
        assert(strlen(text)<1024);
        if(!strncmp(text,"desktop-loop-worst:",19))captured_loop_log[0]=0;
        assert(strlen(captured_loop_log)+strlen(text)<sizeof(captured_loop_log));strcat(captured_loop_log,text);
    }
    else fputs(text,stdout);
}
static uint64_t now=1;
static int alive=1,queue_full,copy_failure,allocations,frees,killed,sends,reaped,alloc_fail;
static unsigned kill_delay;
static int terminal_snapshot,defer_reap;static unsigned worker_reaps;
long srv_proc_list(uint64_t index,struct srv_process_info *info) {
    if(!terminal_snapshot || index)return 0;
    *info=(struct srv_process_info){.pid=42,.parent_pid=4};strcpy(info->state,"exited");return 1;
}
static uint32_t sent[128],sent_values[128];
static uint64_t sent_serials[128],sent_windows[128];
static int send_allow=-1;
static long execute_result=-1;
static char executed_path[160],executed_role[32],executed_argument[160];
long srv_exec(const struct srv_exec_request *request) {
    snprintf(executed_path,sizeof(executed_path),"%s",request->path);
    snprintf(executed_role,sizeof(executed_role),"%s",request->argv[1]);
    snprintf(executed_argument,sizeof(executed_argument),"%s",request->argv[2]);return execute_result;
}
long srv_ticks(void) { return (long)now; }
long srv_monotonic_us(void) { return (long)(now*10000); }
long srv_realtime_us(void) { return 1792326896000000L; }
int desk_process_alive(uint64_t pid) { return pid==42 && alive; }
long srv_kill(int64_t pid) { assert(pid==42); alive=0; killed++; now+=kill_delay; return 0; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    if(defer_reap) {
        if(flags&SRV_WAIT_NOHANG)return 0;
        assert(terminal_snapshot && pid==42);worker_reaps++;*status=0;return 42;
    }
    (void)status;return !alive?(reaped?-1:(long)pid):0;
}
static void asynchronous_client_termination(void) {
    struct lifecycle_worker w={0};struct session s={.worker=&w};
    alive=1;now=602;kill_delay=250;int before=killed;
    s.scene.focus=-1;s.clients[0]=(struct client){.pid=42,.awaiting=1,.progress=1};
    service(&s);assert(killed==before && alive && now==602 && s.clients[0].pid==42);
    for(unsigned n=0;n<100;n++)service(&s);
    assert(now==602 && killed==before && life_terminate_pending(&w,42));
    life_work(&w);assert(now==852 && killed==before+1 && !alive);
    assert(s.clients[0].pid==42 && w.terminate[0].elapsed==2500000);
    service(&s);assert(!s.clients[0].pid && !life_terminate_pending(&w,42));
    service(&s);assert(killed==before+1);
    /* Full/stopped queues keep live UI clients for retry; no direct kill. */
    alive=1;now=1000;s.clients[0]=(struct client){.pid=42,.awaiting=1,.progress=1};
    for(unsigned n=0;n<LIFE_TERMINATE_SLOTS;n++)assert(life_terminate_submit(&w,500+n));
    service(&s);assert(killed==before+1 && alive && s.clients[0].pid==42);
    life_publish(&w.terminate[0].state,LIFE_IDLE);service(&s);
    assert(life_terminate_pending(&w,42) && killed==before+1);
    w=(struct lifecycle_worker){.stop=1};service(&s);
    assert(alive && s.clients[0].pid==42 && killed==before+1);
    kill_delay=0;
    puts("desktop-termination: PASS blocked syscall outside UI, duplicate/full/stopped queues, delayed client retirement");
}
static void asynchronous_child_reap(void) {
    struct lifecycle_worker w={0};struct session s={.worker=&w};s.children[0]=42;
    terminal_snapshot=defer_reap=1;alive=0;worker_reaps=0;
    service(&s);assert(s.children[0]==42 && life_reap_pending(&w,42) && !worker_reaps);
    service(&s);assert(s.children[0]==42 && !s.child_reaped);
    life_work(&w);assert(worker_reaps==1 && s.children[0]==42);
    service(&s);assert(!s.children[0] && s.child_reaped==1 && !s.child_errors);
    service(&s);assert(worker_reaps==1 && !life_reap_pending(&w,42));
    terminal_snapshot=defer_reap=0;alive=1;
    puts("desktop-child-reap: PASS deferred NOHANG, retained PID, worker-owned blocking wait and one result consumption");
}
uint32_t *desk_pixels_alloc(uint32_t w,uint32_t h) { if(alloc_fail && allocations==alloc_fail) return NULL; allocations++; return malloc((size_t)w*h*4); }
void desk_pixels_free(uint32_t *p,uint32_t w,uint32_t h) {
    (void)w; (void)h; if (p) { frees++; free(p); }
}
struct desk_packet desk_packet_new(uint32_t type) {
    return (struct desk_packet){.window=DESK_DEFAULT_WINDOW,.type=type,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet)};
}
int desk_send(uint64_t target,const struct desk_packet *p) {
    assert(target==42);
    if (queue_full || !send_allow) return -1;
    if(send_allow>0) send_allow--;
    assert(sends<128);sent_windows[sends]=p->window;sent_serials[sends]=p->serial;sent_values[sends]=p->value;sent[sends++]=p->type; return 0;
}
int gui_surface_copy(uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,uint32_t *p,uint64_t stride) {
    assert(id==7 && !x && !y && stride==w);
    for (uint64_t i=0;i<w*h/2;i++) p[i]=0x123456;
    if (copy_failure) return -1;
    for (uint64_t i=w*h/2;i<w*h;i++) p[i]=0x123456;
    return 0;
}
static void asynchronous_lifecycle(void) {
    static struct session s;
    static struct lifecycle_worker worker;
    s.worker=&worker;s.home=DESK_RAM_HOME;s.scene.apps=1;s.scene.focus=-1;
    desk_damage_init(&s.damage,1280,800);alive=1;send_allow=-1;queue_full=0;
    char argument[32]="copied-path";strcpy(executed_path,"unchanged");execute_result=42;
    spawn_app(&s,DESK_APP_TEXT,argument);strcpy(argument,"mutated");
    assert(!strcmp(executed_path,"unchanged") && !s.children[0] && child_slot(&s)==1);
    /* A child can send OPEN before the worker publishes its exec result.
     * Defer that packet; never admit a process not registered as our child. */
    struct desk_packet open=desk_packet_new(DESK_OPEN);open.width=400;open.height=280;
    incoming(&s,42,&open);assert(s.pending_opens==1 && !s.clients[0].pid);
    life_work(&worker);assert(!strcmp(executed_argument,"copied-path") && !s.children[0]);
    lifecycle_poll(&s);assert(s.children[0]==42 && s.last_launch_pid==42);
    lifecycle_deliver(&s);assert(!s.pending_opens && s.clients[0].pid==42 && s.clients[0].configure);
    int before=allocations;service(&s);assert(allocations==before && s.clients[0].configure);
    life_work(&worker);service(&s);assert(!s.clients[0].configure && allocations==before+1);
    uint32_t *front=cache_prepare(&s,0,400,280);assert(front);
    s.scene.windows[0].pixels=front;s.scene.windows[0].width=400;s.scene.windows[0].height=280;s.clients[0].awaiting=0;
    struct desk_rect old_frame=s.scene.windows[0].frame;uint64_t old_generation=s.clients[0].generation;
    alloc_fail=allocations;assert(configure(&s,0,500,300));life_work(&worker);service(&s);
    assert(s.scene.windows[0].pixels==front && s.scene.windows[0].frame.w==old_frame.w && s.clients[0].generation==old_generation && !s.clients[0].closing);
    life_work(&worker);lifecycle_poll(&s);alloc_fail=0;
    assert(!cache_prepare(&s,0,400,280) && s.cache_pending);
    /* Close during a queued allocation: retain both reservations until the
     * worker has finished and unmapped them, including the unpublished one. */
    uint64_t held=s.cache_bytes;cache_retire(&s,0);assert(s.cache_bytes==held);
    life_work(&worker);assert(s.cache_bytes==held);lifecycle_poll(&s);assert(!s.cache_bytes);
    s.scene.windows[0].pixels=NULL;
    alloc_fail=allocations;assert(!cache_prepare(&s,0,500,300) && s.cache_pending);
    life_work(&worker);assert(!cache_prepare(&s,0,500,300) && !s.cache_pending);
    life_work(&worker);lifecycle_poll(&s);assert(!s.cache_bytes);alloc_fail=0;
    s.children[0]=0;execute_result=SRV_ERR_NOMEM;spawn_app(&s,DESK_APP_FILES,"");
    assert(!s.last_launch_pid);life_work(&worker);lifecycle_poll(&s);
    assert(!s.children[0] && strstr(s.scene.notice,"could not open"));
    s.exiting=1;strcpy(executed_path,"unchanged");spawn_app(&s,DESK_APP_TEXT,"");life_work(&worker);
    assert(!strcmp(executed_path,"unchanged"));
    memset(&s,0,sizeof(s));memset(&worker,0,sizeof(worker));s.worker=&worker;
    /* Fill the 96 MiB budget with sixteen front/spare pairs. Reclaiming
     * spares must defer admission without granting credit before unmap. */
    for(int i=0;i<16;i++) {
        assert(!cache_prepare(&s,i,1024,768) && s.cache_pending);life_work(&worker);
        s.scene.windows[i].pixels=cache_prepare(&s,i,1024,768);assert(s.scene.windows[i].pixels);
        assert(!cache_prepare(&s,i,1024,768) && s.cache_pending);life_work(&worker);
        assert(cache_prepare(&s,i,1024,768));
    }
    assert(s.cache_bytes==DESKD_CACHE_BUDGET);
    cache_forget(&s,15);s.scene.windows[15].pixels=NULL;
    assert(s.retained_cache[0].job && s.retained_cache[1].job && s.cache_bytes==DESKD_CACHE_BUDGET);
    assert(!cache_prepare(&s,0,1280,768) && s.cache_pending && s.cache_bytes==DESKD_CACHE_BUDGET);
    life_work(&worker);assert(s.cache_bytes==DESKD_CACHE_BUDGET);lifecycle_poll(&s);
    assert(s.cache_bytes==16ull*1024*768*4 && !s.retained_cache[0].job && !s.retained_cache[1].job);
    assert(!cache_prepare(&s,0,1280,768) && s.cache_pending && s.cache_bytes<=DESKD_CACHE_BUDGET);
    life_work(&worker);assert(cache_prepare(&s,0,1280,768));
    cache_free(&s);life_work(&worker);lifecycle_poll(&s);assert(!s.cache_bytes);
    memset(&s,0,sizeof(s));memset(&worker,0,sizeof(worker));execute_result=-1;sends=0;
}
static void seeded_cache_reuse(void) {
    static struct session s;static struct lifecycle_worker worker;
    for(unsigned partial=0;partial<2;partial++) {
        s=(struct session){.worker=&worker};worker=(struct lifecycle_worker){0};
        int before=allocations,freed=frees;
        alloc_fail=partial?allocations+1:0;cache_seed(&s,&worker);alloc_fail=0;
        unsigned count=partial?1:2;
        assert(allocations==before+(int)count && s.cache_bytes==count*720ull*480*4);
        uint32_t *pixels=cache_prepare(&s,0,720,480);
        assert(pixels && allocations==before+(int)count && s.cache_reuses==1);
        cache_free(&s);assert(s.cache_bytes==count*720ull*480*4);
        life_work(&worker);lifecycle_poll(&s);
        assert(!s.cache_bytes && frees==freed+(int)count);
    }
    memset(&s,0,sizeof(s));memset(&worker,0,sizeof(worker));
    puts("desktop-cache-seed: PASS initial/partial reserve, reuse without helper mapping, deferred worker release and zero final budget");
}
static void retired_cache_reuse(void) {
    static struct session s;static struct lifecycle_worker worker;
    s=(struct session){.worker=&worker};worker=(struct lifecycle_worker){0};
    int before=allocations,freed=frees;alive=1;copy_failure=0;
    assert(!cache_prepare(&s,0,400,280) && s.cache_pending);life_work(&worker);
    uint32_t *pixels=cache_prepare(&s,0,400,280);assert(pixels && allocations==before+1);
    for(unsigned n=0;n<100;n++) {
        s.scene.windows[0].pixels=pixels;
        cache_forget(&s,0);s.scene.windows[0].pixels=NULL;
        assert(s.cache_bytes==400u*280u*4u && frees==freed);
        struct operation_trace t=operation_snapshot(&s,100+n);
        assert(t.cache_retained==1 && t.cache_retained_bytes==s.cache_bytes && t.cache_reuses==n);
        assert(cache_prepare(&s,0,400,280)==pixels && !s.cache_pending);
        assert(!s.scene.windows[0].pixels && allocations==before+1 && frees==freed);
    }
    /* A reused staging buffer must not publish old or partially copied pixels. */
    s.clients[0]=(struct client){.pid=42,.window=1,.generation=1,.awaiting=1};
    s.scene.windows[0].frame=(struct desk_rect){20,60,402,325};
    for(unsigned n=0;n<400u*280u;n++)pixels[n]=0xaabbcc;
    struct desk_packet p=desk_packet_new(DESK_COMMIT);p.generation=1;p.serial=1;p.surface=7;p.width=400;p.height=280;
    copy_failure=1;incoming(&s,42,&p);assert(!s.scene.windows[0].pixels && s.clients[0].closing);
    s.clients[0].closing=0;s.clients[0].release.type=0;copy_failure=0;p.serial=2;
    incoming(&s,42,&p);assert(s.scene.windows[0].pixels==pixels);
    for(unsigned n=0;n<400u*280u;n++)assert(pixels[n]==0x123456);
    cache_forget(&s,0);s.scene.windows[0].pixels=NULL;
    cache_free(&s);assert(s.cache_bytes==400u*280u*4u); /* Still reserved until unmap. */
    life_work(&worker);assert(s.cache_bytes==400u*280u*4u);lifecycle_poll(&s);
    assert(!s.cache_bytes && frees==freed+1 && allocations==before+1 && s.cache_reuses==100);
    /* A queued allocation is discarded, never pooled before publication. */
    assert(!cache_prepare(&s,0,400,280));cache_forget(&s,0);
    assert(!s.retained_cache[0].job && !s.retained_cache[1].job);
    life_work(&worker);lifecycle_poll(&s);assert(!s.cache_bytes);
    memset(&s,0,sizeof(s));memset(&worker,0,sizeof(worker));
    puts("desktop-cache-reuse: PASS100 retire/reopen transfers, unchanged reservation, no new mappings, partial-copy isolation and full shutdown");
}
static void delayed_control_delivery(void) {
    static struct session s;
    cache_free(&s);memset(&s,0,sizeof(s));alive=1;queue_full=0;sends=0;send_allow=-1;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.clients[0].progress=now;
    s.clients[0].release=desk_packet_new(DESK_RELEASE);
    now+=601;service(&s);
    assert(alive && s.clients[0].pid==42 && !s.clients[0].release.type);

    queue_full=1;queue_input(&s,0,desk_packet_new(DESK_KEY));service(&s);
    assert(s.clients[0].input_blocked);
    now+=601;queue_full=0;send_allow=1;
    s.clients[0].release=desk_packet_new(DESK_RELEASE);service(&s);
    assert(alive && !s.clients[0].closing && s.clients[0].input_count==1 && s.clients[0].input_since==now);
    send_allow=-1;service(&s);assert(!s.clients[0].input_count);

    s.clients[0].configure=s.clients[0].awaiting=1;s.clients[0].progress=now;
    s.scene.windows[0].frame=(struct desk_rect){0,0,402,345};
    now+=601;service(&s);assert(alive && !s.clients[0].configure);
    now+=400;service(&s);assert(alive);
    now+=101;service(&s);assert(!alive && !s.clients[0].pid);

    cache_free(&s);memset(&s,0,sizeof(s));alive=1;queue_full=1;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.clients[0].progress=now;
    s.clients[0].release=desk_packet_new(DESK_RELEASE);
    now+=601;service(&s);assert(alive && s.clients[0].release.type);
    now+=400;service(&s);assert(alive);
    queue_full=0;now+=200;service(&s);assert(alive && !s.clients[0].release.type);

    s.clients[0].configure=s.clients[0].awaiting=1;s.clients[0].progress=now;
    queue_full=1;service(&s);now+=501;service(&s);assert(!alive && !s.clients[0].pid);

    cache_free(&s);memset(&s,0,sizeof(s));alive=1;queue_full=0;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;close_client(&s,0);now+=601;
    int before=sends;service(&s);assert(alive && sends==before+1 && sent[sends-1]==DESK_CLOSE);
    now+=400;service(&s);assert(alive && sends==before+1); /* No close-message flood. */
    now+=101;service(&s);assert(!alive && !s.clients[0].pid);
}
static void pointer_cancel_keeps_keyboard(void) {
    static struct session s;
    cache_free(&s);memset(&s,0,sizeof(s));alive=1;queue_full=0;sends=0;send_allow=-1;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.clients[0].generation=1;
    for(unsigned i=0;i<DESK_INPUT_QUEUE;i++) {
        struct desk_packet p=desk_packet_new(i&1?DESK_TEXT:DESK_KEY);
        p.serial=i+1;queue_input(&s,0,p);
    }
    cancel_input(&s,0);
    assert(s.clients[0].input_count==DESK_INPUT_QUEUE && !s.clients[0].closing);
    queue_full=1;service(&s);now+=400;service(&s);
    assert(alive && s.clients[0].input_count==DESK_INPUT_QUEUE && !sends);
    queue_full=0;
    while(s.clients[0].input_count) service(&s);
    assert(sends==DESK_INPUT_QUEUE+1 && sent[0]==DESK_POINTER && sent_values[0]==DESK_POINTER_CANCEL);
    for(unsigned i=0;i<DESK_INPUT_QUEUE;i++) {
        assert(sent[i+1]==(i&1?DESK_TEXT:DESK_KEY));
        assert(sent_serials[i+1]==i+1);
    }
    sends=0;s.clients[0].input_head=63;
    struct desk_packet key=desk_packet_new(DESK_KEY);key.serial=91;
    queue_input(&s,0,key);queue_input(&s,0,desk_packet_new(DESK_POINTER));
    queue_input(&s,0,desk_packet_new(DESK_SCROLL));key.serial=92;queue_input(&s,0,key);
    cancel_input(&s,0);cancel_input(&s,0);service(&s);
    assert(sends==3 && sent_values[0]==DESK_POINTER_CANCEL);
    assert(sent_serials[1]==91 && sent_serials[2]==92);
    sends=0;now=1;
}
static void battery_ui_notifications(void) {
    static struct session s;
    cache_free(&s);memset(&s,0,sizeof(s));s.scene.apps=1;s.scene.focus=-1;
    desk_damage_init(&s.damage,1280,800);desktop_notice_view_init(&s.scene.notifications);
    strcpy(s.scene.clock,"12:00");
    battery_mock=(struct srv_battery_status){.size=sizeof(battery_mock),.version=SRV_BATTERY_VERSION,
        .state=SRV_BATTERY_ACTIVE,.flags=SRV_BATTERY_VALID|SRV_BATTERY_FRESH,
        .valid=SRV_BATTERY_SOC_VALID,.soc=20};
    battery_update(&s,1);
    assert(!strcmp(s.scene.battery,"20%") && s.scene.notifications.store.count==1);
    assert(!strcmp(s.scene.notifications.store.records[0].text.title,"Battery low"));
    assert(s.scene.notifications.store.records[0].application==DESK_APP_SYSTEM && s.scene.focus==-1);
    for(int i=0;i<20;i++)battery_update(&s,1);
    assert(s.scene.notifications.store.count==1);
    battery_mock.soc=5;battery_update(&s,1);assert(s.scene.notifications.store.count==2);
    assert(!strcmp(s.scene.notifications.store.records[1].text.title,"Battery very low"));
    battery_mock.flags|=SRV_BATTERY_STALE;battery_update(&s,1);
    assert(!strcmp(s.scene.battery,"Battery stale") && s.scene.notifications.store.count==2);
    battery_mock_error=-1;battery_update(&s,1);assert(!strcmp(s.scene.battery,"Battery —"));
    assert(s.scene.notifications.store.count==2);battery_mock_error=0;
    struct desk_rect r=desktop_battery_rect(1280,s.scene.clock);execute_result=42;
    assert(global_click(&s,r.x+5,10));assert(!strcmp(executed_role,"system"));
    assert(s.children[0]==42 && s.child_apps[0]==DESK_APP_SYSTEM);
    execute_result=-1;
}
static void idle_admission(void) {
    struct session s={0};memset(&clipboard,0,sizeof(clipboard));
    assert(session_quiescent(&s));
    s.children[0]=42;assert(!session_quiescent(&s));
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.clients[0].awaiting=1;assert(!session_quiescent(&s));
    s.clients[0].awaiting=0;s.clients[0].release=desk_packet_new(DESK_RELEASE);
    assert(!session_quiescent(&s));
    alive=1;queue_full=1;service(&s);assert(!session_quiescent(&s));
    queue_full=0;sends=0;send_allow=-1;service(&s);assert(session_quiescent(&s));
    s.preferences.job.busy=1;assert(!session_quiescent(&s));s.preferences.job.busy=0;
    s.preferences.dirty=1;assert(!session_quiescent(&s));s.preferences.dirty=0;
    s.capture.busy=1;assert(!session_quiescent(&s));s.capture.busy=0;
    s.capture_pending=1;assert(!session_quiescent(&s));s.capture_pending=0;
    s.pointer_pid=42;s.pointer_window=DESK_DEFAULT_WINDOW;assert(!session_quiescent(&s));s.pointer_pid=0;
    clipboard.writer=&s.clients[0].clipboard;assert(!session_quiescent(&s));clipboard.writer=0;
    s.clients[0].clipboard.reply=desk_packet_new(DESK_CLOSE);
    assert(!session_quiescent(&s));memset(&s.clients[0].clipboard,0,sizeof(s.clients[0].clipboard));
    assert(session_quiescent(&s));sends=0;
}
static void buffer_budget(void) {
    struct session s={0};int balance=allocations-frees;
    assert(!cache_init(&s) && !s.cache_bytes);
    for(int i=0;i<DESKD_WINDOWS;i++) {
        uint32_t *p=cache_prepare(&s,i,DESK_MAX_WIDTH,DESK_MAX_HEIGHT);assert(p);
        p[0]=100+i;s.scene.windows[i].pixels=p;
    }
    assert(s.cache_bytes==(uint64_t)DESKD_WINDOWS*DESK_MAX_WIDTH*DESK_MAX_HEIGHT*4);
    for(int i=0;i<3;i++) { assert(cache_prepare(&s,i,DESK_MAX_WIDTH,DESK_MAX_HEIGHT));s.clients[i].awaiting=1; }
    uint64_t before=s.cache_bytes;
    assert(!cache_prepare(&s,3,DESK_MAX_WIDTH,DESK_MAX_HEIGHT) && s.cache_bytes==before);
    for(int i=0;i<DESKD_WINDOWS;i++)assert(s.scene.windows[i].pixels[0]==100u+(unsigned)i);
    s.clients[3].pid=42;s.clients[3].window=DESK_DEFAULT_WINDOW;s.clients[3].generation=7;s.clients[3].present_input=12;
    s.scene.windows[3].frame=(struct desk_rect){0,0,DESK_MAX_WIDTH+2,DESK_MAX_HEIGHT+45};
    struct desk_packet commit=desk_packet_new(DESK_COMMIT);
    commit.generation=7;commit.width=DESK_MAX_WIDTH;commit.height=DESK_MAX_HEIGHT;
    commit.surface=7;commit.serial=99;commit.input_serial=13;
    incoming(&s,42,&commit);
    assert(s.clients[3].release.type==DESK_RELEASE && s.clients[3].release.value==DESK_RELEASE_RETRY);
    assert(!s.clients[3].closing && s.clients[3].present_input==12 && s.scene.windows[3].pixels[0]==103);
    s.clients[0].awaiting=0;
    assert(cache_prepare(&s,3,DESK_MAX_WIDTH,DESK_MAX_HEIGHT) && s.cache_bytes<=DESKD_CACHE_BUDGET);
    cache_free(&s);assert(!s.cache_bytes && allocations-frees==balance);
}
static void resize_refusal(void) {
    struct session s={.scene={.focus=0}};desk_damage_init(&s.damage,800,600);
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.scene.windows[0].frame=(struct desk_rect){30,70,402,325};
    uint32_t *old=cache_prepare(&s,0,400,280);assert(old);old[0]=99;
    s.scene.windows[0].pixels=old;s.scene.windows[0].width=400;s.scene.windows[0].height=280;
    assert(configure(&s,0,600,400));s.scene.windows[0].frame.x=8;
    struct desk_packet p=desk_packet_new(DESK_RESIZE_REFUSED);p.generation=s.clients[0].generation;p.width=400;p.height=280;
    uint64_t generation=p.generation;incoming(&s,42,&p);
    assert(s.clients[0].generation==generation+1 && s.scene.windows[0].frame.x==30 && s.scene.windows[0].frame.w==402);
    assert(s.scene.windows[0].pixels==old && old[0]==99 && !s.clients[0].closing);
    incoming(&s,42,&p);assert(s.clients[0].generation==generation+1); /* Stale refusal. */
    cache_free(&s);
}
static void clipboard_client_retirement(void) {
    struct session s={.scene={.focus=-1,.apps=1},.children={42}};
    memset(&clipboard,0,sizeof(clipboard));alive=1;queue_full=0;sends=0;
    desk_damage_init(&s.damage,800,600);
    struct desk_packet q=desk_packet_new(DESK_OPEN);q.width=400;q.height=280;
    incoming(&s,42,&q);assert(s.clients[0].clipboard.pid==42);
    assert(!desk_clipboard_write_local(&clipboard,"retained",8));
    q=desk_packet_new(DESK_CLIPBOARD_SET);q.serial=81;q.width=100;q.height=3;memcpy(q.text,"new",3);
    incoming(&s,42,&q);assert(clipboard.writer==&s.clients[0].clipboard);
    /* Client close retires clipboard ownership before clearing its storage. */
    q=desk_packet_new(DESK_CLOSE);incoming(&s,42,&q);
    assert(!clipboard.writer && !s.clients[0].clipboard.pid && !s.clients[0].clipboard.reply.type);
    assert(clipboard.length==8 && !memcmp(clipboard.data,"retained",8));
    q=desk_packet_new(DESK_OPEN);q.width=400;q.height=280;incoming(&s,42,&q);
    assert(s.clients[0].clipboard.pid==42 && !s.clients[0].clipboard.request);
    q=desk_packet_new(DESK_CLIPBOARD_GET);q.serial=82;incoming(&s,42,&q);
    assert(s.clients[0].clipboard.reading);
    queue_full=1;desk_clipboard_service(&clipboard,&s.clients[0].clipboard,now);
    assert(s.clients[0].clipboard.reading && !s.clients[0].clipboard.offset);
    forget(&s,0);assert(!s.clients[0].clipboard.reading && !s.clients[0].clipboard.pid);
    queue_full=0;sends=0;memset(&clipboard,0,sizeof(clipboard));
}
static void connection_lifetime(void) {
    struct session s={.scene={.focus=-1,.apps=1},.children={42},.child_apps={DESK_APP_TEXT}};
    memset(&clipboard,0,sizeof(clipboard));alive=1;queue_full=0;send_allow=-1;sends=0;
    desk_damage_init(&s.damage,1280,800);
    struct desk_packet q=desk_packet_new(DESK_OPEN);q.value=DESK_OPEN_CONNECTION;q.width=400;q.height=280;
    incoming(&s,42,&q);assert(s.clients[0].managed && s.child_reply[0].type==DESK_OPEN_REPLY);
    queue_full=1;now+=600;service(&s);assert(alive && s.clients[0].configure && s.child_reply[0].type);
    queue_full=0;service(&s);assert(sent[0]==DESK_OPEN_REPLY && !sent_values[0] && sent_windows[0]==1);
    assert(sent[1]==DESK_CONFIGURE && sent_windows[1]==1 && !s.clients[0].configure);
    q.window=2;incoming(&s,42,&q);service(&s);assert(s.clients[1].pid==42 && s.clients[1].window==2);
    assert(s.scene.focus==1 && s.clients[0].clipboard.window==1 && s.clients[1].clipboard.window==2);
    desk_palette_init(&s.scene.launcher);launcher_catalog(&s);
    uint64_t retired_identity=s.clients[0].identity;
    assert(retired_identity && s.clients[1].identity!=retired_identity);
    launcher_result(&s,(struct desk_palette_result){.id=retired_identity+DESK_APP_COUNT});
    assert(s.scene.focus==0);
    launcher_result(&s,(struct desk_palette_result){.id=s.clients[1].identity+DESK_APP_COUNT});
    assert(s.scene.focus==1); /* Sibling windows share a PID, not an action. */
    s.pointer_pid=42;s.pointer_window=2;
    unsigned before=s.clients[0].input_count;pointer_cancel(&s);
    assert(s.clients[0].input_count==before && s.clients[1].input_count);
    struct desk_packet *cancel=&s.clients[1].input[(s.clients[1].input_head+s.clients[1].input_count-1)%DESK_INPUT_QUEUE];
    assert(cancel->window==2 && cancel->type==DESK_POINTER && cancel->value==DESK_POINTER_CANCEL);
    q=desk_packet_new(DESK_NOTIFY);q.serial=1;
    struct desk_notification note={.title="Window notice",.message="Same serial, different window"};memcpy(q.text,&note,sizeof(note));
    incoming(&s,42,&q);q.window=2;incoming(&s,42,&q);
    assert(s.scene.notifications.store.count==2 && s.scene.notifications.store.records[0].window==1 && s.scene.notifications.store.records[1].window==2);
    s.clients[0].release=desk_packet_new(DESK_RELEASE);s.clients[0].release.surface=7;
    q=desk_packet_new(DESK_CLOSE);incoming(&s,42,&q);assert(s.clients[0].retiring);
    sends=0;queue_full=1;service(&s);assert(s.clients[0].pid && s.clients[1].pid);
    queue_full=0;send_allow=1;service(&s);
    assert(sends==1 && sent[0]==DESK_RELEASE && sent_windows[0]==1 && s.clients[0].pid);
    send_allow=-1;sends=0;service(&s);
    assert(sent[0]==DESK_CLOSE_ACK && sent_windows[0]==1 && !s.clients[0].pid && s.clients[1].pid==42 && !s.clients[1].closing);
    q=desk_packet_new(DESK_OPEN);q.value=DESK_OPEN_CONNECTION;q.width=400;q.height=280;
    incoming(&s,42,&q);assert(s.child_reply[0].value==DESK_OPEN_INVALID && !s.clients[0].pid);service(&s);
    q.window=3;alloc_fail=allocations;incoming(&s,42,&q);alloc_fail=0;
    assert(s.child_reply[0].value==DESK_OPEN_MEMORY && !s.clients[0].pid && s.clients[1].pid);service(&s);
    q.window=4;incoming(&s,42,&q);service(&s);assert(s.clients[0].window==4);
    assert(s.clients[0].identity!=retired_identity);focus(&s,1);
    launcher_result(&s,(struct desk_palette_result){.id=retired_identity+DESK_APP_COUNT});
    assert(s.scene.focus==1); /* A stale launcher result cannot select a reused slot. */
    q=desk_packet_new(DESK_CLOSE);incoming(&s,42,&q);assert(!s.clients[0].retiring && !s.clients[1].retiring);
    /* Refusal remains deliverable even with no free window records. */
    for(int i=2;i<DESKD_WINDOWS;i++) { s.clients[i].pid=42;s.clients[i].window=100+i; }
    q=desk_packet_new(DESK_OPEN);q.value=DESK_OPEN_CONNECTION;q.window=5;q.width=400;q.height=280;
    incoming(&s,42,&q);assert(s.child_reply[0].value==DESK_OPEN_CAPACITY && s.child_last_window[0]==5);
    for(int i=2;i<DESKD_WINDOWS;i++)memset(&s.clients[i],0,sizeof(s.clients[i]));
    sends=0;service(&s);assert(sent[0]==DESK_OPEN_REPLY && sent_values[0]==DESK_OPEN_CAPACITY && sent_windows[0]==5);
    alive=0;service(&s);assert(!s.clients[0].pid && !s.clients[1].pid && !s.cache_bytes && !s.children[0]);
    assert(!s.child_mode[0] && !s.child_last_window[0]);alive=1;sends=0;s.pointer_pid=0;
    puts("desktop-connection: PASS acknowledged admission/refusal, sibling capture/notices, release-before-close, monotonic IDs, allocation failure and process retirement");
}
static void shutdown_dialog_lifetime(void) {
    struct session s={.scene={.focus=-1,.apps=1},.children={42},.child_apps={DESK_APP_TEXT}};
    memset(&clipboard,0,sizeof(clipboard));alive=1;queue_full=0;send_allow=-1;sends=0;
    desk_damage_init(&s.damage,1280,800);
    struct desk_packet q=desk_packet_new(DESK_OPEN);q.value=DESK_OPEN_CONNECTION;q.width=400;q.height=280;
    incoming(&s,42,&q);service(&s);
    global_menu_action(&s,GLOBAL_EXIT);
    assert(s.exiting && s.clients[0].close_requested && !s.clients[0].closing);
    q.window=2;q.value|=DESK_OPEN_MODAL;q.generation=1;
    incoming(&s,42,&q);
    /* Saving/canceling needs the newly admitted owned dialog to remain alive. */
    assert(s.clients[1].parent_window==1 && !s.clients[1].closing);
    global_menu_action(&s,GLOBAL_EXIT);
    assert(!s.clients[1].close_requested && !s.clients[1].closing);
    service(&s);
    q=desk_packet_new(DESK_CANCEL_CLOSE);incoming(&s,42,&q);
    assert(!s.exiting && !s.clients[0].close_requested);
    /* A root arriving from a launch already in flight participates through
     * the same close request; it must not be forced closed either. */
    global_menu_action(&s,GLOBAL_EXIT);
    q=desk_packet_new(DESK_OPEN);q.window=3;q.value=DESK_OPEN_CONNECTION;q.width=400;q.height=280;
    incoming(&s,42,&q);
    assert(s.clients[2].pid==42 && s.clients[2].close_requested && !s.clients[2].closing);
    service(&s);
    alive=0;service(&s);assert(!s.cache_bytes && !s.children[0]);alive=1;sends=0;
    puts("desktop-shutdown: PASS owned prompts survive exit, repeated exit skips modal children, cancel retains desktop and late roots close cooperatively");
}
static void dynamic_registry(void) {
    static struct session s;
    s=(struct session){.dynamic=1,.registry_budget=DESK_REGISTRY_BYTES,.scene={.apps=1,.focus=-1}};
    desk_damage_init(&s.damage,1280,800);desk_menu_init(&s.scene.menu);desk_palette_init(&s.scene.launcher);
    alive=1;queue_full=0;send_allow=-1;sends=0;alloc_fail=0;
    /* Refuse at either mapping without changing capacity, tables or accounting. */
    for(unsigned fail=1;fail<=2;fail++) {
        registry_fail_at=registry_calls+fail;
        assert(!windows_grow(&s) && session_capacity(&s)==16 && !s.records && !s.registry_bytes && !registry_live);
    }
    registry_fail_at=0;assert(windows_grow(&s) && session_capacity(&s)==32);
    struct client *held=session_client(&s,31);struct desktop_window *held_window=desktop_window_at(&s.scene,31);
    held->pid=42;held->identity=32;held->clipboard.pid=42;clipboard.writer=&held->clipboard;
    assert(windows_grow(&s) && windows_grow(&s) && session_capacity(&s)==128);
    assert(session_client(&s,31)==held && desktop_window_at(&s.scene,31)==held_window && clipboard.writer==&held->clipboard);
    clipboard.writer=NULL;
    for(int i=0;i<128;i++) {
        struct client *c=session_client(&s,i);struct desktop_window *w=desktop_window_at(&s.scene,i);
        c->pid=42;c->window=i+1;c->identity=i+1;
        w->visible=1;w->z=i+1;w->frame=(struct desk_rect){40,80,282,285};
        snprintf(w->title,sizeof(w->title),"Document %03d",i+1);
    }
    s.window_serial=128;s.scene.focus=127;s.children[0]=42;s.child_mode[0]=2;
    global_menu_rebuild(&s);launcher_catalog(&s);
    assert(s.scene.menu.count>128 && s.scene.launcher.count==128+DESK_APP_COUNT);
    for(int i=0;i<128;i++) {
        unsigned found=0;for(unsigned j=0;j<s.scene.menu.count;j++)found+=s.scene.menu.items[j].id==GLOBAL_WINDOW+(unsigned)i+1;
        assert(found==1);
    }
    desk_text_buffer_set(&s.scene.launcher.text,"Document 128",12);desk_palette_filter(&s.scene.launcher);
    assert(s.scene.launcher.list.count==1 && s.scene.launcher.visible[0].id==128+DESK_APP_COUNT);
    assert(hit(&s,80,120)==127);
    global_menu_action(&s,GLOBAL_WINDOW+100);assert(s.scene.focus==99);
    assert(window_lifetime_valid(&s,99,100));
    session_client(&s,99)->identity=1000;
    assert(!window_lifetime_valid(&s,99,100));
    s.scene.focus=127;global_menu_action(&s,GLOBAL_WINDOW+100);assert(s.scene.focus==127);
    s.scene.launcher_open=1;s.overlay_return=99;s.overlay_identity=100;launcher_close(&s);assert(s.scene.focus==127);
    /* Child records grow independently; window records/clipboard remain stable. */
    for(int i=0;i<16;i++)s.children[i]=42+i;
    assert(child_slot(&s)==16 && children_capacity(&s)==32 && session_capacity(&s)==128);
    *session_children(&s,31)=999;
    uint64_t *child= session_children(&s,31);
    assert(children_grow(&s) && children_capacity(&s)==64 && session_children(&s,31)==child && *child==999);
    uint64_t before=s.registry_bytes;unsigned calls=registry_calls;
    s.registry_budget=before;
    assert(!windows_grow(&s) && !children_grow(&s) && registry_calls==calls && s.registry_bytes==before);
    s.children[0]=42;s.child_mode[0]=2;s.child_last_window[0]=128;s.child_reply[0]=(struct desk_packet){0};
    struct desk_packet open=desk_packet_new(DESK_OPEN);open.window=129;open.value=DESK_OPEN_CONNECTION;
    incoming(&s,42,&open);
    assert(s.child_reply[0].value==DESK_OPEN_CAPACITY && session_capacity(&s)==128 && session_client(&s,31)==held);
    s.registry_budget=DESK_REGISTRY_BYTES;
    for(unsigned fail=1;fail<=2;fail++) {
        registry_fail_at=registry_calls+fail;
        assert(!windows_grow(&s) && s.registry_bytes==before && session_capacity(&s)==128 && session_client(&s,31)==held);
    }
    registry_fail_at=0;
    assert(registry_live_bytes==s.registry_bytes);
    cache_free(&s);registry_destroy(&s);
    assert(!s.registry_bytes && !registry_live && !registry_live_bytes && session_capacity(&s)==16);
    puts("desktop-registry: PASS 128 stable records, independent child growth, full menu/search, stale identities, quota/mapping failure and complete retirement");
}

/* Exercise the compositor as the routing authority, independently of the
 * client receiver tests and the successful real-app VM route. */
static struct session *route_fixture(void) {
    static struct session s;
    memset(&s,0,sizeof(s));memset(&clipboard,0,sizeof(clipboard));
    alive=1;queue_full=0;send_allow=-1;sends=0;execute_result=-1;
    strcpy(executed_path,"untouched");executed_argument[0]=0;
    s.scene.apps=1;s.scene.focus=0;s.home=DESK_RAM_HOME;
    desk_damage_init(&s.damage,1280,800);
    for(int i=0;i<2;i++) {
        s.clients[i].pid=42;s.clients[i].window=i+1;s.clients[i].identity=i+1;
        s.clients[i].progress=now;
        s.scene.windows[i].visible=1;s.scene.windows[i].frame=(struct desk_rect){20+i*50,60,402,325};
    }
    s.scene.windows[0].application=DESK_APP_TEXT;
    s.scene.windows[1].application=DESK_APP_FILES;
    struct desk_packet p=desk_packet_new(DESK_APP_SERVICE);p.value=DESK_SERVICE_OPEN_DOCUMENT;
    incoming(&s,42,&p);assert(s.clients[0].service_bits==DESK_SERVICE_OPEN_DOCUMENT);
    p.window=2;incoming(&s,42,&p);assert(!s.clients[1].service_bits);
    return &s;
}
static void route_from_files(struct session *s,const char *path) {
    size_t length=strlen(path)+1;
    for(size_t offset=0;offset<length;) {
        struct desk_packet p=desk_packet_new(DESK_LAUNCH);p.window=2;p.value=DESK_APP_TEXT;
        size_t n=length-offset>sizeof(p.text)?sizeof(p.text):length-offset;
        p.serial=offset;p.height=n;p.width=offset+n==length;
        memcpy(p.text,path+offset,n);incoming(s,42,&p);offset+=n;
    }
}
static void document_routing_failures(void) {
    struct session *s=route_fixture();
    s->clients[0].input_head=DESK_INPUT_QUEUE-1; /* Chunk pair wraps the ring. */
    char path[160];memset(path,'a',sizeof(path));path[0]='/';path[159]=0;
    route_from_files(s,path);
    struct client *c=&s->clients[0];uint64_t request=c->open_request;
    assert(request && c->input_count==2 && !strcmp(c->open_path,path));
    char assembled[160];unsigned length=0;
    for(unsigned n=0;n<c->input_count;n++) {
        const struct desk_packet *p=&c->input[(c->input_head+n)%DESK_INPUT_QUEUE];
        assert(p->type==DESK_OPEN_DOCUMENT && p->window==1 && p->input_serial==request);
        assert(p->serial==length && p->width==(n+1==c->input_count));
        assert(p->height && p->height<=sizeof(p->text));
        memcpy(assembled+length,p->text,p->height);length+=p->height;
    }
    assert(length==sizeof(path) && !memcmp(assembled,path,sizeof(path)));
    strcpy(path,"/changed-after-send");assert(strcmp(c->open_path,path));
    assert(!route_document(s,DESK_APP_TEXT,"/second.txt") && c->open_request==request);
    struct desk_packet reply=desk_packet_new(DESK_OPEN_DOCUMENT_REPLY);reply.serial=request;
    incoming(s,777,&reply);assert(c->open_request==request); /* Different owner. */
    reply.window=2;incoming(s,42,&reply);assert(c->open_request==request); /* Same-PID sibling. */
    reply.window=1;reply.serial=request+1;incoming(s,42,&reply);assert(c->open_request==request);
    reply.serial=request;reply.value=2;incoming(s,42,&reply);assert(c->open_request==request);
    reply.value=0;incoming(s,42,&reply);assert(!c->open_request && !strcmp(executed_path,"untouched"));
    reply.value=1;incoming(s,42,&reply);assert(!strcmp(executed_path,"untouched")); /* Late duplicate. */

    /* Only a matching explicit refusal permits a new instance, with the copied
     * original path. A stale reply cannot refuse a newer request. */
    route_from_files(s,"/refused.txt");request=c->open_request;assert(request>reply.serial);
    incoming(s,42,&reply);assert(c->open_request==request && !strcmp(executed_path,"untouched"));
    reply.serial=request;incoming(s,42,&reply);
    assert(!c->open_request && !strcmp(executed_argument,"/refused.txt"));
    strcpy(executed_path,"untouched");incoming(s,42,&reply);assert(!strcmp(executed_path,"untouched"));

    /* Queue headroom refuses routing without consuming an ID or closing an
     * existing document. It must not partially enqueue a long path. */
    s=route_fixture();c=&s->clients[0];
    for(unsigned n=0;n<DESK_INPUT_QUEUE-4;n++)queue_input(s,0,desk_packet_new(DESK_KEY));
    unsigned queued=c->input_count;uint64_t serial=s->activation_serial;
    assert(!route_document(s,DESK_APP_TEXT,"/pressure.txt"));
    assert(c->input_count==queued && !c->open_request && !c->closing && s->activation_serial==serial);
    c->input_count=0;s->activation_serial=UINT64_MAX;
    assert(!route_document(s,DESK_APP_TEXT,"/exhausted.txt") && !c->open_request);
    s->activation_serial=0;assert(!route_document(s,DESK_APP_TEXT,"relative.txt"));
    char oversized[161];memset(oversized,'x',sizeof(oversized));oversized[0]='/';oversized[160]=0;
    assert(!route_document(s,DESK_APP_TEXT,oversized) && !s->activation_serial);

    /* A timeout is uncertain delivery, never permission to replay an Open.
     * Even a subsequent refusal of the old request cannot spawn a duplicate. */
    s=route_fixture();c=&s->clients[0];route_from_files(s,"/uncertain.txt");
    reply.serial=c->open_request;reply.value=1;now+=1001;service(s);
    assert(!c->open_request && !c->service_bits && !c->closing);
    assert(strstr(s->scene.notice,"not confirmed") && !strcmp(executed_path,"untouched"));
    incoming(s,42,&reply);assert(!strcmp(executed_path,"untouched"));
    assert(!route_document(s,DESK_APP_TEXT,"/next.txt"));

    /* Closing/exiting must not resurrect a refused target behind shutdown.
     * Incoming file-launch messages are ignored once desktop exit begins. */
    s=route_fixture();c=&s->clients[0];route_from_files(s,"/exit.txt");
    reply.serial=c->open_request;s->exiting=1;incoming(s,42,&reply);
    assert(!c->open_request && !strcmp(executed_path,"untouched"));
    route_from_files(s,"/after-exit.txt");assert(!c->open_request && !strcmp(executed_path,"untouched"));
    s=route_fixture();c=&s->clients[0];route_from_files(s,"/close.txt");
    reply.serial=c->open_request;c->closing=1;incoming(s,42,&reply);
    assert(!c->open_request && !strcmp(executed_path,"untouched"));
    assert(!route_document(s,DESK_APP_TEXT,"/closing.txt"));

    /* Fragment errors reset the sender's assembler without publishing an
     * incomplete path or spawning a fallback with untrusted trailing bytes. */
    s=route_fixture();c=&s->clients[0];
    struct desk_packet fragment=desk_packet_new(DESK_LAUNCH);fragment.window=2;
    fragment.value=DESK_APP_TEXT;fragment.height=5;memcpy(fragment.text,"/part",5);
    incoming(s,42,&fragment);assert(s->clients[1].launch_length==5 && !c->open_request);
    fragment.serial=4;fragment.width=1;fragment.height=2;memcpy(fragment.text,"x",2);
    incoming(s,42,&fragment);assert(!s->clients[1].launch_length && !c->open_request);
    fragment.serial=0;fragment.height=5;memcpy(fragment.text,"/a\0b",5);
    incoming(s,42,&fragment);assert(!c->open_request && !strcmp(executed_path,"untouched"));
    c->parent_window=99;assert(!route_document(s,DESK_APP_TEXT,"/dialog.txt"));
    c->parent_window=0;c->service_bits=0;assert(!route_document(s,DESK_APP_TEXT,"/unregistered.txt"));

    s=route_fixture();route_from_files(s,"/retired.txt");forget(s,0);
    assert(!s->clients[0].pid && strstr(s->scene.notice,"before confirming"));
    assert(!strcmp(executed_path,"untouched"));
    sends=0;execute_result=-1;
    puts("desktop-document-route: PASS copied chunks, authenticated replies, pressure, uncertainty and shutdown without replay");
}

static void persistent_loop_timing(void) {
    struct loop_trace t={0};
    loop_trace_end(&t,100,0,0,0);assert(!t.samples);
    loop_trace_begin(&t,100,2,41,1);
    loop_trace_mark(&t,LOOP_WAIT,200);
    loop_trace_end(&t,500100,3,0,0);
    assert(t.worst.gap==500000 && !t.over_limit && t.worst.stage_us[LOOP_WAIT]==499900);
    uint64_t at=600100;
    loop_trace_begin(&t,at,3,42,3);
    for(unsigned stage=1;stage<LOOP_STAGE_COUNT;stage++) {
        at+=stage*10;loop_trace_mark(&t,stage,at);
    }
    t.current.wait=(struct loop_wait_record){.valid=1,.begin=at,.sleep_begin=at+10,
        .sleep_end=at+640000,.end=at+640010,.scheduler={.version=1,.ready_us=620000,.max_ready_us=620000}};
    at+=650000;loop_trace_end(&t,at,4,0,0);
    struct loop_record retained=t.worst;
    assert(retained.sequence==2 && retained.cycle==3 && retained.pid==42 && retained.phase==3);
    assert(retained.end_cycle==4 && retained.end_phase==0 && t.over_limit==1);
    uint64_t sum=0;for(unsigned n=0;n<LOOP_STAGE_COUNT;n++)sum+=retained.stage_us[n];
    assert(sum==retained.gap && sum==retained.end-retained.start);
    /* Shorter loops and repeated emission cannot erase a historical maximum. */
    capture_loop_log=1;
    for(unsigned n=0;n<1000;n++) {
        loop_trace_begin(&t,at,4+n,43+n,0);loop_trace_mark(&t,LOOP_MESSAGES,at+10);
        at+=1000;loop_trace_end(&t,at,5+n,0,0);loop_trace_report(&t,0);
        assert(!memcmp(&retained,&t.worst,sizeof(retained)) && t.over_limit==1);
        assert(strstr(captured_loop_log,"wait_us=650000 final=0\n"));
        assert(strstr(captured_loop_log,"desktop-loop-wait: seq=2 valid=1"));
        assert(strstr(captured_loop_log,"ready_us=620000"));
    }
    assert(t.samples==1002 && !t.invalid);
    /* A final early break still accounts the unfinished stage and full gap. */
    loop_trace_begin(&t,at,1004,2000,1);loop_trace_mark(&t,LOOP_RENDER,at+100);
    at+=800000;loop_trace_end(&t,at,1004,2000,1);loop_trace_report(&t,1);
    assert(t.worst.gap==800000 && t.worst.stage_us[LOOP_RENDER]==799900 && t.over_limit==2);
    assert(strstr(captured_loop_log,"render_us=799900 post_us=0 wait_us=0 final=1\n"));
    retained=t.worst;
    loop_trace_begin(&t,at,0,0,0);loop_trace_mark(&t,LOOP_INPUT,at-1);
    loop_trace_end(&t,at+900000,0,0,0);assert(t.invalid==1 && !memcmp(&retained,&t.worst,sizeof(retained)));
    loop_trace_begin(&t,at,0,0,0);loop_trace_end(&t,at-1,0,0,0);assert(t.invalid==2);
    memset(&t,255,sizeof(t));loop_trace_report(&t,1);
    assert(strstr(captured_loop_log,"wait_us=18446744073709551615 final=1\n"));
    assert(strstr(captured_loop_log,"desktop-loop-sched: seq=18446744073709551615"));
    assert(strstr(captured_loop_log,"late_ticks=18446744073709551615 final=1"));
    capture_loop_log=0;
    puts("desktop-loop-timing: PASS exact bound, conserved stages, 1000-loop retention, periodic/final records, partial exit, invalid clock and bounded formatting");
}
static void asynchronous_loop_report(void) {
    struct lifecycle_worker w={0};struct loop_trace trace={0};struct operation_trace operations={0};
    capture_loop_log=1;captured_loop_log[0]=0;
    assert(!life_report_submit(&w,&trace,&operations));
    loop_trace_begin(&trace,100,1,42,2);loop_trace_end(&trace,100100,1,42,2);
    assert(life_report_submit(&w,&trace,&operations));assert(!captured_loop_log[0]);
    loop_trace_begin(&trace,200000,2,43,1);loop_trace_end(&trace,800001,2,43,1);
    assert(!life_report_submit(&w,&trace,&operations));
    life_report_work(&w);assert(life_state(&w.report_state)==LIFE_IDLE);
    assert(strstr(captured_loop_log,"gap_us=100000 cycle=1 pid=42 phase=2"));
    assert(life_report_submit(&w,&trace,&operations));life_report_work(&w);
    assert(strstr(captured_loop_log,"over_limit=1") && strstr(captured_loop_log,"gap_us=600001 cycle=2 pid=43 phase=1"));
    capture_loop_log=0;puts("desktop-loop-report: PASS asynchronous immutable snapshot, busy refusal, retry and retained limit violation");
}
static void retained_operations(void) {
    struct lifecycle_worker w={0};struct session s={.worker=&w,.exiting=1,.exit_started_us=100,.launch_requests=3,.launch_started=1};
    s.capture.request.id=7;s.capture.started_us=100;s.capture.last_progress_us=250;s.capture.busy=1;s.capture.phase=CAPTURE_TEMP_VERIFY;
    s.capture.phase_us[CAPTURE_WRITE]=80;s.capture.sent=400;s.capture.progress_bytes=300;
    w.launch[0]=(struct life_launch){.state=LIFE_QUEUED,.application=DESK_APP_FILES,.queued_us=200};
    w.reap[0]=(struct life_reap){.state=LIFE_QUEUED,.pid=42,.queued_us=200,.begin_us=999,.end_us=999};
    s.clients[0]=(struct client){.pid=42,.window=2,.close_requested=1};
    struct operation_trace t=operation_snapshot(&s,500);
    assert(t.queued==1 && t.launch_age_us==300 && t.oldest_app==DESK_APP_FILES);
    assert(t.reap_queued==1 && t.reap_pid==42 && t.reap_queue_age_us==300 && !t.reap_run_age_us && !t.reap_completed_us);
    w.reap[0].begin_us=300;life_publish(&w.reap[0].state,LIFE_RUNNING);
    struct operation_trace running=operation_snapshot(&s,500);
    assert(running.reap_running==1 && !running.reap_queued && running.reap_run_age_us==200 && !running.reap_completed_us);
    w.reap[0].end_us=450;life_publish(&w.reap[0].state,LIFE_DONE);
    struct operation_trace done=operation_snapshot(&s,500);
    assert(done.reap_done==1 && !done.reap_running && done.reap_completed_us==150);
    assert(t.exiting && t.exit_age_us==400 && t.close_pid==42 && t.close_requested==1);
    assert(t.capture.id==7 && t.capture.busy && t.capture.age_us==400 && t.capture.progress_age_us==250 && t.capture.phases[CAPTURE_WRITE]==80);
    struct loop_trace trace={0};loop_trace_begin(&trace,1,0,0,0);loop_trace_end(&trace,2,0,0,0);
    assert(life_report_submit(&w,&trace,&t));s.capture.sent=900;w.launch[0].state=LIFE_IDLE;
    w.reap[0].state=LIFE_IDLE;w.reap[0].queued_us=499;
    assert(w.operations.capture.sent==400 && w.operations.queued==1); /* Frozen until worker consumes. */
    assert(w.operations.reap_queued==1 && w.operations.reap_queue_age_us==300 && !w.operations.reap_run_age_us);
    puts("desktop-operations: PASS pending capture/launch/close ages and immutable retained snapshot");
}
static struct srv_key_event navigation_event(unsigned usage,unsigned modifiers,unsigned flags) {
    return (struct srv_key_event){.usage=usage,.modifiers=modifiers,.flags=flags};
}
static void window_navigation(void) {
    static struct session s;
    s=(struct session){.dynamic=1,.registry_budget=DESK_REGISTRY_BYTES,.scene={.apps=1,.focus=-1}};
    desk_damage_init(&s.damage,1280,800);desk_menu_init(&s.scene.menu);desk_palette_init(&s.scene.launcher);
    while(session_capacity(&s)<128)assert(windows_grow(&s));
    for(int i=0;i<80;i++) {
        *session_client(&s,i)=(struct client){.pid=42,.identity=i+1,.window=i+1};
        *desktop_window_at(&s.scene,i)=(struct desktop_window){.visible=1,.application=i%2?DESK_APP_FILES:DESK_APP_TEXT,.z=i+1,.frame={32,64,602,365}};
        snprintf(desktop_window_at(&s.scene,i)->title,96,"Document %d",i+1);
    }
    s.scene.stack_serial=80;focus(&s,79);
    struct srv_key_event key=navigation_event(SRV_KEY_TAB,SRV_KEY_ALT,SRV_KEY_DOWN_EVENT);
    assert(navigation_key(&s,&key) && s.switching && s.scene.focus==-1 && s.scene.launcher.list.count==80);
    assert(s.scene.launcher.visible[s.scene.launcher.list.selected].id==79+DESK_APP_COUNT);
    uint64_t first=s.scene.launcher.visible[0].id;global_menu_rebuild(&s);focus(&s,3);
    assert(s.overlay_return==79 && s.scene.launcher.visible[0].id==first); /* No focus steal or reorder. */
    for(int n=0;n<78;n++)assert(navigation_key(&s,&key));
    assert(s.scene.launcher.visible[s.scene.launcher.list.selected].id==1+DESK_APP_COUNT);
    key.flags=0;key.usage=0xe2;key.modifiers=0;
    assert(navigation_key(&s,&key) && !s.switching && s.scene.focus==0); /* Commit on modifier release. */
    key=navigation_event(SRV_KEY_TAB,SRV_KEY_ALT,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);
    assert(s.scene.launcher.visible[s.scene.launcher.list.selected].id==80+DESK_APP_COUNT); /* MRU previous. */
    key=navigation_event(SRV_KEY_ESCAPE,SRV_KEY_ALT,SRV_KEY_DOWN_EVENT);assert(navigation_key(&s,&key) && s.scene.focus==0);
    key=navigation_event(SRV_KEY_TAB,SRV_KEY_ALT|SRV_KEY_SHIFT,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);
    assert(s.scene.launcher.visible[s.scene.launcher.list.selected].id==2+DESK_APP_COUNT);
    key=navigation_event(0,0,SRV_KEY_CANCEL);navigation_key(&s,&key);assert(!s.switching && s.scene.focus==0);
    /* A reused slot cannot activate the old snapshot identity. */
    key=navigation_event(SRV_KEY_TAB,SRV_KEY_ALT,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);
    session_client(&s,79)->identity=1000;
    key=navigation_event(0xe2,0,0);navigation_key(&s,&key);assert(s.scene.focus==0);
    /* Same-app order visits all roots in both directions, skipping other apps. */
    key=navigation_event(0x35,SRV_KEY_META,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);assert(s.scene.focus==2);
    navigation_key(&s,&key);assert(s.scene.focus==4);
    key.modifiers|=SRV_KEY_SHIFT;navigation_key(&s,&key);assert(s.scene.focus==2);
    /* A modal is one task with its parent; minimization hides the whole group. */
    *session_client(&s,80)=(struct client){.pid=42,.identity=81,.window=81,.parent_window=3};
    *desktop_window_at(&s.scene,80)=(struct desktop_window){.visible=1,.dialog=1,.application=DESK_APP_TEXT,.frame={80,100,400,300}};
    focus(&s,2);assert(s.scene.focus==80);
    unsigned checked=0;for(unsigned n=0;n<s.scene.menu.count;n++)if(s.scene.menu.items[n].id==GLOBAL_WINDOW+3)checked=(s.scene.menu.items[n].flags&DESK_MENU_CHECKED)!=0;
    assert(checked);launcher_catalog(&s);assert(s.scene.launcher.count==80+DESK_APP_COUNT); /* No extra modal task. */
    unsigned input_before=session_client(&s,80)->input_count;
    key=navigation_event(0x1a,SRV_KEY_META,SRV_KEY_DOWN_EVENT);assert(navigation_key(&s,&key));
    assert(session_client(&s,80)->close_requested && session_client(&s,80)->input_count==input_before+1);
    assert(session_client(&s,80)->input[(session_client(&s,80)->input_head+input_before)%DESK_INPUT_QUEUE].type==DESK_CLOSE_REQUEST);
    navigation_key(&s,&key);assert(session_client(&s,80)->input_count==input_before+1); /* Repeated press deduplicated. */
    key.modifiers=SRV_KEY_CTRL;assert(!navigation_key(&s,&key));
    session_client(&s,80)->close_requested=0;
    key=navigation_event(0x10,SRV_KEY_META,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);
    assert(desktop_window_at(&s.scene,2)->minimized && desktop_window_at(&s.scene,80)->minimized && s.scene.focus==4);
    focus(&s,2);assert(s.scene.focus==80 && !desktop_window_at(&s.scene,2)->minimized);
    forget(&s,80);assert(s.scene.focus==2); /* Modal closure restores its own parent. */
    focus(&s,6);focus(&s,8);desktop_window_at(&s.scene,6)->minimized=1;
    forget(&s,8);assert(s.scene.focus==2 && desktop_window_at(&s.scene,6)->minimized); /* No hidden resurrection. */
    key=navigation_event(SRV_KEY_F2,SRV_KEY_CTRL,SRV_KEY_DOWN_EVENT);assert(navigation_key(&s,&key) && s.scene.menu.open==0);
    key=navigation_event(SRV_KEY_ESCAPE,0,SRV_KEY_DOWN_EVENT);assert(desk_menu_key(&s.scene.menu,&key).consumed);
    for(int j=0;j<session_capacity(&s);j++)if(session_client(&s,j)->pid)desktop_window_at(&s.scene,j)->minimized=1;
    char minimized_label[96];window_label(&s,2,minimized_label,sizeof(minimized_label));assert(!strncmp(minimized_label,"[Minimized] ",12));
    memset(desktop_window_at(&s.scene,2)->title,'a',94);memcpy(desktop_window_at(&s.scene,2)->title+82,"\xc3\xa9",3);
    window_label(&s,2,minimized_label,sizeof(minimized_label));assert(desk_text_utf8_valid(minimized_label,strlen(minimized_label)) && strlen(minimized_label)==94);
    focus_best(&s);assert(s.scene.focus==-1);
    key=navigation_event(SRV_KEY_TAB,SRV_KEY_ALT,SRV_KEY_DOWN_EVENT);navigation_key(&s,&key);
    assert(s.switching && s.scene.launcher.choose_only && !s.scene.launcher.query.enabled && s.scene.launcher.list.count==79);
    key=navigation_event(0xe2,0,0);navigation_key(&s,&key);assert(s.scene.focus>=0 && !desktop_window_at(&s.scene,s.scene.focus)->minimized);
    registry_destroy(&s);assert(!registry_live);
    puts("desktop-navigation: PASS 80-entry stable MRU/reverse/cancel, stale identity refusal, same-app cycle, grouped minimize, focus fallback and keyboard menu");
}
int main(void) {
    seeded_cache_reuse();
    retained_operations();window_navigation();
    persistent_loop_timing();asynchronous_loop_report();
    /* A timeout breaks before the next loop header. Its final interval still
     * counts against the same responsiveness limit. */
    uint64_t loop_last=0,loop_max=0;
    assert(loop_sample(&loop_last,&loop_max,100)==0);
    assert(loop_sample(&loop_last,&loop_max,100100)==100000);
    assert(loop_sample(&loop_last,&loop_max,800100)==700000);
    assert(loop_max==700000);
    buffer_budget();resize_refusal();
    idle_admission();
    battery_ui_notifications();
    pointer_cancel_keeps_keyboard();
    struct session s={.scene={.focus=-1},.children={42}};
    assert(cache_init(&s)==0 && !s.cache_bytes);
    desk_damage_init(&s.damage,800,600);
    struct desk_packet p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    strcpy(p.text,"Test"); incoming(&s,42,&p); service(&s);
    assert(s.clients[0].awaiting && !s.clients[0].configure && sends==1 && sent[0]==DESK_CONFIGURE);
    assert(s.clients[0].clipboard.pid==42 && s.clients[0].clipboard.window==DESK_DEFAULT_WINDOW);
    struct desk_packet foreign=desk_packet_new(DESK_CLOSE);foreign.window=2;
    incoming(&s,42,&foreign);assert(s.clients[0].pid==42);
    p=desk_packet_new(DESK_COMMIT); p.generation=1; p.surface=7; p.serial=1; p.width=400; p.height=280; p.input_serial=5;
    incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels && !s.clients[0].awaiting && s.clients[0].present_input==5);
    const uint32_t *front=s.scene.windows[0].pixels;
    assert(s.cache_bytes==400u*280*4 && allocations-frees==1);

    queue_full=1; service(&s);
    assert(s.clients[0].release.type==DESK_RELEASE && sends==1);
    queue_full=0; service(&s); assert(sent[1]==DESK_RELEASE);
    p.serial=2; incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels!=front && allocations-frees==2);
    front=s.scene.windows[0].pixels; service(&s);
    configure(&s,0,450,300);
    p.serial=3; p.input_serial=6; incoming(&s,42,&p); /* Old generation still gets released. */
    assert(s.scene.windows[0].pixels==front && s.clients[0].awaiting);
    assert(s.clients[0].present_input==5); /* Stale frame cannot acknowledge text. */
    service(&s); assert(sent[3]==DESK_RELEASE && sent[4]==DESK_CONFIGURE);
    p.generation=2; p.width=450; p.height=300; p.serial=4;
    copy_failure=1; incoming(&s,42,&p);
    assert(s.clients[0].closing && s.scene.windows[0].pixels==front && front[0]==0x123456);
    assert(s.clients[0].present_input==5); /* Nor can a partial copy. */
    service(&s); assert(sent[5]==DESK_RELEASE && sent[6]==DESK_CLOSE);
    now+=501; service(&s);
    assert(killed==1 && !s.clients[0].pid && !s.children[0] && !s.cache_bytes);
    /* A live process that never answers CONFIGURE is also bounded. */
    alive=1; s.children[0]=42; copy_failure=0; reaped=1;
    p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    incoming(&s,42,&p); service(&s); now+=501; service(&s);
    assert(killed==2 && !s.clients[0].pid && !s.children[0] && !s.cache_bytes);
    cache_free(&s); assert(allocations==frees);
    /* Scroll targets content under the cursor, keeps focus, and retries the
     * same recipient when the transport queue is full. */
    cache_free(&s);memset(&s,0,sizeof(s)); s.scene.focus=1;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW; s.scene.windows[0].visible=1;
    s.scene.windows[0].frame=(struct desk_rect){20,60,400,300};
    alive=1; queue_full=1;
    scroll_at(&s,60,70,0,120); assert(!s.clients[0].input_count); /* Title bar. */
    scroll_at(&s,60,150,0,120); scroll_at(&s,60,150,0,40);
    scroll_at(&s,60,150,0,-20);
    assert(s.scene.focus==1 && s.clients[0].input_count==2);
    assert(s.clients[0].input[0].y==160 && s.clients[0].input[1].y==-20);
    service(&s); assert(s.clients[0].input_count==2);
    queue_full=0; service(&s); assert(!s.clients[0].input_count && sent[sends-1]==DESK_SCROLL);
    /* Reproduce the former scroll/hover alternation while the client cannot
     * accept IPC. The app must survive and later receive its ordered release. */
    s.scene.apps=1;queue_full=1;
    for(unsigned n=0;n<4*DESK_INPUT_QUEUE;n++) {
        scroll_at(&s,60,150,0,n%2?40:-40);
        pointer_at(&s,60,150,0,0);
    }
    assert(s.clients[0].input_count==DESK_INPUT_QUEUE && s.clients[0].input_dropped && !s.clients[0].closing);
    p=desk_packet_new(DESK_POINTER);p.value=0;queue_input(&s,0,p);
    assert(s.clients[0].input_count==DESK_INPUT_QUEUE && !s.clients[0].closing);
    unsigned tail_index=(s.clients[0].input_head+s.clients[0].input_count-1)%DESK_INPUT_QUEUE;
    assert(s.clients[0].input[tail_index].type==DESK_POINTER && !s.clients[0].input[tail_index].value);
    queue_full=0;sends=0;
    while(s.clients[0].input_count) service(&s);
    assert(!s.clients[0].closing && sent[sends-1]==DESK_POINTER);
    /* Same-direction wheel packets at one target remain one bounded entry. */
    for(unsigned n=0;n<1024;n++) scroll_at(&s,60,150,0,1);
    assert(s.clients[0].input_count==1 && s.clients[0].input[s.clients[0].input_head].y==1024);
    scroll_at(&s,70,150,0,1);assert(s.clients[0].input_count==2);
    s.scene.apps=0;
    scroll_at(&s,60,150,0,80); forget(&s,0); assert(!s.clients[0].input_count);
    cache_free(&s);memset(&s,0,sizeof(s)); s.scene.focus=0;
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW; s.clients[0].generation=1; s.scene.windows[0].visible=1;
    s.scene.windows[0].frame=(struct desk_rect){20,60,400,300};
    pointer_at(&s,400,200,MOUSE_LEFT,1);
    pointer_at(&s,780,500,MOUSE_LEFT,0); pointer_at(&s,790,510,MOUSE_LEFT,0);
    assert(s.pointer_pid==42 && s.clients[0].input_count==2);
    assert(s.clients[0].input[0].value==(DESK_POINTER_LEFT|DESK_POINTER_PRESS));
    assert(s.clients[0].input[0].x==379 && s.clients[0].input[0].y==96);
    assert(s.clients[0].input[1].x==769 && s.clients[0].input[1].y==406);
    pointer_at(&s,790,510,0,0);
    assert(!s.pointer_pid && s.clients[0].input_count==3 && !s.clients[0].input[2].value);
    queue_full=1; service(&s); assert(s.clients[0].input_count==3);
    queue_full=0; service(&s); service(&s); service(&s); assert(!s.clients[0].input_count);
    pointer_at(&s,400,200,MOUSE_LEFT,1); pointer_at(&s,400,200,0,0);
    cancel_input(&s,0);
    assert(!s.pointer_pid && !s.clients[0].input_count && s.clients[0].pointer_cancel_pending);
    service(&s);
    pointer_at(&s,400,200,MOUSE_LEFT,1); focus(&s,-1);
    assert(!s.pointer_pid && s.pointer_blocked && s.clients[0].input_count==2);
    unsigned tail=(s.clients[0].input_head+1)%DESK_INPUT_QUEUE;
    assert(s.clients[0].input[tail].value==DESK_POINTER_CANCEL);
    pointer_at(&s,400,200,MOUSE_LEFT,0); assert(!s.pointer_pid);
    pointer_at(&s,400,200,0,0); assert(!s.pointer_blocked);
    focus(&s,0); pointer_at(&s,400,200,MOUSE_LEFT,1);
    configure(&s,0,400,300); assert(!s.pointer_pid && s.pointer_blocked);
    pointer_at(&s,400,200,0,0); s.clients[0].awaiting=0;
    pointer_at(&s,400,200,MOUSE_LEFT,1); forget(&s,0);
    assert(!s.pointer_pid && s.pointer_blocked && !s.clients[0].input_count);
    cache_free(&s);memset(&s,0,sizeof(s)); s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW; s.pointer_pid=42;s.pointer_window=DESK_DEFAULT_WINDOW;
    p=desk_packet_new(DESK_POINTER); p.value=DESK_POINTER_PRESS|DESK_POINTER_LEFT;
    for (int i=0;i<DESK_INPUT_QUEUE;i++) queue_input(&s,0,p);
    assert(s.clients[0].input_count==DESK_INPUT_QUEUE && !s.clients[0].closing);
    queue_input(&s,0,p);
    assert(s.clients[0].closing && !s.pointer_pid && s.pointer_blocked);
    /* Slow rendering still drains an ordered batch. Partial IPC capacity keeps
     * the first unsent edge at the head, without duplication or reordering. */
    cache_free(&s);memset(&s,0,sizeof(s));s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;sends=0;
    for(unsigned i=0;i<24;i++) { p=desk_packet_new(DESK_KEY);p.serial=i+1;queue_input(&s,0,p); }
    service(&s);assert(sends==DESK_INPUT_BATCH&&s.clients[0].input_count==24-DESK_INPUT_BATCH);
    send_allow=3;service(&s);assert(sends==DESK_INPUT_BATCH+3);
    unsigned blocked_count=s.clients[0].input_count;service(&s);assert(s.clients[0].input_count==blocked_count);
    send_allow=-1;while(s.clients[0].input_count) service(&s);
    assert(sends==24&&!s.clients[0].closing);
    for(unsigned i=0;i<24;i++) assert(sent[i]==DESK_KEY&&sent_serials[i]==i+1);
    /* Reproduce a compositor pause longer than the old queue-age timeout.
     * A healthy client can accept this ordered key and must not be closed. */
    cache_free(&s);memset(&s,0,sizeof(s));s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;sends=0;
    p=desk_packet_new(DESK_KEY);p.serial=101;queue_input(&s,0,p);
    now+=601;service(&s);
    assert(sends==1 && sent[0]==DESK_KEY && sent_serials[0]==101 && !s.clients[0].closing);
    /* First failed attempt after a long server pause gets a fresh deadline.
     * Partial progress also resets the deadline, preserving ordered edges. */
    queue_full=1;queue_input(&s,0,p);now+=601;service(&s);
    assert(s.clients[0].input_blocked && !s.clients[0].closing);
    now+=400;service(&s);assert(!s.clients[0].closing);
    queue_full=0;now+=200;service(&s);
    assert(!s.clients[0].input_count && !s.clients[0].input_blocked && !s.clients[0].closing);
    /* A client which really refuses input across repeated service attempts
     * still reaches the bounded close path; the change is not an infinite wait. */
    queue_full=1;queue_input(&s,0,p);service(&s);now+=501;service(&s);
    assert(s.clients[0].closing);queue_full=0;
    cache_free(&s);memset(&s,0,sizeof(s));s.scene.apps=1;s.scene.focus=0;desk_damage_init(&s.damage,1280,800);
    s.clients[0].pid=42;s.clients[0].window=DESK_DEFAULT_WINDOW;s.scene.windows[0].visible=1;s.scene.windows[0].application=DESK_APP_TEXT;
    s.scene.windows[0].frame=(struct desk_rect){32,64,602,365};
    struct desk_rect original=s.scene.windows[0].frame;
    maximize(&s,0);assert(s.scene.windows[0].maximized && s.clients[0].generation==1);
    assert(s.scene.windows[0].frame.x==8 && s.scene.windows[0].frame.w==1264 && s.scene.windows[0].frame.h==668);
    maximize(&s,0);assert(!s.scene.windows[0].maximized && !memcmp(&original,&s.scene.windows[0].frame,sizeof(original)));
    arrange_window(&s,0,DESKTOP_TILE_LEFT);assert(s.scene.windows[0].maximized && s.scene.windows[0].frame.x==8 && !memcmp(&original,&s.scene.windows[0].restore,sizeof(original)));
    uint64_t arranged_generation=s.clients[0].generation;arrange_window(&s,0,DESKTOP_TILE_LEFT);assert(s.clients[0].generation==arranged_generation);
    s.pointer_pid=42;s.pointer_window=DESK_DEFAULT_WINDOW;arrange_window(&s,0,DESKTOP_TILE_RIGHT);assert(!s.pointer_pid && s.pointer_blocked);s.pointer_blocked=0;assert(s.scene.windows[0].frame.x>s.damage.width/2 && !memcmp(&original,&s.scene.windows[0].restore,sizeof(original)));
    maximize(&s,0);assert(!s.scene.windows[0].maximized && !memcmp(&original,&s.scene.windows[0].frame,sizeof(original)));
    arrange_window(&s,0,DESKTOP_TILE_LEFT);arrange_window(&s,0,DESKTOP_CENTER);assert(!s.scene.windows[0].maximized && s.scene.windows[0].frame.w==original.w && s.scene.windows[0].frame.h==original.h);
    s.scene.windows[0].frame=original;
    desk_damage_init(&s.damage,1440,900);
    maximize(&s,0);
    assert(s.scene.windows[0].frame.x==8 && s.scene.windows[0].frame.y==52 && s.scene.windows[0].frame.w==1424 && s.scene.windows[0].frame.h==768);
    maximize(&s,0);assert(!memcmp(&original,&s.scene.windows[0].frame,sizeof(original)));
    desk_damage_init(&s.damage,1280,800);
    s.scene.windows[0].frame=original;
    struct srv_key_event placement_key={.usage=SRV_KEY_LEFT,.modifiers=SRV_KEY_META|SRV_KEY_ALT,.flags=SRV_KEY_DOWN_EVENT};
    assert(arrange_shortcut(&placement_key)==GLOBAL_TILE_LEFT);placement_key.flags|=SRV_KEY_REPEAT;assert(!arrange_shortcut(&placement_key));placement_key.flags=0;assert(!arrange_shortcut(&placement_key));
    placement_key.flags=SRV_KEY_DOWN_EVENT;placement_key.modifiers|=SRV_KEY_CTRL;assert(!arrange_shortcut(&placement_key));
    minimize(&s,0);assert(s.scene.windows[0].minimized && s.scene.focus==-1 && hit(&s,80,120)==-1);
    focus(&s,0);assert(!s.scene.windows[0].minimized && s.scene.focus==0);
    /* Dock clicks select actual main windows, cycle all siblings and preserve
     * modal focus. A closing app must not become a duplicate launch. */
    s.scene.stack_serial=10;s.scene.windows[0].z=10;s.clients[0].identity=101;
    s.clients[1].pid=42;s.clients[1].window=2;s.clients[1].identity=102;
    s.scene.windows[1]=(struct desktop_window){.visible=1,.application=DESK_APP_TEXT,.frame=original,.z=8,.minimized=1};
    s.clients[2].pid=42;s.clients[2].window=3;s.clients[2].identity=103;
    s.scene.windows[2]=(struct desktop_window){.visible=1,.application=DESK_APP_TEXT,.frame=original,.z=9};
    assert(dock_activate(&s,DESK_APP_TEXT)==1 && s.scene.focus==1 && !s.scene.windows[1].minimized);
    assert(dock_activate(&s,DESK_APP_TEXT)==2 && s.scene.focus==2);
    assert(dock_activate(&s,DESK_APP_TEXT)==0 && s.scene.focus==0);
    s.clients[3].pid=42;s.clients[3].window=4;s.clients[3].identity=104;s.clients[3].parent_window=2;
    s.scene.windows[3]=(struct desktop_window){.visible=1,.dialog=1,.application=DESK_APP_TEXT,.frame=original,.z=20,.minimized=1};
    assert(dock_activate(&s,DESK_APP_TEXT)==1 && s.scene.focus==3 && !s.scene.windows[3].minimized);
    s.clients[1].closing=1;assert(dock_activate(&s,DESK_APP_TEXT)==2);
    char prior_executed[sizeof(executed_path)];strcpy(prior_executed,executed_path);
    s.clients[0].closing=s.clients[2].closing=1;strcpy(executed_path,"dock-unchanged");
    assert(dock_activate(&s,DESK_APP_TEXT)==-1 && !strcmp(executed_path,"dock-unchanged"));
    s.clients[0].closing=s.clients[2].closing=0;strcpy(executed_path,prior_executed);
    desk_palette_init(&s.scene.launcher);s.scene.launcher_open=0;s.launcher_application=DESK_APP_TEXT;
    launcher_catalog(&s);assert(s.scene.launcher.count==3); /* Two main windows plus New app; no dialog/closing entry. */
    s.launcher_application=0;
    for(unsigned i=1;i<4;i++){memset(&s.clients[i],0,sizeof(s.clients[i]));memset(&s.scene.windows[i],0,sizeof(s.scene.windows[i]));}
    focus(&s,0);
    s.preferences.value=desk_preferences_default();
    p=desk_packet_new(DESK_BRIGHTNESS);p.serial=1;p.value=50;
    unsigned bc=brightness_calls;
    incoming(&s,42,&p);
    assert(brightness_calls==bc); /* Existing client is not Settings. */
    int old_app=s.scene.windows[0].application,old_focus=s.scene.focus;
    s.scene.windows[0].application=DESK_APP_SETTINGS;s.scene.focus=0;
    incoming(&s,42,&p);assert(brightness_calls==bc+1 && s.clients[0].brightness_reply.value==50);
    s.scene.focus=-1;incoming(&s,42,&p);assert(brightness_calls==bc+1);
    p.value=0;incoming(&s,42,&p);assert(brightness_calls==bc+2 && s.clients[0].brightness_reply.value==14);
    p.value=101;incoming(&s,42,&p);assert(brightness_calls==bc+2);
    int was_configuring=s.clients[0].configure;s.clients[0].configure=0;
    queue_full=1;service(&s);assert(s.clients[0].brightness_reply.type==DESK_BRIGHTNESS);
    queue_full=0;service(&s);assert(!s.clients[0].brightness_reply.type);
    s.clients[0].configure=was_configuring;
    s.clients[0].brightness_reply.type=0;s.scene.windows[0].application=old_app;s.scene.focus=old_focus;
    p=desk_packet_new(DESK_AUDIO);p.serial=4;p.value=SRV_AUDIO_TEST;
    incoming(&s,42,&p);assert(!audio_calls && s.clients[0].audio_reply.value==1);
    s.scene.windows[0].application=DESK_APP_SETTINGS;s.scene.focus=0;
    incoming(&s,42,&p);assert(audio_calls==1 && !s.clients[0].audio_reply.value);
    s.scene.focus=-1;incoming(&s,42,&p);assert(audio_calls==1);
    p.value=SRV_AUDIO_QUERY;incoming(&s,42,&p);assert(audio_calls==1);
    s.clients[0].configure=0;queue_full=1;service(&s);assert(s.clients[0].audio_reply.type==DESK_AUDIO);
    queue_full=0;service(&s);assert(!s.clients[0].audio_reply.type);
    s.clients[0].configure=was_configuring;s.scene.windows[0].application=old_app;s.scene.focus=old_focus;
    p=desk_packet_new(DESK_PREFERENCES_SET);p.value=DESK_PREF_TEXT;
    struct desk_preferences changed=desk_preferences_default();changed.text_size=20;memcpy(p.text,&changed,sizeof(changed));
    incoming(&s,42,&p);assert(s.preferences.value.text_size==16);
    s.scene.windows[0].application=DESK_APP_SETTINGS;incoming(&s,42,&p);assert(s.preferences.value.text_size==20 && s.clients[0].preferences_dirty);
    changed.disable_while_typing=1;memcpy(p.text,&changed,sizeof(changed));p.value=DESK_PREF_DISABLE_WHILE_TYPING;
    incoming(&s,42,&p);assert(s.preferences.value.disable_while_typing==1 && pointer_options==(SRV_POINTER_ACCELERATION|SRV_POINTER_DISABLE_WHILE_TYPING|SRV_POINTER_TAPS));
    changed.disable_while_typing=0;memcpy(p.text,&changed,sizeof(changed));incoming(&s,42,&p);
    assert(!s.preferences.value.disable_while_typing && pointer_options==(SRV_POINTER_ACCELERATION|SRV_POINTER_TAPS));
    changed.tap_to_click=0;memcpy(p.text,&changed,sizeof(changed));p.value=DESK_PREF_TAP_CLICK;
    incoming(&s,42,&p);assert(pointer_options==(SRV_POINTER_ACCELERATION|SRV_POINTER_TWO_FINGER_TAP));
    changed.two_finger_tap=0;memcpy(p.text,&changed,sizeof(changed));p.value=DESK_PREF_TWO_FINGER_TAP;
    incoming(&s,42,&p);assert(pointer_options==SRV_POINTER_ACCELERATION);
    p.value=2048;incoming(&s,42,&p);assert(s.preferences.value.text_size==20);
    request_close(&s,0);assert(s.clients[0].close_requested && !s.clients[0].closing);
    p=desk_packet_new(DESK_CANCEL_CLOSE);incoming(&s,42,&p);assert(!s.clients[0].close_requested);
    const struct desk_menu_item menu[]={ {0,DESK_MENU_HEADING,"File",""},{7,0,"New","Ctrl-N"} };
    p=desk_packet_new(DESK_MENU_SET);p.serial=19;p.width=2;
    memcpy(p.text,&menu[0],sizeof(menu[0]));incoming(&s,42,&p);assert(!s.clients[0].menus.revision);
    p.generation=1;p.value=1;memcpy(p.text,&menu[1],sizeof(menu[1]));incoming(&s,42,&p);
    assert(s.clients[0].menus.revision==19 && s.scene.menu.groups==4);
    unsigned system_actions[]={GLOBAL_ABOUT,GLOBAL_SETTINGS,GLOBAL_LAUNCHER,GLOBAL_FIND,GLOBAL_CALENDAR,GLOBAL_TEXT,GLOBAL_TERMINAL,GLOBAL_HELP,GLOBAL_NOTIFICATIONS,GLOBAL_CAPTURE,GLOBAL_EXIT};
    for(unsigned i=0;i<sizeof(system_actions)/sizeof(*system_actions);i++) {
        unsigned found=0;for(unsigned j=0;j<s.scene.menu.count;j++) found+=s.scene.menu.items[j].id==system_actions[i];assert(found==1);
    }
    struct desk_menu_store saved_menus=s.clients[0].menus;
    const char *headings[]={"File","Edit","Search","Tools"};
    s.clients[0].menus.count=32;
    for(unsigned i=0;i<32;i++) {
        struct desk_menu_item *item=&s.clients[0].menus.items[i];memset(item,0,sizeof(*item));
        if(!(i%8)) { item->flags=DESK_MENU_HEADING;strcpy(item->label,headings[i/8]); }
        else { item->id=i;snprintf(item->label,sizeof(item->label),"Action %u",i); }
    }
    for(unsigned i=0;i<DESKD_WINDOWS;i++) { s.clients[i].pid=42+i;s.clients[i].identity=i+1;s.clients[i].parent_window=0;s.scene.windows[i].visible=1; }
    global_menu_rebuild(&s);assert(s.scene.menu.count==57+DESKD_WINDOWS && s.scene.menu.groups==7 && desk_menu_valid(s.scene.menu.items,s.scene.menu.count));
    for(unsigned i=0;i<DESKD_WINDOWS;i++) { unsigned found=0;for(unsigned j=0;j<s.scene.menu.count;j++) found+=s.scene.menu.items[j].id==GLOBAL_WINDOW+s.clients[i].identity;assert(found==1); }
    s.clients[0].menus=saved_menus;for(unsigned i=1;i<DESKD_WINDOWS;i++) s.clients[i].pid=0;global_menu_rebuild(&s);
    global_menu_action(&s,GLOBAL_FIND);assert(!strcmp(executed_path,"/fat/bin/deskfind") && !strcmp(executed_role,"find"));
    unsigned before_input=s.clients[0].input_count;global_menu_action(&s,7);
    assert(s.clients[0].input_count==before_input+1);
    struct desk_packet *command=&s.clients[0].input[(s.clients[0].input_head+before_input)%DESK_INPUT_QUEUE];
    assert(command->type==DESK_MENU_ACTION && command->value==7 && command->serial==19);
    global_menu_action(&s,GLOBAL_MINIMIZE);assert(s.scene.focus==-1 && s.scene.menu.groups==3);
    global_menu_action(&s,GLOBAL_WINDOW+s.clients[0].identity);assert(s.scene.focus==0 && s.scene.menu.groups==4);
    global_menu_action(&s,GLOBAL_ABOUT);
    assert(!strcmp(executed_path,"/fat/bin/deskhelp") && !strcmp(executed_role,"help") && !strcmp(executed_argument,"about"));
    s.scene.windows[0].application=DESK_APP_TEXT;global_menu_action(&s,GLOBAL_HELP);
    assert(!strcmp(executed_argument,"text"));
    for(unsigned mods=0;mods<256;mods++) {
        struct srv_key_event key={.flags=SRV_KEY_DOWN_EVENT,.usage=17,.modifiers=mods};
        assert(!!notifications_shortcut(&key)==!!((mods&0x88) && (mods&0x22) && !(mods&0x55)));
        key.usage=32;assert(!!capture_shortcut(&key)==!!((mods&0x88) && (mods&0x22) && !(mods&0x55)));
        key.flags|=SRV_KEY_REPEAT;assert(!capture_shortcut(&key));key.usage=17;assert(!notifications_shortcut(&key));
        key.flags=0;assert(!notifications_shortcut(&key));
    }
    /* An exec/resource refusal must not silently close the launcher. Keep the
     * existing clients intact and publish a warning without trace mode. */
    desktop_notice_view_init(&s.scene.notifications);desktop_notice_view_configure(&s.scene.notifications,800,600);
    s.launch_notice_serial=0;execute_result=SRV_ERR_AGAIN;
    uint64_t existing=s.clients[0].pid;
    spawn_app(&s,DESK_APP_TEXT,"");
    assert(s.clients[0].pid==existing && strstr(s.scene.notice,"Text could not open") && s.scene.notifications.store.count==1);
    uint64_t warning=s.scene.notifications.toast_id;
    spawn_app(&s,DESK_APP_TEXT,"");assert(s.scene.notifications.store.count==1 && s.scene.notifications.toast_id==warning);
    now+=101;execute_result=SRV_ERR_NOENT;spawn_app(&s,DESK_APP_HELP,"");
    assert(strstr(s.scene.notice,"Help could not open") && s.scene.notifications.store.count==2);
    execute_result=-1;
    desktop_notice_view_init(&s.scene.notifications);desktop_notice_view_configure(&s.scene.notifications,800,600);
    p=desk_packet_new(DESK_NOTIFY);p.serial=1;p.value=1;
    struct desk_notification notice={.title="Timer finished",.message="A completed timer."};memcpy(p.text,&notice,sizeof(notice));
    s.clients[0].progress=s.clients[0].input_since=now;
    now+=100;incoming(&s,43,&p);assert(!s.scene.notifications.store.count);
    incoming(&s,42,&p);assert(s.scene.notifications.store.count==1 && s.scene.focus==0 && !s.clients[0].notice_ack_result);
    uint64_t notice_id=s.scene.notifications.toast_id;incoming(&s,42,&p);assert(s.scene.notifications.store.count==1 && s.scene.notifications.toast_id==notice_id);
    s.scene.notifications.store.count=0;incoming(&s,42,&p);assert(!s.scene.notifications.store.count && !s.clients[0].notice_ack_result);
    p.serial=2;incoming(&s,42,&p);assert(!s.scene.notifications.store.count && s.clients[0].notice_ack_result);
    now+=100;p.serial=3;incoming(&s,42,&p);assert(s.scene.notifications.store.count==1 && !s.clients[0].notice_ack_result);
    now+=100;p.serial=4;p.width=1;incoming(&s,42,&p);assert(s.clients[0].notice_ack_result && s.scene.notifications.store.count==1);p.width=0;
    p.serial=1;incoming(&s,42,&p);assert(s.clients[0].notice_ack_result && s.clients[0].notice_last==4);
    queue_full=1;service(&s);assert(s.clients[0].notice_ack_pending);queue_full=0;service(&s);assert(!s.clients[0].notice_ack_pending);
    notifications_open(&s);assert(s.scene.notifications.opened && s.scene.focus==-1 && !desktop_notifications_unread(&s.scene.notifications.store));
    notifications_close(&s);assert(!s.scene.notifications.opened && s.scene.focus==0);
    struct desktop_notice_action target={.owner=42,.application=DESK_APP_FILES};strcpy(target.target,DESK_CAPTURE_FOLDER);
    notifications_apply(&s,target);assert(!strcmp(executed_argument,DESK_CAPTURE_FOLDER));
    global_menu_action(&s,GLOBAL_CAPTURE);assert(s.capture_pending);s.capture_pending=0;s.capture.busy=1;
    global_menu_action(&s,GLOBAL_CAPTURE);assert(!s.capture_pending);s.capture.busy=0;
    s.clients[0].input_count=0;s.clients[0].awaiting=0;s.pointer_blocked=0;
    context_at(&s,80,120,0);unsigned last=(s.clients[0].input_head+s.clients[0].input_count-1)%DESK_INPUT_QUEUE;
    assert(s.clients[0].input[last].type==DESK_CONTEXT && s.clients[0].input[last].x==47 && s.clients[0].input[last].y==12);
    s.clients[0].input_count=0;context_at(&s,80,80,0);assert(!s.clients[0].input_count);
    context_at(&s,0,0,1);assert(s.clients[0].input_count==1 && s.clients[0].input[s.clients[0].input_head].value==1);
    s.clients[0].input_count=0;pointer_at(&s,80,120,0,0);pointer_at(&s,90,130,0,0);
    assert(s.clients[0].input_count==1 && s.clients[0].input[s.clients[0].input_head].value==DESK_POINTER_HOVER);
    assert(s.clients[0].input[s.clients[0].input_head].x==57 && s.scene.focus==0);
    struct srv_key_event contextual={.flags=SRV_KEY_DOWN_EVENT,.usage=SRV_KEY_F10,.modifiers=2};assert(context_key(&contextual));
    contextual.modifiers=32;assert(context_key(&contextual));contextual.modifiers=0;assert(!context_key(&contextual));
    contextual.usage=0x65;assert(context_key(&contextual));contextual.flags|=SRV_KEY_REPEAT;assert(!context_key(&contextual));
    context_at(&s,760,550,0);assert(s.scene.context.opened && s.scene.focus==0);
    notifications_open(&s);assert(!desk_popup_active(&s.scene.context));notifications_close(&s);
    forget(&s,0);assert(s.scene.menu.groups==3 && s.scene.focus==-1);
    global_menu_action(&s,GLOBAL_HELP);assert(!strcmp(executed_argument,"welcome"));
    struct session failed={0}; alloc_fail=allocations+1;
    assert(cache_init(&failed)==0 && !failed.cache_bytes);
    uint32_t *kept=cache_prepare(&failed,0,100,100);assert(kept);
    failed.scene.windows[0].pixels=kept;
    failed.scene.windows[0].frame=(struct desk_rect){20,60,102,145};
    failed.clients[0].pid=42;
    assert(!configure(&failed,0,200,200));
    assert(failed.scene.windows[0].pixels==kept && failed.scene.windows[0].frame.w==102 && !failed.clients[0].generation);
    cache_free(&failed);assert(!failed.cache_bytes);alloc_fail=0;
    memset(&failed,0,sizeof(failed));
    /* Refusal must not spawn or replace a child, and successful slot reuse
     * clears the now-obsolete capacity/error message. */
    for(int i=0;i<DESKD_WINDOWS;i++) failed.children[i]=100+i;
    strcpy(executed_path,"untouched");spawn_app(&failed,DESK_APP_TEXT,"");
    assert(!strcmp(executed_path,"untouched") && strstr(failed.scene.notice,"Close an app"));
    failed.children[4]=0;execute_result=-1;spawn_app(&failed,DESK_APP_TEXT,"");
    assert(!failed.children[4] && strstr(failed.scene.notice,"Text could not open"));
    execute_result=42;spawn_app(&failed,DESK_APP_TEXT,"");
    assert(failed.children[4]==42 && failed.child_apps[4]==DESK_APP_TEXT && !failed.scene.notice[0]);
    capture_cleanup_ownership();
    asynchronous_client_termination();
    asynchronous_child_reap();
    asynchronous_lifecycle();
    retired_cache_reuse();
    clipboard_client_retirement();
    delayed_control_delivery();
    connection_lifetime();
    shutdown_dialog_lifetime();
    dynamic_registry();
    document_routing_failures();
    puts("desktop-session: PASS stale resize release, queue retry, partial-copy isolation, dead/unresponsive/already-reaped client cleanup");
    return 0;
}
