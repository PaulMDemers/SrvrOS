#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
static void *test_registry_alloc(size_t bytes);
static void test_registry_free(void *p,size_t bytes);
#define DESKD_STORAGE_ALLOC test_registry_alloc
#define DESKD_STORAGE_FREE test_registry_free
#define main desktop_guest_main
#include "../userspace/deskd/deskd.c"
#undef main

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
void srv_puts(const char *text) { fputs(text,stdout); }
static uint64_t now=1;
static int alive=1,queue_full,copy_failure,allocations,frees,killed,sends,reaped,alloc_fail;
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
long srv_kill(int64_t pid) { assert(pid==42); alive=0; killed++; return 0; }
long srv_wait(uint64_t pid,uint64_t *status,uint64_t flags) {
    (void)status; (void)flags; return !alive?(reaped?-1:(long)pid):0;
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
    s.scene.launcher_open=1;s.overlay_return=99;s.overlay_identity=100;launcher_close(&s);assert(s.scene.focus==-1);
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

int main(void) {
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
    for(unsigned i=0;i<DESKD_WINDOWS;i++) { s.clients[i].pid=42+i;s.clients[i].identity=i+1; }
    global_menu_rebuild(&s);assert(s.scene.menu.count==54+DESKD_WINDOWS && s.scene.menu.groups==7 && desk_menu_valid(s.scene.menu.items,s.scene.menu.count));
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
    clipboard_client_retirement();
    delayed_control_delivery();
    connection_lifetime();
    dynamic_registry();
    document_routing_failures();
    puts("desktop-session: PASS stale resize release, queue retry, partial-copy isolation, dead/unresponsive/already-reaped client cleanup");
    return 0;
}
