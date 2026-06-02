#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 380
#define HEIGHT 260

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

static void append_line(char *list, uint64_t capacity, const char *line) {
    uint64_t out = 0;
    if (list == 0 || line == 0 || line[0] == '\0' || capacity == 0) {
        return;
    }
    while (list[out] != '\0' && out + 1 < capacity) {
        out++;
    }
    if (out != 0 && out + 1 < capacity) {
        list[out++] = '\n';
    }
    for (uint64_t i = 0; line[i] != '\0' && out + 1 < capacity; i++) {
        list[out++] = line[i];
    }
    list[out] = '\0';
}

static void draw_notes(struct gui2_window *window,
    struct gui2_textbox *entry,
    struct gui2_button *add,
    struct gui2_button *clear,
    const char *items) {
    const struct gui2_theme *theme = gui2_theme_default();
    struct gui2_layout layout;
    struct gui2_rect row;
    uint64_t layout_width = window->width > 28 ? window->width - 28 : 1;
    uint64_t list_height = window->height > 148 ? window->height - 148 : 44;

    gui2_clear(window, theme->canvas);
    gui2_label(window, 14, 12, "NOTES2");
    gui2_text(window, 14, 28, window->focused ? "READY" : "BACKGROUND", theme->text_muted);

    gui2_layout_begin(&layout, 14, 48, layout_width);
    row = gui2_layout_next(&layout, list_height);
    gui2_panel(window, row.x, row.y, row.width, row.height, theme->panel);
    gui2_text(window, row.x + 10, row.y + 10,
        items != 0 && items[0] != '\0' ? items : "NO NOTES YET", theme->text);

    row = gui2_layout_next(&layout, theme->control_h + 6);
    entry->x = row.x;
    entry->y = row.y;
    entry->width = row.width;
    entry->height = row.height;
    gui2_textbox_draw(window, entry);

    row = gui2_layout_next(&layout, theme->control_h);
    add->x = row.x;
    add->y = row.y;
    add->width = 96;
    add->height = row.height;
    clear->x = row.x + 108;
    clear->y = row.y;
    clear->width = 96;
    clear->height = row.height;
    gui2_button_draw(window, add);
    gui2_button_draw(window, clear);
}

int main(void) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_textbox entry;
    struct gui2_button add;
    struct gui2_button clear;
    char draft[80];
    char items[512];
    uint64_t start;

    srv_puts("notes2: start\n");
    if (gui2_window_open(&window, WIN, "NOTES2",
            260, 330, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("notes2: window open failed\n");
        return 1;
    }

    gui2_context_init(&context);
    gui2_textbox_init(&entry, 14, 166, WIDTH - 28, 34, draft, sizeof(draft));
    gui2_textbox_set_placeholder(&entry, "TYPE A NOTE");
    gui2_button_init(&add, 14, 210, 96, 30, "ADD");
    gui2_button_init(&clear, 122, 210, 96, 30, "CLEAR");
    items[0] = '\0';
    draw_notes(&window, &entry, &add, &clear, items);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[] = {
            { GUI2_CONTROL_TEXTBOX, &entry },
            { GUI2_CONTROL_BUTTON, &add },
            { GUI2_CONTROL_BUTTON, &clear },
        };
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 260) {
            break;
        }
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("notes2: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("notes2: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("notes2: close\n");
                closing = 1;
                break;
            }
            changed |= gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            if (add.clicks != 0) {
                add.clicks = 0;
                if (draft[0] != '\0') {
                    append_line(items, sizeof(items), draft);
                    copy_text(draft, sizeof(draft), "");
                    entry.length = 0;
                    entry.cursor = 0;
                    srv_puts("notes2: add\n");
                }
                changed = 1;
            }
            if (clear.clicks != 0) {
                clear.clicks = 0;
                items[0] = '\0';
                copy_text(draft, sizeof(draft), "");
                entry.length = 0;
                entry.cursor = 0;
                srv_puts("notes2: clear\n");
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_notes(&window, &entry, &add, &clear, items);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("notes2: exited\n");
    return 0;
}
