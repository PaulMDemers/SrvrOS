#include <srvros/gfx.h>
#include <srvros/sys.h>

#define SYS_GFX_INFO 15
#define SYS_GFX_PUT_PIXEL 16
#define SYS_GFX_FILL_RECT 17

int gfx_info(struct gfx_info *info) {
    if (info != 0) {
        info->abi_version = SRV_ABI_VERSION;
        info->struct_size = sizeof(*info);
    }
    return (int)srv_syscall1(SYS_GFX_INFO, (long)info);
}

int putpixel(uint64_t x, uint64_t y, uint32_t rgb) {
    return (int)srv_syscall3(SYS_GFX_PUT_PIXEL, (long)x, (long)y, (long)rgb);
}

int fillrect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb) {
    return (int)srv_syscall5(SYS_GFX_FILL_RECT, (long)x, (long)y, (long)width, (long)height, (long)rgb);
}
