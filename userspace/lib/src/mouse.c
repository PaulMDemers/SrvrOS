#include <srvros/mouse.h>
#include <srvros/sys.h>

#define SYS_MOUSE_SCAN 18

int mouse_scan(struct mouse_event *event) {
    return (int)srv_syscall1(SYS_MOUSE_SCAN, (long)event);
}
