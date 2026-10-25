#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_GUI_HOST_TEST
#include "../kernel/src/gui.c"

static unsigned brightness_calls, poll_wakes;
void process_file_poll_wake(void) { poll_wakes++; }
int audio_request(unsigned op,unsigned value) { return op==1 && value==0?0:-1; }
int backlight_percent(unsigned value) { brightness_calls++;return value?(int)value:14; }
static uint64_t current_pid=1;
static bool keyboard_events,keys_pending,pointer_pending;
bool keyboard_events_pending(void) { return keys_pending; }
bool mouse_pending(void) { return pointer_pending; }
static unsigned pointer_speed=3;static bool pointer_accel,pointer_typing;static unsigned pointer_flags;
void mouse_trackpad_settings(unsigned speed,unsigned options) { pointer_speed=speed;pointer_flags=options;pointer_accel=options&SRV_POINTER_ACCELERATION;pointer_typing=options&SRV_POINTER_DISABLE_WHILE_TYPING; }
void keyboard_set_event_mode(bool enabled) { keyboard_events=enabled; }
bool keyboard_read_event(struct srv_key_event *event) { (void)event; return keyboard_events; }
struct process *process_current(void) { return (struct process *)(uintptr_t)current_pid; }
uint64_t process_pid(const struct process *p) { return (uintptr_t)p; }
void *kmalloc(size_t bytes) { return malloc(bytes); }
void kfree(void *p) { free(p); }

static struct gui_message packet_message(unsigned type, uint64_t target,
    uint64_t surface, uint64_t serial) {
    struct gui_message m={.type=DESK_TRANSPORT_MESSAGE,.target_pid=target};
    struct desk_packet p={.version=DESK_PROTOCOL_VERSION,.size=sizeof(p),
        .type=type,.window=DESK_DEFAULT_WINDOW,.surface=surface,.serial=serial,.generation=7,.width=2,.height=2};
    memcpy(m.text,&p,sizeof(p));
    return m;
}

static void packet_window(struct gui_message *message,uint64_t window) {
    struct desk_packet packet;memcpy(&packet,message->text,sizeof(packet));
    packet.window=window;memcpy(message->text,&packet,sizeof(packet));
}

static void protocol_tests(void) {
    for (unsigned i=0;i<GUI_MAX_QUEUES;i++) gui_process_cleanup(i+10);
    current_pid=1; assert(gui_register_server()==0);
    assert(gui_audio_request(1,0)==0 && gui_audio_request(0x100000001ull,0)==-1);
    assert(gui_brightness(0)==14 && gui_brightness(50)==50 && brightness_calls==2);
    assert(gui_brightness(1)==-1 && gui_brightness(101)==-1 && brightness_calls==2);
    struct srv_key_event key;
    assert(gui_keyboard_mode(2)==-1 && gui_keyboard_mode(1)==0 && keyboard_events);
    assert(gui_keyboard_read(&key)==1);
    assert(gui_pointer_config(0,1)==-1 && gui_pointer_config(6,1)==-1 && gui_pointer_config(3,16)==-1);
    assert(gui_pointer_config(5,1)==0 && pointer_speed==5 && pointer_accel);
    assert(gui_pointer_config(5,3)==0 && pointer_accel && pointer_typing);
    assert(gui_pointer_config(5,1)==0 && pointer_accel && !pointer_typing);
    assert(gui_pointer_config(5,SRV_POINTER_FLAGS)==0 && pointer_flags==SRV_POINTER_FLAGS);
    current_pid=2;
    assert(gui_audio_request(1,0)==-1);
    assert(gui_brightness(50)==-1 && brightness_calls==2);
    assert(gui_pointer_config(1,0)==-1 && pointer_speed==5 && pointer_accel);
    assert(gui_keyboard_mode(0)==-1 && gui_keyboard_read(&key)==-1 && keyboard_events);
    assert(gui_surface_create(2,2,2,1ull<<40)==-1);
    int64_t laptop=gui_surface_create(2,1422,723,DESK_SURFACE_STREAM);
    assert(laptop>0 && gui_surface_destroy(2,laptop)==0);
    assert(gui_surface_create(2,DESK_MAX_WIDTH,DESK_MAX_HEIGHT+1,DESK_SURFACE_STREAM)==-1);
    uint32_t pixels[4]={11,22,33,44}, copy[4]={0};
    int64_t id=gui_surface_create(2,2,2,DESK_SURFACE_STREAM);
    assert(id>0 && gui_surface_blit(2,id,0,0,2,2,pixels,2)==0);
    struct gui_message commit=packet_message(DESK_COMMIT,0,id,1), temp;
    current_pid=3; assert(gui_send(&commit)==-1);
    current_pid=2;
    struct gui_message bad=commit;
    bad.text[0]=99; assert(gui_send(&bad)==-1);
    bad=commit; packet_window(&bad,0); assert(gui_send(&bad)==-1);
    bad=commit; bad.text[0]=1; assert(gui_send(&bad)==-1); /* v1 has no text acknowledgement. */
    struct gui_message legacy={.type=1};
    for (unsigned i=0;i<GUI_QUEUE_DEPTH;i++) assert(gui_send(&legacy)==0);
    assert(gui_send(&commit)==-1 && !surface_for_id(id)->sealed);
    current_pid=1; while (gui_recv(&temp)>0) {}
    current_pid=2; assert(gui_send(&commit)==0);
    assert(gui_surface_blit(2,id,0,0,2,2,pixels,2)==-1);
    assert(gui_surface_blit(1,id,0,0,2,2,pixels,2)==-1);
    assert(gui_surface_destroy(2,id)==-1 && gui_surface_destroy(1,id)==-1);
    assert(gui_send(&commit)==-1);
    assert(gui_surface_copy(1,id,0,0,2,2,copy,2)==0 && memcmp(pixels,copy,sizeof(copy))==0);
    struct gui_message release=packet_message(DESK_RELEASE,2,id,1);
    assert(gui_send(&release)==-1); /* A client cannot forge release. */
    current_pid=1;
    bad=packet_message(DESK_RELEASE,3,id,1); assert(gui_send(&bad)==-1);
    bad=packet_message(DESK_RELEASE,2,id,2); assert(gui_send(&bad)==-1);
    bad=release; packet_window(&bad,2);
    assert(gui_send(&bad)==-1 && surface_for_id(id)->sealed);
    legacy.target_pid=2;
    for (unsigned i=0;i<GUI_QUEUE_DEPTH;i++) assert(gui_send(&legacy)==0);
    assert(gui_send(&release)==-1 && surface_for_id(id)->sealed);
    current_pid=2; while (gui_recv(&temp)>0) {}
    current_pid=1; assert(gui_send(&release)==0 && !surface_for_id(id)->sealed);
    assert(gui_send(&release)==-1);
    current_pid=2; assert(gui_send(&commit)==-1); /* Replay rejected. */
    commit=packet_message(DESK_COMMIT,0,id,2);
    bad=commit; packet_window(&bad,2);
    assert(gui_send(&bad)==-1 && !surface_for_id(id)->sealed); /* No surface rebinding. */
    assert(gui_send(&commit)==0);
    gui_process_cleanup(1); /* Pending client can reclaim after server death. */
    assert(!keyboard_events && pointer_speed==3 && !pointer_accel && !pointer_flags);
    assert(!surface_for_id(id)->sealed);
    assert(gui_surface_destroy(2,id)==0);
    gui_process_cleanup(2);
    current_pid=1; assert(gui_register_server()==0);
    current_pid=2; id=gui_surface_create(2,2,2,DESK_SURFACE_STREAM);
    commit=packet_message(DESK_COMMIT,0,id,1); assert(gui_send(&commit)==0);
    gui_process_cleanup(2);
    assert(gui_surface_copy(1,id,0,0,2,2,copy,2)==-1);
    current_pid=1; assert(gui_recv(&temp)==1); /* Stale queued commit has no dangling pixels. */
    release=packet_message(DESK_RELEASE,2,id,1); assert(gui_send(&release)==-1);
    gui_process_cleanup(1);
    puts("desktop protocol: PASS version, owner, seal, replay, release, queue pressure, server death");
}

int main(void) {
    assert(gui_register_server()==0);
    assert(gui_register_server()==0);
    current_pid=2;
    int64_t surface=gui_surface_create(2,8,8,0);
    assert(surface>0);
    uint32_t pixels[64]={0};
    assert(gui_surface_copy(3,surface,0,0,8,8,pixels,8)==-1);
    assert(gui_register_server()==-1);
    assert(server_pid==1);
    assert(gui_surface_copy(1,surface,0,0,8,8,pixels,8)==0);
    assert(gui_surface_copy(3,surface,0,0,8,8,pixels,8)==-1);
    assert(gui_surface_blit(2,surface,7,7,2,2,pixels,2)==-1);
    assert(gui_surface_copy(2,surface,0,0,8,8,pixels,UINT64_MAX)==-1);
    struct gui_message message={.type=1};
    for (unsigned i=0;i<GUI_QUEUE_DEPTH;i++) assert(gui_send(&message)==0);
    assert(gui_send(&message)==-1);
    current_pid=1;
    assert(gui_recv(&message)==1 && message.source_pid==2 && message.target_pid==1);
    gui_process_cleanup(1);
    current_pid=3;
    assert(gui_register_server()==0);
    assert(gui_surface_copy(1,surface,0,0,8,8,pixels,8)==-1);
    gui_process_cleanup(2);
    assert(gui_surface_copy(3,surface,0,0,8,8,pixels,8)==-1);
    gui_process_cleanup(3);
    assert(server_pid==0);
    for (unsigned i=0;i<GUI_MAX_QUEUES;i++) assert(queue_for_pid(i+10,true));
    current_pid=100;
    assert(gui_register_server()==-1 && server_pid==0);
    protocol_tests();
    current_pid=1; assert(gui_register_server()==0);
    assert(gui_server_ready()==0);
    keys_pending=true;assert(gui_server_ready()==1 && keys_pending);keys_pending=false;
    pointer_pending=true;assert(gui_server_ready()==1 && pointer_pending);pointer_pending=false;
    assert(gui_server_ready()==0);
    struct gui_message open=packet_message(DESK_OPEN,0,0,0);
    current_pid=2;assert(gui_server_ready()==-1 && gui_send(&open)==0);
    current_pid=1;assert(gui_server_ready()==1);
    struct gui_message queued;assert(gui_recv(&queued)==1 && gui_server_ready()==0);
    assert(gui_client_ready(1)==-1); /* Server cannot sleep on client messages. */
    gui_process_cleanup(2);assert(queue_for_pid(2,false)==NULL);
    current_pid=0;assert(gui_client_ready(1)==-1 && gui_server_ready()==-1);
    current_pid=2;assert(gui_client_ready(0)==-1 && gui_client_ready(99)==-1);
    assert(gui_client_ready(1)==0 && queue_for_pid(2,false)==NULL); /* No allocation. */
    current_pid=1;
    struct gui_message wheel=packet_message(DESK_SCROLL,2,0,0),received;
    current_pid=2; assert(gui_send(&wheel)==-1); /* Clients cannot forge input. */
    current_pid=1; assert(gui_send(&wheel)==0);
    current_pid=2;assert(gui_client_ready(1)==1 && poll_wakes);
    assert(gui_recv(&received)==1 && received.source_pid==1);
    assert(gui_client_ready(1)==0);
    wheel=packet_message(DESK_POINTER,2,0,0);
    assert(gui_send(&wheel)==-1);
    current_pid=1; assert(gui_send(&wheel)==0);
    current_pid=2; assert(gui_recv(&received)==1 && received.source_pid==1);
    struct gui_message menu=packet_message(DESK_MENU_SET,0,0,1);
    assert(gui_send(&menu)==0);
    current_pid=1;assert(gui_recv(&received)==1 && received.source_pid==2);
    menu.target_pid=2;assert(gui_send(&menu)==-1); /* Server cannot impersonate app menus. */
    menu=packet_message(DESK_MENU_ACTION,2,0,1);assert(gui_send(&menu)==0);
    current_pid=2;assert(gui_recv(&received)==1 && received.source_pid==1);
    assert(gui_send(&menu)==-1); /* Clients cannot forge desktop menu commands. */
    struct gui_message notice=packet_message(DESK_NOTIFY,0,0,1);
    assert(gui_send(&notice)==0);
    current_pid=1;assert(gui_recv(&received)==1 && received.source_pid==2);
    notice.target_pid=2;assert(gui_send(&notice)==-1);
    notice=packet_message(DESK_NOTIFY_ACK,2,0,1);assert(gui_send(&notice)==0);
    current_pid=2;assert(gui_recv(&received)==1 && received.source_pid==1);
    assert(gui_send(&notice)==-1); /* An app cannot forge an acknowledgement. */
    struct gui_message context=packet_message(DESK_CONTEXT,2,0,1);
    assert(gui_send(&context)==-1);
    current_pid=1;assert(gui_send(&context)==0);
    current_pid=2;assert(gui_recv(&received)==1 && received.source_pid==1);
    gui_process_cleanup(1); gui_process_cleanup(2);
    current_pid=2;unsigned wakes_before=poll_wakes;
    gui_process_cleanup(1);
    assert(poll_wakes>wakes_before && gui_client_ready(1)==-1);
    current_pid=3;assert(gui_register_server()==0);
    current_pid=2;assert(gui_client_ready(1)==-1 && gui_client_ready(3)==0);
    puts("gui: PASS exclusive registration, full queue, ownership, bounds, cleanup, allocation failure");
    return 0;
}
