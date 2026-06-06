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
#define BUTTON_COUNT 16
#define SWATCH_COUNT 5

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
    { "Z-", 0, 10 },
    { "Z+", 0, 11 },
    { "FIT", 0, 12 },
    { "<", 0, 13 },
    { "^", 0, 14 },
    { "v", 0, 15 },
    { ">", 0, 16 },
    { "OPEN", 0, 3 },
    { "CLEAR", 0, 1 },
    { "SAVE", 0, 2 },
    { "AS", 0, 4 },
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

static int text_equal(const char *a, const char *b) {
    uint64_t i = 0;
    if (a == 0 || b == 0) {
        return a == b;
    }
    while (a[i] != '\0' && b[i] != '\0') {
        if (a[i] != b[i]) {
            return 0;
        }
        i++;
    }
    return a[i] == b[i];
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
    if (capacity == 0 || *length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
}

static void append_text(char *out, uint64_t capacity, uint64_t *length,
    const char *text) {
    for (const char *p = text; p != 0 && *p != '\0'; p++) {
        append_char(out, capacity, length, *p);
    }
}

static void append_u64(char *out, uint64_t capacity, uint64_t *length,
    uint64_t value) {
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

static void fill_canvas_local(uint32_t color) {
    for (uint64_t i = 0; i < CANVAS_W * CANVAS_H; i++) {
        pixels[i] = color;
    }
}

static void set_status(char *status, uint64_t capacity, const char *text) {
    copy_text(status, capacity, text);
}

static void parent_dir(char *out, uint64_t capacity, const char *path) {
    uint64_t end = 0;
    uint64_t slash = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    if (path == 0 || path[0] == '\0') {
        copy_text(out, capacity, "/");
        return;
    }
    while (path[end] != '\0') {
        end++;
    }
    while (end > 1 && path[end - 1] == '/') {
        end--;
    }
    for (uint64_t i = 0; i < end; i++) {
        if (path[i] == '/') {
            slash = i;
        }
    }
    if (slash == 0) {
        copy_text(out, capacity, "/");
        return;
    }
    for (uint64_t i = 0; i < slash && i + 1 < capacity; i++) {
        out[i] = path[i];
        out[i + 1] = '\0';
    }
}

static int save_canvas(const char *path, char *status, uint64_t status_capacity) {
    size_t size = 0;
    if (bmp_encode_rgba32(pixels, CANVAS_W, CANVAS_H,
            bmp_buffer, sizeof(bmp_buffer), &size) < 0) {
        set_status(status, status_capacity, "BMP ENCODE FAILED");
        srv_puts("paint: bmp encode failed\n");
        return -1;
    }
    if (srv_fs_write(path, bmp_buffer, size) < 0) {
        set_status(status, status_capacity, "SAVE FAILED");
        srv_puts("paint: save failed\n");
        return -1;
    }
    set_status(status, status_capacity, "SAVED");
    srv_puts("paint: save ");
    srv_puts(path);
    srv_puts("\n");
    return 0;
}

static int load_canvas(const char *path, char *status, uint64_t status_capacity) {
    long fd;
    long read_bytes;
    struct srv_stat info;
    uint64_t width = 0;
    uint64_t height = 0;
    if (srv_stat(path, &info) != 0 || info.size > sizeof(bmp_buffer)) {
        set_status(status, status_capacity, "OPEN FAILED");
        srv_puts("paint: open failed\n");
        return -1;
    }
    fd = srv_open(path);
    if (fd < 0) {
        set_status(status, status_capacity, "OPEN FAILED");
        srv_puts("paint: open failed\n");
        return -1;
    }
    read_bytes = srv_read((int)fd, bmp_buffer, info.size);
    srv_close((int)fd);
    if (read_bytes < 0 || (uint64_t)read_bytes != info.size ||
        bmp_decode_rgba(bmp_buffer, (size_t)read_bytes, pixels,
            CANVAS_W * CANVAS_H, &width, &height) != 0 ||
        width != CANVAS_W || height != CANVAS_H) {
        set_status(status, status_capacity, "BMP MUST BE 160X120");
        srv_puts("paint: bmp open failed\n");
        return -1;
    }
    set_status(status, status_capacity, "OPENED");
    srv_puts("paint: open ");
    srv_puts(path);
    srv_puts("\n");
    return 0;
}

static uint64_t min_u64(uint64_t a, uint64_t b) {
    return a < b ? a : b;
}

static void clamp_view(uint64_t zoom, uint64_t *view_x, uint64_t *view_y,
    uint64_t *view_w, uint64_t *view_h) {
    if (zoom == 0) {
        zoom = 1;
    }
    *view_w = CANVAS_W / zoom;
    *view_h = CANVAS_H / zoom;
    if (*view_w == 0) {
        *view_w = 1;
    }
    if (*view_h == 0) {
        *view_h = 1;
    }
    if (*view_x + *view_w > CANVAS_W) {
        *view_x = CANVAS_W > *view_w ? CANVAS_W - *view_w : 0;
    }
    if (*view_y + *view_h > CANVAS_H) {
        *view_y = CANVAS_H > *view_h ? CANVAS_H - *view_h : 0;
    }
}

static void pan_view(uint64_t zoom, uint64_t *view_x, uint64_t *view_y,
    int dx, int dy) {
    uint64_t view_w;
    uint64_t view_h;
    uint64_t step_x;
    uint64_t step_y;
    clamp_view(zoom, view_x, view_y, &view_w, &view_h);
    step_x = view_w > 4 ? view_w / 4 : 1;
    step_y = view_h > 4 ? view_h / 4 : 1;
    if (dx < 0) {
        *view_x = *view_x > step_x ? *view_x - step_x : 0;
    } else if (dx > 0) {
        *view_x += step_x;
    }
    if (dy < 0) {
        *view_y = *view_y > step_y ? *view_y - step_y : 0;
    } else if (dy > 0) {
        *view_y += step_y;
    }
    clamp_view(zoom, view_x, view_y, &view_w, &view_h);
}

static void set_zoom(uint64_t *zoom, uint64_t *view_x, uint64_t *view_y,
    uint64_t next) {
    uint64_t old_view_w;
    uint64_t old_view_h;
    uint64_t old_center_x;
    uint64_t old_center_y;
    uint64_t view_w;
    uint64_t view_h;
    if (next < 1) {
        next = 1;
    }
    if (next > 8) {
        next = 8;
    }
    clamp_view(*zoom, view_x, view_y, &old_view_w, &old_view_h);
    old_center_x = *view_x + old_view_w / 2;
    old_center_y = *view_y + old_view_h / 2;
    *zoom = next;
    clamp_view(*zoom, view_x, view_y, &view_w, &view_h);
    *view_x = old_center_x > view_w / 2 ? old_center_x - view_w / 2 : 0;
    *view_y = old_center_y > view_h / 2 ? old_center_y - view_h / 2 : 0;
    clamp_view(*zoom, view_x, view_y, &view_w, &view_h);
}

static void format_canvas_info(char *out, uint64_t capacity, uint64_t zoom,
    uint64_t view_x, uint64_t view_y) {
    uint64_t length = 0;
    uint64_t view_w;
    uint64_t view_h;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    clamp_view(zoom, &view_x, &view_y, &view_w, &view_h);
    append_text(out, capacity, &length, "IMG ");
    append_u64(out, capacity, &length, CANVAS_W);
    append_char(out, capacity, &length, 'x');
    append_u64(out, capacity, &length, CANVAS_H);
    append_text(out, capacity, &length, "  Z");
    append_u64(out, capacity, &length, zoom);
    append_text(out, capacity, &length, "  VIEW ");
    append_u64(out, capacity, &length, view_w);
    append_char(out, capacity, &length, 'x');
    append_u64(out, capacity, &length, view_h);
    append_text(out, capacity, &length, " @");
    append_u64(out, capacity, &length, view_x);
    append_char(out, capacity, &length, ',');
    append_u64(out, capacity, &length, view_y);
}

static void layout_buttons(struct gui2_window *window, struct gui2_button *buttons) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t y1 = window->height >
        pad + theme->status_h + 2 * theme->control_h + gap ?
        window->height - pad - theme->status_h - 2 * theme->control_h - gap :
        pad;
    uint64_t y2 = y1 + theme->control_h + gap;
    uint64_t usable_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t row_count = 8;
    uint64_t row_w = usable_w;
    if (usable_w > (row_count - 1) * gap) {
        row_w = (usable_w - (row_count - 1) * gap) / row_count;
    }
    if (row_w < 36) {
        row_w = 36;
    }
    for (uint64_t i = 0; i < row_count; i++) {
        buttons[i].x = (int64_t)(pad + i * (row_w + gap));
        buttons[i].y = (int64_t)y1;
        buttons[i].width = row_w;
        buttons[i].height = theme->control_h;
    }
    for (uint64_t i = row_count; i < BUTTON_COUNT; i++) {
        uint64_t index = i - row_count;
        buttons[i].x = (int64_t)(pad + index * (row_w + gap));
        buttons[i].y = (int64_t)y2;
        buttons[i].width = row_w;
        buttons[i].height = theme->control_h;
    }
}

static void layout_canvas(const struct gui2_window *window, struct gui2_canvas *canvas,
    uint64_t zoom, uint64_t *view_x, uint64_t *view_y) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t view_w;
    uint64_t view_h;
    uint64_t button_y = window->height >
        pad + theme->status_h + 2 * theme->control_h + theme->gap ?
        window->height - pad - theme->status_h - 2 * theme->control_h -
            theme->gap : pad + 260;
    uint64_t top = theme->toolbar_h + pad;
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
    clamp_view(zoom, view_x, view_y, &view_w, &view_h);
    gui2_canvas_set_view(canvas, *view_x, *view_y, view_w, view_h);
}

static void draw_paint(struct gui2_window *window,
    struct gui2_button *buttons,
    struct gui2_canvas *canvas,
    struct gui2_file_dialog *open_dialog,
    struct gui2_file_dialog *save_dialog,
    const char *status,
    const char *path,
    uint32_t selected,
    uint64_t zoom,
    uint64_t *view_x,
    uint64_t *view_y) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    char info[80];
    layout_buttons(window, buttons);
    layout_canvas(window, canvas, zoom, view_x, view_y);
    format_canvas_info(info, sizeof(info), zoom, *view_x, *view_y);
    gui2_clear(window, theme->canvas);
    gui2_app_header(window, "PAINT", path);
    gui2_rect(window, (int64_t)(window->width > 48 ? window->width - 36 : pad),
        12, 20, 14, selected);
    gui2_rect(window, (int64_t)(window->width > 48 ? window->width - 36 : pad),
        12, 20, 1, theme->border);
    gui2_canvas_draw(window, canvas);
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        gui2_button_draw(window, &buttons[i]);
    }
    gui2_status_bar(window,
        status != 0 && status[0] != '\0' ? status : "READY", info);
    gui2_file_dialog_draw(window, open_dialog);
    gui2_file_dialog_draw(window, save_dialog);
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

static int run_selftest(void) {
    const char *path = "/fat/home/paint-selftest.bmp";
    char status[48];
    char info[80];
    struct gui2_canvas canvas;
    struct gui2_event event;
    uint64_t x = 0;
    uint64_t y = 0;
    uint64_t zoom = 1;
    uint64_t view_x = 0;
    uint64_t view_y = 0;
    uint64_t view_w = 0;
    uint64_t view_h = 0;
    (void)srv_mkdir("/fat/home");
    (void)srv_unlink(path);
    fill_canvas_local(0xffffff);
    pixels[0] = 0xd9534f;
    pixels[CANVAS_W - 1] = 0x2ea043;
    pixels[(CANVAS_H - 1) * CANVAS_W] = 0x1f6feb;
    pixels[CANVAS_W * CANVAS_H - 1] = 0x000000;
    if (save_canvas(path, status, sizeof(status)) != 0) {
        srv_puts("paint-selftest: save failed\n");
        return 1;
    }
    fill_canvas_local(0xffffff);
    if (load_canvas(path, status, sizeof(status)) != 0) {
        srv_puts("paint-selftest: load failed\n");
        return 1;
    }
    if (pixels[0] != 0xd9534f ||
        pixels[CANVAS_W - 1] != 0x2ea043 ||
        pixels[(CANVAS_H - 1) * CANVAS_W] != 0x1f6feb ||
        pixels[CANVAS_W * CANVAS_H - 1] != 0x000000) {
        srv_puts("paint-selftest: pixel mismatch\n");
        return 1;
    }
    gui2_canvas_init(&canvas, 10, 20, 80, 60, pixels,
        CANVAS_W, CANVAS_H, 0xffffff);
    gui2_canvas_set_view(&canvas, 40, 30, 80, 60);
    event.type = GUI2_EVENT_POINTER_MOVE;
    event.x = 50;
    event.y = 50;
    event.buttons = 0;
    event.changed_buttons = 0;
    if (!gui2_canvas_event_pixel(&canvas, &event, &x, &y) ||
        x != 80 || y != 60) {
        srv_puts("paint-selftest: viewport failed\n");
        return 1;
    }
    clamp_view(zoom, &view_x, &view_y, &view_w, &view_h);
    if (view_w != 160 || view_h != 120 || view_x != 0 || view_y != 0) {
        srv_puts("paint-selftest: fit view failed\n");
        return 1;
    }
    set_zoom(&zoom, &view_x, &view_y, 2);
    if (zoom != 2 || view_x != 40 || view_y != 30) {
        srv_puts("paint-selftest: zoom failed\n");
        return 1;
    }
    pan_view(zoom, &view_x, &view_y, 1, 1);
    if (view_x != 60 || view_y != 45) {
        srv_puts("paint-selftest: pan failed\n");
        return 1;
    }
    set_zoom(&zoom, &view_x, &view_y, 8);
    if (zoom != 8 || view_x != 90 || view_y != 68) {
        srv_puts("paint-selftest: zoom center failed\n");
        return 1;
    }
    format_canvas_info(info, sizeof(info), zoom, view_x, view_y);
    if (!text_equal(info, "IMG 160x120  Z8  VIEW 20x15 @90,68")) {
        srv_puts("paint-selftest: canvas info failed\n");
        return 1;
    }
    set_zoom(&zoom, &view_x, &view_y, 1);
    if (zoom != 1 || view_x != 0 || view_y != 0) {
        srv_puts("paint-selftest: fit zoom failed\n");
        return 1;
    }
    if (srv_unlink(path) != 0) {
        srv_puts("paint-selftest: cleanup failed\n");
        return 1;
    }
    srv_puts("paint-selftest: ok\n");
    return 0;
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_button buttons[BUTTON_COUNT];
    struct gui2_canvas canvas;
    struct gui2_file_dialog open_dialog;
    struct gui2_file_dialog save_dialog;
    char status[48];
    char path[CLI_PATH_MAX];
    char path_dir[CLI_PATH_MAX];
    char path_base[CLI_PATH_MAX];
    uint32_t selected = 0x000000;
    uint64_t zoom = 1;
    uint64_t view_x = 0;
    uint64_t view_y = 0;

    if (argc > 1 && cli_streq(argv[1], "--selftest")) {
        return run_selftest();
    }

    srv_puts("paint: start\n");
    cli_normalize_path(path, sizeof(path), "/", argc > 1 ? argv[1] : "/fat/paint.bmp");
    fill_canvas_local(0xffffff);
    if (argc > 1) {
        if (load_canvas(path, status, sizeof(status)) != 0) {
            set_status(status, sizeof(status), "BMP READY");
        }
    } else {
        set_status(status, sizeof(status), "BMP READY");
    }
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
    gui2_file_dialog_init(&open_dialog, "OPEN BMP", "OPEN", 0);
    gui2_file_dialog_init(&save_dialog, "SAVE BMP AS", "SAVE", 1);

    draw_paint(&window, buttons, &canvas, &open_dialog, &save_dialog,
        status, path, selected, zoom, &view_x, &view_y);
    gui2_window_present_dirty(&window);

    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[BUTTON_COUNT + 1];
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
            } else if (open_dialog.active || save_dialog.active) {
                struct gui2_file_dialog *dialog = open_dialog.active ?
                    &open_dialog : &save_dialog;
                changed |= gui2_file_dialog_event(dialog, &event);
                if (dialog->cancel_clicks != 0) {
                    dialog->cancel_clicks = 0;
                    gui2_file_dialog_close(dialog);
                    set_status(status, sizeof(status), "CANCELED");
                    changed = 1;
                }
                if (dialog->primary_clicks != 0) {
                    dialog->primary_clicks = 0;
                    if (dialog == &open_dialog) {
                        if (load_canvas(dialog->result_path, status,
                                sizeof(status)) == 0) {
                            copy_text(path, sizeof(path), dialog->result_path);
                        }
                    } else {
                        copy_text(path, sizeof(path), dialog->result_path);
                        save_canvas(path, status, sizeof(status));
                    }
                    gui2_file_dialog_close(dialog);
                    changed = 1;
                }
                continue;
            } else if ((event.type == GUI2_EVENT_POINTER_BUTTON ||
                    event.type == GUI2_EVENT_POINTER_MOVE) &&
                (event.buttons & 1) != 0) {
                layout_canvas(&window, &canvas, zoom, &view_x, &view_y);
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
                } else if (button_defs[i].action == 3) {
                    parent_dir(path_dir, sizeof(path_dir), path);
                    gui2_file_dialog_open(&open_dialog, path_dir, "");
                    set_status(status, sizeof(status), "OPEN BMP");
                } else if (button_defs[i].action == 4) {
                    parent_dir(path_dir, sizeof(path_dir), path);
                    cli_basename(path_base, sizeof(path_base), path);
                    gui2_file_dialog_open(&save_dialog, path_dir, path_base);
                    set_status(status, sizeof(status), "SAVE AS");
                } else if (button_defs[i].action == 10) {
                    set_zoom(&zoom, &view_x, &view_y, zoom / 2);
                    set_status(status, sizeof(status), "ZOOM OUT");
                } else if (button_defs[i].action == 11) {
                    set_zoom(&zoom, &view_x, &view_y, zoom * 2);
                    set_status(status, sizeof(status), "ZOOM IN");
                } else if (button_defs[i].action == 12) {
                    set_zoom(&zoom, &view_x, &view_y, 1);
                    view_x = 0;
                    view_y = 0;
                    set_status(status, sizeof(status), "FIT");
                } else if (button_defs[i].action == 13) {
                    pan_view(zoom, &view_x, &view_y, -1, 0);
                    set_status(status, sizeof(status), "PAN LEFT");
                } else if (button_defs[i].action == 14) {
                    pan_view(zoom, &view_x, &view_y, 0, -1);
                    set_status(status, sizeof(status), "PAN UP");
                } else if (button_defs[i].action == 15) {
                    pan_view(zoom, &view_x, &view_y, 0, 1);
                    set_status(status, sizeof(status), "PAN DOWN");
                } else if (button_defs[i].action == 16) {
                    pan_view(zoom, &view_x, &view_y, 1, 0);
                    set_status(status, sizeof(status), "PAN RIGHT");
                }
                changed = 1;
            }
            if (event.type == GUI2_EVENT_KEY_DOWN) {
                if (event.key == '+' || event.key == '=') {
                    set_zoom(&zoom, &view_x, &view_y, zoom * 2);
                    set_status(status, sizeof(status), "ZOOM IN");
                    changed = 1;
                } else if (event.key == '-') {
                    set_zoom(&zoom, &view_x, &view_y, zoom / 2);
                    set_status(status, sizeof(status), "ZOOM OUT");
                    changed = 1;
                } else if (event.key == '0') {
                    set_zoom(&zoom, &view_x, &view_y, 1);
                    view_x = 0;
                    view_y = 0;
                    set_status(status, sizeof(status), "FIT");
                    changed = 1;
                } else if (event.key == 'a' || event.key == 'A') {
                    pan_view(zoom, &view_x, &view_y, -1, 0);
                    set_status(status, sizeof(status), "PAN LEFT");
                    changed = 1;
                } else if (event.key == 'd' || event.key == 'D') {
                    pan_view(zoom, &view_x, &view_y, 1, 0);
                    set_status(status, sizeof(status), "PAN RIGHT");
                    changed = 1;
                } else if (event.key == 'w' || event.key == 'W') {
                    pan_view(zoom, &view_x, &view_y, 0, -1);
                    set_status(status, sizeof(status), "PAN UP");
                    changed = 1;
                } else if (event.key == 's' || event.key == 'S') {
                    pan_view(zoom, &view_x, &view_y, 0, 1);
                    set_status(status, sizeof(status), "PAN DOWN");
                    changed = 1;
                }
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_paint(&window, buttons, &canvas, &open_dialog, &save_dialog,
                status, path, selected, zoom, &view_x, &view_y);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("paint: exited\n");
    return 0;
}
