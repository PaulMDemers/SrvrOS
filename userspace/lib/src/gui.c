#include <srvros/gui.h>
#include <srvros/sys.h>

#define SYS_SPAWN_BG 19
#define SYS_GUI_REGISTER 20
#define SYS_GUI_SEND 21
#define SYS_GUI_RECV 22

int gui_register_server(void) {
    return (int)srv_syscall0(SYS_GUI_REGISTER);
}

int gui_send(const struct gui_message *message) {
    return (int)srv_syscall1(SYS_GUI_SEND, (long)message);
}

int gui_recv(struct gui_message *message) {
    return (int)srv_syscall1(SYS_GUI_RECV, (long)message);
}

long spawn_bg(const char *path) {
    return srv_syscall1(SYS_SPAWN_BG, (long)path);
}
