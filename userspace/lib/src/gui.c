#include <srvros/gui.h>
#include <srvros/sys.h>

int gui_register_server(void) {
    return (int)srv_syscall0(SYS_GUI_REGISTER);
}
int gui_audio(unsigned operation,unsigned value,struct srv_audio_status *status) {
    return (int)srv_syscall4(SYS_AUDIO,operation,value,(long)status,sizeof(*status));
}
int gui_brightness(unsigned percent) { return (int)srv_syscall1(SYS_GUI_BRIGHTNESS,percent); }
int gui_pointer_config(unsigned speed,unsigned options) {
    return (int)srv_syscall2(SYS_GUI_POINTER_CONFIG,speed,options);
}
int gui_keyboard_mode(int enabled) {
    return (int)srv_syscall1(SYS_GUI_KEYBOARD_MODE,enabled);
}
int gui_keyboard_read(struct srv_key_event *event) {
    if (!event) return -1;
    event->abi_version=SRV_KEY_EVENT_VERSION; event->struct_size=sizeof(*event);
    return (int)srv_syscall1(SYS_GUI_KEYBOARD_READ,(long)event);
}

int gui_send(const struct gui_message *message) {
    if (message == 0) {
        return -1;
    }
    struct gui_message copy = *message;
    copy.abi_version = SRV_ABI_VERSION;
    copy.struct_size = sizeof(copy);
    return (int)srv_syscall1(SYS_GUI_SEND, (long)&copy);
}

int gui_wait(uint64_t expected_server,uint64_t ticks) {
    return (int)srv_syscall2(SYS_GUI_WAIT,expected_server,ticks);
}
int gui_wait_io(struct srv_gui_wait *request) {
    return (int)srv_syscall1(SYS_GUI_WAIT_IO,(long)request);
}
long gui_cancel_wait(void) { return srv_syscall0(SYS_GUI_CANCEL_WAIT); }
int gui_server_wait(uint64_t ticks) {
    return (int)srv_syscall1(SYS_GUI_SERVER_WAIT,ticks);
}

int gui_recv(struct gui_message *message) {
    if (message != 0) {
        message->abi_version = SRV_ABI_VERSION;
        message->struct_size = sizeof(*message);
    }
    return (int)srv_syscall1(SYS_GUI_RECV, (long)message);
}

int gui_surface_create(uint64_t width, uint64_t height, uint64_t flags, uint64_t *surface_id_out) {
    struct srv_gui_surface_create request = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(request),
        .width = width,
        .height = height,
        .flags = flags,
    };
    int result = (int)srv_syscall1(SYS_GUI_SURFACE_CREATE, (long)&request);
    if (result == 0 && surface_id_out != 0) {
        *surface_id_out = request.surface_id;
    }
    return result;
}

int gui_surface_destroy(uint64_t surface_id) {
    return (int)srv_syscall1(SYS_GUI_SURFACE_DESTROY, (long)surface_id);
}

int gui_surface_blit(uint64_t surface_id, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height, const uint32_t *pixels, uint64_t stride) {
    struct srv_gui_surface_rect request = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(request),
        .surface_id = surface_id,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .stride = stride,
        .pixels = (uint32_t *)pixels,
    };
    return (int)srv_syscall1(SYS_GUI_SURFACE_BLIT, (long)&request);
}

int gui_surface_copy(uint64_t surface_id, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height, uint32_t *pixels, uint64_t stride) {
    struct srv_gui_surface_rect request = {
        .abi_version = SRV_ABI_VERSION,
        .struct_size = sizeof(request),
        .surface_id = surface_id,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .stride = stride,
        .pixels = pixels,
    };
    return (int)srv_syscall1(SYS_GUI_SURFACE_COPY, (long)&request);
}

long spawn_bg(const char *path) {
    return srv_syscall1(SYS_SPAWN_BG, (long)path);
}
