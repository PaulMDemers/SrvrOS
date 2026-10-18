#include <assert.h>
#include <stdlib.h>
#define main desktop_guest_main
#include "../userspace/deskd/deskd.c"
#undef main

static uint64_t now=1;
static int alive=1,queue_full,copy_failure,allocations,frees,killed,sends,reaped,alloc_fail;
static uint32_t sent[128];
long srv_ticks(void) { return (long)now; }
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
    return (struct desk_packet){.type=type,.version=1,.size=sizeof(struct desk_packet)};
}
int desk_send(uint64_t target,const struct desk_packet *p) {
    assert(target==42);
    if (queue_full) return -1;
    assert(sends<128); sent[sends++]=p->type; return 0;
}
int gui_surface_copy(uint64_t id,uint64_t x,uint64_t y,uint64_t w,uint64_t h,uint32_t *p,uint64_t stride) {
    assert(id==7 && !x && !y && stride==w);
    for (uint64_t i=0;i<w*h/2;i++) p[i]=0x123456;
    if (copy_failure) return -1;
    for (uint64_t i=w*h/2;i<w*h;i++) p[i]=0x123456;
    return 0;
}
int main(void) {
    struct session s={.scene={.focus=-1},.children={42}};
    assert(cache_init(&s)==0 && allocations==4);
    desk_damage_init(&s.damage,800,600);
    struct desk_packet p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    strcpy(p.text,"Test"); incoming(&s,42,&p); service(&s);
    assert(s.clients[0].awaiting && !s.clients[0].configure && sends==1 && sent[0]==DESK_CONFIGURE);
    p=desk_packet_new(DESK_COMMIT); p.generation=1; p.surface=7; p.serial=1; p.width=400; p.height=280;
    incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels && !s.clients[0].awaiting);
    const uint32_t *front=s.scene.windows[0].pixels;
    assert(allocations==4 && frees==0);

    queue_full=1; service(&s);
    assert(s.clients[0].release.type==DESK_RELEASE && sends==1);
    queue_full=0; service(&s); assert(sent[1]==DESK_RELEASE);
    p.serial=2; incoming(&s,42,&p);
    assert(s.scene.windows[0].pixels!=front && allocations==4 && frees==0);
    front=s.scene.windows[0].pixels; service(&s);
    configure(&s,0,450,300);
    p.serial=3; incoming(&s,42,&p); /* Old generation still gets released. */
    assert(s.scene.windows[0].pixels==front && s.clients[0].awaiting);
    service(&s); assert(sent[3]==DESK_RELEASE && sent[4]==DESK_CONFIGURE);
    p.generation=2; p.width=450; p.height=300; p.serial=4;
    copy_failure=1; incoming(&s,42,&p);
    assert(s.clients[0].closing && s.scene.windows[0].pixels==front && front[0]==0x123456);
    service(&s); assert(sent[5]==DESK_RELEASE && sent[6]==DESK_CLOSE);
    now+=501; service(&s);
    assert(killed==1 && !s.clients[0].pid && !s.children[0] && allocations==4 && frees==0);
    /* A live process that never answers CONFIGURE is also bounded. */
    alive=1; s.children[0]=42; copy_failure=0; reaped=1;
    p=desk_packet_new(DESK_OPEN); p.width=400; p.height=280;
    incoming(&s,42,&p); service(&s); now+=501; service(&s);
    assert(killed==2 && !s.clients[0].pid && !s.children[0] && allocations==4 && frees==0);
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
    queue_full=0; service(&s); assert(s.clients[0].input_count==1);
    service(&s); assert(!s.clients[0].input_count && sent[sends-1]==DESK_SCROLL);
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
    assert(!s.pointer_pid && s.clients[0].input_count==1);
    assert(s.clients[0].input[s.clients[0].input_head].value==DESK_POINTER_CANCEL);
    service(&s);
    pointer_at(&s,400,200,MOUSE_LEFT,1); focus(&s,-1);
    assert(!s.pointer_pid && s.pointer_blocked && s.clients[0].input_count==2);
    unsigned tail=(s.clients[0].input_head+1)%16;
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
    for (int i=0;i<16;i++) queue_input(&s,0,p);
    assert(s.clients[0].input_count==16 && !s.clients[0].closing);
    queue_input(&s,0,p);
    assert(s.clients[0].closing && !s.pointer_pid && s.pointer_blocked);
    struct session failed={0}; alloc_fail=allocations+1;
    assert(cache_init(&failed)<0 && allocations==frees);
    for(int i=0;i<DESKD_WINDOWS;i++) for(int j=0;j<2;j++) assert(!failed.cache[i][j]);
    puts("desktop-session: PASS stale resize release, queue retry, partial-copy isolation, dead/unresponsive/already-reaped client cleanup");
    return 0;
}
