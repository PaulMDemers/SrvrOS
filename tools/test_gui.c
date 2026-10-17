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
    puts("gui: PASS exclusive registration, full queue, ownership, bounds, cleanup, allocation failure");
    return 0;
}
