#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 440
#define HEIGHT 320
#define DOC_MAX 1024
#define DRAFT_MAX 128

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

static void append_line(char *doc, uint64_t capacity, const char *line) {
    uint64_t out = 0;
    if (doc == 0 || line == 0 || line[0] == '\0' || capacity == 0) {
        return;
    }
    while (doc[out] != '\0' && out + 1 < capacity) {
        out++;
    }
    if (out != 0 && out + 1 < capacity) {
        doc[out++] = '\n';
    }
    for (uint64_t i = 0; line[i] != '\0' && out + 1 < capacity; i++) {
        doc[out++] = line[i];
    }
    doc[out] = '\0';
}

static void load_document(const char *path, char *doc, uint64_t capacity) {
    int fd = (int)srv_open(path);
    uint64_t out = 0;
    if (doc == 0 || capacity == 0) {
        return;
    }
    if (fd < 0) {
        copy_text(doc, capacity, "TEXT EDITOR");
        return;
    }
    for (;;) {
        long count = srv_read(fd, doc + out, capacity - out - 1);
        if (count <= 0) {
            break;
        }
        out += (uint64_t)count;
        if (out + 1 >= capacity) {
            break;
        }
    }
    doc[out] = '\0';
    srv_close(fd);
    if (doc[0] == '\0') {
        copy_text(doc, capacity, "TEXT EDITOR");
    }
}

static void set_status(char *status, uint64_t capacity, const char *text) {
    copy_text(status, capacity, text);
}

static void reset_textbox(struct gui2_textbox *textbox, char *buffer, uint64_t capacity) {
    copy_text(buffer, capacity, "");
    textbox->length = 0;
    textbox->cursor = 0;
}

static void draw_textedit(struct gui2_window *window,
    struct gui2_textbox *entry,
    struct gui2_button *add,
    struct gui2_button *save,
    struct gui2_button *clear,
    const char *doc,
    const char *status,
    const char *path) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t content_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t doc_h = window->height > 156 ? window->height - 156 : 72;
    uint64_t button_w = content_w > 2 * gap ? (content_w - 2 * gap) / 3 : 1;
    int64_t entry_y = (int64_t)(pad + 42 + doc_h + gap);
    int64_t buttons_y = entry_y + (int64_t)theme->control_h + (int64_t)gap;

    gui2_clear(window, theme->canvas);
    gui2_label(window, (int64_t)pad, (int64_t)pad, "TEXT EDIT");
    gui2_text(window, (int64_t)pad, (int64_t)(pad + 18), path, theme->text_muted);
    gui2_text(window, (int64_t)(window->width > 130 ? window->width - 118 : pad),
        (int64_t)pad, status, theme->text_muted);

    gui2_panel(window, (int64_t)pad, (int64_t)(pad + 42), content_w, doc_h, theme->panel);
    gui2_text(window, (int64_t)(pad + 10), (int64_t)(pad + 52),
        doc != 0 && doc[0] != '\0' ? doc : "EMPTY", theme->text);

    entry->x = (int64_t)pad;
    entry->y = entry_y;
    entry->width = content_w;
    entry->height = theme->control_h + 6;
    gui2_textbox_draw(window, entry);

    add->x = (int64_t)pad;
    add->y = buttons_y;
    add->width = button_w;
    add->height = theme->control_h;
    save->x = (int64_t)(pad + button_w + gap);
    save->y = buttons_y;
    save->width = button_w;
    save->height = theme->control_h;
    clear->x = (int64_t)(pad + 2 * (button_w + gap));
    clear->y = buttons_y;
    clear->width = button_w;
    clear->height = theme->control_h;
    gui2_button_draw(window, add);
    gui2_button_draw(window, save);
    gui2_button_draw(window, clear);
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_textbox entry;
    struct gui2_button add;
    struct gui2_button save;
    struct gui2_button clear;
    char draft[DRAFT_MAX];
    char doc[DOC_MAX];
    char status[48];
    const char *path = argc > 1 ? argv[1] : "/fat/text.txt";
    uint64_t start;

    srv_puts("textedit: start\n");
    load_document(path, doc, sizeof(doc));
    set_status(status, sizeof(status), "READY");

    if (gui2_window_open(&window, WIN, "TEXT EDIT",
            220, 110, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("textedit: window open failed\n");
        return 1;
    }

    gui2_context_init(&context);
    gui2_textbox_init(&entry, 14, 230, WIDTH - 28, 34, draft, sizeof(draft));
    gui2_textbox_set_placeholder(&entry, "TYPE A LINE");
    gui2_button_init(&add, 14, 274, 96, 30, "ADD");
    gui2_button_init(&save, 122, 274, 96, 30, "SAVE");
    gui2_button_init(&clear, 230, 274, 96, 30, "CLEAR");

    draw_textedit(&window, &entry, &add, &save, &clear, doc, status, path);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[] = {
            { GUI2_CONTROL_TEXTBOX, &entry },
            { GUI2_CONTROL_BUTTON, &add },
            { GUI2_CONTROL_BUTTON, &save },
            { GUI2_CONTROL_BUTTON, &clear },
        };
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 320) {
            break;
        }
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("textedit: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("textedit: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("textedit: close\n");
                closing = 1;
                break;
            }
            changed |= gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            if (add.clicks != 0) {
                add.clicks = 0;
                if (draft[0] != '\0') {
                    if (doc[0] == 'T' && doc[1] == 'E' && doc[2] == 'X') {
                        doc[0] = '\0';
                    }
                    append_line(doc, sizeof(doc), draft);
                    reset_textbox(&entry, draft, sizeof(draft));
                    set_status(status, sizeof(status), "ADDED");
                    srv_puts("textedit: add\n");
                }
                changed = 1;
            }
            if (save.clicks != 0) {
                save.clicks = 0;
                if (srv_fs_write(path, doc, srv_strlen(doc)) >= 0) {
                    set_status(status, sizeof(status), "SAVED");
                    srv_puts("textedit: save\n");
                } else {
                    set_status(status, sizeof(status), "SAVE FAILED");
                    srv_puts("textedit: save failed\n");
                }
                changed = 1;
            }
            if (clear.clicks != 0) {
                clear.clicks = 0;
                copy_text(doc, sizeof(doc), "TEXT EDITOR");
                reset_textbox(&entry, draft, sizeof(draft));
                set_status(status, sizeof(status), "CLEARED");
                srv_puts("textedit: clear\n");
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_textedit(&window, &entry, &add, &save, &clear, doc, status, path);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("textedit: exited\n");
    return 0;
}
