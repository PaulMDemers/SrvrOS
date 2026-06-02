#include <srvros/bmp.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stddef.h>
#include <stdint.h>

#define WIN 1
#define WIDTH 460
#define HEIGHT 360
#define CANVAS_W 160
#define CANVAS_H 120
#define BRUSH 4
#define BUTTON_COUNT 7

struct paint_button_def {
    const char *label;
    uint32_t color;
    int action;
};

static const struct paint_button_def button_defs[BUTTON_COUNT] = {
    { "BLK", 0x000000, 0 },
    { "RED", 0xd9534f, 0 },
    { "GRN", 0x2ea043, 0 },
    { "BLU", 0x1f6feb, 0 },
    { "WHT", 0xffffff, 0 },
    { "CLEAR", 0, 1 },
    { "SAVE", 0, 2 },
};

static uint32_t pixels[CANVAS_W * CANVAS_H];
static uint8_t bmp_buffer[54 + CANVAS_W * CANVAS_H * 4];

static void copy_text(char *to, uint64_t capacity, const char *from) {
    uint64_t i = 0;
    if (to == 0 || capacity == 0) {
        return;
    }
    if (from != 0) {
        while (from[i] != '\0' && i + 1 < capacity) {
            to[i] = from[i];
            i++;
        }
    }
    to[i] = '\0';
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

static void fill_canvas_local(uint32_t color) {
    for (uint64_t i = 0; i < CANVAS_W * CANVAS_H; i++) {
        pixels[i] = color;
    }
}

static void set_status(char *status, uint64_t capacity, const char *text) {
    copy_text(status, capacity, text);
}

static void save_canvas(const char *path, char *status, uint64_t status_capacity) {
    size_t size = 0;
    if (bmp_encode_rgba32(pixels, CANVAS_W, CANVAS_H,
            bmp_buffer, sizeof(bmp_buffer), &size) < 0) {
        set_status(status, status_capacity, "BMP ENCODE FAILED");
        srv_puts("paint: bmp encode failed\n");
        return;
    }
    if (srv_fs_write(path, bmp_buffer, size) < 0) {
        set_status(status, status_capacity, "SAVE FAILED");
        srv_puts("paint: save failed\n");
        return;
    }
    set_status(status, status_capacity, "SAVED");
    srv_puts("paint: save\n");
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

static void layout_buttons(struct gui2_window *window, struct gui2_button *buttons) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t y = window->height > pad + theme->control_h ?
        window->height - pad - theme->control_h : pad;
    uint64_t usable_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t swatch_count = 5;
    uint64_t button_w = 56;
    uint64_t remaining_w = usable_w;
    if (remaining_w > (BUTTON_COUNT - 1) * gap) {
        remaining_w -= (BUTTON_COUNT - 1) * gap;
        button_w = remaining_w / BUTTON_COUNT;
    }
    if (button_w < 34) {
        button_w = 34;
    }
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        buttons[i].x = (int64_t)(pad + i * (button_w + gap));
        buttons[i].y = (int64_t)y;
        buttons[i].width = button_w;
        buttons[i].height = theme->control_h;
        if (i < swatch_count && button_w > 48) {
            buttons[i].width = 48;
        }
    }
}

static void layout_canvas(const struct gui2_window *window, struct gui2_canvas *canvas) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t button_y = window->height > pad + theme->control_h ?
        window->height - pad - theme->control_h : pad + 260;
    uint64_t top = pad + 44;
    uint64_t bottom_gap = theme->gap;
    uint64_t avail_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t avail_h = button_y > top + bottom_gap ? button_y - top - bottom_gap : 1;
    uint64_t scale_w = avail_w / CANVAS_W;
    uint64_t scale_h = avail_h / CANVAS_H;
    uint64_t scale = min_u64(scale_w, scale_h);
    if (scale == 0) {
        scale = 1;
    }
    canvas->width = CANVAS_W * scale;
    canvas->height = CANVAS_H * scale;
    if (canvas->width > avail_w) {
        canvas->width = avail_w;
    }
    if (canvas->height > avail_h) {
        canvas->height = avail_h;
    }
    canvas->x = (int64_t)(pad + (avail_w > canvas->width ? (avail_w - canvas->width) / 2 : 0));
    canvas->y = (int64_t)top;
    canvas->pixels = pixels;
    canvas->pixel_width = CANVAS_W;
    canvas->pixel_height = CANVAS_H;
    canvas->background = 0xffffff;
}

static void draw_paint(struct gui2_window *window,
    struct gui2_button *buttons,
    struct gui2_canvas *canvas,
    const char *status,
    const char *path,
    uint32_t selected) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    layout_buttons(window, buttons);
    layout_canvas(window, canvas);
    gui2_clear(window, theme->canvas);
    gui2_text(window, (int64_t)pad, (int64_t)pad, "PAINT", theme->text);
    gui2_text(window, (int64_t)(pad + 64), (int64_t)pad, path, theme->text_muted);
    gui2_text(window, (int64_t)pad, (int64_t)(pad + 18),
        status != 0 && status[0] != '\0' ? status : "READY", theme->text_muted);
    gui2_rect(window, (int64_t)(window->width > 48 ? window->width - 36 : pad),
        (int64_t)pad, 20, 14, selected);
    gui2_rect(window, (int64_t)(window->width > 48 ? window->width - 36 : pad),
        (int64_t)pad, 20, 1, theme->border);
    gui2_canvas_draw(window, canvas);
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        gui2_button_draw(window, &buttons[i]);
    }
}

static int paint_at_event(const struct gui2_canvas *canvas,
    const struct gui2_event *event,
    uint32_t color) {
    uint64_t cx = 0;
    uint64_t cy = 0;
    if (!gui2_canvas_event_pixel(canvas, event, &cx, &cy)) {
        return 0;
    }
    for (int64_t py = (int64_t)cy - (BRUSH / 2);
        py < (int64_t)cy + (int64_t)(BRUSH / 2);
        py++) {
        for (int64_t px = (int64_t)cx - (BRUSH / 2);
            px < (int64_t)cx + (int64_t)(BRUSH / 2);
            px++) {
            if (px >= 0 && py >= 0 && px < CANVAS_W && py < CANVAS_H) {
                pixels[(uint64_t)py * CANVAS_W + (uint64_t)px] = color;
            }
        }
    }
    return 1;
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_button buttons[BUTTON_COUNT];
    struct gui2_canvas canvas;
    char status[48];
    uint32_t selected = 0x000000;
    const char *path = argc > 1 ? argv[1] : "/fat/paint.bmp";
    uint64_t start;

    srv_puts("paint: start\n");
    fill_canvas_local(0xffffff);
    set_status(status, sizeof(status), "BMP READY");
    if (gui2_window_open(&window, WIN, "PAINT",
            260, 150, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("paint: window open failed\n");
        return 1;
    }
    gui2_context_init(&context);
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        gui2_button_init(&buttons[i], 0, 0, 1, 1, button_defs[i].label);
    }
    gui2_canvas_init(&canvas, 0, 0, 1, 1, pixels, CANVAS_W, CANVAS_H, 0xffffff);

    draw_paint(&window, buttons, &canvas, status, path, selected);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[BUTTON_COUNT + 1];
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 320) {
            break;
        }
        for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
            controls[i].kind = GUI2_CONTROL_BUTTON;
            controls[i].ptr = &buttons[i];
        }
        controls[BUTTON_COUNT].kind = GUI2_CONTROL_CANVAS;
        controls[BUTTON_COUNT].ptr = &canvas;
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("paint: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("paint: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("paint: close\n");
                closing = 1;
                break;
            } else if ((event.type == GUI2_EVENT_POINTER_BUTTON ||
                    event.type == GUI2_EVENT_POINTER_MOVE) &&
                (event.buttons & 1) != 0) {
                layout_canvas(&window, &canvas);
                if (paint_at_event(&canvas, &event, selected)) {
                    set_status(status, sizeof(status), "PAINTING");
                    changed = 1;
                }
            }

            changed |= gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
                if (buttons[i].clicks == 0) {
                    continue;
                }
                buttons[i].clicks = 0;
                if (button_defs[i].action == 0) {
                    selected = button_defs[i].color;
                    set_status(status, sizeof(status), button_defs[i].label);
                    srv_puts("paint: color ");
                    srv_puts(button_defs[i].label);
                    srv_puts("\n");
                } else if (button_defs[i].action == 1) {
                    fill_canvas_local(0xffffff);
                    set_status(status, sizeof(status), "CLEARED");
                    srv_puts("paint: clear\n");
                } else if (button_defs[i].action == 2) {
                    save_canvas(path, status, sizeof(status));
                }
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_paint(&window, buttons, &canvas, status, path, selected);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("paint: exited\n");
    return 0;
}
