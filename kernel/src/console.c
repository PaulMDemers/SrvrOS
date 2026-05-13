#include <srvros/console.h>
#include <srvros/serial.h>

#include <limine.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define CELL_WIDTH 8
#define CELL_HEIGHT 16

static struct limine_framebuffer *fb;
static uint64_t columns;
static uint64_t rows;
static uint64_t cursor_x;
static uint64_t cursor_y;
static bool framebuffer_console_ready;

static const uint8_t glyph_space[GLYPH_HEIGHT] = { 0, 0, 0, 0, 0, 0, 0 };
static const uint8_t glyph_unknown[GLYPH_HEIGHT] = { 0x1e, 0x01, 0x01, 0x0e, 0x04, 0x00, 0x04 };

struct glyph_entry {
    char ch;
    uint8_t rows[GLYPH_HEIGHT];
};

static const struct glyph_entry glyphs[] = {
    { '0', { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e } },
    { '1', { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { '2', { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f } },
    { '3', { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e } },
    { '4', { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 } },
    { '5', { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e } },
    { '6', { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e } },
    { '7', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 } },
    { '8', { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e } },
    { '9', { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e } },
    { 'A', { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'B', { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e } },
    { 'C', { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e } },
    { 'D', { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e } },
    { 'E', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f } },
    { 'F', { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 } },
    { 'G', { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f } },
    { 'H', { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 } },
    { 'I', { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e } },
    { 'J', { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c } },
    { 'K', { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 } },
    { 'L', { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f } },
    { 'M', { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 } },
    { 'N', { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 } },
    { 'O', { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { 'P', { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 } },
    { 'Q', { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d } },
    { 'R', { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 } },
    { 'S', { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e } },
    { 'T', { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { 'U', { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e } },
    { 'V', { 0x11, 0x11, 0x11, 0x11, 0x0a, 0x0a, 0x04 } },
    { 'W', { 0x11, 0x11, 0x11, 0x15, 0x15, 0x1b, 0x11 } },
    { 'X', { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 } },
    { 'Y', { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 } },
    { 'Z', { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f } },
    { '.', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x0c, 0x0c } },
    { ',', { 0x00, 0x00, 0x00, 0x00, 0x0c, 0x04, 0x08 } },
    { ':', { 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x0c, 0x00 } },
    { ';', { 0x00, 0x0c, 0x0c, 0x00, 0x0c, 0x04, 0x08 } },
    { '-', { 0x00, 0x00, 0x00, 0x1f, 0x00, 0x00, 0x00 } },
    { '_', { 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x1f } },
    { '=', { 0x00, 0x00, 0x1f, 0x00, 0x1f, 0x00, 0x00 } },
    { '+', { 0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00 } },
    { '/', { 0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10 } },
    { '\\', { 0x10, 0x10, 0x08, 0x04, 0x02, 0x01, 0x01 } },
    { '[', { 0x0e, 0x08, 0x08, 0x08, 0x08, 0x08, 0x0e } },
    { ']', { 0x0e, 0x02, 0x02, 0x02, 0x02, 0x02, 0x0e } },
    { '(', { 0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02 } },
    { ')', { 0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08 } },
    { '<', { 0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02 } },
    { '>', { 0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08 } },
    { '*', { 0x00, 0x15, 0x0e, 0x1f, 0x0e, 0x15, 0x00 } },
    { '!', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x00, 0x04 } },
    { '?', { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x00, 0x04 } },
    { '\'', { 0x04, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00 } },
    { '"', { 0x0a, 0x0a, 0x0a, 0x00, 0x00, 0x00, 0x00 } },
    { '`', { 0x08, 0x04, 0x02, 0x00, 0x00, 0x00, 0x00 } },
    { '|', { 0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 } },
    { '#', { 0x0a, 0x1f, 0x0a, 0x0a, 0x1f, 0x0a, 0x00 } },
    { '$', { 0x04, 0x0f, 0x14, 0x0e, 0x05, 0x1e, 0x04 } },
    { '%', { 0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03 } },
    { '&', { 0x0c, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0d } },
    { '@', { 0x0e, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0f } },
    { '^', { 0x04, 0x0a, 0x11, 0x00, 0x00, 0x00, 0x00 } },
    { '~', { 0x00, 0x00, 0x08, 0x15, 0x02, 0x00, 0x00 } },
};

static uint32_t color(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << fb->red_mask_shift) |
        ((uint32_t)g << fb->green_mask_shift) |
        ((uint32_t)b << fb->blue_mask_shift);
}

static uint32_t rgb_color(uint32_t rgb) {
    return color((uint8_t)(rgb >> 16), (uint8_t)(rgb >> 8), (uint8_t)rgb);
}

static uint32_t bg_color(void) {
    return color(0x0b, 0x10, 0x1a);
}

static uint32_t fg_color(void) {
    return color(0xe6, 0xed, 0xf3);
}

static uint32_t accent_color(void) {
    return color(0x7d, 0xde, 0xb5);
}

static void put_pixel(uint64_t x, uint64_t y, uint32_t value) {
    if (fb == NULL || x >= fb->width || y >= fb->height) {
        return;
    }

    volatile uint32_t *pixels = fb->address;
    pixels[y * (fb->pitch / 4) + x] = value;
}

static void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t value) {
    for (uint64_t py = y; py < y + height && py < fb->height; py++) {
        for (uint64_t px = x; px < x + width && px < fb->width; px++) {
            put_pixel(px, py, value);
        }
    }
}

static const uint8_t *glyph_for(char c) {
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }

    if (c == ' ') {
        return glyph_space;
    }

    for (size_t i = 0; i < sizeof(glyphs) / sizeof(glyphs[0]); i++) {
        if (glyphs[i].ch == c) {
            return glyphs[i].rows;
        }
    }

    return glyph_unknown;
}

static void draw_cursor(bool visible) {
    if (!framebuffer_console_ready) {
        return;
    }

    uint64_t x = cursor_x * CELL_WIDTH;
    uint64_t y = cursor_y * CELL_HEIGHT + CELL_HEIGHT - 2;
    fill_rect(x, y, CELL_WIDTH - 1, 2, visible ? accent_color() : bg_color());
}

static void draw_glyph(char c, uint64_t cell_x, uint64_t cell_y) {
    const uint8_t *glyph = glyph_for(c);
    uint64_t x = cell_x * CELL_WIDTH;
    uint64_t y = cell_y * CELL_HEIGHT;

    fill_rect(x, y, CELL_WIDTH, CELL_HEIGHT, bg_color());

    for (uint64_t row = 0; row < GLYPH_HEIGHT; row++) {
        uint8_t bits = glyph[row];
        for (uint64_t col = 0; col < GLYPH_WIDTH; col++) {
            if ((bits & (1u << (GLYPH_WIDTH - 1 - col))) != 0) {
                uint64_t px = x + col + 1;
                uint64_t py = y + row * 2 + 1;
                put_pixel(px, py, fg_color());
                put_pixel(px, py + 1, fg_color());
            }
        }
    }
}

static void clear_last_row(void) {
    fill_rect(0, (rows - 1) * CELL_HEIGHT, columns * CELL_WIDTH, CELL_HEIGHT, bg_color());
}

static void scroll_one_row(void) {
    volatile uint32_t *pixels = fb->address;
    uint64_t stride = fb->pitch / 4;
    uint64_t scroll_height = (rows - 1) * CELL_HEIGHT;

    for (uint64_t y = 0; y < scroll_height; y++) {
        for (uint64_t x = 0; x < columns * CELL_WIDTH; x++) {
            pixels[y * stride + x] = pixels[(y + CELL_HEIGHT) * stride + x];
        }
    }

    clear_last_row();
}

static void newline(void) {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= rows) {
        scroll_one_row();
        cursor_y = rows - 1;
    }
}

static void framebuffer_putc(char c) {
    if (!framebuffer_console_ready) {
        return;
    }

    draw_cursor(false);

    if (c == '\n') {
        newline();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            framebuffer_putc(' ');
        }
    } else if (c == '\b') {
        if (cursor_x > 0) {
            cursor_x--;
        } else if (cursor_y > 0) {
            cursor_y--;
            cursor_x = columns - 1;
        }
        draw_glyph(' ', cursor_x, cursor_y);
    } else {
        draw_glyph(c, cursor_x, cursor_y);
        cursor_x++;
        if (cursor_x >= columns) {
            newline();
        }
    }

    draw_cursor(true);
}

void console_clear(void) {
    if (!framebuffer_console_ready) {
        return;
    }

    fill_rect(0, 0, fb->width, fb->height, bg_color());
    cursor_x = 0;
    cursor_y = 0;
    draw_cursor(true);
}

void console_get_size(uint64_t *columns_out, uint64_t *rows_out) {
    if (columns_out != NULL) {
        *columns_out = framebuffer_console_ready ? columns : 0;
    }
    if (rows_out != NULL) {
        *rows_out = framebuffer_console_ready ? rows : 0;
    }
}

void console_set_cursor(uint64_t x, uint64_t y) {
    if (!framebuffer_console_ready) {
        return;
    }

    draw_cursor(false);
    cursor_x = x < columns ? x : columns - 1;
    cursor_y = y < rows ? y : rows - 1;
    draw_cursor(true);
}

void console_init(struct limine_framebuffer *framebuffer) {
    fb = framebuffer;
    framebuffer_console_ready = false;

    if (fb == NULL || fb->bpp != 32 || fb->memory_model != LIMINE_FRAMEBUFFER_RGB) {
        return;
    }

    columns = fb->width / CELL_WIDTH;
    rows = fb->height / CELL_HEIGHT;
    if (columns == 0 || rows == 0) {
        return;
    }

    framebuffer_console_ready = true;
    console_clear();
}

void console_putc(char c) {
    serial_putc(c);
    framebuffer_putc(c);
}

void console_write(const char *s) {
    while (*s != '\0') {
        console_putc(*s++);
    }
}

void console_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";

    console_write("0x");
    for (int shift = 60; shift >= 0; shift -= 4) {
        console_putc(digits[(value >> shift) & 0xf]);
    }
}

void console_write_dec(uint64_t value) {
    char buf[21];
    size_t i = sizeof(buf);

    buf[--i] = '\0';
    if (value == 0) {
        console_putc('0');
        return;
    }

    while (value > 0 && i > 0) {
        buf[--i] = (char)('0' + (value % 10));
        value /= 10;
    }

    console_write(&buf[i]);
}

static void console_vprintf(const char *fmt, va_list args) {
    for (const char *p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            console_putc(*p);
            continue;
        }

        p++;
        if (*p == '\0') {
            break;
        }

        switch (*p) {
        case '%':
            console_putc('%');
            break;
        case 'c':
            console_putc((char)va_arg(args, int));
            break;
        case 's': {
            const char *s = va_arg(args, const char *);
            console_write(s != NULL ? s : "(null)");
            break;
        }
        case 'u':
            console_write_dec(va_arg(args, uint64_t));
            break;
        case 'x':
            console_write_hex64(va_arg(args, uint64_t));
            break;
        default:
            console_putc('%');
            console_putc(*p);
            break;
        }
    }
}

void console_printf(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    console_vprintf(fmt, args);
    va_end(args);
}

bool console_framebuffer_info(uint64_t *width_out, uint64_t *height_out, uint64_t *pitch_out) {
    if (fb == NULL || width_out == NULL || height_out == NULL || pitch_out == NULL) {
        return false;
    }

    *width_out = fb->width;
    *height_out = fb->height;
    *pitch_out = fb->pitch;
    return true;
}

bool console_put_pixel(uint64_t x, uint64_t y, uint32_t rgb) {
    if (fb == NULL || x >= fb->width || y >= fb->height) {
        return false;
    }

    put_pixel(x, y, rgb_color(rgb));
    return true;
}

bool console_fill_rectangle(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t rgb) {
    if (fb == NULL || x >= fb->width || y >= fb->height) {
        return false;
    }

    fill_rect(x, y, width, height, rgb_color(rgb));
    return true;
}
