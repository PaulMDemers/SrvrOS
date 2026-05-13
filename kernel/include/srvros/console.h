#ifndef SRVROS_CONSOLE_H
#define SRVROS_CONSOLE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

struct limine_framebuffer;

void console_init(struct limine_framebuffer *framebuffer);
void console_clear(void);
void console_get_size(uint64_t *columns_out, uint64_t *rows_out);
void console_set_cursor(uint64_t x, uint64_t y);
void console_putc(char c);
void console_write(const char *s);
void console_write_hex64(uint64_t value);
void console_write_dec(uint64_t value);
void console_printf(const char *fmt, ...);
bool console_framebuffer_info(uint64_t *width_out, uint64_t *height_out, uint64_t *pitch_out);
bool console_put_pixel(uint64_t x, uint64_t y, uint32_t rgb);
bool console_fill_rectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb);

#endif
