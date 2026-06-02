#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 300
#define HEIGHT 180

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

static void draw_app(struct gui2_window *window, struct gui2_button *button,
    struct gui2_textbox *textbox, uint64_t event_count) {
    char status[64];
    uint64_t length = 0;
    uint64_t field_width = window->width > 28 ? window->width - 28 : 1;
    status[0] = '\0';
    append_text(status, sizeof(status), &length, "EVENTS ");
    append_u64(status, sizeof(status), &length, event_count);
    append_text(status, sizeof(status), &length, window->focused ? " FOCUS" : " BLUR");

    textbox->x = 14;
    textbox->y = 72;
    textbox->width = field_width;
    textbox->height = 34;
    button->x = 14;
    button->y = window->height > 42 ? (int64_t)window->height - 42 : 14;
    button->width = 92;
    button->height = 28;

    gui2_clear(window, gui2_rgb(0x10, 0x18, 0x22));
    gui2_text(window, 14, 14, "GUI2 DEMO", gui2_rgb(0xff, 0xff, 0xff));
    gui2_text(window, 14, 32, status, gui2_rgb(0xb9, 0xd8, 0xdf));
    gui2_textbox_draw(window, textbox);
    gui2_button_draw(window, button);
}

int main(void) {
    struct gui2_window window;
    struct gui2_button button;
    struct gui2_textbox textbox;
    char input[48];
    uint64_t event_count = 0;
    uint64_t start;

    srv_puts("gui2demo: start\n");
    if (gui2_window_open(&window, WIN, "GUI2 DEMO",
            620, 240, WIDTH, HEIGHT, gui2_rgb(0x10, 0x18, 0x22)) != 0) {
        srv_puts("gui2demo: window open failed\n");
        return 1;
    }

    gui2_button_init(&button, 14, 132, 92, 28, "CLICK");
    gui2_textbox_init(&textbox, 14, 72, 260, 34, input, sizeof(input));
    draw_app(&window, &button, &textbox, event_count);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 150) {
            break;
        }
        while (gui2_poll_event(&window, &event) > 0) {
            event_count++;
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("gui2demo: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("gui2demo: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("gui2demo: close\n");
                closing = 1;
                break;
            }
            changed |= gui2_textbox_event(&textbox, &event);
            changed |= gui2_button_event(&button, &event);
            if (button.clicks != 0) {
                button.clicks = 0;
                srv_puts("gui2demo: click\n");
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_app(&window, &button, &textbox, event_count);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("gui2demo: exited\n");
    return 0;
}
