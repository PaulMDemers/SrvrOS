#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdlib.h>
#define main desktop_guest_main
#include "../userspace/deskd/deskd.c"
#undef main

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
static uint64_t sent_serials[128];
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
    return (struct desk_packet){.type=type,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet)};
}
int desk_send(uint64_t target,const struct desk_packet *p) {
    assert(target==42);
    if (queue_full || !send_allow) return -1;
    if(send_allow>0) send_allow--;
    assert(sends<128);sent_serials[sends]=p->serial;sent_values[sends]=p->value;sent[sends++]=p->type; return 0;
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
    memset(&s,0,sizeof(s));alive=1;queue_full=0;sends=0;send_allow=-1;
    s.clients[0].pid=42;s.clients[0].progress=now;
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

    memset(&s,0,sizeof(s));alive=1;queue_full=1;
    s.clients[0].pid=42;s.clients[0].progress=now;
    s.clients[0].release=desk_packet_new(DESK_RELEASE);
    now+=601;service(&s);assert(alive && s.clients[0].release.type);
    now+=400;service(&s);assert(alive);
    queue_full=0;now+=200;service(&s);assert(alive && !s.clients[0].release.type);

    s.clients[0].configure=s.clients[0].awaiting=1;s.clients[0].progress=now;
    queue_full=1;service(&s);now+=501;service(&s);assert(!alive && !s.clients[0].pid);

    memset(&s,0,sizeof(s));alive=1;queue_full=0;
    s.clients[0].pid=42;close_client(&s,0);now+=601;
    int before=sends;service(&s);assert(alive && sends==before+1 && sent[sends-1]==DESK_CLOSE);
    now+=400;service(&s);assert(alive && sends==before+1); /* No close-message flood. */
    now+=101;service(&s);assert(!alive && !s.clients[0].pid);
}
static void pointer_cancel_keeps_keyboard(void) {
    static struct session s;
    memset(&s,0,sizeof(s));alive=1;queue_full=0;sends=0;send_allow=-1;
    s.clients[0].pid=42;s.clients[0].generation=1;
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
int main(void) {
    pointer_cancel_keeps_keyboard();
    struct session s={.scene={.focus=-1},.children={42}};
    assert(cache_init(&s)==0 && allocations==2*DESKD_WINDOWS);
    desk_damage_init(&s.damage,800,600);
    struct desk_packet p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    strcpy(p.text,"Test"); incoming(&s,42,&p); service(&s);
    assert(s.clients[0].awaiting && !s.clients[0].configure && sends==1 && sent[0]==DESK_CONFIGURE);
    p=desk_packet_new(DESK_COMMIT); p.generation=1; p.surface=7; p.serial=1; p.width=400; p.height=280; p.input_serial=5;
    incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels && !s.clients[0].awaiting && s.clients[0].present_input==5);
    const uint32_t *front=s.scene.windows[0].pixels;
    assert(allocations==2*DESKD_WINDOWS && frees==0);

    queue_full=1; service(&s);
    assert(s.clients[0].release.type==DESK_RELEASE && sends==1);
    queue_full=0; service(&s); assert(sent[1]==DESK_RELEASE);
    p.serial=2; incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels!=front && allocations==2*DESKD_WINDOWS && frees==0);
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
    assert(killed==1 && !s.clients[0].pid && !s.children[0] && allocations==2*DESKD_WINDOWS && frees==0);
    /* A live process that never answers CONFIGURE is also bounded. */
    alive=1; s.children[0]=42; copy_failure=0; reaped=1;
    p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    incoming(&s,42,&p); service(&s); now+=501; service(&s);
    assert(killed==2 && !s.clients[0].pid && !s.children[0] && allocations==2*DESKD_WINDOWS && frees==0);
    cache_free(&s); assert(allocations==frees);
    /* Scroll targets content under the cursor, keeps focus, and retries the
     * same recipient when the transport queue is full. */
    memset(&s,0,sizeof(s)); s.scene.focus=1;
    s.clients[0].pid=42; s.scene.windows[0].visible=1;
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
    memset(&s,0,sizeof(s)); s.scene.focus=0;
    s.clients[0].pid=42; s.clients[0].generation=1; s.scene.windows[0].visible=1;
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
    memset(&s,0,sizeof(s)); s.clients[0].pid=42; s.pointer_pid=42;
    p=desk_packet_new(DESK_POINTER); p.value=DESK_POINTER_PRESS|DESK_POINTER_LEFT;
    for (int i=0;i<DESK_INPUT_QUEUE;i++) queue_input(&s,0,p);
    assert(s.clients[0].input_count==DESK_INPUT_QUEUE && !s.clients[0].closing);
    queue_input(&s,0,p);
    assert(s.clients[0].closing && !s.pointer_pid && s.pointer_blocked);
    /* Slow rendering still drains an ordered batch. Partial IPC capacity keeps
     * the first unsent edge at the head, without duplication or reordering. */
    memset(&s,0,sizeof(s));s.clients[0].pid=42;sends=0;
    for(unsigned i=0;i<24;i++) { p=desk_packet_new(DESK_KEY);p.serial=i+1;queue_input(&s,0,p); }
    service(&s);assert(sends==DESK_INPUT_BATCH&&s.clients[0].input_count==24-DESK_INPUT_BATCH);
    send_allow=3;service(&s);assert(sends==DESK_INPUT_BATCH+3);
    unsigned blocked_count=s.clients[0].input_count;service(&s);assert(s.clients[0].input_count==blocked_count);
    send_allow=-1;while(s.clients[0].input_count) service(&s);
    assert(sends==24&&!s.clients[0].closing);
    for(unsigned i=0;i<24;i++) assert(sent[i]==DESK_KEY&&sent_serials[i]==i+1);
    /* Reproduce a compositor pause longer than the old queue-age timeout.
     * A healthy client can accept this ordered key and must not be closed. */
    memset(&s,0,sizeof(s));s.clients[0].pid=42;sends=0;
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
    memset(&s,0,sizeof(s));s.scene.apps=1;s.scene.focus=0;desk_damage_init(&s.damage,1280,800);
    s.clients[0].pid=42;s.scene.windows[0].visible=1;s.scene.windows[0].application=DESK_APP_TEXT;
    s.scene.windows[0].frame=(struct desk_rect){32,64,602,365};
    struct desk_rect original=s.scene.windows[0].frame;
    maximize(&s,0);assert(s.scene.windows[0].maximized && s.clients[0].generation==1);
    assert(s.scene.windows[0].frame.x==8 && s.scene.windows[0].frame.w==1264 && s.scene.windows[0].frame.h==668);
    maximize(&s,0);assert(!s.scene.windows[0].maximized && !memcmp(&original,&s.scene.windows[0].frame,sizeof(original)));
    arrange_window(&s,0,DESKTOP_TILE_LEFT);assert(s.scene.windows[0].maximized && s.scene.windows[0].frame.x==8 && !memcmp(&original,&s.scene.windows[0].restore,sizeof(original)));
    uint64_t arranged_generation=s.clients[0].generation;arrange_window(&s,0,DESKTOP_TILE_LEFT);assert(s.clients[0].generation==arranged_generation);
    s.pointer_pid=42;arrange_window(&s,0,DESKTOP_TILE_RIGHT);assert(!s.pointer_pid && s.pointer_blocked);s.pointer_blocked=0;assert(s.scene.windows[0].frame.x>s.damage.width/2 && !memcmp(&original,&s.scene.windows[0].restore,sizeof(original)));
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
    for(unsigned i=1;i<DESKD_WINDOWS;i++) s.clients[i].pid=42+i;
    global_menu_rebuild(&s);assert(s.scene.menu.count==62 && s.scene.menu.groups==7 && desk_menu_valid(s.scene.menu.items,s.scene.menu.count));
    for(unsigned i=0;i<DESKD_WINDOWS;i++) { unsigned found=0;for(unsigned j=0;j<s.scene.menu.count;j++) found+=s.scene.menu.items[j].id==GLOBAL_WINDOW+i;assert(found==1); }
    s.clients[0].menus=saved_menus;for(unsigned i=1;i<DESKD_WINDOWS;i++) s.clients[i].pid=0;global_menu_rebuild(&s);
    global_menu_action(&s,GLOBAL_FIND);assert(!strcmp(executed_path,"/fat/bin/deskfind") && !strcmp(executed_role,"find"));
    unsigned before_input=s.clients[0].input_count;global_menu_action(&s,7);
    assert(s.clients[0].input_count==before_input+1);
    struct desk_packet *command=&s.clients[0].input[(s.clients[0].input_head+before_input)%DESK_INPUT_QUEUE];
    assert(command->type==DESK_MENU_ACTION && command->value==7 && command->serial==19);
    global_menu_action(&s,GLOBAL_MINIMIZE);assert(s.scene.focus==-1 && s.scene.menu.groups==3);
    global_menu_action(&s,GLOBAL_WINDOW);assert(s.scene.focus==0 && s.scene.menu.groups==4);
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
    assert(cache_init(&failed)<0 && allocations==frees);
    for(int i=0;i<DESKD_WINDOWS;i++) for(int j=0;j<2;j++) assert(!failed.cache[i][j]);
    /* Refusal must not spawn or replace a child, and successful slot reuse
     * clears the now-obsolete capacity/error message. */
    for(int i=0;i<DESKD_WINDOWS;i++) failed.children[i]=100+i;
    strcpy(executed_path,"untouched");spawn_app(&failed,DESK_APP_TEXT,"");
    assert(!strcmp(executed_path,"untouched") && strstr(failed.scene.notice,"Close a window"));
    failed.children[4]=0;execute_result=-1;spawn_app(&failed,DESK_APP_TEXT,"");
    assert(!failed.children[4] && strstr(failed.scene.notice,"could not be opened"));
    execute_result=42;spawn_app(&failed,DESK_APP_TEXT,"");
    assert(failed.children[4]==42 && failed.child_apps[4]==DESK_APP_TEXT && !failed.scene.notice[0]);
    delayed_control_delivery();
    puts("desktop-session: PASS stale resize release, queue retry, partial-copy isolation, dead/unresponsive/already-reaped client cleanup");
    return 0;
}
