#ifndef SRVROS_USER_GFX_H
#define SRVROS_USER_GFX_H

#include <stdint.h>

#define GFX_ACCEL_NONE 0
#define GFX_ACCEL_INTEL_GEN8 1

#define GFX_FLAG_ACCEL_DEVICE_PRESENT 0x00000001ull
#define GFX_FLAG_ACCEL_MMIO_MAPPED 0x00000002ull
#define GFX_FLAG_ACCEL_BLITTER_PLANNED 0x00000004ull
#define GFX_FLAG_ACCEL_RENDER_PLANNED 0x00000008ull

struct gfx_info {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint64_t flags;
    uint32_t bpp;
    uint32_t memory_model;
    uint32_t red_mask_size;
    uint32_t red_mask_shift;
    uint32_t green_mask_size;
    uint32_t green_mask_shift;
    uint32_t blue_mask_size;
    uint32_t blue_mask_shift;
    uint64_t scale_num;
    uint64_t scale_den;
    uint64_t accel_backend;
    uint64_t reserved;
};

int gfx_info(struct gfx_info *info);
int putpixel(uint64_t x, uint64_t y, uint32_t rgb);
int fillrect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb);
int blitrect(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
    const uint32_t *rgb_pixels, uint64_t stride);
int gfx_console_mute(int muted);
int gfx_console_muted(void);

#endif
