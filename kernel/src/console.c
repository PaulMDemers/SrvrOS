#include <srvros/latency.h>
#include <srvros/console.h>
#include <srvros/bootlog.h>
#include <srvros/serial.h>
#include <srvros/font5x7.h>
#include <srvros/workqueue.h>
#include <srvros/scheduler.h>

#include <limine.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define GLYPH_WIDTH 5
#define GLYPH_HEIGHT 7
#define CELL_WIDTH 8
#define CELL_HEIGHT 16
#define ANSI_PARAM_MAX 4

enum ansi_state {
    ANSI_NORMAL,
    ANSI_ESC,
    ANSI_CSI,
};

static struct limine_framebuffer *fb;
static uint64_t columns;
static uint64_t rows;
static uint64_t cursor_x;
static uint64_t cursor_y;
static bool framebuffer_console_ready;
static bool framebuffer_console_muted;
static uint64_t framebuffer_console_mute_owner;
static enum ansi_state ansi_state;
static uint64_t ansi_params[ANSI_PARAM_MAX];
static uint64_t ansi_param_count;
static bool ansi_param_active;
static bool ansi_private;

/* Fixed storage also works before the allocator and in the panic path.
 * The text viewport is capped at 4096 x 4096; framebuffer clearing/blits retain
 * their full size. No heap allocation or framebuffer read is used by text. */
#define MAX_COLUMNS 512
#define MAX_ROWS 256
#define WRITE_BATCH 64
#define LOG_RECORD_BATCH 1024
static unsigned char cells[MAX_ROWS][MAX_COLUMNS];
static uint16_t dirty_left[MAX_ROWS], dirty_right[MAX_ROWS];
static uint32_t glyph_cache[SRV_FONT_COUNT][CELL_HEIGHT][CELL_WIDTH];
static uint32_t scanline[MAX_COLUMNS * CELL_WIDTH];
static bool presenting;
static bool deferred_output;
static bool serial_output = true;
static uint64_t previous_cursor_x, previous_cursor_y;
static struct console_stats stats;

static uint64_t console_irq_save(void) {
#ifdef SRVROS_CONSOLE_HOST_TEST
#ifdef SRVROS_CONSOLE_TEST_IRQ_SAVE
    return SRVROS_CONSOLE_TEST_IRQ_SAVE();
#else
    return 0;
#endif
#else
    uint64_t flags;
    __asm__ volatile ("pushfq; popq %0; cli" : "=r"(flags) : : "memory");
    latency_enter(LATENCY_CONSOLE);
    return flags;
#endif
}

static void console_irq_restore(uint64_t flags) {
#ifdef SRVROS_CONSOLE_HOST_TEST
#ifdef SRVROS_CONSOLE_TEST_IRQ_RESTORE
    SRVROS_CONSOLE_TEST_IRQ_RESTORE(flags);
#else
    (void)flags;
#endif
#else
    latency_leave(LATENCY_CONSOLE);
    if (flags & (1ull << 9)) __asm__ volatile ("sti" : : : "memory");
#endif
}

static void dirty_range(uint64_t row, uint64_t left, uint64_t right) {
    if (row >= rows || left >= right) return;
    if (left < dirty_left[row]) dirty_left[row] = (uint16_t)left;
    if (right > dirty_right[row]) dirty_right[row] = (uint16_t)right;
}

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

/* Public pixel APIs still draw directly; console presentation uses contiguous
 * device-memory writes. Never use device memory as the scroll source. */
static void fill_rect(uint64_t x, uint64_t y, uint64_t width, uint64_t height, uint32_t value) {
    if (fb == NULL || x >= fb->width || y >= fb->height) return;
    if (width > fb->width - x) width = fb->width - x;
    if (height > fb->height - y) height = fb->height - y;
    volatile uint32_t *pixels = fb->address;
    for (uint64_t py = y; py < y + height; py++) {
        volatile uint32_t *line = pixels + py * (fb->pitch / 4) + x;
        for (uint64_t px = 0; px < width; px++) line[px] = value;
    }
}

static void draw_glyph(char c, uint64_t cell_x, uint64_t cell_y) {
    unsigned char ch = (unsigned char)c;
    if (ch < SRV_FONT_FIRST || ch >= SRV_FONT_FIRST + SRV_FONT_COUNT) ch = '?';
    if (cells[cell_y][cell_x] != ch) {
        cells[cell_y][cell_x] = ch;
        dirty_range(cell_y, cell_x, cell_x + 1);
    }
}

static void clear_row_range(uint64_t row, uint64_t start, uint64_t end) {
    if (row >= rows || start >= columns || end <= start) return;
    if (end > columns) end = columns;
    for (uint64_t col = start; col < end; col++) cells[row][col] = ' ';
    dirty_range(row, start, end);
}

static void clear_from_cursor(void) {
    clear_row_range(cursor_y, cursor_x, columns);
    for (uint64_t row = cursor_y + 1; row < rows; row++) clear_row_range(row, 0, columns);
}

static void clear_to_cursor(void) {
    for (uint64_t row = 0; row < cursor_y; row++) clear_row_range(row, 0, columns);
    clear_row_range(cursor_y, 0, cursor_x + 1);
}

static void scroll_one_row(void) {
    for (uint64_t row = 0; row + 1 < rows; row++) {
        for (uint64_t col = 0; col < columns; col++) cells[row][col] = cells[row + 1][col];
        dirty_range(row, 0, columns);
    }
    clear_row_range(rows - 1, 0, columns);
    stats.scrolls++;
}

void console_flush(void) {
    uint64_t flags = console_irq_save();
    if (!framebuffer_console_ready || framebuffer_console_muted || presenting) {
        console_irq_restore(flags);
        return;
    }
    presenting = true;
    dirty_range(previous_cursor_y, previous_cursor_x, previous_cursor_x + 1);
    dirty_range(cursor_y, cursor_x, cursor_x + 1);
    stats.presents++;
    console_irq_restore(flags);
    /* Snapshot one text row under a short critical section. IRQs/other writers
     * may update the model during rasterization; their damage stays queued.
     * Reentrant writers never wait for the presenter or reuse its scanline. */
    for (uint64_t row = 0; row < rows; row++) {
        unsigned char chars[MAX_COLUMNS];
        flags = console_irq_save();
        uint64_t left = dirty_left[row], right = dirty_right[row];
        dirty_left[row] = (uint16_t)columns;
        dirty_right[row] = 0;
        for (uint64_t col = left; col < right; col++) chars[col] = cells[row][col];
        console_irq_restore(flags);
        if (left >= right) continue;
        for (uint64_t py = 0; py < CELL_HEIGHT; py++) {
            for (uint64_t col = left; col < right; col++) {
                const uint32_t *tile = glyph_cache[chars[col] - SRV_FONT_FIRST][py];
                for (uint64_t px = 0; px < CELL_WIDTH; px++) scanline[col * CELL_WIDTH + px] = tile[px];
            }
            volatile uint32_t *dst = (volatile uint32_t *)fb->address +
                (row * CELL_HEIGHT + py) * (fb->pitch / 4) + left * CELL_WIDTH;
            const uint32_t *src = scanline + left * CELL_WIDTH;
            uint64_t length = (right - left) * CELL_WIDTH;
            /* A graphics owner may claim the display while rasterizing. Check
             * under the same short critical section as each scanline store so
             * an interrupted console cannot paint over its new framebuffer. */
            flags = console_irq_save();
            if (framebuffer_console_muted) {
                presenting = false;
                console_irq_restore(flags);
                return;
            }
            for (uint64_t i = 0; i < length; i++) dst[i] = src[i];
            console_irq_restore(flags);
        }
        stats.framebuffer_bytes_written += (right - left) * CELL_WIDTH * CELL_HEIGHT * 4;
        stats.cells_painted += right - left;
    }
    flags = console_irq_save();
    previous_cursor_x = cursor_x;
    previous_cursor_y = cursor_y;
    if (!framebuffer_console_muted) {
        fill_rect(cursor_x * CELL_WIDTH, cursor_y * CELL_HEIGHT + CELL_HEIGHT - 1,
            CELL_WIDTH - 1, 1, accent_color());
        stats.framebuffer_bytes_written += (CELL_WIDTH - 1) * 4;
    }
    presenting = false;
    console_irq_restore(flags);
}

static bool console_damage_pending(void) {
    for (uint64_t row = 0; row < rows; row++) {
        if (dirty_left[row] < dirty_right[row]) return true;
    }
    return false;
}

static void console_present_work(void *arg) {
    (void)arg;
    if (framebuffer_console_muted) return;
    console_flush();
    uint64_t flags = console_irq_save();
    /* A preempting writer can dirty a row already presented. Queue another
     * bounded pass so the final update cannot remain stranded. */
    if (console_damage_pending()) (void)workqueue_schedule_io_safe(console_present_work, NULL);
    console_irq_restore(flags);
}

void console_enable_deferred(void) { deferred_output = true; }

/* Only for a terminal fault: the interrupted presenter will never resume. */
void console_emergency(void) {
    deferred_output = false;
    presenting = false;
    framebuffer_console_muted = false;
    serial_output = true;
}

/* Normal thread context, interrupts enabled. Wait for any preempted presenter,
 * then drain its remaining damage before reporting a completed benchmark. */
bool console_sync(void) {
    if (framebuffer_console_muted) return false;
    for (unsigned attempt = 0; attempt < 1000; attempt++) {
        console_flush();
        uint64_t flags = console_irq_save();
        bool done = !presenting && !console_damage_pending();
        console_irq_restore(flags);
        if (done) return true;
        scheduler_yield();
    }
    return false;
}

static void console_request_present(void) {
    if (deferred_output && workqueue_schedule_io_safe(console_present_work, NULL)) return;
    console_flush();
}

void console_get_stats(struct console_stats *out) {
    if (out == NULL) return;
    uint64_t flags = console_irq_save();
    *out = stats;
    console_irq_restore(flags);
}

bool console_set_serial_output(bool enabled) {
    uint64_t flags = console_irq_save();
    bool previous = serial_output;
    serial_output = enabled;
    console_irq_restore(flags);
    return previous;
}

static void newline(void) {
    cursor_x = 0;
    cursor_y++;

    if (cursor_y >= rows) {
        scroll_one_row();
        cursor_y = rows - 1;
    }
}

static void move_cursor(uint64_t x, uint64_t y) {
    cursor_x = x < columns ? x : columns - 1;
    cursor_y = y < rows ? y : rows - 1;
}

static uint64_t ansi_param(uint64_t index, uint64_t fallback) {
    if (index >= ansi_param_count || ansi_params[index] == 0) {
        return fallback;
    }
    return ansi_params[index];
}

static void ansi_reset(void) {
    ansi_state = ANSI_NORMAL;
    ansi_param_count = 0;
    ansi_param_active = false;
    ansi_private = false;
}

static void ansi_start_param(void) {
    if (!ansi_param_active) {
        if (ansi_param_count < ANSI_PARAM_MAX) {
            ansi_params[ansi_param_count] = 0;
        }
        ansi_param_active = true;
    }
}

static void ansi_finish_param(void) {
    if (ansi_param_active && ansi_param_count < ANSI_PARAM_MAX) {
        ansi_param_count++;
    } else if (!ansi_param_active && ansi_param_count < ANSI_PARAM_MAX) {
        ansi_params[ansi_param_count++] = 0;
    }
    ansi_param_active = false;
}

static void ansi_execute(char command) {
    if (ansi_param_active || ansi_param_count == 0) {
        ansi_finish_param();
    }

    if (command == 'H' || command == 'f') {
        uint64_t row = ansi_param(0, 1);
        uint64_t column = ansi_param(1, 1);
        move_cursor(column > 0 ? column - 1 : 0, row > 0 ? row - 1 : 0);
    } else if (command == 'A') {
        uint64_t count = ansi_param(0, 1);
        cursor_y = count > cursor_y ? 0 : cursor_y - count;
    } else if (command == 'B') {
        move_cursor(cursor_x, cursor_y + ansi_param(0, 1));
    } else if (command == 'C') {
        move_cursor(cursor_x + ansi_param(0, 1), cursor_y);
    } else if (command == 'D') {
        uint64_t count = ansi_param(0, 1);
        cursor_x = count > cursor_x ? 0 : cursor_x - count;
    } else if (command == 'G') {
        uint64_t column = ansi_param(0, 1);
        move_cursor(column > 0 ? column - 1 : 0, cursor_y);
    } else if (command == 'J') {
        uint64_t mode = ansi_param(0, 0);
        if (mode == 2) {
            for (uint64_t row = 0; row < rows; row++) clear_row_range(row, 0, columns);
            move_cursor(0, 0);
        } else if (mode == 1) {
            clear_to_cursor();
        } else {
            clear_from_cursor();
        }
    } else if (command == 'K') {
        uint64_t mode = ansi_param(0, 0);
        if (mode == 2) {
            clear_row_range(cursor_y, 0, columns);
        } else if (mode == 1) {
            clear_row_range(cursor_y, 0, cursor_x + 1);
        } else {
            clear_row_range(cursor_y, cursor_x, columns);
        }
    }
    ansi_reset();
}

static bool framebuffer_ansi_putc(char c) {
    if (ansi_state == ANSI_NORMAL) {
        if (c == 27) {
            ansi_state = ANSI_ESC;
            return true;
        }
        return false;
    }
    if (ansi_state == ANSI_ESC) {
        if (c == '[') {
            ansi_state = ANSI_CSI;
            ansi_param_count = 0;
            ansi_param_active = false;
            ansi_private = false;
            return true;
        }
        ansi_reset();
        return true;
    }
    if (c == '?') {
        ansi_private = true;
        return true;
    }
    if (c >= '0' && c <= '9') {
        ansi_start_param();
        if (ansi_param_count < ANSI_PARAM_MAX) {
            ansi_params[ansi_param_count] = ansi_params[ansi_param_count] * 10 + (uint64_t)(c - '0');
        }
        return true;
    }
    if (c == ';') {
        ansi_finish_param();
        return true;
    }
    if (c >= '@' && c <= '~') {
        if (ansi_private) {
            ansi_reset();
        } else {
            ansi_execute(c);
        }
        return true;
    }

    ansi_reset();
    return true;
}

static void framebuffer_putc_raw(char c) {
    if (!framebuffer_console_ready) {
        return;
    }


    if (c == '\n') {
        newline();
    } else if (c == '\r') {
        cursor_x = 0;
    } else if (c == '\t') {
        for (int i = 0; i < 4; i++) {
            framebuffer_putc_raw(' ');
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

}

static void framebuffer_putc(char c) {
    if (!framebuffer_console_ready || framebuffer_ansi_putc(c)) {
        return;
    }

    framebuffer_putc_raw(c);
}

void console_clear(void) {
    if (!framebuffer_console_ready) {
        return;
    }

    uint64_t flags = console_irq_save();
    for (uint64_t row = 0; row < rows; row++) clear_row_range(row, 0, columns);
    cursor_x = cursor_y = 0;
    ansi_reset();
    console_irq_restore(flags);
    if (framebuffer_console_muted) return;
    /* Clear unused margins once, not on every text update. */
    fill_rect(columns * CELL_WIDTH, 0, fb->width - columns * CELL_WIDTH, fb->height, bg_color());
    fill_rect(0, rows * CELL_HEIGHT, fb->width, fb->height - rows * CELL_HEIGHT, bg_color());
    console_flush();
}

void console_set_framebuffer_muted(bool muted) {
    framebuffer_console_muted = muted;
    framebuffer_console_mute_owner = 0;
}

void console_set_framebuffer_muted_owner(uint64_t owner_pid, bool muted) {
    if (muted) {
        framebuffer_console_muted = true;
        framebuffer_console_mute_owner = owner_pid;
        return;
    }
    if (framebuffer_console_mute_owner == 0 ||
        framebuffer_console_mute_owner == owner_pid) {
        framebuffer_console_muted = false;
        framebuffer_console_mute_owner = 0;
    }
}

void console_release_framebuffer_mute_owner(uint64_t owner_pid) {
    if (owner_pid != 0 && framebuffer_console_mute_owner == owner_pid) {
        framebuffer_console_muted = false;
        framebuffer_console_mute_owner = 0;
    }
}

bool console_framebuffer_muted(void) {
    return framebuffer_console_muted;
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

    uint64_t flags = console_irq_save();
    move_cursor(x, y);
    ansi_reset();
    console_irq_restore(flags);
    console_flush();
}

void console_init(struct limine_framebuffer *framebuffer) {
    fb = framebuffer;
    framebuffer_console_ready = false;
    framebuffer_console_muted = false;
    framebuffer_console_mute_owner = 0;

    if (fb == NULL || fb->address == NULL || fb->bpp != 32 ||
        fb->memory_model != LIMINE_FRAMEBUFFER_RGB || fb->width > UINT64_MAX / 4 ||
        fb->pitch < fb->width * 4 || fb->pitch % 4 != 0 ||
        fb->height > UINT64_MAX / (fb->pitch ? fb->pitch : 1) ||
        fb->red_mask_size != 8 || fb->green_mask_size != 8 || fb->blue_mask_size != 8 ||
        fb->red_mask_shift > 24 || fb->green_mask_shift > 24 || fb->blue_mask_shift > 24) {
        fb = NULL;
        return;
    }

    columns = fb->width / CELL_WIDTH;
    rows = fb->height / CELL_HEIGHT;
    if (columns > MAX_COLUMNS) columns = MAX_COLUMNS;
    if (rows > MAX_ROWS) rows = MAX_ROWS;
    if (columns == 0 || rows == 0) {
        return;
    }

    for (uint64_t g = 0; g < SRV_FONT_COUNT; g++) {
        for (uint64_t y = 0; y < CELL_HEIGHT; y++) {
            for (uint64_t x = 0; x < CELL_WIDTH; x++) {
                bool ink = y > 0 && y < 15 && x > 0 && x <= GLYPH_WIDTH &&
                    (srv_font5x7[g][(y - 1) / 2] & (1u << (GLYPH_WIDTH - x)));
                glyph_cache[g][y][x] = ink ? fg_color() : bg_color();
            }
        }
    }
    for (uint64_t row = 0; row < rows; row++) {
        dirty_left[row] = (uint16_t)columns;
        dirty_right[row] = 0;
    }
    presenting = false;
    deferred_output = false;
    previous_cursor_x = previous_cursor_y = 0;
    stats = (struct console_stats){0};
    framebuffer_console_ready = true;
    ansi_reset();
    console_clear();
}

void console_write_n(const char *s, size_t length) {
    while (length != 0) {
        /* Save one bounded record before any rendering or UART can yield.
         * Long streams remain chunked; diagnostic records fit in this bound.
         * Do not extend IRQ exclusion over framebuffer work or serial waits. */
        size_t record = length < LOG_RECORD_BATCH ? length : LOG_RECORD_BATCH;
        uint64_t flags = console_irq_save();
        for (size_t i = 0; i < record; i++) bootlog_putc(s[i]);
        console_irq_restore(flags);
        for (size_t offset = 0; offset < record;) {
            size_t count = record - offset;
            if (count > WRITE_BATCH) count = WRITE_BATCH;
            flags = console_irq_save();
            bool visible = !framebuffer_console_muted;
            bool mirror = serial_output;
            if (visible) for (size_t i = 0; i < count; i++) framebuffer_putc(s[offset+i]);
            stats.bytes_received += count;
            console_irq_restore(flags);
            if (visible) console_request_present();
            /* Live UART writers may interleave; saved records remain whole. */
            if (mirror) for (size_t i = 0; i < count; i++) serial_putc(s[offset+i]);
            offset += count;
        }
        s += record;
        length -= record;
    }
}

void console_putc(char c) { console_write_n(&c, 1); }

void console_write(const char *s) {
    size_t length = 0;
    while (s[length] != '\0') length++;
    console_write_n(s, length);
}

void console_write_hex64(uint64_t value) {
    static const char digits[] = "0123456789abcdef";
    char text[18] = {'0', 'x'};
    for (unsigned i = 0; i < 16; i++) text[i + 2] = digits[(value >> ((15 - i) * 4)) & 15];
    console_write_n(text, sizeof(text));
}

static size_t decimal_text(uint64_t value, char text[20]) {
    size_t count = 0;
    do { text[count++] = (char)('0' + value % 10); value /= 10; } while (value);
    for (size_t i = 0; i < count / 2; i++) {
        char c = text[i]; text[i] = text[count - 1 - i]; text[count - 1 - i] = c;
    }
    return count;
}

void console_write_dec(uint64_t value) {
    char text[20];
    size_t count = decimal_text(value, text);
    console_write_n(text, count);
}

struct format_buffer { char data[LOG_RECORD_BATCH]; size_t used; };
static void format_emit(struct format_buffer *buffer, char c) {
    buffer->data[buffer->used++] = c;
    if (buffer->used == sizeof(buffer->data)) {
        console_write_n(buffer->data, buffer->used);
        buffer->used = 0;
    }
}

void console_printf(const char *fmt, ...) {
    struct format_buffer buffer = { .used = 0 };
    va_list args;
    va_start(args, fmt);
    for (const char *p = fmt; *p; p++) {
        if (*p != '%') { format_emit(&buffer, *p); continue; }
        p++;
        if (*p == '\0') break;
        switch (*p) {
        case '%': format_emit(&buffer, '%'); break;
        case 'c': format_emit(&buffer, (char)va_arg(args, int)); break;
        case 's': {
            const char *text = va_arg(args, const char *);
            if (text == NULL) text = "(null)";
            while (*text) format_emit(&buffer, *text++);
            break;
        }
        case 'u': {
            char text[20];
            size_t count = decimal_text(va_arg(args, uint64_t), text);
            for (size_t i = 0; i < count; i++) format_emit(&buffer, text[i]);
            break;
        }
        case 'd': {
            int64_t value = va_arg(args, int64_t);
            uint64_t magnitude = (uint64_t)value;
            if (value < 0) {
                format_emit(&buffer, '-');
                magnitude = 0 - magnitude; /* Includes INT64_MIN without signed overflow. */
            }
            char text[20];
            size_t count = decimal_text(magnitude, text);
            for (size_t i = 0; i < count; i++) format_emit(&buffer, text[i]);
            break;
        }
        case 'x': {
            uint64_t value = va_arg(args, uint64_t);
            const char *digits = "0123456789abcdef";
            format_emit(&buffer, '0'); format_emit(&buffer, 'x');
            for (int shift = 60; shift >= 0; shift -= 4) format_emit(&buffer, digits[(value >> shift) & 15]);
            break;
        }
        default: format_emit(&buffer, '%'); format_emit(&buffer, *p); break;
        }
    }
    va_end(args);
    if (buffer.used) console_write_n(buffer.data, buffer.used);
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

bool console_display_info(struct console_display_info *info) {
    if (fb == NULL || info == NULL) {
        return false;
    }

    *info = (struct console_display_info) {
        .width = fb->width,
        .height = fb->height,
        .pitch = fb->pitch,
        .bpp = fb->bpp,
        .memory_model = fb->memory_model,
        .red_mask_size = fb->red_mask_size,
        .red_mask_shift = fb->red_mask_shift,
        .green_mask_size = fb->green_mask_size,
        .green_mask_shift = fb->green_mask_shift,
        .blue_mask_size = fb->blue_mask_size,
        .blue_mask_shift = fb->blue_mask_shift,
    };
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

bool console_blit_rgb(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
    const uint32_t *rgb_pixels, uint64_t stride) {
    if (fb == NULL || rgb_pixels == NULL || width == 0 || height == 0 ||
        stride < width || x >= fb->width || y >= fb->height) {
        return false;
    }

    uint64_t copy_width = width;
    uint64_t copy_height = height;
    if (copy_width > fb->width - x) {
        copy_width = fb->width - x;
    }
    if (copy_height > fb->height - y) {
        copy_height = fb->height - y;
    }

    volatile uint32_t *pixels = fb->address;
    uint64_t fb_stride = fb->pitch / 4;
    bool direct_rgb = fb->bpp == 32 &&
        fb->red_mask_size == 8 && fb->red_mask_shift == 16 &&
        fb->green_mask_size == 8 && fb->green_mask_shift == 8 &&
        fb->blue_mask_size == 8 && fb->blue_mask_shift == 0;

    for (uint64_t py = 0; py < copy_height; py++) {
        volatile uint32_t *destination = pixels + (y + py) * fb_stride + x;
        const uint32_t *source = rgb_pixels + py * stride;
        for (uint64_t px = 0; px < copy_width; px++) {
            destination[px] = direct_rgb ? source[px] : rgb_color(source[px]);
        }
    }
    return true;
}

/* Expand directly into the write-only framebuffer. Keep application surfaces
 * logical-sized; neither a physical backbuffer nor framebuffer reads are needed. */
bool console_blit_rgb_scaled(uint64_t x, uint64_t y, uint64_t width, uint64_t height,
    const uint32_t *rgb_pixels, uint64_t stride, uint64_t scale) {
    if ((scale != 1 && scale != 2) || fb == NULL || rgb_pixels == NULL ||
        !width || !height || stride < width || x >= fb->width || y >= fb->height ||
        width > UINT64_MAX / scale || height > UINT64_MAX / scale ||
        width > UINT64_MAX / sizeof(uint32_t) ||
        height - 1 > (UINT64_MAX / sizeof(uint32_t) - width) / stride) return false;
    if (scale == 1) return console_blit_rgb(x,y,width,height,rgb_pixels,stride);
    uint64_t copy_width = width * scale, copy_height = height * scale;
    if (copy_width > fb->width - x) copy_width = fb->width - x;
    if (copy_height > fb->height - y) copy_height = fb->height - y;
    volatile uint32_t *pixels = fb->address;
    uint64_t fb_stride = fb->pitch / 4;
    bool direct_rgb = fb->bpp == 32 &&
        fb->red_mask_size == 8 && fb->red_mask_shift == 16 &&
        fb->green_mask_size == 8 && fb->green_mask_shift == 8 &&
        fb->blue_mask_size == 8 && fb->blue_mask_shift == 0;
    for (uint64_t py=0; py<copy_height; py++) {
        volatile uint32_t *destination = pixels + (y+py)*fb_stride+x;
        const uint32_t *source = rgb_pixels + (py/2)*stride;
        for (uint64_t px=0; px<copy_width; px++)
            destination[px] = direct_rgb ? source[px/2] : rgb_color(source[px/2]);
    }
    return true;
}
