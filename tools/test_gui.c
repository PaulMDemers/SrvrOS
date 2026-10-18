#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#define SRVROS_GUI_HOST_TEST
#include "../kernel/src/gui.c"

static uint64_t current_pid=1;
struct process *process_current(void) { return (struct process *)(uintptr_t)current_pid; }
uint64_t process_pid(const struct process *p) { return (uintptr_t)p; }
void *kmalloc(size_t bytes) { return malloc(bytes); }
void kfree(void *p) { free(p); }

static struct gui_message packet_message(unsigned type, uint64_t target,
    uint64_t surface, uint64_t serial) {
    struct gui_message m={.type=DESK_TRANSPORT_MESSAGE,.target_pid=target};
    struct desk_packet p={.version=DESK_PROTOCOL_VERSION,.size=sizeof(p),
        .type=type,.surface=surface,.serial=serial,.generation=7,.width=2,.height=2};
    memcpy(m.text,&p,sizeof(p));
    return m;
}

static void protocol_tests(void) {
    for (unsigned i=0;i<GUI_MAX_QUEUES;i++) gui_process_cleanup(i+10);
    current_pid=1; assert(gui_register_server()==0);
    current_pid=2;
    assert(gui_surface_create(2,2,2,1ull<<40)==-1);
    uint32_t pixels[4]={11,22,33,44}, copy[4]={0};
    int64_t id=gui_surface_create(2,2,2,DESK_SURFACE_STREAM);
    assert(id>0 && gui_surface_blit(2,id,0,0,2,2,pixels,2)==0);
    struct gui_message commit=packet_message(DESK_COMMIT,0,id,1), temp;
    current_pid=3; assert(gui_send(&commit)==-1);
    current_pid=2;
    struct gui_message bad=commit;
    bad.text[0]=99; assert(gui_send(&bad)==-1);
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
    legacy.target_pid=2;
    for (unsigned i=0;i<GUI_QUEUE_DEPTH;i++) assert(gui_send(&legacy)==0);
    assert(gui_send(&release)==-1 && surface_for_id(id)->sealed);
    current_pid=2; while (gui_recv(&temp)>0) {}
    current_pid=1; assert(gui_send(&release)==0 && !surface_for_id(id)->sealed);
    assert(gui_send(&release)==-1);
    current_pid=2; assert(gui_send(&commit)==-1); /* Replay rejected. */
    commit=packet_message(DESK_COMMIT,0,id,2); assert(gui_send(&commit)==0);
    gui_process_cleanup(1); /* Pending client can reclaim after server death. */
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
    struct gui_message wheel=packet_message(DESK_SCROLL,2,0,0),received;
    current_pid=2; assert(gui_send(&wheel)==-1); /* Clients cannot forge input. */
    current_pid=1; assert(gui_send(&wheel)==0);
    current_pid=2; assert(gui_recv(&received)==1 && received.source_pid==1);
    gui_process_cleanup(1); gui_process_cleanup(2);
    puts("gui: PASS exclusive registration, full queue, ownership, bounds, cleanup, allocation failure");
    return 0;
}
