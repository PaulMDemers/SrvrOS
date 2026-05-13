#ifndef SRVROS_USER_GFX_H
#define SRVROS_USER_GFX_H

#include <stdint.h>

struct gfx_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
};

int gfx_info(struct gfx_info *info);
int putpixel(uint64_t x, uint64_t y, uint32_t rgb);
int fillrect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb);

#endif
