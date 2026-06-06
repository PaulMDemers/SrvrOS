#include <srvros/cli.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 720
#define HEIGHT 430
#define NOTES_DIR "/fat/home/notes"
#define NOTE_MAX 48
#define TITLE_MAX 64
#define BODY_MAX 4096
#define STATUS_MAX 80
#define FIND_MAX 64

enum pending_note_action {
    PENDING_NONE = 0,
    PENDING_DELETE,
    PENDING_SELECT,
    PENDING_RELOAD,
    PENDING_CLOSE,
    PENDING_DELETE_AFTER_DISCARD
};

struct note_entry {
    char title[TITLE_MAX];
    char path[CLI_PATH_MAX];
    char detail[24];
    uint64_t size;
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
    if (*length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
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

static void format_detail(char *out, uint64_t capacity, uint64_t size) {
    uint64_t length = 0;
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    append_u64(out, capacity, &length, size);
    append_char(out, capacity, &length, 'B');
}

static int path_is_directory(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
}

static int ensure_notes_dir(void) {
    if (srv_mkdir("/fat/home") < 0 && !path_is_directory("/fat/home")) {
        return -1;
    }
    if (srv_mkdir(NOTES_DIR) < 0 && !path_is_directory(NOTES_DIR)) {
        return -1;
    }
    return 0;
}

static int title_char_ok(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
        (c >= '0' && c <= '9') || c == '-' || c == '_' || c == ' ';
}

static void sanitize_title(char *out, uint64_t capacity, const char *input) {
    uint64_t length = 0;
    const char *text = input != 0 ? input : "";
    while (*text == ' ' || *text == '\t') {
        text++;
    }
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    for (uint64_t i = 0; text[i] != '\0' && length + 1 < capacity; i++) {
        char c = text[i];
        if (c == '\t') {
            c = ' ';
        }
        if (c == '/' || c == '\\') {
            c = '-';
        } else if (!title_char_ok(c)) {
            c = '-';
        }
        if (c == ' ' && length == 0) {
            continue;
        }
        append_char(out, capacity, &length, c);
    }
    while (length > 0 && out[length - 1] == ' ') {
        out[--length] = '\0';
    }
    if (out[0] == '\0') {
        cli_copy(out, capacity, "untitled");
    }
}

static void note_path_for_title(char *out, uint64_t capacity, const char *title) {
    char filename[TITLE_MAX + 5];
    uint64_t length = 0;
    filename[0] = '\0';
    for (uint64_t i = 0; title != 0 && title[i] != '\0' &&
            length + 5 < sizeof(filename); i++) {
        append_char(filename, sizeof(filename), &length, title[i]);
    }
    append_char(filename, sizeof(filename), &length, '.');
    append_char(filename, sizeof(filename), &length, 't');
    append_char(filename, sizeof(filename), &length, 'x');
    append_char(filename, sizeof(filename), &length, 't');
    cli_join_path(out, capacity, NOTES_DIR, filename);
}

static void title_from_path(char *out, uint64_t capacity, const char *path) {
    cli_basename(out, capacity, path);
    uint64_t length = cli_strlen(out);
    if (length > 4 && out[length - 4] == '.' &&
        (out[length - 3] == 't' || out[length - 3] == 'T') &&
        (out[length - 2] == 'x' || out[length - 2] == 'X') &&
        (out[length - 1] == 't' || out[length - 1] == 'T')) {
        out[length - 4] = '\0';
    }
}

static int cmp_char(char a, char b) {
    if (a >= 'A' && a <= 'Z') {
        a = (char)(a - 'A' + 'a');
    }
    if (b >= 'A' && b <= 'Z') {
        b = (char)(b - 'A' + 'a');
    }
    if (a < b) {
        return -1;
    }
    if (a > b) {
        return 1;
    }
    return 0;
}

static int cmp_text(const char *a, const char *b) {
    uint64_t i = 0;
    while (a[i] != '\0' && b[i] != '\0') {
        int result = cmp_char(a[i], b[i]);
        if (result != 0) {
            return result;
        }
        i++;
    }
    if (a[i] == '\0' && b[i] == '\0') {
        return 0;
    }
    return a[i] == '\0' ? -1 : 1;
}

static int text_matches_at(const char *text, size_t length, size_t index,
    const char *query, size_t query_length) {
    if (index + query_length > length) {
        return 0;
    }
    for (size_t i = 0; i < query_length; i++) {
        if (cmp_char(text[index + i], query[i]) != 0) {
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

static void sort_notes(struct note_entry *entries, uint64_t count) {
    for (uint64_t i = 1; i < count; i++) {
        struct note_entry value = entries[i];
        uint64_t j = i;
        while (j > 0 && cmp_text(value.title, entries[j - 1].title) < 0) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = value;
    }
}

static void bind_items(const struct note_entry *entries,
    struct gui2_list_item *items, uint64_t count) {
    for (uint64_t i = 0; i < count; i++) {
        items[i].label = entries[i].title;
        items[i].detail = entries[i].detail;
        items[i].flags = 0;
    }
}

static int load_note_file(const char *path, char *body, uint64_t capacity) {
    int fd = (int)srv_open(path);
    uint64_t out = 0;
    if (body == 0 || capacity == 0) {
        return -1;
    }
    body[0] = '\0';
    if (fd < 0) {
        return -1;
    }
    for (;;) {
        long count = srv_read(fd, body + out, capacity - out - 1);
        if (count <= 0) {
            break;
        }
        out += (uint64_t)count;
        if (out + 1 >= capacity) {
            break;
        }
    }
    body[out] = '\0';
    srv_close(fd);
    return 0;
}

static int load_note_list(struct note_entry *entries, struct gui2_list_item *items,
    uint64_t capacity, uint64_t *count_out) {
    char listed[CLI_PATH_MAX];
    char prefix[CLI_PATH_MAX];
    uint64_t size = 0;
    uint64_t count = 0;
    if (count_out == 0) {
        return -1;
    }
    *count_out = 0;
    if (ensure_notes_dir() != 0) {
        return -1;
    }
    cli_join_path(prefix, sizeof(prefix), NOTES_DIR, "");
    for (uint64_t i = 0;; i++) {
        struct srv_stat info;
        long result = srv_list(i, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        if (!cli_starts_with(listed, prefix) || cli_streq(listed, NOTES_DIR)) {
            continue;
        }
        const char *rest = listed + cli_strlen(prefix);
        if (rest[0] == '\0') {
            continue;
        }
        int nested = 0;
        for (uint64_t j = 0; rest[j] != '\0'; j++) {
            if (rest[j] == '/') {
                nested = 1;
                break;
            }
        }
        if (nested || srv_stat(listed, &info) != 0 || info.type == 1) {
            continue;
        }
        if (count >= capacity) {
            break;
        }
        title_from_path(entries[count].title, sizeof(entries[count].title), listed);
        cli_copy(entries[count].path, sizeof(entries[count].path), listed);
        entries[count].size = info.size;
        format_detail(entries[count].detail, sizeof(entries[count].detail), info.size);
        count++;
    }
    sort_notes(entries, count);
    bind_items(entries, items, count);
    *count_out = count;
    return 0;
}

static void sync_editor(struct gui2_textarea *editor, char *body) {
    (void)body;
    gui2_textarea_sync(editor);
}

static void reset_textbox(struct gui2_textbox *textbox) {
    if (textbox == 0 || textbox->buffer == 0 || textbox->capacity == 0) {
        return;
    }
    textbox->buffer[0] = '\0';
    textbox->length = 0;
    textbox->cursor = 0;
}

static int find_note_index(const struct note_entry *entries, uint64_t count,
    const char *title) {
    for (uint64_t i = 0; i < count; i++) {
        if (cli_streq(entries[i].title, title)) {
            return (int)i;
        }
    }
    return -1;
}

static void make_unique_title(char *out, uint64_t capacity,
    const struct note_entry *entries, uint64_t count) {
    for (uint64_t i = 1; i < 1000; i++) {
        char candidate[TITLE_MAX];
        uint64_t length = 0;
        cli_copy(candidate, sizeof(candidate), "note-");
        length = cli_strlen(candidate);
        append_u64(candidate, sizeof(candidate), &length, i);
        if (find_note_index(entries, count, candidate) < 0) {
            cli_copy(out, capacity, candidate);
            return;
        }
    }
    cli_copy(out, capacity, "note");
}

static void select_note(struct gui2_list *list, const struct note_entry *entries,
    uint64_t count, uint64_t index, struct gui2_textarea *editor, char *body,
    char *current_path, uint64_t current_path_capacity, char *current_title,
    uint64_t current_title_capacity, char *status, uint64_t status_capacity,
    int *dirty) {
    if (count == 0 || index >= count) {
        current_path[0] = '\0';
        current_title[0] = '\0';
        body[0] = '\0';
        sync_editor(editor, body);
        if (dirty != 0) {
            *dirty = 0;
        }
        cli_copy(status, status_capacity, "NO NOTES");
        return;
    }
    list->selected = index;
    gui2_list_keep_selected_visible(list);
    cli_copy(current_path, current_path_capacity, entries[index].path);
    cli_copy(current_title, current_title_capacity, entries[index].title);
    if (load_note_file(current_path, body, BODY_MAX) == 0) {
        sync_editor(editor, body);
        if (dirty != 0) {
            *dirty = 0;
        }
        cli_copy(status, status_capacity, "LOADED");
    } else {
        body[0] = '\0';
        sync_editor(editor, body);
        if (dirty != 0) {
            *dirty = 0;
        }
        cli_copy(status, status_capacity, "LOAD FAILED");
    }
}

static void refresh_notes(struct gui2_list *list, struct note_entry *entries,
    struct gui2_list_item *items, uint64_t *count, const char *preferred_title,
    struct gui2_textarea *editor, char *body, char *current_path,
    uint64_t current_path_capacity, char *current_title,
    uint64_t current_title_capacity, char *status, uint64_t status_capacity,
    int *dirty) {
    if (load_note_list(entries, items, NOTE_MAX, count) != 0) {
        *count = 0;
        gui2_list_set_items(list, items, 0);
        select_note(list, entries, *count, 0, editor, body, current_path,
            current_path_capacity, current_title, current_title_capacity,
            status, status_capacity, dirty);
        return;
    }
    gui2_list_set_items(list, items, (size_t)*count);
    if (*count == 0) {
        select_note(list, entries, *count, 0, editor, body, current_path,
            current_path_capacity, current_title, current_title_capacity,
            status, status_capacity, dirty);
        return;
    }
    int selected = preferred_title != 0 && preferred_title[0] != '\0' ?
        find_note_index(entries, *count, preferred_title) : -1;
    if (selected < 0) {
        selected = list->selected < *count ? (int)list->selected : 0;
    }
    select_note(list, entries, *count, (uint64_t)selected, editor, body,
        current_path, current_path_capacity, current_title,
        current_title_capacity, status, status_capacity, dirty);
}

static void confirm_message(char *out, uint64_t capacity, const char *title) {
    uint64_t length = 0;
    const char *prefix = "DELETE ";
    if (capacity == 0) {
        return;
    }
    out[0] = '\0';
    while (*prefix != '\0') {
        append_char(out, capacity, &length, *prefix++);
    }
    for (const char *p = title; p != 0 && *p != '\0'; p++) {
        append_char(out, capacity, &length, *p);
    }
    append_char(out, capacity, &length, '?');
}

static void restore_list_selection(struct gui2_list *list,
    const struct note_entry *entries, uint64_t count, const char *current_title) {
    int selected;
    if (list == 0 || current_title == 0 || current_title[0] == '\0') {
        return;
    }
    selected = find_note_index(entries, count, current_title);
    if (selected >= 0) {
        list->selected = (size_t)selected;
        gui2_list_keep_selected_visible(list);
    }
}

static void open_delete_dialog(struct gui2_dialog *dialog, char *confirm_text,
    uint64_t confirm_capacity, const char *title) {
    if (dialog == 0) {
        return;
    }
    dialog->primary.label = "DELETE";
    dialog->secondary.label = "CANCEL";
    confirm_message(confirm_text, confirm_capacity, title);
    gui2_dialog_open(dialog, "CONFIRM DELETE", confirm_text);
}

static void open_discard_dialog(struct gui2_dialog *dialog,
    enum pending_note_action *pending_action, enum pending_note_action action,
    const char *message) {
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

static int save_current_note(const char *path, const char *body, uint64_t length,
    char *status, uint64_t status_capacity, int *dirty) {
    if (path == 0 || path[0] == '\0') {
        cli_copy(status, status_capacity, "NO NOTE SELECTED");
        return -1;
    }
    if (srv_fs_write(path, body, length) < 0) {
        cli_copy(status, status_capacity, "SAVE FAILED");
        return -1;
    }
    if (dirty != 0) {
        *dirty = 0;
    }
    cli_copy(status, status_capacity, "SAVED");
    return 0;
}

static void draw_notes(struct gui2_window *window, struct gui2_list *list,
    struct gui2_textarea *editor, struct gui2_textbox *name_box,
    struct gui2_textbox *find_box, struct gui2_button *find_next,
    struct gui2_button *new_note, struct gui2_button *rename,
    struct gui2_button *del, struct gui2_button *save,
    struct gui2_button *reload, const char *current_title,
    const char *status, int dirty) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t content_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t left_w = content_w > 520 ? 220 : content_w / 3;
    uint64_t right_w = content_w > left_w + gap ? content_w - left_w - gap : 1;
    uint64_t reserved = theme->toolbar_h + theme->status_h + 3 * pad +
        2 * theme->control_h + 2 * gap;
    uint64_t body_h = window->height > reserved ? window->height - reserved : 120;
    int64_t body_y = (int64_t)(theme->toolbar_h + pad);
    int64_t find_y = body_y + (int64_t)body_h + (int64_t)gap;
    int64_t name_y = find_y + (int64_t)theme->control_h + (int64_t)gap;
    uint64_t find_button_w = right_w > 120 ? 86 : right_w / 3;
    uint64_t find_box_w = right_w > find_button_w + gap ?
        right_w - find_button_w - gap : right_w;
    char title_line[96];

    cli_copy(title_line, sizeof(title_line), current_title != 0 &&
        current_title[0] != '\0' ? current_title : "NO NOTE");
    if (dirty) {
        uint64_t length = cli_strlen(title_line);
        append_char(title_line, sizeof(title_line), &length, ' ');
        append_char(title_line, sizeof(title_line), &length, '*');
    }

    gui2_clear(window, theme->canvas);
    gui2_app_header(window, "NOTES", title_line);

    list->x = (int64_t)pad;
    list->y = body_y;
    list->width = left_w;
    list->height = body_h;
    gui2_list_draw(window, list);

    editor->x = (int64_t)(pad + left_w + gap);
    editor->y = body_y;
    editor->width = right_w;
    editor->height = body_h;
    gui2_textarea_draw(window, editor);

    find_box->x = (int64_t)(pad + left_w + gap);
    find_box->y = find_y;
    find_box->width = find_box_w;
    find_box->height = theme->control_h;
    find_next->x = (int64_t)(pad + left_w + 2 * gap + find_box_w);
    find_next->y = find_y;
    find_next->width = find_button_w;
    find_next->height = theme->control_h;
    gui2_textbox_draw(window, find_box);
    gui2_button_draw(window, find_next);

    name_box->x = (int64_t)pad;
    name_box->y = name_y;
    name_box->width = left_w;
    name_box->height = theme->control_h;
    gui2_textbox_draw(window, name_box);

    struct gui2_button buttons[] = {
        *new_note, *rename, *del, *save, *reload
    };
    gui2_layout_button_row(buttons, 5, (int64_t)(pad + left_w + gap),
        name_y, right_w, theme->control_h, gap);
    *new_note = buttons[0];
    *rename = buttons[1];
    *del = buttons[2];
    *save = buttons[3];
    *reload = buttons[4];
    gui2_button_draw(window, new_note);
    gui2_button_draw(window, rename);
    gui2_button_draw(window, del);
    gui2_button_draw(window, save);
    gui2_button_draw(window, reload);
    gui2_status_bar(window, status != 0 ? status : "READY", NOTES_DIR);
}

static int run_selftest(void) {
    const char *a = "/fat/home/notes/codex-note-a.txt";
    const char *b = "/fat/home/notes/codex-note-b.txt";
    char body[BODY_MAX];
    char status[STATUS_MAX];
    struct note_entry entries[NOTE_MAX];
    struct gui2_list_item items[NOTE_MAX];
    struct gui2_textarea editor;
    uint64_t count = 0;
    int found_a;
    int found_b;
    (void)srv_unlink(a);
    (void)srv_unlink(b);
    if (ensure_notes_dir() != 0) {
        srv_puts("notes-selftest: mkdir failed\n");
        return 1;
    }
    if (srv_fs_write(a, "alpha\n", 6) < 0) {
        srv_puts("notes-selftest: write failed\n");
        return 1;
    }
    if (load_note_file(a, body, sizeof(body)) != 0 || !cli_streq(body, "alpha\n")) {
        srv_puts("notes-selftest: load failed\n");
        return 1;
    }
    gui2_textarea_init(&editor, 0, 0, 80, 40, body, sizeof(body));
    cli_copy(body, sizeof(body), "alpha\n");
    gui2_textarea_sync(&editor);
    gui2_textarea_set_cursor(&editor, 0);
    if (find_next_in_editor(&editor, "LPHA", status, sizeof(status)) != 0 ||
        editor.cursor != 1) {
        srv_puts("notes-selftest: find failed\n");
        return 1;
    }
    if (srv_rename(a, b) != 0) {
        srv_puts("notes-selftest: rename failed\n");
        return 1;
    }
    if (load_note_list(entries, items, NOTE_MAX, &count) != 0) {
        srv_puts("notes-selftest: list failed\n");
        return 1;
    }
    found_a = find_note_index(entries, count, "codex-note-a") >= 0;
    found_b = find_note_index(entries, count, "codex-note-b") >= 0;
    if (found_a || !found_b) {
        srv_puts("notes-selftest: list content failed\n");
        return 1;
    }
    if (srv_unlink(b) != 0) {
        srv_puts("notes-selftest: delete failed\n");
        return 1;
    }
    srv_puts("notes-selftest: ok\n");
    return 0;
}

int main(int argc, char **argv) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_list list;
    struct gui2_textarea editor;
    struct gui2_textbox name_box;
    struct gui2_textbox find_box;
    struct gui2_button find_next;
    struct gui2_button new_note;
    struct gui2_button rename;
    struct gui2_button del;
    struct gui2_button save;
    struct gui2_button reload;
    struct gui2_dialog confirm;
    struct note_entry entries[NOTE_MAX];
    struct gui2_list_item items[NOTE_MAX];
    char body[BODY_MAX];
    char name[TITLE_MAX];
    char find_text[FIND_MAX];
    char current_path[CLI_PATH_MAX];
    char current_title[TITLE_MAX];
    char pending_delete[CLI_PATH_MAX];
    char confirm_text[96];
    char status[STATUS_MAX];
    uint64_t note_count = 0;
    uint64_t pending_index = 0;
    int dirty = 0;
    enum pending_note_action pending_action = PENDING_NONE;

    if (argc > 1 && cli_streq(argv[1], "--selftest")) {
        return run_selftest();
    }

    srv_puts("notes: start\n");
    body[0] = '\0';
    name[0] = '\0';
    find_text[0] = '\0';
    current_path[0] = '\0';
    current_title[0] = '\0';
    pending_delete[0] = '\0';
    confirm_text[0] = '\0';
    cli_copy(status, sizeof(status), "READY");

    if (ensure_notes_dir() != 0) {
        cli_copy(status, sizeof(status), "NOTES DIR FAILED");
    }

    if (gui2_window_open(&window, WIN, "NOTES",
            260, 330, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("notes: window open failed\n");
        return 1;
    }

    gui2_context_init(&context);
    gui2_list_init(&list, 14, 60, 220, 260);
    gui2_textarea_init(&editor, 246, 60, 456, 260, body, sizeof(body));
    gui2_textarea_set_placeholder(&editor, "SELECT OR CREATE A NOTE");
    gui2_textbox_init(&name_box, 14, 332, 220, 30, name, sizeof(name));
    gui2_textbox_set_placeholder(&name_box, "NOTE TITLE");
    gui2_textbox_init(&find_box, 246, 294, 360, 30, find_text,
        sizeof(find_text));
    gui2_textbox_set_placeholder(&find_box, "FIND IN NOTE");
    gui2_button_init(&find_next, 616, 294, 82, 30, "FIND");
    gui2_button_init(&new_note, 246, 332, 82, 30, "NEW");
    gui2_button_init(&rename, 336, 332, 82, 30, "REN");
    gui2_button_init(&del, 426, 332, 82, 30, "DEL");
    gui2_button_init(&save, 516, 332, 82, 30, "SAVE");
    gui2_button_init(&reload, 606, 332, 82, 30, "RELOAD");
    gui2_dialog_init(&confirm, "CONFIRM", "DELETE?", "DELETE", "CANCEL");
    list.empty_text = "NO NOTES";

    refresh_notes(&list, entries, items, &note_count, 0, &editor, body,
        current_path, sizeof(current_path), current_title, sizeof(current_title),
        status, sizeof(status), &dirty);

    draw_notes(&window, &list, &editor, &name_box, &find_box, &find_next,
        &new_note, &rename, &del, &save, &reload, current_title, status, dirty);
    gui2_dialog_draw(&window, &confirm);
    gui2_window_present_dirty(&window);

    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[] = {
            { GUI2_CONTROL_LIST, &list },
            { GUI2_CONTROL_TEXTAREA, &editor },
            { GUI2_CONTROL_TEXTBOX, &name_box },
            { GUI2_CONTROL_TEXTBOX, &find_box },
            { GUI2_CONTROL_BUTTON, &find_next },
            { GUI2_CONTROL_BUTTON, &new_note },
            { GUI2_CONTROL_BUTTON, &rename },
            { GUI2_CONTROL_BUTTON, &del },
            { GUI2_CONTROL_BUTTON, &save },
            { GUI2_CONTROL_BUTTON, &reload },
        };
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("notes: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("notes: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("notes: close\n");
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
                    if (pending_action == PENDING_DELETE) {
                        int deleted = 0;
                        if (srv_unlink(pending_delete) == 0) {
                            deleted = 1;
                            cli_copy(status, sizeof(status), "DELETED");
                            srv_puts("notes: delete\n");
                        } else {
                            cli_copy(status, sizeof(status), "DELETE FAILED");
                        }
                        refresh_notes(&list, entries, items, &note_count, 0,
                            &editor, body, current_path, sizeof(current_path),
                            current_title, sizeof(current_title), status, sizeof(status),
                            &dirty);
                        if (deleted) {
                            cli_copy(status, sizeof(status), "DELETED");
                        }
                        pending_delete[0] = '\0';
                        pending_action = PENDING_NONE;
                    } else if (pending_action == PENDING_SELECT) {
                        if (pending_index < note_count) {
                            select_note(&list, entries, note_count, pending_index,
                                &editor, body, current_path, sizeof(current_path),
                                current_title, sizeof(current_title), status,
                                sizeof(status), &dirty);
                            srv_puts("notes: select\n");
                        }
                    } else if (pending_action == PENDING_RELOAD) {
                        if (current_path[0] != '\0') {
                            select_note(&list, entries, note_count, list.selected,
                                &editor, body, current_path, sizeof(current_path),
                                current_title, sizeof(current_title), status,
                                sizeof(status), &dirty);
                            srv_puts("notes: reload\n");
                        }
                    } else if (pending_action == PENDING_CLOSE) {
                        closing = 1;
                    } else if (pending_action == PENDING_DELETE_AFTER_DISCARD) {
                        dirty = 0;
                        open_delete_dialog(&confirm, confirm_text,
                            sizeof(confirm_text), current_title);
                        pending_action = PENDING_DELETE;
                        changed = 1;
                        continue;
                    }
                    pending_action = PENDING_NONE;
                    changed = 1;
                }
                if (confirm.secondary_clicks != 0) {
                    confirm.secondary_clicks = 0;
                    gui2_dialog_close(&confirm);
                    if (pending_action == PENDING_SELECT) {
                        restore_list_selection(&list, entries, note_count,
                            current_title);
                    }
                    pending_delete[0] = '\0';
                    pending_action = PENDING_NONE;
                    cli_copy(status, sizeof(status), "CANCELED");
                    changed = 1;
                }
                if (closing) {
                    break;
                }
                continue;
            }
            int result = gui2_dispatch_event(&context, &event,
                controls, sizeof(controls) / sizeof(controls[0]));
            changed |= result;
            if ((result & GUI2_WIDGET_VALUE) != 0 && editor.focused) {
                dirty = 1;
                cli_copy(status, sizeof(status), "DIRTY");
            }
            if ((result & GUI2_WIDGET_VALUE) != 0 && list.focused &&
                note_count != 0 && list.selected < note_count &&
                !cli_streq(entries[list.selected].title, current_title)) {
                if (dirty) {
                    pending_index = list.selected;
                    restore_list_selection(&list, entries, note_count,
                        current_title);
                    open_discard_dialog(&confirm, &pending_action,
                        PENDING_SELECT, "DISCARD CHANGES AND SWITCH NOTES?");
                } else {
                    select_note(&list, entries, note_count, list.selected, &editor,
                        body, current_path, sizeof(current_path), current_title,
                        sizeof(current_title), status, sizeof(status), &dirty);
                    srv_puts("notes: select\n");
                }
                changed = 1;
            }
            if (event.type == GUI2_EVENT_KEY_DOWN && event.key == 7) {
                find_next.clicks++;
                changed = 1;
            }
            if (find_next.clicks != 0) {
                find_next.clicks = 0;
                (void)find_next_in_editor(&editor, find_text, status,
                    sizeof(status));
                changed = 1;
            }
            if (new_note.clicks != 0) {
                char title[TITLE_MAX];
                char path[CLI_PATH_MAX];
                struct srv_stat info;
                new_note.clicks = 0;
                if (dirty) {
                    cli_copy(status, sizeof(status), "SAVE BEFORE NEW");
                } else {
                    if (name_box.length != 0) {
                        sanitize_title(title, sizeof(title), name_box.buffer);
                    } else {
                        make_unique_title(title, sizeof(title), entries, note_count);
                    }
                    note_path_for_title(path, sizeof(path), title);
                    if (srv_stat(path, &info) == 0) {
                        cli_copy(status, sizeof(status), "NOTE EXISTS");
                    } else if (srv_fs_write(path, "", 0) == 0) {
                        reset_textbox(&name_box);
                        refresh_notes(&list, entries, items, &note_count, title,
                            &editor, body, current_path, sizeof(current_path),
                            current_title, sizeof(current_title), status, sizeof(status),
                            &dirty);
                        cli_copy(status, sizeof(status), "CREATED");
                        srv_puts("notes: new\n");
                    } else {
                        cli_copy(status, sizeof(status), "CREATE FAILED");
                    }
                }
                changed = 1;
            }
            if (rename.clicks != 0) {
                char title[TITLE_MAX];
                char path[CLI_PATH_MAX];
                rename.clicks = 0;
                if (current_path[0] == '\0' || name_box.length == 0) {
                    cli_copy(status, sizeof(status), "SELECT AND TITLE REQUIRED");
                } else if (dirty) {
                    cli_copy(status, sizeof(status), "SAVE BEFORE RENAME");
                } else {
                    sanitize_title(title, sizeof(title), name_box.buffer);
                    note_path_for_title(path, sizeof(path), title);
                    if (srv_rename(current_path, path) == 0) {
                        reset_textbox(&name_box);
                        refresh_notes(&list, entries, items, &note_count, title,
                            &editor, body, current_path, sizeof(current_path),
                            current_title, sizeof(current_title), status, sizeof(status),
                            &dirty);
                        cli_copy(status, sizeof(status), "RENAMED");
                        srv_puts("notes: rename\n");
                    } else {
                        cli_copy(status, sizeof(status), "RENAME FAILED");
                    }
                }
                changed = 1;
            }
            if (del.clicks != 0) {
                del.clicks = 0;
                if (current_path[0] == '\0') {
                    cli_copy(status, sizeof(status), "NO NOTE SELECTED");
                } else {
                    cli_copy(pending_delete, sizeof(pending_delete), current_path);
                    if (dirty) {
                        open_discard_dialog(&confirm, &pending_action,
                            PENDING_DELETE_AFTER_DISCARD,
                            "DISCARD CHANGES BEFORE DELETE?");
                    } else {
                        pending_action = PENDING_DELETE;
                        open_delete_dialog(&confirm, confirm_text,
                            sizeof(confirm_text), current_title);
                    }
                }
                changed = 1;
            }
            if (save.clicks != 0) {
                save.clicks = 0;
                if (save_current_note(current_path, body, editor.length, status,
                        sizeof(status), &dirty) == 0) {
                    refresh_notes(&list, entries, items, &note_count, current_title,
                        &editor, body, current_path, sizeof(current_path),
                        current_title, sizeof(current_title), status, sizeof(status),
                        &dirty);
                    cli_copy(status, sizeof(status), "SAVED");
                    srv_puts("notes: save\n");
                } else {
                    srv_puts("notes: save failed\n");
                }
                changed = 1;
            }
            if (reload.clicks != 0) {
                reload.clicks = 0;
                if (current_path[0] != '\0') {
                    if (dirty) {
                        open_discard_dialog(&confirm, &pending_action,
                            PENDING_RELOAD, "DISCARD CHANGES AND RELOAD?");
                    } else {
                        select_note(&list, entries, note_count, list.selected, &editor,
                            body, current_path, sizeof(current_path), current_title,
                            sizeof(current_title), status, sizeof(status), &dirty);
                        srv_puts("notes: reload\n");
                    }
                } else {
                    cli_copy(status, sizeof(status), "NO NOTE SELECTED");
                }
                changed = 1;
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_notes(&window, &list, &editor, &name_box, &find_box,
                &find_next, &new_note, &rename, &del, &save, &reload,
                current_title, status, dirty);
            gui2_dialog_draw(&window, &confirm);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("notes: exited\n");
    return 0;
}
