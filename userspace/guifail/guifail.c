#include <srvros/gfx.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>
#include <stdlib.h>

#define WIN 1
#define WIDTH 340
#define HEIGHT 176

static int streq(const char *a, const char *b) {
    while (*a != '\0' && *b != '\0' && *a == *b) {
        a++;
        b++;
    }
    return *a == '\0' && *b == '\0';
}

static uint64_t parse_u64(const char *text, uint64_t fallback) {
    uint64_t value = 0;
    int any = 0;
    while (text != 0 && *text >= '0' && *text <= '9') {
        value = value * 10 + (uint64_t)(*text - '0');
        text++;
        any = 1;
    }
    return any ? value : fallback;
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

static void draw_app(struct gui2_window *window, const char *mode) {
    const struct gui2_theme *theme = gui2_theme_default();
    gui2_clear(window, theme->canvas);
    gui2_app_header(window, "GUI FAIL", mode);
    gui2_panel(window, 14, 54, window->width > 28 ? window->width - 28 : 1,
        window->height > 92 ? window->height - 92 : 1, theme->panel);
    gui2_text(window, 28, 72, "DIAGNOSTIC", theme->text_muted);
    gui2_text(window, 28, 92, "This app intentionally exits for displayd tests.", theme->text);
    gui2_text(window, 28, 114, "It should produce a desktop notice, not a crash.", theme->text_muted);
    gui2_status_bar(window, "SMOKE TARGET", 0);
}

int main(int argc, char **argv) {
    int exit_mode = 0;
    int fault_mode = 0;
    int console_probe = 0;
    uint64_t exit_code = 23;
    const char *mode = "IDLE";

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--exit")) {
            exit_mode = 1;
            mode = "EXIT";
        } else if (streq(argv[i], "--fault")) {
            fault_mode = 1;
            mode = "FAULT";
        } else if (streq(argv[i], "--console-muted")) {
            console_probe = 1;
        } else if (streq(argv[i], "--exit-code") && i + 1 < argc) {
            exit_code = parse_u64(argv[++i], exit_code);
        }
    }

    if (console_probe) {
        int muted = gfx_console_muted();
        srv_puts("guifail: console-muted=");
        print_u64(muted > 0 ? 1 : 0);
        srv_puts("\n");
        return muted > 0 ? 1 : 0;
    }

    struct gui2_window window;
    srv_puts("guifail: start\n");
    if (gui2_window_open(&window, WIN, "GUI FAIL",
            520, 300, WIDTH, HEIGHT, gui2_rgb(0x10, 0x18, 0x22)) != 0) {
        srv_puts("guifail: window open failed\n");
        return 1;
    }

    draw_app(&window, mode);
    gui2_window_present_dirty(&window);

    uint64_t start = (uint64_t)srv_ticks();
    uint64_t configured = 0;
    for (;;) {
        struct gui2_event event;
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        int changed = 0;
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                configured = 1;
                if (gui2_window_resize(&window, event.width, event.height) == 0) {
                    changed = 1;
                }
            } else if (event.type == GUI2_EVENT_CLOSE) {
                gui2_window_close(&window);
                srv_puts("guifail: close\n");
                return 0;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            }
        }
        if (changed) {
            draw_app(&window, mode);
            gui2_window_present_dirty(&window);
        }
        if ((configured || elapsed > 20) && exit_mode) {
            srv_puts("guifail: exiting status=");
            print_u64(exit_code);
            srv_puts("\n");
            return (int)exit_code;
        }
        if ((configured || elapsed > 20) && fault_mode) {
            srv_puts("guifail: faulting\n");
            volatile uint64_t *bad = (uint64_t *)0;
            *bad = 0x4755494641494cull;
        }
        if (elapsed > 240 && !exit_mode && !fault_mode) {
            break;
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("guifail: exited\n");
    return 0;
}
