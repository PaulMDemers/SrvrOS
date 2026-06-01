#include <srvros/gui.h>
#include <srvros/sys.h>

int gui_register_server(void) {
    return (int)srv_syscall0(SYS_GUI_REGISTER);
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
