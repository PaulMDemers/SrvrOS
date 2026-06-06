#ifndef SRVROS_CONSOLE_H
#define SRVROS_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct limine_framebuffer;

struct console_display_info {
    uint64_t width;
    uint64_t height;
    uint64_t pitch;
    uint16_t bpp;
    uint16_t memory_model;
    uint16_t red_mask_size;
    uint16_t red_mask_shift;
    uint16_t green_mask_size;
    uint16_t green_mask_shift;
    uint16_t blue_mask_size;
    uint16_t blue_mask_shift;
    uint16_t reserved;
};

void console_init(struct limine_framebuffer *framebuffer);
void console_clear(void);
void console_set_framebuffer_muted(bool muted);
void console_set_framebuffer_muted_owner(uint64_t owner_pid, bool muted);
void console_release_framebuffer_mute_owner(uint64_t owner_pid);
bool console_framebuffer_muted(void);
void console_get_size(uint64_t *columns_out, uint64_t *rows_out);
void console_set_cursor(uint64_t x, uint64_t y);
void console_putc(char c);
void console_write(const char *s);
void console_write_hex64(uint64_t value);
void console_write_dec(uint64_t value);
void console_printf(const char *fmt, ...);
bool console_framebuffer_info(uint64_t *width_out, uint64_t *height_out, uint64_t *pitch_out);
bool console_display_info(struct console_display_info *info);
bool console_put_pixel(uint64_t x, uint64_t y, uint32_t rgb);
bool console_fill_rectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb);
bool console_blit_rgb(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
    const uint32_t *rgb_pixels, uint64_t stride);

#endif
