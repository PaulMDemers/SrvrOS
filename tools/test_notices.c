#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <srvros/desk_notify.h>
#include "../userspace/deskd/notification_view.h"
static struct desk_packet sent;static int blocked,sends;
struct desk_packet desk_packet_new(uint32_t type) { return (struct desk_packet){.type=type,.version=DESK_PROTOCOL_VERSION,.size=sizeof(struct desk_packet)}; }
int desk_send(uint64_t target,const struct desk_packet *p) { assert(target==42);if(blocked) return -1;sent=*p;sends++;return 0; }
static void transport(void) {
    struct desk_notify_client c;desk_notify_init(&c);char title[]="Timer finished",message[]="Your timer has finished.";
    assert(!desk_notify_begin(&c,42,1,title,message,50));title[0]='X';assert(!strcmp(c.text.title,"Timer finished"));
    assert(desk_notify_begin(&c,42,0,"second","message",50)<0);blocked=1;desk_notify_tick(&c,51);assert(!sends && !c.sent);
    blocked=0;desk_notify_tick(&c,52);assert(sends==1 && sent.type==DESK_NOTIFY && sent.serial==1 && sent.value==1);
    desk_notify_tick(&c,151);assert(sends==1);desk_notify_tick(&c,152);assert(sends==2);
    struct desk_packet ack=desk_packet_new(DESK_NOTIFY_ACK);ack.serial=2;assert(!desk_notify_receive(&c,&ack) && c.busy);
    ack.serial=1;assert(desk_notify_receive(&c,&ack)==1 && !c.busy && !desk_notify_receive(&c,&ack));
    assert(!desk_notify_begin(&c,42,0,"Next","",200));desk_notify_tick(&c,200);ack.serial=c.serial;ack.value=1;assert(desk_notify_receive(&c,&ack)==-1);
    assert(!desk_notify_begin(&c,42,0,"Timeout","",300));assert(!desk_notify_tick(&c,1300));assert(desk_notify_tick(&c,1301)==-1 && !c.busy);
    assert(!desk_notify_begin(&c,42,0,"Rollback","",500));assert(desk_notify_tick(&c,499)==-1);
    struct desk_notify_client saved=c;assert(desk_notify_begin(&c,42,3,"bad","",1)<0 && !memcmp(&c,&saved,sizeof(c)));
    assert(desk_notify_begin(&c,0,0,"bad","",1)<0 && desk_notify_begin(&c,42,0,"bad\n","",1)<0 && desk_notify_begin(&c,42,0,"","",1)<0);
    c.serial=UINT64_MAX;assert(desk_notify_begin(&c,42,0,"full","",1)<0);
}
static void model(void) {
    struct desktop_notifications store={0};struct desk_notification text={.title="Timer finished",.message="The timer reached zero."};uint64_t id;
    assert(desktop_notification_add(&store,10,1,1,0,1,&text,&id)==1 && store.count==1 && desktop_notifications_unread(&store)==1);
    assert(desktop_notification_add(&store,10,1,1,99,1,&text,NULL)==0 && store.count==1);
    struct desktop_notifications saved=store;text.title[0]='X';assert(desktop_notification_add(&store,10,1,1,99,1,&text,NULL)<0 && !memcmp(&store,&saved,sizeof(store)));text.title[0]='T';
    text.message[0]='\n';assert(!desktop_notification_valid(&text,0));strcpy(text.message,"bad\xc0\x80");assert(!desktop_notification_valid(&text,0));
    memset(text.title,'x',sizeof(text.title));assert(!desktop_notification_valid(&text,0));strcpy(text.title,"Timer finished");strcpy(text.message,"Done");
    assert(desktop_notification_add(&store,0,1,2,0,0,&text,NULL)<0 && desktop_notification_add(&store,10,0,2,0,0,&text,NULL)<0);
    for(unsigned i=2;i<=8;i++) assert(desktop_notification_add(&store,10,1,i,0,0,&text,NULL)==1);
    assert(store.count==4 && !desktop_notification_find(&store,id) && store.records[0].serial==5);
    for(unsigned owner=11;owner<30;owner++) assert(desktop_notification_add(&store,owner,1,1,owner,0,&text,NULL)==1);
    assert(store.count==DESKTOP_NOTICES && desktop_notifications_unread(&store)==DESKTOP_NOTICES);
    desktop_notifications_read(&store);assert(!desktop_notifications_unread(&store));
    id=store.records[4].id;assert(desktop_notification_remove(&store,id)==1 && !desktop_notification_find(&store,id) && !desktop_notification_remove(&store,id));
    store.next_id=UINT64_MAX;assert(desktop_notification_add(&store,100,2,1,0,0,&store.records[0].text,&id)==1 && id && store.count==16);
}
static void view(void) {
    struct desktop_notice_view v;desktop_notice_view_init(&v);desktop_notice_view_configure(&v,1280,800);
    struct desk_notification text={.title="Timer finished",.message="Your 00:05:00 timer has finished."};uint64_t id;
    assert(desktop_notification_add(&v.store,10,1,1,0,1,&text,&id)==1);desktop_notice_view_arrived(&v,id,100);assert(v.toast_id==id);
    assert(!desktop_notice_view_tick(&v,8000099));assert(desktop_notice_view_tick(&v,8000100) && !v.toast_id && v.store.count==1);
    desktop_notice_view_arrived(&v,id,100);struct desk_widget_event e={.type=DESK_WIDGET_POINTER,.x=v.toast_frame.x+20,.y=v.toast_frame.y+20,.down=1,.pressed=1};
    desk_ui_pointer(&v.toast_ui,e,1);assert(v.toast_ui.capture>=0);uint64_t second;
    assert(desktop_notification_add(&v.store,10,1,2,0,1,&text,&second)==1);desktop_notice_view_arrived(&v,second,200);assert(v.toast_id==id && v.pending_toast==second);
    assert(!desktop_notice_view_tick(&v,9000000));e.down=e.pressed=0;
    struct desktop_notice_action a=desktop_notice_toast_action(&v,desk_ui_pointer(&v.toast_ui,e,2));assert(a.owner==10 && !v.toast_id);
    assert(desktop_notice_view_tick(&v,9000010) && v.toast_id==second);
    char path[]="/fat/home/Screenshots";
    assert(!desktop_notification_target(&v.store,second,path));path[5]='X';
    assert(desktop_notification_target(&v.store,second,"relative")<0);
    assert(desktop_notification_target(&v.store,second,"/bad\npath")<0);
    assert(desktop_notification_target(&v.store,999,"/fat")<0);
    desktop_notice_view_open(&v);assert(v.opened && !v.toast_id && !v.pending_toast && !desktop_notifications_unread(&v.store));
    assert(v.list.count==2 && v.items[0].id==second && v.open.enabled);
    a=desktop_notice_view_action(&v,(struct desk_action){NOTICE_OPEN,DESK_WIDGET_ACTIVATED});assert(a.owner==10 && a.application==1 && a.close && !strcmp(a.target,"/fat/home/Screenshots"));
    assert(!strcmp(v.ui.controls[1].label,"Open folder"));
    desktop_notice_view_action(&v,(struct desk_action){NOTICE_REMOVE,DESK_WIDGET_ACTIVATED});assert(v.store.count==1 && v.items[0].id==id);
    desktop_notice_view_action(&v,(struct desk_action){NOTICE_CLEAR,DESK_WIDGET_ACTIVATED});assert(!v.store.count && !v.open.enabled && !v.remove.enabled && !v.clear.enabled);
    desktop_notice_view_close(&v);assert(!v.opened);
    static uint32_t full[1320*820],tiled[1320*820];
    for(unsigned mode=0;mode<4;mode++) {
        int width=mode%2?640:1280,height=mode%2?480:800;desktop_notice_view_configure(&v,width,height);
        desktop_notification_add(&v.store,10,1,mode+3,0,mode%3,&text,&id);desktop_notice_view_arrived(&v,id,1);
        if(mode<2) desktop_notice_view_open(&v);else desktop_notice_view_close(&v);
        if(mode>=2) desktop_notice_view_arrived(&v,id,1);
        struct desk_canvas a,b;memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,width,height,1320);desk_canvas_init(&b,tiled,width,height,1320);
        desktop_notice_view_draw(&a,&v,&desk_theme_quiet);
        for(int y=0;y<height;y+=47) for(int x=0;x<width;x+=61) { desk_clip(&b,(struct desk_rect){x,y,61,47});struct desk_rect clip=b.clip;desktop_notice_view_draw(&b,&v,&desk_theme_quiet);assert(!memcmp(&clip,&b.clip,sizeof(clip))); }
        assert(!memcmp(full,tiled,sizeof(full)));
        for(unsigned y=0;y<820;y++) for(unsigned x=0;x<1320;x++) if(x>=(unsigned)width || y>=(unsigned)height) assert(!full[y*1320+x]);
    }
}
static void controls(void) {
    struct desk_notice n;desk_notice_init(&n);assert(!desk_notice_set(&n,"Heading","Body",DESK_NOTICE_INFO));struct desk_notice saved=n;
    assert(desk_notice_set(&n,"Bad\nheading","Body",DESK_NOTICE_INFO)<0 && !memcmp(&n,&saved,sizeof(n)));
    assert(!desk_notice_set(&n,n.message,n.heading,DESK_NOTICE_WARNING) && !strcmp(n.heading,"Body") && !strcmp(n.message,"Heading"));
    uint32_t full[144*100],tiled[144*100];struct desk_canvas a,b;
    for(unsigned mode=0;mode<8;mode++) {
        memset(full,0,sizeof(full));memset(tiled,0,sizeof(tiled));desk_canvas_init(&a,full,128,96,144);desk_canvas_init(&b,tiled,128,96,144);
        struct desk_rect r=mode%2?(struct desk_rect){-5,8,120,70}:(struct desk_rect){1,2,mode==2?1:120,mode==4?1:70};
        desk_notice_configure(&n,r,mode%2?&desk_font_20:&desk_font_16);n.heading_trailing=mode==6?UINT32_MAX:0;n.message_bottom=mode==7?UINT32_MAX:0;
        desk_notice_draw(&a,&n,&desk_theme_quiet);desk_progress_draw(&a,(struct desk_rect){1,80,120,10},mode?UINT32_MAX/2:0,mode?UINT32_MAX:0,&desk_theme_quiet);
        for(int y=0;y<96;y+=13) for(int x=0;x<128;x+=17) { desk_clip(&b,(struct desk_rect){x,y,17,13});desk_notice_draw(&b,&n,&desk_theme_quiet);desk_progress_draw(&b,(struct desk_rect){1,80,120,10},mode?UINT32_MAX/2:0,mode?UINT32_MAX:0,&desk_theme_quiet); }
        assert(!memcmp(full,tiled,sizeof(full)));
    }
}
int main(void) { transport();model();view();controls();puts("notices: PASS validation, snapshots, retries/ACK/timeout, bounded history/ownership, dedup, expiry/capture, focus/actions, cards/progress clipping and tiled rendering"); }
