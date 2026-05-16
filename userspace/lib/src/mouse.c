#include <srvros/mouse.h>
#include <srvros/sys.h>

#define SYS_MOUSE_SCAN 18

int mouse_scan(struct mouse_event *event) {
    if (event != 0) {
        event->abi_version = SRV_ABI_VERSION;
        event->struct_size = sizeof(*event);
    }
    return (int)srv_syscall1(SYS_MOUSE_SCAN, (long)event);
}
