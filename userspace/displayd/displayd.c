#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>
#include <srvros/ui.h>

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CURSOR_W 16
#define CURSOR_H 16

struct display_metrics {
    uint64_t width;
    uint64_t height;
    uint64_t top_h;
    uint64_t dock_w;
    uint64_t margin;
    uint64_t gap;
    uint64_t button_h;
    uint64_t status_h;
};

struct display_state {
    struct display_metrics metrics;
    uint64_t gui_messages;
    uint64_t mouse_events;
};

static uint64_t max_u64(uint64_t a, uint64_t b) {
    return a > b ? a : b;
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

static uint64_t clamp_u64(uint64_t value, uint64_t low, uint64_t high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static uint64_t clamp_add(uint64_t value, int32_t delta, uint64_t min, uint64_t max) {
    int64_t next = (int64_t)value + delta;
    if (next < (int64_t)min) {
        return min;
    }
    if (next > (int64_t)max) {
        return max;
    }
    return (uint64_t)next;
}

static int streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0') {
        if (*a != *b) {
            return 0;
        }
        a++;
        b++;
    }
    return *a == *b;
}

static void print_u64(uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        srv_puts("0");
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        char text[2] = { digits[--count], '\0' };
        srv_puts(text);
    }
}

static void append_char(char *out, uint64_t capacity, uint64_t *length, char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
}

static void append_text(char *out, uint64_t capacity, uint64_t *length, const char *text) {
    while (text != 0 && *text != '\0') {
        append_char(out, capacity, length, *text++);
    }
}

static void append_u64(char *out, uint64_t capacity, uint64_t *length, uint64_t value) {
    char digits[21];
    uint64_t count = 0;
    if (value == 0) {
        append_char(out, capacity, length, '0');
        return;
    }
    while (value > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (value % 10));
        value /= 10;
    }
    while (count > 0) {
        append_char(out, capacity, length, digits[--count]);
    }
}

static struct display_metrics make_metrics(uint64_t width, uint64_t height) {
    uint64_t min_axis = min_u64(width, height);
    uint64_t unit = clamp_u64(min_axis / 64, 8, 18);
    struct display_metrics metrics;
    metrics.width = width;
    metrics.height = height;
    metrics.top_h = clamp_u64(height / 24, 26, 48);
    metrics.dock_w = clamp_u64(width / 8, 112, 192);
    metrics.margin = unit;
    metrics.gap = max_u64(6, unit / 2);
    metrics.button_h = clamp_u64(height / 28, 28, 44);
    metrics.status_h = clamp_u64(height / 18, 42, 72);
    return metrics;
}

static void draw_panel(struct ui_element *root, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t fill, uint32_t border) {
    if (width == 0 || height == 0) {
        return;
    }
    ui_draw_rect(root, x, y, width, height, fill);
    ui_draw_rect(root, x, y, width, 1, border);
    ui_draw_rect(root, x, y + (int64_t)height - 1, width, 1, border);
    ui_draw_rect(root, x, y, 1, height, border);
    ui_draw_rect(root, x + (int64_t)width - 1, y, 1, height, border);
}

static void draw_dock_button(struct ui_element *root, const struct display_metrics *m,
    uint64_t index, const char *label, uint32_t color) {
    int64_t x = (int64_t)m->margin;
    int64_t y = (int64_t)(m->top_h + m->margin + index * (m->button_h + m->gap));
    uint64_t width = m->dock_w - 2 * m->margin;
    draw_panel(root, x, y, width, m->button_h, color, 0x8fd0d4);
    ui_draw_text(root, x + 10, y + (int64_t)((m->button_h - 7) / 2), label, 0xf2f7ff);
}

static void draw_scene(struct ui_element *root) {
    struct display_state *state = (struct display_state *)root->userdata;
    const struct display_metrics *m = &state->metrics;
    uint64_t work_x = m->dock_w + m->margin;
    uint64_t work_y = m->top_h + m->margin;
    uint64_t work_w = m->width > work_x + m->margin ? m->width - work_x - m->margin : 1;
    uint64_t work_h = m->height > work_y + m->status_h + m->margin ?
        m->height - work_y - m->status_h - m->margin : 1;
    uint64_t card_w = work_w > 3 * m->gap ? (work_w - 2 * m->gap) / 3 : work_w;
    uint64_t card_h = clamp_u64(work_h / 3, 84, 180);
    char text[48];

    ui_clear(root, 0x0b1118);
    ui_draw_rect(root, 0, 0, m->width, m->top_h, 0x233640);
    ui_draw_text(root, 16, (int64_t)((m->top_h - 7) / 2), "DISPLAYD", 0xffffff);
    ui_draw_text(root, (int64_t)(m->width > 220 ? m->width - 220 : 16),
        (int64_t)((m->top_h - 7) / 2), "Q OR ESC EXITS", 0xb9d8df);

    ui_draw_rect(root, 0, m->top_h, m->dock_w, m->height - m->top_h, 0x101a22);
    draw_dock_button(root, m, 0, "APPS", 0x2f6f68);
    draw_dock_button(root, m, 1, "FILES", 0x335b7a);
    draw_dock_button(root, m, 2, "NET", 0x60548d);
    draw_dock_button(root, m, 3, "TERMINAL", 0x70485f);

    draw_panel(root, (int64_t)work_x, (int64_t)work_y, card_w, card_h,
        0x15232d, 0x486476);
    ui_draw_text(root, (int64_t)work_x + 14, (int64_t)work_y + 16,
        "COMPOSITOR READY", 0xffffff);
    ui_draw_text(root, (int64_t)work_x + 14, (int64_t)work_y + 34,
        "ROOT BACKBUFFER", 0xb9d8df);

    draw_panel(root, (int64_t)(work_x + card_w + m->gap), (int64_t)work_y,
        card_w, card_h, 0x142620, 0x50786c);
    ui_draw_text(root, (int64_t)(work_x + card_w + m->gap + 14),
        (int64_t)work_y + 16, "DISPLAY SCALE", 0xffffff);
    text[0] = '\0';
    uint64_t text_len = 0;
    append_text(text, sizeof(text), &text_len, "SIZE ");
    append_u64(text, sizeof(text), &text_len, m->width);
    append_char(text, sizeof(text), &text_len, 'X');
    append_u64(text, sizeof(text), &text_len, m->height);
    ui_draw_text(root, (int64_t)(work_x + card_w + m->gap + 14),
        (int64_t)work_y + 34, text, 0xb9d8df);

    draw_panel(root, (int64_t)(work_x + 2 * (card_w + m->gap)), (int64_t)work_y,
        card_w, card_h, 0x241c28, 0x7f668f);
    ui_draw_text(root, (int64_t)(work_x + 2 * (card_w + m->gap) + 14),
        (int64_t)work_y + 16, "INPUT PIPE", 0xffffff);
    ui_draw_text(root, (int64_t)(work_x + 2 * (card_w + m->gap) + 14),
        (int64_t)work_y + 34, "MOUSE AND KEYS", 0xb9d8df);

    draw_panel(root, (int64_t)work_x, (int64_t)(m->height - m->status_h - m->margin),
        work_w, m->status_h, 0x111b24, 0x3f5d6b);
    ui_draw_text(root, (int64_t)work_x + 14,
        (int64_t)(m->height - m->status_h - m->margin + 14),
        "STATUS", 0xffffff);
    ui_draw_text(root, (int64_t)work_x + 14,
        (int64_t)(m->height - m->status_h - m->margin + 32),
        "GUI IPC SERVER ONLINE", 0xb9d8df);
}

static void draw_cursor(uint64_t x, uint64_t y, uint64_t screen_width,
    uint64_t screen_height, uint32_t color) {
    if (x >= screen_width || y >= screen_height) {
        return;
    }
    uint64_t w = min_u64(CURSOR_W, screen_width - x);
    uint64_t h = min_u64(CURSOR_H, screen_height - y);
    if (w == 0 || h == 0) {
        return;
    }
    fillrect(x, y, w, 2, color);
    fillrect(x, y, 2, h, color);
    if (w > 5 && h > 5) {
        fillrect(x + 4, y + 4, 3, 3, color);
    }
}

static void present_dirty(struct ui_element *root, uint64_t mouse_x, uint64_t mouse_y,
    uint64_t old_mouse_x, uint64_t old_mouse_y, int cursor_dirty, uint32_t cursor_color) {
    int64_t dirty_x = 0;
    int64_t dirty_y = 0;
    uint64_t dirty_width = 0;
    uint64_t dirty_height = 0;
    int has_dirty = ui_dirty_rect(root, &dirty_x, &dirty_y, &dirty_width, &dirty_height);

    if (has_dirty) {
        ui_render_tree(root);
        ui_present_rect(root, dirty_x, dirty_y, dirty_width, dirty_height);
    }
    if (cursor_dirty) {
        ui_present_rect(root, (int64_t)old_mouse_x, (int64_t)old_mouse_y, CURSOR_W, CURSOR_H);
        ui_present_rect(root, (int64_t)mouse_x, (int64_t)mouse_y, CURSOR_W, CURSOR_H);
    }
    if (has_dirty || cursor_dirty) {
        draw_cursor(mouse_x, mouse_y, root->width, root->height, cursor_color);
    }
}

static void handle_gui_messages(struct display_state *state) {
    struct gui_message msg;
    while (gui_recv(&msg) > 0) {
        state->gui_messages++;
        if (msg.type == GUI_MSG_CREATE_WINDOW) {
            srv_puts("displayd: client window ");
            srv_puts(msg.text);
            srv_puts(" pid=");
            print_u64(msg.source_pid);
            srv_puts("\n");
        }
    }
}

int main(int argc, char **argv) {
    struct gfx_info gfx = {0};
    struct mouse_event mouse = {0};
    struct ui_surface root_surface;
    struct ui_element root;
    struct display_state state;
    uint32_t *pixels;
    uint64_t pixel_count;
    uint64_t mouse_x = 96;
    uint64_t mouse_y = 96;
    uint64_t old_mouse_x = 96;
    uint64_t old_mouse_y = 96;
    uint8_t buttons = 0;
    uint64_t start_ticks;
    int smoke = 0;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--smoke")) {
            smoke = 1;
        }
    }

    clrscr();
    srv_puts("displayd: start\n");
    if (gfx_info(&gfx) != 0 || gfx.width == 0 || gfx.height == 0) {
        gfx.width = 640;
        gfx.height = 480;
    }
    if (gfx.width > UINT64_MAX / gfx.height || gfx.width * gfx.height > SIZE_MAX / sizeof(uint32_t)) {
        srv_puts("displayd: framebuffer too large\n");
        return 1;
    }
    pixel_count = gfx.width * gfx.height;
    pixels = (uint32_t *)malloc((size_t)(pixel_count * sizeof(uint32_t)));
    if (pixels == 0) {
        srv_puts("displayd: root backbuffer alloc failed\n");
        return 1;
    }

    memset(&state, 0, sizeof(state));
    state.metrics = make_metrics(gfx.width, gfx.height);
    srv_puts("displayd: framebuffer ");
    print_u64(gfx.width);
    srv_puts("x");
    print_u64(gfx.height);
    srv_puts("\n");

    if (gui_register_server() != 0) {
        srv_puts("displayd: gui server registration failed\n");
    }

    ui_surface_init(&root_surface, gfx.width, gfx.height, gfx.width, pixels);
    ui_element_init(&root, 0, 0, gfx.width, gfx.height, root_surface);
    root.background = 0x0b1118;
    root.draw = draw_scene;
    root.userdata = &state;

    ui_render_tree(&root);
    ui_present(&root);
    draw_cursor(mouse_x, mouse_y, gfx.width, gfx.height, 0xffffff);
    srv_puts("displayd: root backbuffer ready\n");

    start_ticks = (uint64_t)srv_ticks();
    for (;;) {
        int key = kbhit();
        int cursor_dirty = 0;
        old_mouse_x = mouse_x;
        old_mouse_y = mouse_y;

        handle_gui_messages(&state);
        if (key == 'q' || key == 'Q' || key == 27) {
            break;
        }

        if (mouse_scan(&mouse) > 0) {
            if (gfx.width > CURSOR_W) {
                mouse_x = clamp_add(mouse_x, mouse.dx, 0, gfx.width - CURSOR_W);
            }
            if (gfx.height > CURSOR_H) {
                mouse_y = clamp_add(mouse_y, mouse.dy, 0, gfx.height - CURSOR_H);
            }
            buttons = mouse.buttons;
            cursor_dirty = mouse_x != old_mouse_x || mouse_y != old_mouse_y;
            state.mouse_events++;
        }

        present_dirty(&root, mouse_x, mouse_y, old_mouse_x, old_mouse_y,
            cursor_dirty, buttons != 0 ? 0xf87171 : 0xffffff);

        if (smoke && (uint64_t)srv_ticks() - start_ticks > 20) {
            srv_puts("displayd: smoke ok\n");
            break;
        }
        srv_yield();
    }

    free(pixels);
    srv_puts("displayd: exited\n");
    return 0;
}
