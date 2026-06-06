#include <srvros/cli.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 560
#define HEIGHT 400
#define DOC_MAX 4096
#define STATUS_MAX 96
#define INFO_MAX 96
#define FIND_MAX 64

enum pending_textedit_action {
    PENDING_NONE = 0,
    PENDING_OPEN,
    PENDING_RELOAD,
    PENDING_CLEAR,
    PENDING_CLOSE
};

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

static int ensure_home_dir(void) {
    struct srv_stat info;
    if (srv_stat("/fat/home", &info) == 0 && info.type == 1) {
        return 0;
    }
    if (srv_mkdir("/fat/home") == 0) {
        return 0;
    }
    return srv_stat("/fat/home", &info) == 0 && info.type == 1 ? 0 : -1;
}

static int load_document(const char *path, char *doc, uint64_t capacity,
    uint64_t *length_out) {
    int fd = (int)srv_open(path);
    uint64_t out = 0;
    if (doc == 0 || capacity == 0) {
        return -1;
    }
    doc[0] = '\0';
    if (length_out != 0) {
        *length_out = 0;
    }
    if (fd < 0) {
        return -1;
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
    if (length_out != 0) {
        *length_out = out;
    }
    return 0;
}

static int save_document(const char *path, const char *doc, uint64_t length) {
    return srv_fs_write(path, doc, length) >= 0 ? 0 : -1;
}

static void parent_dir(char *out, size_t capacity, const char *path) {
    size_t length = cli_strlen(path);
    if (capacity == 0) {
        return;
    }
    if (length <= 1) {
        cli_copy(out, capacity, "/");
        return;
    }
    while (length > 1 && path[length - 1] == '/') {
        length--;
    }
    while (length > 1 && path[length - 1] != '/') {
        length--;
    }
    if (length > 1 && path[length - 1] == '/') {
        length--;
    }
    if (length >= capacity) {
        length = capacity - 1;
    }
    for (size_t i = 0; i < length; i++) {
        out[i] = path[i];
    }
    out[length] = '\0';
}

static int load_path_into_editor(const char *source_path, char *path,
    size_t path_capacity, char *doc, struct gui2_textarea *editor,
    char *status, size_t status_capacity, int *dirty) {
    if (load_document(source_path, doc, DOC_MAX, 0) != 0) {
        cli_copy(status, status_capacity, "OPEN FAILED");
        return -1;
    }
    cli_copy(path, path_capacity, source_path);
    gui2_textarea_sync(editor);
    if (dirty != 0) {
        *dirty = 0;
    }
    cli_copy(status, status_capacity, "LOADED");
    srv_puts("textedit: open ");
    srv_puts(source_path);
    srv_puts("\n");
    return 0;
}

static void clear_document(char *doc, struct gui2_textarea *editor) {
    if (doc != 0) {
        doc[0] = '\0';
    }
    gui2_textarea_sync(editor);
}

static void cursor_position(const struct gui2_textarea *editor,
    uint64_t *line_out, uint64_t *column_out) {
    uint64_t line = 1;
    uint64_t column = 1;
    size_t cursor = editor != 0 ? editor->cursor : 0;
    if (editor == 0 || editor->buffer == 0) {
        if (line_out != 0) {
            *line_out = line;
        }
        if (column_out != 0) {
            *column_out = column;
        }
        return;
    }
    if (cursor > editor->length) {
        cursor = editor->length;
    }
    for (size_t i = 0; i < cursor; i++) {
        if (editor->buffer[i] == '\n') {
            line++;
            column = 1;
        } else {
            column++;
        }
    }
    if (line_out != 0) {
        *line_out = line;
    }
    if (column_out != 0) {
        *column_out = column;
    }
}

static void format_info(char *out, uint64_t capacity,
    const struct gui2_textarea *editor, int dirty) {
    uint64_t length = 0;
    uint64_t line = 1;
    uint64_t column = 1;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    append_text(out, capacity, &length, dirty ? "DIRTY" : "SAVED");
    append_text(out, capacity, &length, "  LN ");
    cursor_position(editor, &line, &column);
    append_u64(out, capacity, &length, line);
    append_text(out, capacity, &length, " COL ");
    append_u64(out, capacity, &length, column);
    append_text(out, capacity, &length, "  BYTES ");
    append_u64(out, capacity, &length, editor != 0 ? editor->length : 0);
}

static void format_header_path(char *out, uint64_t capacity, const char *path,
    int dirty) {
    uint64_t length = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    append_text(out, capacity, &length, path != 0 ? path : "");
    if (dirty) {
        append_char(out, capacity, &length, ' ');
        append_char(out, capacity, &length, '*');
    }
}

static int text_char_equal(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    return a == b;
}

static int text_matches_at(const char *text, size_t length, size_t index,
    const char *query, size_t query_length) {
    if (index + query_length > length) {
        return 0;
    }
    for (size_t i = 0; i < query_length; i++) {
        if (!text_char_equal(text[index + i], query[i])) {
            return 0;
        }
    }
    return 1;
}

static int find_next_in_editor(struct gui2_textarea *editor, const char *query,
    char *status, size_t status_capacity) {
    size_t query_length = cli_strlen(query);
    size_t start;
    if (query_length == 0) {
        cli_copy(status, status_capacity, "TYPE FIND TEXT");
        return -1;
    }
    if (editor == 0 || editor->buffer == 0 || query_length > editor->length) {
        cli_copy(status, status_capacity, "NOT FOUND");
        return -1;
    }
    start = editor->cursor < editor->length ? editor->cursor + 1 : 0;
    for (size_t i = start; i < editor->length; i++) {
        if (text_matches_at(editor->buffer, editor->length, i, query,
                query_length)) {
            gui2_textarea_set_cursor(editor, i);
            cli_copy(status, status_capacity, "FOUND");
            return 0;
        }
    }
    for (size_t i = 0; i < start && i < editor->length; i++) {
        if (text_matches_at(editor->buffer, editor->length, i, query,
                query_length)) {
            gui2_textarea_set_cursor(editor, i);
            cli_copy(status, status_capacity, "FOUND");
            return 0;
        }
    }
    cli_copy(status, status_capacity, "NOT FOUND");
    return -1;
}

static void open_discard_dialog(struct gui2_dialog *dialog,
    enum pending_textedit_action *pending_action,
    enum pending_textedit_action action, const char *message) {
    if (dialog == 0) {
        return;
    }
    if (pending_action != 0) {
        *pending_action = action;
    }
    dialog->primary.label = "DISCARD";
    dialog->secondary.label = "CANCEL";
    gui2_dialog_open(dialog, "UNSAVED CHANGES",
        message != 0 ? message : "DISCARD UNSAVED CHANGES?");
}

static void draw_textedit(struct gui2_window *window,
    struct gui2_textarea *editor,
    struct gui2_button *open,
    struct gui2_button *save,
    struct gui2_button *save_as,
    struct gui2_button *reload,
    struct gui2_button *clear,
    struct gui2_textbox *find_box,
    struct gui2_button *find_next,
    struct gui2_dialog *confirm,
    struct gui2_file_dialog *open_dialog,
    struct gui2_file_dialog *save_dialog,
    const char *status,
    const char *path,
    int dirty) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t content_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t footer_h = 2 * theme->control_h + 2 * gap + theme->status_h + pad;
    uint64_t editor_h = window->height > theme->toolbar_h + pad + footer_h ?
        window->height - theme->toolbar_h - pad - footer_h : 72;
    int64_t editor_y = (int64_t)(theme->toolbar_h + pad);
    int64_t find_y = editor_y + (int64_t)editor_h + (int64_t)gap;
    int64_t buttons_y = find_y + (int64_t)theme->control_h + (int64_t)gap;
    uint64_t find_button_w = content_w > 120 ? 96 : content_w / 3;
    uint64_t find_box_w = content_w > find_button_w + gap ?
        content_w - find_button_w - gap : content_w;
    char info[INFO_MAX];
    char header_path[CLI_PATH_MAX + 4];

    format_info(info, sizeof(info), editor, dirty);
    format_header_path(header_path, sizeof(header_path), path, dirty);

    gui2_clear(window, theme->canvas);
    gui2_app_header(window, "TEXT EDIT", header_path);

    editor->x = (int64_t)pad;
    editor->y = editor_y;
    editor->width = content_w;
    editor->height = editor_h;
    gui2_textarea_draw(window, editor);

    find_box->x = (int64_t)pad;
    find_box->y = find_y;
    find_box->width = find_box_w;
    find_box->height = theme->control_h;
    find_next->x = (int64_t)(pad + find_box_w + gap);
    find_next->y = find_y;
    find_next->width = find_button_w;
    find_next->height = theme->control_h;
    gui2_textbox_draw(window, find_box);
    gui2_button_draw(window, find_next);

    struct gui2_button buttons[] = { *open, *save, *save_as, *reload, *clear };
    gui2_layout_button_row(buttons, 5, (int64_t)pad, buttons_y,
        content_w, theme->control_h, gap);
    *open = buttons[0];
    *save = buttons[1];
    *save_as = buttons[2];
    *reload = buttons[3];
    *clear = buttons[4];
    gui2_button_draw(window, open);
    gui2_button_draw(window, save);
    gui2_button_draw(window, save_as);
    gui2_button_draw(window, reload);
    gui2_button_draw(window, clear);
    gui2_status_bar(window, status != 0 ? status : "READY", info);
    gui2_dialog_draw(window, confirm);
    gui2_file_dialog_draw(window, open_dialog);
    gui2_file_dialog_draw(window, save_dialog);
}

static int run_selftest(void) {
    const char *path = "/fat/home/textedit-selftest.txt";
    char doc[DOC_MAX];
    char status[STATUS_MAX];
    struct gui2_textarea editor;
    uint64_t length = 0;
    (void)srv_unlink(path);
    if (ensure_home_dir() != 0) {
        srv_puts("textedit-selftest: mkdir failed\n");
        return 1;
    }
    if (save_document(path, "alpha\n", 6) != 0) {
        srv_puts("textedit-selftest: save failed\n");
        return 1;
    }
    if (load_document(path, doc, sizeof(doc), &length) != 0 ||
        length != 6 || !cli_streq(doc, "alpha\n")) {
        srv_puts("textedit-selftest: load failed\n");
        return 1;
    }
    if (save_document(path, "beta\nline2\n", 11) != 0) {
        srv_puts("textedit-selftest: rewrite failed\n");
        return 1;
    }
    if (load_document(path, doc, sizeof(doc), &length) != 0 ||
        length != 11 || !cli_streq(doc, "beta\nline2\n")) {
        srv_puts("textedit-selftest: reload failed\n");
        return 1;
    }
    gui2_textarea_init(&editor, 0, 0, 80, 40, doc, sizeof(doc));
    cli_copy(doc, sizeof(doc), "beta\nline2\n");
    gui2_textarea_sync(&editor);
    gui2_textarea_set_cursor(&editor, 0);
    if (find_next_in_editor(&editor, "LINE2", status, sizeof(status)) != 0 ||
        editor.cursor != 5) {
        srv_puts("textedit-selftest: find failed\n");
        return 1;
    }
    if (srv_unlink(path) != 0) {
        srv_puts("textedit-selftest: cleanup failed\n");
        return 1;
    }
    srv_puts("textedit-selftest: ok\n");
    return 0;
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_textarea editor;
    struct gui2_button open;
    struct gui2_button save;
    struct gui2_button save_as;
    struct gui2_button reload;
    struct gui2_button clear;
    struct gui2_textbox find_box;
    struct gui2_button find_next;
    struct gui2_dialog confirm;
    struct gui2_file_dialog open_dialog;
    struct gui2_file_dialog save_dialog;
    char doc[DOC_MAX];
    char find_text[FIND_MAX];
    char path[CLI_PATH_MAX];
    char path_dir[CLI_PATH_MAX];
    char path_base[CLI_PATH_MAX];
    char pending_path[CLI_PATH_MAX];
    char status[STATUS_MAX];
    int dirty = 0;
    enum pending_textedit_action pending_action = PENDING_NONE;

    if (argc > 1 && cli_streq(argv[1], "--selftest")) {
        return run_selftest();
    }

    srv_puts("textedit: start\n");
    cli_copy(status, sizeof(status), "READY");
    cli_normalize_path(path, sizeof(path), "/", argc > 1 ? argv[1] : "/fat/text.txt");
    pending_path[0] = '\0';
    doc[0] = '\0';
    find_text[0] = '\0';

    if (gui2_window_open(&window, WIN, "TEXT EDIT",
            220, 110, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("textedit: window open failed\n");
        return 1;
    }

    gui2_context_init(&context);
    gui2_textarea_init(&editor, 14, 66, WIDTH - 28, 240, doc, sizeof(doc));
    if (load_document(path, doc, sizeof(doc), 0) == 0) {
        cli_copy(status, sizeof(status), "LOADED");
    } else {
        cli_copy(status, sizeof(status), "NEW FILE");
    }
    gui2_textarea_sync(&editor);
    gui2_textarea_set_placeholder(&editor, "TYPE TEXT");
    gui2_button_init(&open, 14, 340, 120, 30, "OPEN");
    gui2_button_init(&save, 146, 340, 120, 30, "SAVE");
    gui2_button_init(&save_as, 278, 340, 120, 30, "SAVE AS");
    gui2_button_init(&reload, 410, 340, 120, 30, "RELOAD");
    gui2_button_init(&clear, 542, 340, 120, 30, "CLEAR");
    gui2_textbox_init(&find_box, 14, 304, 390, 30, find_text,
        sizeof(find_text));
    gui2_textbox_set_placeholder(&find_box, "FIND TEXT");
    gui2_button_init(&find_next, 414, 304, 120, 30, "FIND");
    gui2_dialog_init(&confirm, "UNSAVED CHANGES",
        "DISCARD UNSAVED CHANGES?", "DISCARD", "CANCEL");
    gui2_file_dialog_init(&open_dialog, "OPEN FILE", "OPEN", 0);
    gui2_file_dialog_init(&save_dialog, "SAVE AS", "SAVE", 1);

    draw_textedit(&window, &editor, &open, &save, &save_as, &reload, &clear,
        &find_box, &find_next, &confirm, &open_dialog, &save_dialog, status,
        path, dirty);
    gui2_window_present_dirty(&window);

    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[] = {
            { GUI2_CONTROL_TEXTAREA, &editor },
            { GUI2_CONTROL_BUTTON, &open },
            { GUI2_CONTROL_BUTTON, &save },
            { GUI2_CONTROL_BUTTON, &save_as },
            { GUI2_CONTROL_BUTTON, &reload },
            { GUI2_CONTROL_BUTTON, &clear },
            { GUI2_CONTROL_TEXTBOX, &find_box },
            { GUI2_CONTROL_BUTTON, &find_next },
        };
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
                if (dirty) {
                    open_discard_dialog(&confirm, &pending_action,
                        PENDING_CLOSE, "DISCARD CHANGES AND CLOSE?");
                    changed = 1;
                } else {
                    closing = 1;
                }
                break;
            }

            if (confirm.active) {
                changed |= gui2_dialog_event(&confirm, &event);
                if (confirm.primary_clicks != 0) {
                    confirm.primary_clicks = 0;
                    gui2_dialog_close(&confirm);
                    if (pending_action == PENDING_OPEN) {
                        (void)load_path_into_editor(pending_path, path,
                            sizeof(path), doc, &editor, status, sizeof(status),
                            &dirty);
                    } else if (pending_action == PENDING_RELOAD) {
                        if (load_document(path, doc, sizeof(doc), 0) == 0) {
                            gui2_textarea_sync(&editor);
                            dirty = 0;
                            cli_copy(status, sizeof(status), "RELOADED");
                            srv_puts("textedit: reload\n");
                        } else {
                            cli_copy(status, sizeof(status), "RELOAD FAILED");
                        }
                    } else if (pending_action == PENDING_CLEAR) {
                        clear_document(doc, &editor);
                        dirty = 1;
                        cli_copy(status, sizeof(status), "CLEARED");
                        srv_puts("textedit: clear\n");
                    } else if (pending_action == PENDING_CLOSE) {
                        closing = 1;
                    }
                    pending_action = PENDING_NONE;
                    changed = 1;
                }
                if (confirm.secondary_clicks != 0) {
                    confirm.secondary_clicks = 0;
                    gui2_dialog_close(&confirm);
                    pending_action = PENDING_NONE;
                    cli_copy(status, sizeof(status), "CANCELED");
                    changed = 1;
                }
                if (closing) {
                    break;
                }
                continue;
            }
            if (open_dialog.active || save_dialog.active) {
                struct gui2_file_dialog *dialog = open_dialog.active ?
                    &open_dialog : &save_dialog;
                changed |= gui2_file_dialog_event(dialog, &event);
                if (dialog->cancel_clicks != 0) {
                    dialog->cancel_clicks = 0;
                    gui2_file_dialog_close(dialog);
                    cli_copy(status, sizeof(status), "CANCELED");
                    changed = 1;
                }
                if (dialog->primary_clicks != 0) {
                    dialog->primary_clicks = 0;
                    if (dialog == &open_dialog) {
                        cli_copy(pending_path, sizeof(pending_path),
                            dialog->result_path);
                        gui2_file_dialog_close(dialog);
                        if (dirty) {
                            open_discard_dialog(&confirm, &pending_action,
                                PENDING_OPEN, "DISCARD CHANGES AND OPEN?");
                        } else {
                            (void)load_path_into_editor(pending_path, path,
                                sizeof(path), doc, &editor, status,
                                sizeof(status), &dirty);
                        }
                    } else {
                        cli_copy(path, sizeof(path), dialog->result_path);
                        gui2_file_dialog_close(dialog);
                        if (save_document(path, doc, editor.length) == 0) {
                            dirty = 0;
                            cli_copy(status, sizeof(status), "SAVED AS");
                            srv_puts("textedit: save-as\n");
                        } else {
                            cli_copy(status, sizeof(status), "SAVE AS FAILED");
                        }
                    }
                    changed = 1;
                }
                continue;
            }

            int result = gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            changed |= result;
            if ((result & GUI2_WIDGET_VALUE) != 0 && editor.focused) {
                dirty = 1;
                cli_copy(status, sizeof(status), "DIRTY");
            } else if ((result & GUI2_WIDGET_DIRTY) != 0 && editor.focused) {
                changed = 1;
            }

            if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 15) {
                open.clicks++;
                changed = 1;
            } else if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 19) {
                save.clicks++;
                changed = 1;
            } else if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 18) {
                reload.clicks++;
                changed = 1;
            } else if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 12) {
                clear.clicks++;
                changed = 1;
            } else if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 17) {
                if (dirty) {
                    open_discard_dialog(&confirm, &pending_action,
                        PENDING_CLOSE, "DISCARD CHANGES AND CLOSE?");
                } else {
                    closing = 1;
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 7) {
                find_next.clicks++;
                changed = 1;
            }

            if (open.clicks != 0) {
                open.clicks = 0;
                parent_dir(path_dir, sizeof(path_dir), path);
                gui2_file_dialog_open(&open_dialog, path_dir, "");
                cli_copy(status, sizeof(status), "OPEN FILE");
                changed = 1;
            }
            if (save.clicks != 0) {
                save.clicks = 0;
                if (save_document(path, doc, editor.length) == 0) {
                    dirty = 0;
                    cli_copy(status, sizeof(status), "SAVED");
                    srv_puts("textedit: save\n");
                } else {
                    cli_copy(status, sizeof(status), "SAVE FAILED");
                    srv_puts("textedit: save failed\n");
                }
                changed = 1;
            }
            if (save_as.clicks != 0) {
                save_as.clicks = 0;
                parent_dir(path_dir, sizeof(path_dir), path);
                cli_basename(path_base, sizeof(path_base), path);
                gui2_file_dialog_open(&save_dialog, path_dir, path_base);
                cli_copy(status, sizeof(status), "SAVE AS");
                changed = 1;
            }
            if (reload.clicks != 0) {
                reload.clicks = 0;
                if (dirty) {
                    open_discard_dialog(&confirm, &pending_action,
                        PENDING_RELOAD, "DISCARD CHANGES AND RELOAD?");
                } else if (load_document(path, doc, sizeof(doc), 0) == 0) {
                    gui2_textarea_sync(&editor);
                    cli_copy(status, sizeof(status), "RELOADED");
                    srv_puts("textedit: reload\n");
                } else {
                    cli_copy(status, sizeof(status), "RELOAD FAILED");
                }
                changed = 1;
            }
            if (clear.clicks != 0) {
                clear.clicks = 0;
                if (dirty) {
                    open_discard_dialog(&confirm, &pending_action,
                        PENDING_CLEAR, "DISCARD CHANGES AND CLEAR?");
                } else {
                    clear_document(doc, &editor);
                    dirty = 1;
                    cli_copy(status, sizeof(status), "CLEARED");
                    srv_puts("textedit: clear\n");
                }
                changed = 1;
            }
            if (find_next.clicks != 0) {
                find_next.clicks = 0;
                (void)find_next_in_editor(&editor, find_text, status,
                    sizeof(status));
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_textedit(&window, &editor, &open, &save, &save_as, &reload,
                &clear, &find_box, &find_next, &confirm, &open_dialog,
                &save_dialog, status, path, dirty);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("textedit: exited\n");
    return 0;
}
