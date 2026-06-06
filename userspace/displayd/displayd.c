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
#define DISPLAY_CLIENT_MAX 8
#define WINDOW_TITLE_H 24
#define WINDOW_BUTTON_SIZE 12
#define WINDOW_BUTTON_GAP 7
#define WINDOW_RESIZE_GRIP 16
#define WINDOW_MIN_WIDTH 160
#define WINDOW_MIN_HEIGHT 96
#define DISPLAY_LIFECYCLE_SWEEP_TICKS 8
#define DISPLAY_CLOSE_KILL_TICKS 80
#define DISPLAY_CHROME_SCRUB_TICKS 24
#define TASKBAR_BUTTON_MIN_WIDTH 104
#define TASKBAR_BUTTON_MAX_WIDTH 176
#define DISPLAY_SESSION_SHUTDOWN_TICKS 140
#define DISPLAY_KEY_QUEUE_MAX 4
#define DISPLAY_LAUNCH_RECORD_MAX 16
#define DISPLAY_NOTICE_MAX 4
#define DISPLAY_NOTICE_TEXT_MAX 96
#define DISPLAY_NOTICE_TICKS 180
#define DISPLAY_DAMAGE_DEBUG_TICKS 8
#define DISPLAY_APP_NONE UINT64_MAX
#define DISPLAY_APP_MAX 16
#define DISPLAY_APP_ID_MAX 32
#define DISPLAY_APP_LABEL_MAX 32
#define DISPLAY_APP_CATEGORY_MAX 32
#define DISPLAY_APP_ICON_MAX 16
#define DISPLAY_APP_PATH_MAX 128
#define DISPLAY_APP_CONFIG_PATH "/fat/etc/displayd/apps.conf"
#define DISPLAY_APP_CONFIG_MAX 4096
#define DISPLAY_APP_FLAG_HIDDEN 0x1
#define DISPLAY_APP_FLAG_DISABLED 0x2

#define COLOR_DESKTOP_BG 0x0f1417
#define COLOR_TOP_BAR 0x202c30
#define COLOR_DOCK_BG 0x151d20
#define COLOR_PANEL_BG 0x192327
#define COLOR_PANEL_ALT 0x202b30
#define COLOR_PANEL_BORDER 0x50656a
#define COLOR_PANEL_BORDER_DIM 0x36464b
#define COLOR_FRAME_BG 0x161f23
#define COLOR_FRAME_ACTIVE 0x3f6f68
#define COLOR_FRAME_INACTIVE 0x2b3a3f
#define COLOR_FOCUS 0xd6a64a
#define COLOR_TEXT 0xf3f5f0
#define COLOR_TEXT_MUTED 0xa2b0ac
#define COLOR_FIELD 0x0c1114
#define COLOR_DANGER 0x8b4b50

enum display_desktop_focus_kind {
    DISPLAY_DESKTOP_FOCUS_NONE = 0,
    DISPLAY_DESKTOP_FOCUS_LAUNCHER = 1,
    DISPLAY_DESKTOP_FOCUS_EXIT = 2,
    DISPLAY_DESKTOP_FOCUS_TASKBAR = 3,
};

struct display_app {
    char id[DISPLAY_APP_ID_MAX];
    char label[DISPLAY_APP_LABEL_MAX];
    char title[GUI_TEXT_MAX];
    char category[DISPLAY_APP_CATEGORY_MAX];
    char path[DISPLAY_APP_PATH_MAX];
    char icon[DISPLAY_APP_ICON_MAX];
    uint32_t color;
    uint64_t default_width;
    uint64_t default_height;
    uint64_t flags;
    uint64_t order;
};

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

struct display_client {
    int used;
    uint64_t pid;
    uint64_t window_id;
    uint64_t surface_id;
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    int focused;
    int minimized;
    int closing;
    uint64_t close_tick;
    uint64_t z;
    uint64_t app_index;
    char title[GUI_TEXT_MAX];
};

struct display_launch_record {
    int used;
    uint64_t pid;
    uint64_t app_index;
};

struct display_notice {
    int used;
    char text[DISPLAY_NOTICE_TEXT_MAX];
    uint64_t until_ticks;
    uint32_t color;
};

struct display_state {
    struct display_metrics metrics;
    uint64_t gui_messages;
    uint64_t mouse_events;
    uint64_t focused_surface_id;
    uint64_t hovered_surface_id;
    uint64_t dragging_surface_id;
    uint64_t resizing_surface_id;
    int64_t drag_offset_x;
    int64_t drag_offset_y;
    int64_t resize_offset_x;
    int64_t resize_offset_y;
    uint64_t next_z;
    uint64_t launch_count;
    uint64_t last_lifecycle_sweep_ticks;
    uint64_t chrome_scrub_until_ticks;
    int desktop_focus_kind;
    uint64_t desktop_focus_index;
    int shutdown_requested;
    uint64_t shutdown_tick;
    int damage_debug;
    int damage_debug_pulse;
    uint64_t notice_count;
    int64_t last_damage_x;
    int64_t last_damage_y;
    uint64_t last_damage_width;
    uint64_t last_damage_height;
    uint8_t last_buttons;
    struct display_client clients[DISPLAY_CLIENT_MAX];
    struct display_launch_record launches[DISPLAY_LAUNCH_RECORD_MAX];
    struct display_notice notices[DISPLAY_NOTICE_MAX];
};

struct display_key_state {
    int keys[DISPLAY_KEY_QUEUE_MAX];
    uint64_t count;
};

static const struct display_app default_apps[] = {
    { "notes", "NOTES", "NOTES", "Productivity", "/fat/bin/notes", "N", 0x2f6f68, 720, 430, 0, 10 },
    { "fileman", "FILES", "FILE MANAGER", "System", "/fat/bin/fileman", "F", 0x5a7b8c, 720, 430, 0, 20 },
    { "textedit", "EDIT", "TEXT EDIT", "Productivity", "/fat/bin/textedit", "E", 0x3f6c8f, 620, 430, 0, 30 },
    { "paint", "PAINT", "PAINT", "Creative", "/fat/bin/paint", "P", 0x84643d, 720, 520, 0, 40 },
    { "gui2demo", "GUI2", "GUI2 DEMO", "Demos", "/fat/bin/gui2demo", "G2", 0x335b7a, 300, 180, 0, 50 },
    { "surfacedemo", "SURFACE", "SURFACE DEMO", "Demos", "/fat/bin/surfacedemo", "S", 0x60548d, 240, 140, 0, 60 },
    { "calc", "CALC", "CALCULATOR", "Utilities", "/fat/bin/calc", "C", 0x70485f, 280, 220, 0, 70 },
};

static struct display_app apps[DISPLAY_APP_MAX];
static uint64_t display_app_count;

#define DEFAULT_APP_COUNT (sizeof(default_apps) / sizeof(default_apps[0]))
#define DISPLAY_APP_COUNT (display_app_count)

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

static void copy_string(char *to, uint64_t capacity, const char *from) {
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

static const struct display_app *app_at(uint64_t index) {
    return index < DISPLAY_APP_COUNT ? &apps[index] : 0;
}

static int app_is_launcher_visible(const struct display_app *app) {
    return app != 0 && app->id[0] != '\0' && (app->flags & DISPLAY_APP_FLAG_HIDDEN) == 0;
}

static int app_is_launch_enabled(const struct display_app *app) {
    return app_is_launcher_visible(app) && (app->flags & DISPLAY_APP_FLAG_DISABLED) == 0 &&
        app->path[0] != '\0';
}

static uint64_t display_launcher_count(void) {
    uint64_t count = 0;
    for (uint64_t i = 0; i < DISPLAY_APP_COUNT; i++) {
        if (app_is_launcher_visible(&apps[i])) {
            count++;
        }
    }
    return count;
}

static int launcher_app_index(uint64_t launcher_index, uint64_t *app_index) {
    uint64_t seen = 0;
    for (uint64_t i = 0; i < DISPLAY_APP_COUNT; i++) {
        if (!app_is_launcher_visible(&apps[i])) {
            continue;
        }
        if (seen == launcher_index) {
            if (app_index != 0) {
                *app_index = i;
            }
            return 1;
        }
        seen++;
    }
    return 0;
}

static int app_title_matches(const struct display_app *app, const char *title) {
    return app != 0 && title != 0 && title[0] != '\0' &&
        ((app->title[0] != '\0' && streq(app->title, title)) ||
        (app->label[0] != '\0' && streq(app->label, title)));
}

static int find_app_by_title(const char *title, uint64_t *index) {
    if (title == 0 || title[0] == '\0') {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_APP_COUNT; i++) {
        if (app_title_matches(&apps[i], title)) {
            if (index != 0) {
                *index = i;
            }
            return 1;
        }
    }
    return 0;
}

static int find_app_by_id(const char *id, uint64_t *index) {
    if (id == 0 || id[0] == '\0') {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_APP_COUNT; i++) {
        if (streq(apps[i].id, id)) {
            if (index != 0) {
                *index = i;
            }
            return 1;
        }
    }
    return 0;
}

static void remember_launch(struct display_state *state, uint64_t pid, uint64_t app_index) {
    uint64_t free_slot = DISPLAY_LAUNCH_RECORD_MAX;
    if (state == 0 || pid == 0 || app_index >= DISPLAY_APP_COUNT) {
        return;
    }
    for (uint64_t i = 0; i < DISPLAY_LAUNCH_RECORD_MAX; i++) {
        if (state->launches[i].used && state->launches[i].pid == pid) {
            state->launches[i].app_index = app_index;
            return;
        }
        if (!state->launches[i].used && free_slot == DISPLAY_LAUNCH_RECORD_MAX) {
            free_slot = i;
        }
    }
    if (free_slot != DISPLAY_LAUNCH_RECORD_MAX) {
        state->launches[free_slot].used = 1;
        state->launches[free_slot].pid = pid;
        state->launches[free_slot].app_index = app_index;
    }
}

static int find_app_by_pid(const struct display_state *state, uint64_t pid, uint64_t *index) {
    if (state == 0 || pid == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_LAUNCH_RECORD_MAX; i++) {
        if (state->launches[i].used && state->launches[i].pid == pid &&
            state->launches[i].app_index < DISPLAY_APP_COUNT) {
            if (index != 0) {
                *index = state->launches[i].app_index;
            }
            return 1;
        }
    }
    return 0;
}

static void forget_launch_pid(struct display_state *state, uint64_t pid) {
    if (state == 0 || pid == 0) {
        return;
    }
    for (uint64_t i = 0; i < DISPLAY_LAUNCH_RECORD_MAX; i++) {
        if (state->launches[i].used && state->launches[i].pid == pid) {
            memset(&state->launches[i], 0, sizeof(state->launches[i]));
        }
    }
}

static void display_key_push(struct display_key_state *state, int key) {
    if (state == 0 || key == 0 || state->count >= DISPLAY_KEY_QUEUE_MAX) {
        return;
    }
    state->keys[state->count++] = key;
}

static int display_key_pop(struct display_key_state *state) {
    int key;
    if (state == 0 || state->count == 0) {
        return 0;
    }
    key = state->keys[0];
    for (uint64_t i = 1; i < state->count; i++) {
        state->keys[i - 1] = state->keys[i];
    }
    state->count--;
    return key;
}

static int display_read_key(struct display_key_state *state) {
    int key = display_key_pop(state);
    if (key != 0) {
        return key;
    }

    key = kbhit();
    if (key != 27) {
        return key;
    }

    int second = kbhit();
    if (second == 0) {
        return key;
    }
    if (second != '[') {
        display_key_push(state, second);
        return key;
    }

    int third = kbhit();
    if (third == 0) {
        display_key_push(state, second);
        return key;
    }
    if (third == 'Z') {
        return GUI_KEY_BACKTAB;
    }

    display_key_push(state, second);
    display_key_push(state, third);
    return key;
}

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    if (to == 0) {
        return;
    }
    if (from != 0) {
        while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
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

static int is_config_space(char c) {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static char *trim_config_field(char *text) {
    char *end;
    if (text == 0) {
        return text;
    }
    while (is_config_space(*text)) {
        text++;
    }
    end = text;
    while (*end != '\0') {
        end++;
    }
    while (end > text && is_config_space(end[-1])) {
        end--;
    }
    *end = '\0';
    return text;
}

static int parse_u64_config(const char *text, uint64_t *value) {
    char *end = 0;
    unsigned long long parsed;
    if (text == 0 || text[0] == '\0' || value == 0) {
        return 0;
    }
    parsed = strtoull(text, &end, 0);
    if (end == text || (end != 0 && *end != '\0')) {
        return 0;
    }
    *value = (uint64_t)parsed;
    return 1;
}

static int parse_flags_config(char *text, uint64_t *flags) {
    uint64_t parsed = 0;
    if (flags == 0) {
        return 0;
    }
    *flags = 0;
    text = trim_config_field(text);
    if (text == 0 || text[0] == '\0' || streq(text, "none")) {
        return 1;
    }
    if (parse_u64_config(text, &parsed)) {
        *flags = parsed;
        return 1;
    }
    while (text != 0 && text[0] != '\0') {
        char *token = text;
        while (*text != '\0' && *text != ',' && *text != '+') {
            text++;
        }
        if (*text != '\0') {
            *text = '\0';
            text++;
        }
        token = trim_config_field(token);
        if (streq(token, "hidden")) {
            *flags |= DISPLAY_APP_FLAG_HIDDEN;
        } else if (streq(token, "disabled")) {
            *flags |= DISPLAY_APP_FLAG_DISABLED;
        } else if (!streq(token, "none") && token[0] != '\0') {
            return 0;
        }
    }
    return 1;
}

static void sort_app_registry(void) {
    for (uint64_t i = 1; i < DISPLAY_APP_COUNT; i++) {
        struct display_app item = apps[i];
        uint64_t j = i;
        while (j > 0 && apps[j - 1].order > item.order) {
            apps[j] = apps[j - 1];
            j--;
        }
        apps[j] = item;
    }
}

static void copy_default_app_registry(void) {
    uint64_t count = min_u64(DEFAULT_APP_COUNT, DISPLAY_APP_MAX);
    for (uint64_t i = 0; i < count; i++) {
        apps[i] = default_apps[i];
    }
    for (uint64_t i = count; i < DISPLAY_APP_MAX; i++) {
        memset(&apps[i], 0, sizeof(apps[i]));
    }
    display_app_count = count;
    sort_app_registry();
}

static int parse_app_config_line(char *line, struct display_app *app) {
    char *fields[11] = {0};
    char *cursor = line;
    uint64_t field_count = 0;
    uint64_t color_field = 5;
    uint64_t width_field = 6;
    uint64_t height_field = 7;
    uint64_t color = 0;
    uint64_t width = 0;
    uint64_t height = 0;
    uint64_t flags = 0;
    uint64_t order = 0;

    if (line == 0 || app == 0) {
        return 0;
    }
    line = trim_config_field(line);
    if (line[0] == '\0' || line[0] == '#') {
        return 0;
    }
    cursor = line;
    while (cursor != 0 && field_count < 11) {
        char *scan = cursor;
        while (*scan != '\0' && *scan != '|') {
            scan++;
        }
        if (*scan == '|') {
            *scan = '\0';
            fields[field_count++] = trim_config_field(cursor);
            cursor = scan + 1;
        } else {
            fields[field_count++] = trim_config_field(cursor);
            cursor = 0;
        }
    }
    if (field_count < 8) {
        return 0;
    }
    if (field_count >= 11) {
        color_field = 6;
        width_field = 7;
        height_field = 8;
    }
    if (fields[0][0] == '\0' || fields[1][0] == '\0' ||
        fields[2][0] == '\0' || fields[4][0] == '\0') {
        return 0;
    }
    if (!parse_u64_config(fields[color_field], &color) ||
        !parse_u64_config(fields[width_field], &width) ||
        !parse_u64_config(fields[height_field], &height)) {
        return 0;
    }
    if (field_count >= 11 &&
        (!parse_flags_config(fields[9], &flags) || !parse_u64_config(fields[10], &order))) {
        return 0;
    }

    memset(app, 0, sizeof(*app));
    copy_string(app->id, sizeof(app->id), fields[0]);
    copy_string(app->label, sizeof(app->label), fields[1]);
    copy_string(app->title, sizeof(app->title), fields[2]);
    copy_string(app->category, sizeof(app->category), fields[3]);
    copy_string(app->path, sizeof(app->path), fields[4]);
    if (field_count >= 11) {
        copy_string(app->icon, sizeof(app->icon), fields[5]);
    } else {
        char fallback_icon[2] = { fields[1][0], '\0' };
        copy_string(app->icon, sizeof(app->icon), fallback_icon);
    }
    app->color = (uint32_t)color;
    app->default_width = max_u64(width, WINDOW_MIN_WIDTH);
    app->default_height = max_u64(height, WINDOW_MIN_HEIGHT);
    app->flags = flags;
    app->order = order;
    return 1;
}

static void load_app_registry(void) {
    int fd;
    long count;
    uint64_t total = 0;
    uint64_t loaded_count = 0;
    static char buffer[DISPLAY_APP_CONFIG_MAX];
    struct display_app loaded[DISPLAY_APP_MAX];

    copy_default_app_registry();
    fd = (int)srv_open(DISPLAY_APP_CONFIG_PATH);
    if (fd < 0) {
        srv_puts("displayd: app registry defaults count=");
        print_u64(DISPLAY_APP_COUNT);
        srv_puts("\n");
        return;
    }
    while (total + 1 < sizeof(buffer)) {
        count = srv_read(fd, buffer + total, sizeof(buffer) - total - 1);
        if (count <= 0) {
            break;
        }
        total += (uint64_t)count;
    }
    srv_close(fd);
    buffer[total] = '\0';

    memset(loaded, 0, sizeof(loaded));
    char *line = buffer;
    while (line != 0 && *line != '\0' && loaded_count < DISPLAY_APP_MAX) {
        char *next = line;
        while (*next != '\0' && *next != '\n') {
            next++;
        }
        if (*next == '\n') {
            *next = '\0';
            next++;
        } else {
            next = 0;
        }
        if (parse_app_config_line(line, &loaded[loaded_count])) {
            loaded_count++;
        }
        line = next;
    }

    if (loaded_count > 0) {
        for (uint64_t i = 0; i < loaded_count; i++) {
            apps[i] = loaded[i];
        }
        for (uint64_t i = loaded_count; i < DISPLAY_APP_MAX; i++) {
            memset(&apps[i], 0, sizeof(apps[i]));
        }
        display_app_count = loaded_count;
        sort_app_registry();
        srv_puts("displayd: app registry loaded ");
        srv_puts(DISPLAY_APP_CONFIG_PATH);
        srv_puts(" count=");
        print_u64(DISPLAY_APP_COUNT);
        srv_puts("\n");
        return;
    }

    srv_puts("displayd: app registry config empty, defaults count=");
    print_u64(DISPLAY_APP_COUNT);
    srv_puts("\n");
}

static void print_i64(int64_t value) {
    if (value < 0) {
        srv_puts("-");
        print_u64((uint64_t)(-value));
        return;
    }
    print_u64((uint64_t)value);
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

static int64_t work_x(const struct display_metrics *m) {
    return (int64_t)(m->dock_w + m->margin);
}

static int64_t work_y(const struct display_metrics *m) {
    return (int64_t)(m->top_h + m->margin);
}

static uint64_t work_width(const struct display_metrics *m) {
    uint64_t x = (uint64_t)work_x(m);
    return m->width > x + m->margin ? m->width - x - m->margin : 1;
}

static uint64_t work_height(const struct display_metrics *m) {
    uint64_t y = (uint64_t)work_y(m);
    return m->height > y + m->status_h + m->margin ?
        m->height - y - m->status_h - m->margin : 1;
}

static int launcher_rect(const struct display_metrics *m, uint64_t index,
    int64_t *x, int64_t *y, uint64_t *width, uint64_t *height) {
    if (m == 0 || index >= display_launcher_count() || m->dock_w <= 2 * m->margin) {
        return 0;
    }
    if (x != 0) {
        *x = (int64_t)m->margin;
    }
    if (y != 0) {
        *y = (int64_t)(m->top_h + m->margin + index * (m->button_h + m->gap));
    }
    if (width != 0) {
        *width = m->dock_w - 2 * m->margin;
    }
    if (height != 0) {
        *height = m->button_h;
    }
    return 1;
}

static int exit_rect(const struct display_metrics *m,
    int64_t *x, int64_t *y, uint64_t *width, uint64_t *height) {
    uint64_t button_y;
    if (m == 0 || m->dock_w <= 2 * m->margin) {
        return 0;
    }
    button_y = m->top_h + m->margin + display_launcher_count() * (m->button_h + m->gap) + m->gap;
    if (button_y + m->button_h > m->height) {
        return 0;
    }
    if (x != 0) {
        *x = (int64_t)m->margin;
    }
    if (y != 0) {
        *y = (int64_t)button_y;
    }
    if (width != 0) {
        *width = m->dock_w - 2 * m->margin;
    }
    if (height != 0) {
        *height = m->button_h;
    }
    return 1;
}

static int taskbar_rect(const struct display_metrics *m,
    int64_t *x, int64_t *y, uint64_t *width, uint64_t *height) {
    if (m == 0) {
        return 0;
    }
    if (x != 0) {
        *x = work_x(m);
    }
    if (y != 0) {
        *y = (int64_t)(m->height - m->status_h - m->margin);
    }
    if (width != 0) {
        *width = work_width(m);
    }
    if (height != 0) {
        *height = m->status_h;
    }
    return m->height > m->status_h + m->margin;
}

static uint64_t visible_client_count(const struct display_state *state) {
    uint64_t count = 0;
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used) {
            count++;
        }
    }
    return count;
}

static int taskbar_button_rect(const struct display_state *state,
    uint64_t client_index,
    int64_t *x,
    int64_t *y,
    uint64_t *width,
    uint64_t *height) {
    const struct display_metrics *m;
    int64_t bar_x;
    int64_t bar_y;
    uint64_t bar_w;
    uint64_t bar_h;
    uint64_t count;
    uint64_t button_w;
    uint64_t slot = 0;
    uint64_t label_w;
    if (state == 0 || client_index >= DISPLAY_CLIENT_MAX || !state->clients[client_index].used) {
        return 0;
    }
    m = &state->metrics;
    if (!taskbar_rect(m, &bar_x, &bar_y, &bar_w, &bar_h)) {
        return 0;
    }
    label_w = min_u64(180, bar_w / 4);
    if (bar_w <= label_w + 3 * m->gap || bar_h <= 2 * m->gap) {
        return 0;
    }
    count = max_u64(1, visible_client_count(state));
    button_w = (bar_w - label_w - 3 * m->gap) / count;
    button_w = min_u64(TASKBAR_BUTTON_MAX_WIDTH, button_w);
    if (button_w < TASKBAR_BUTTON_MIN_WIDTH && count <= 4) {
        button_w = min_u64(TASKBAR_BUTTON_MIN_WIDTH, bar_w - label_w - 3 * m->gap);
    }
    for (uint64_t i = 0; i < client_index; i++) {
        if (state->clients[i].used) {
            slot++;
        }
    }
    if (x != 0) {
        *x = bar_x + (int64_t)label_w + (int64_t)(2 * m->gap) +
            (int64_t)(slot * (button_w + m->gap));
    }
    if (y != 0) {
        *y = bar_y + (int64_t)m->gap;
    }
    if (width != 0) {
        *width = button_w;
    }
    if (height != 0) {
        *height = bar_h > 2 * m->gap ? bar_h - 2 * m->gap : 1;
    }
    return slot < count;
}

static uint64_t client_frame_width(const struct display_client *client) {
    return client != 0 ? client->width + 2 : 0;
}

static uint64_t client_frame_height(const struct display_client *client) {
    if (client == 0) {
        return 0;
    }
    return client->minimized ? WINDOW_TITLE_H + 2 : client->height + WINDOW_TITLE_H + 2;
}

static int rect_hit(int64_t x, int64_t y, uint64_t width, uint64_t height,
    int64_t px, int64_t py) {
    return px >= x && py >= y &&
        (uint64_t)(px - x) < width &&
        (uint64_t)(py - y) < height;
}

static int close_button_rect(const struct display_client *client,
    int64_t *x, int64_t *y, uint64_t *width, uint64_t *height) {
    if (client == 0 || client_frame_width(client) < 2 * WINDOW_BUTTON_SIZE + 3 * WINDOW_BUTTON_GAP) {
        return 0;
    }
    if (x != 0) {
        *x = client->x + WINDOW_BUTTON_GAP;
    }
    if (y != 0) {
        *y = client->y + (WINDOW_TITLE_H - WINDOW_BUTTON_SIZE) / 2;
    }
    if (width != 0) {
        *width = WINDOW_BUTTON_SIZE;
    }
    if (height != 0) {
        *height = WINDOW_BUTTON_SIZE;
    }
    return 1;
}

static int minimize_button_rect(const struct display_client *client,
    int64_t *x, int64_t *y, uint64_t *width, uint64_t *height) {
    if (client == 0 || client_frame_width(client) < 2 * WINDOW_BUTTON_SIZE + 3 * WINDOW_BUTTON_GAP) {
        return 0;
    }
    if (x != 0) {
        *x = client->x + WINDOW_BUTTON_GAP + WINDOW_BUTTON_SIZE + WINDOW_BUTTON_GAP;
    }
    if (y != 0) {
        *y = client->y + (WINDOW_TITLE_H - WINDOW_BUTTON_SIZE) / 2;
    }
    if (width != 0) {
        *width = WINDOW_BUTTON_SIZE;
    }
    if (height != 0) {
        *height = WINDOW_BUTTON_SIZE;
    }
    return 1;
}

static int client_close_hit(const struct display_client *client, int64_t x, int64_t y) {
    int64_t bx;
    int64_t by;
    uint64_t bw;
    uint64_t bh;
    return close_button_rect(client, &bx, &by, &bw, &bh) &&
        rect_hit(bx, by, bw, bh, x, y);
}

static int client_minimize_hit(const struct display_client *client, int64_t x, int64_t y) {
    int64_t bx;
    int64_t by;
    uint64_t bw;
    uint64_t bh;
    return minimize_button_rect(client, &bx, &by, &bw, &bh) &&
        rect_hit(bx, by, bw, bh, x, y);
}

static int client_title_hit(const struct display_client *client, int64_t x, int64_t y) {
    if (client == 0 || !client->used) {
        return 0;
    }
    return rect_hit(client->x, client->y, client_frame_width(client), WINDOW_TITLE_H, x, y);
}

static int client_resize_hit(const struct display_client *client, int64_t x, int64_t y) {
    uint64_t frame_w;
    uint64_t frame_h;
    if (client == 0 || !client->used || client->minimized) {
        return 0;
    }
    frame_w = client_frame_width(client);
    frame_h = client_frame_height(client);
    if (frame_w < WINDOW_RESIZE_GRIP || frame_h < WINDOW_RESIZE_GRIP) {
        return 0;
    }
    return rect_hit(client->x + (int64_t)(frame_w - WINDOW_RESIZE_GRIP),
        client->y + (int64_t)(frame_h - WINDOW_RESIZE_GRIP),
        WINDOW_RESIZE_GRIP,
        WINDOW_RESIZE_GRIP,
        x,
        y);
}

static int client_frame_hit(const struct display_client *client, int64_t x, int64_t y) {
    if (client == 0 || !client->used) {
        return 0;
    }
    return rect_hit(client->x, client->y, client_frame_width(client),
        client_frame_height(client), x, y);
}

static int launcher_at(const struct display_state *state, int64_t x, int64_t y) {
    const struct display_metrics *m;
    if (state == 0) {
        return -1;
    }
    m = &state->metrics;
    for (uint64_t i = 0; i < display_launcher_count(); i++) {
        int64_t bx;
        int64_t by;
        uint64_t bw;
        uint64_t bh;
        if (launcher_rect(m, i, &bx, &by, &bw, &bh) && rect_hit(bx, by, bw, bh, x, y)) {
            uint64_t app_index = 0;
            return launcher_app_index(i, &app_index) ? (int)app_index : -1;
        }
    }
    return -1;
}

static int exit_hit(const struct display_state *state, int64_t x, int64_t y) {
    int64_t bx;
    int64_t by;
    uint64_t bw;
    uint64_t bh;
    return state != 0 &&
        exit_rect(&state->metrics, &bx, &by, &bw, &bh) &&
        rect_hit(bx, by, bw, bh, x, y);
}

static struct display_client *taskbar_client_at(struct display_state *state, int64_t x, int64_t y) {
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        int64_t bx;
        int64_t by;
        uint64_t bw;
        uint64_t bh;
        if (!state->clients[i].used) {
            continue;
        }
        if (taskbar_button_rect(state, i, &bx, &by, &bw, &bh) &&
            rect_hit(bx, by, bw, bh, x, y)) {
            return &state->clients[i];
        }
    }
    return 0;
}

static struct display_client *find_client_for_surface_create(struct display_state *state,
    uint64_t pid,
    uint64_t window_id,
    uint64_t surface_id) {
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *client = &state->clients[i];
        if (!client->used) {
            continue;
        }
        if (surface_id != 0 && client->surface_id == surface_id) {
            return client;
        }
        if (client->pid == pid && client->window_id == window_id) {
            return client;
        }
    }
    return 0;
}

static struct display_client *alloc_client(struct display_state *state) {
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (!state->clients[i].used) {
            return &state->clients[i];
        }
    }
    return 0;
}

static struct display_client *find_client_by_surface(struct display_state *state, uint64_t surface_id) {
    if (surface_id == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used && state->clients[i].surface_id == surface_id) {
            return &state->clients[i];
        }
    }
    return 0;
}

static uint64_t mapped_client_count(const struct display_state *state) {
    uint64_t count = 0;
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used) {
            count++;
        }
    }
    return count;
}

static void send_client_event(const struct display_client *client, uint64_t type,
    int64_t x, int64_t y, uint64_t width, uint64_t height, int64_t value, const char *text) {
    struct gui_message event;
    if (client == 0 || !client->used || client->pid == 0) {
        return;
    }
    memset(&event, 0, sizeof(event));
    event.type = type;
    event.target_pid = client->pid;
    event.window_id = client->window_id;
    event.control_id = client->surface_id;
    event.x = x;
    event.y = y;
    event.width = width;
    event.height = height;
    event.value = value;
    copy_text(event.text, text);
    gui_send(&event);
}

static void mark_client_frame_dirty(struct ui_element *root, const struct display_client *client) {
    if (root == 0 || client == 0 || !client->used) {
        return;
    }
    ui_mark_dirty_rect(root,
        client->x,
        client->y,
        client_frame_width(client),
        client_frame_height(client));
}

static void mark_taskbar_dirty(struct ui_element *root, const struct display_state *state) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (root == 0 || state == 0 ||
        !taskbar_rect(&state->metrics, &x, &y, &width, &height)) {
        return;
    }
    ui_mark_dirty_rect(root, x, y, width, height);
}

static void mark_dock_dirty(struct ui_element *root, const struct display_state *state) {
    if (root == 0 || state == 0 || state->metrics.height <= state->metrics.top_h) {
        return;
    }
    ui_mark_dirty_rect(root, 0, state->metrics.top_h, state->metrics.dock_w,
        state->metrics.height - state->metrics.top_h);
}

static void mark_chrome_dirty(struct ui_element *root, const struct display_state *state) {
    mark_dock_dirty(root, state);
    mark_taskbar_dirty(root, state);
}

static int notices_rect(const struct display_metrics *m, int64_t *x, int64_t *y,
    uint64_t *width, uint64_t *height) {
    uint64_t rect_width;
    uint64_t rect_height;
    int64_t tx;
    int64_t ty;
    uint64_t tw;
    uint64_t th;
    if (m == 0 || m->width == 0 || m->height == 0) {
        return 0;
    }
    rect_width = clamp_u64(m->width / 3, 260, 480);
    rect_height = DISPLAY_NOTICE_MAX * 34 + 8;
    if (taskbar_rect(m, &tx, &ty, &tw, &th) && ty > (int64_t)(rect_height + m->gap)) {
        *y = ty - (int64_t)(rect_height + m->gap);
    } else {
        *y = (int64_t)(m->top_h + m->gap);
    }
    *x = (int64_t)(m->width > rect_width + m->gap ? m->width - rect_width - m->gap : m->gap);
    *width = rect_width;
    *height = rect_height;
    return 1;
}

static void mark_notices_dirty(struct ui_element *root, const struct display_state *state) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (root == 0 || state == 0 ||
        !notices_rect(&state->metrics, &x, &y, &width, &height)) {
        return;
    }
    ui_mark_dirty_rect(root, x, y, width, height);
}

static void request_chrome_scrub(struct display_state *state) {
    uint64_t until;
    if (state == 0) {
        return;
    }
    until = (uint64_t)srv_ticks() + DISPLAY_CHROME_SCRUB_TICKS;
    if (state->chrome_scrub_until_ticks < until) {
        state->chrome_scrub_until_ticks = until;
    }
}

static void scrub_chrome_if_needed(struct ui_element *root, struct display_state *state) {
    if (root == 0 || state == 0) {
        return;
    }
    if ((uint64_t)srv_ticks() <= state->chrome_scrub_until_ticks) {
        mark_chrome_dirty(root, state);
    }
}

static void add_notice(struct ui_element *root, struct display_state *state,
    const char *text, uint32_t color) {
    uint64_t slot = DISPLAY_NOTICE_MAX;
    uint64_t now;
    if (root == 0 || state == 0 || text == 0 || text[0] == '\0') {
        return;
    }
    now = (uint64_t)srv_ticks();
    for (uint64_t i = 0; i < DISPLAY_NOTICE_MAX; i++) {
        if (!state->notices[i].used || state->notices[i].until_ticks <= now) {
            slot = i;
            break;
        }
    }
    if (slot == DISPLAY_NOTICE_MAX) {
        slot = 0;
        for (uint64_t i = 1; i < DISPLAY_NOTICE_MAX; i++) {
            if (state->notices[i].until_ticks < state->notices[slot].until_ticks) {
                slot = i;
            }
        }
    }
    state->notices[slot].used = 1;
    copy_string(state->notices[slot].text, sizeof(state->notices[slot].text), text);
    state->notices[slot].until_ticks = now + DISPLAY_NOTICE_TICKS;
    state->notices[slot].color = color;
    state->notice_count++;
    mark_notices_dirty(root, state);
}

static void sweep_notices(struct ui_element *root, struct display_state *state) {
    uint64_t now;
    int changed = 0;
    if (root == 0 || state == 0) {
        return;
    }
    now = (uint64_t)srv_ticks();
    for (uint64_t i = 0; i < DISPLAY_NOTICE_MAX; i++) {
        if (state->notices[i].used && state->notices[i].until_ticks <= now) {
            memset(&state->notices[i], 0, sizeof(state->notices[i]));
            changed = 1;
        }
    }
    if (changed) {
        mark_notices_dirty(root, state);
    }
}

static int process_state_is_running(const char *state) {
    if (state == 0 || state[0] == '\0') {
        return 0;
    }
    return !streq(state, "exited");
}

static int process_is_running(uint64_t pid) {
    uint64_t index = 0;
    struct srv_process_info info;
    if (pid == 0) {
        return 0;
    }
    while ((index = (uint64_t)srv_proc_list(index, &info)) != 0) {
        if (info.pid == pid) {
            return process_state_is_running(info.state);
        }
    }
    return 0;
}

static int client_pid_present(const struct display_state *state, uint64_t pid) {
    if (state == 0 || pid == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used && state->clients[i].pid == pid) {
            return 1;
        }
    }
    return 0;
}

static uint64_t pending_launch_count(const struct display_state *state) {
    uint64_t count = 0;
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_LAUNCH_RECORD_MAX; i++) {
        if (state->launches[i].used && !client_pid_present(state, state->launches[i].pid)) {
            count++;
        }
    }
    return count;
}

static uint64_t launch_reservation_count(const struct display_state *state) {
    return mapped_client_count(state) + pending_launch_count(state);
}

static void sweep_unmapped_launches(struct display_state *state) {
    if (state == 0) {
        return;
    }
    for (uint64_t i = 0; i < DISPLAY_LAUNCH_RECORD_MAX; i++) {
        uint64_t status = 0;
        long waited;
        if (!state->launches[i].used || client_pid_present(state, state->launches[i].pid)) {
            continue;
        }
        waited = srv_wait(state->launches[i].pid, &status, SRV_WAIT_NOHANG);
        if (waited == (long)state->launches[i].pid ||
            (waited < 0 && !process_is_running(state->launches[i].pid))) {
            srv_puts("displayd: launch dropped pid=");
            print_u64(state->launches[i].pid);
            srv_puts("\n");
            memset(&state->launches[i], 0, sizeof(state->launches[i]));
        }
    }
}

static void log_client_title(const struct display_client *client) {
    srv_puts(client != 0 && client->title[0] != '\0' ? client->title : "SURFACE");
}

static void append_client_title(char *text, uint64_t capacity, uint64_t *length,
    const struct display_client *client) {
    append_text(text, capacity, length,
        client != 0 && client->title[0] != '\0' ? client->title : "SURFACE");
}

static void notify_client_exit(struct ui_element *root, struct display_state *state,
    const struct display_client *client, const char *reason, uint64_t status, int has_status) {
    char text[DISPLAY_NOTICE_TEXT_MAX] = {0};
    uint64_t length = 0;
    append_client_title(text, sizeof(text), &length, client);
    append_text(text, sizeof(text), &length, " ");
    append_text(text, sizeof(text), &length, reason != 0 ? reason : "exited");
    if (has_status) {
        append_text(text, sizeof(text), &length, " status ");
        append_u64(text, sizeof(text), &length, status);
    }
    srv_puts("displayd: notice ");
    srv_puts(text);
    srv_puts("\n");
    add_notice(root, state, text, COLOR_DANGER);
}

static const struct display_app *client_app(const struct display_client *client) {
    if (client == 0 || client->app_index >= DISPLAY_APP_COUNT) {
        return 0;
    }
    return &apps[client->app_index];
}

static uint32_t client_accent_color(const struct display_client *client) {
    const struct display_app *app = client_app(client);
    return app != 0 ? app->color : 0x2f6f68;
}

static int desktop_focus_valid(const struct display_state *state, int kind, uint64_t index) {
    if (state == 0) {
        return 0;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_NONE) {
        return 1;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_LAUNCHER) {
        return index < display_launcher_count();
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_EXIT) {
        return 1;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_TASKBAR) {
        return index < DISPLAY_CLIENT_MAX && state->clients[index].used;
    }
    return 0;
}

static uint64_t desktop_focus_total(const struct display_state *state) {
    uint64_t count = display_launcher_count() + 1;
    if (state == 0) {
        return count;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used) {
            count++;
        }
    }
    return count;
}

static int64_t desktop_focus_to_ordinal(const struct display_state *state,
    int kind, uint64_t index) {
    uint64_t launcher_count = display_launcher_count();
    uint64_t ordinal = launcher_count + 1;
    if (!desktop_focus_valid(state, kind, index) || kind == DISPLAY_DESKTOP_FOCUS_NONE) {
        return -1;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_LAUNCHER) {
        return (int64_t)index;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_EXIT) {
        return (int64_t)launcher_count;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (!state->clients[i].used) {
            continue;
        }
        if (kind == DISPLAY_DESKTOP_FOCUS_TASKBAR && i == index) {
            return (int64_t)ordinal;
        }
        ordinal++;
    }
    return -1;
}

static int desktop_focus_from_ordinal(const struct display_state *state,
    uint64_t ordinal, int *kind, uint64_t *index) {
    uint64_t launcher_count = display_launcher_count();
    uint64_t task_ordinal = launcher_count + 1;
    if (kind == 0 || index == 0) {
        return 0;
    }
    if (ordinal < launcher_count) {
        *kind = DISPLAY_DESKTOP_FOCUS_LAUNCHER;
        *index = ordinal;
        return 1;
    }
    if (ordinal == launcher_count) {
        *kind = DISPLAY_DESKTOP_FOCUS_EXIT;
        *index = 0;
        return 1;
    }
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (!state->clients[i].used) {
            continue;
        }
        if (task_ordinal == ordinal) {
            *kind = DISPLAY_DESKTOP_FOCUS_TASKBAR;
            *index = i;
            return 1;
        }
        task_ordinal++;
    }
    return 0;
}

static void set_desktop_focus(struct ui_element *root, struct display_state *state,
    int kind, uint64_t index) {
    if (root == 0 || state == 0) {
        return;
    }
    if (!desktop_focus_valid(state, kind, index)) {
        kind = DISPLAY_DESKTOP_FOCUS_NONE;
        index = 0;
    }
    if (state->desktop_focus_kind == kind && state->desktop_focus_index == index) {
        return;
    }
    state->desktop_focus_kind = kind;
    state->desktop_focus_index = index;
    mark_dock_dirty(root, state);
    mark_taskbar_dirty(root, state);
    if (kind == DISPLAY_DESKTOP_FOCUS_LAUNCHER) {
        uint64_t app_index = 0;
        srv_puts("displayd: desktop focus launcher ");
        if (launcher_app_index(index, &app_index)) {
            srv_puts(apps[app_index].label);
        } else {
            srv_puts("UNKNOWN");
        }
        srv_puts("\n");
    } else if (kind == DISPLAY_DESKTOP_FOCUS_EXIT) {
        srv_puts("displayd: desktop focus exit\n");
    } else if (kind == DISPLAY_DESKTOP_FOCUS_TASKBAR) {
        srv_puts("displayd: desktop focus taskbar ");
        log_client_title(&state->clients[index]);
        srv_puts("\n");
    }
}

static void desktop_focus_step(struct ui_element *root, struct display_state *state,
    int reverse) {
    uint64_t total = desktop_focus_total(state);
    int64_t current;
    uint64_t next;
    int kind;
    uint64_t index;
    if (root == 0 || state == 0 || total == 0) {
        return;
    }
    current = desktop_focus_to_ordinal(state, state->desktop_focus_kind,
        state->desktop_focus_index);
    if (current < 0) {
        next = reverse ? total - 1 : 0;
    } else if (reverse) {
        next = current == 0 ? total - 1 : (uint64_t)current - 1;
    } else {
        next = ((uint64_t)current + 1) % total;
    }
    if (desktop_focus_from_ordinal(state, next, &kind, &index)) {
        set_desktop_focus(root, state, kind, index);
    }
}

static void remove_client(struct ui_element *root, struct display_state *state,
    struct display_client *client, const char *reason, uint64_t status, int has_status) {
    uint64_t surface_id;
    uint64_t client_index;
    if (root == 0 || state == 0 || client == 0 || !client->used) {
        return;
    }
    surface_id = client->surface_id;
    client_index = (uint64_t)(client - state->clients);
    mark_client_frame_dirty(root, client);
    if (state->focused_surface_id == surface_id) {
        state->focused_surface_id = 0;
    }
    if (state->hovered_surface_id == surface_id) {
        state->hovered_surface_id = 0;
    }
    if (state->dragging_surface_id == surface_id) {
        state->dragging_surface_id = 0;
    }
    if (state->resizing_surface_id == surface_id) {
        state->resizing_surface_id = 0;
    }
    if (state->desktop_focus_kind == DISPLAY_DESKTOP_FOCUS_TASKBAR &&
        state->desktop_focus_index == client_index) {
        set_desktop_focus(root, state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
    }
    srv_puts("displayd: remove ");
    log_client_title(client);
    srv_puts(" reason=");
    srv_puts(reason != 0 ? reason : "unknown");
    srv_puts(" pid=");
    print_u64(client->pid);
    if (has_status) {
        srv_puts(" status=");
        print_u64(status);
    }
    srv_puts("\n");
    if ((streq(reason != 0 ? reason : "", "gone")) ||
        (streq(reason != 0 ? reason : "", "exit") && (!client->closing || status != 0))) {
        notify_client_exit(root, state, client, reason, status, has_status);
    }
    memset(client, 0, sizeof(*client));
    ui_mark_dirty_rect(root, 0, state->metrics.top_h, state->metrics.width,
        state->metrics.height - state->metrics.top_h);
    request_chrome_scrub(state);
}

static void raise_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    if (client == 0 || !client->used) {
        return;
    }
    state->next_z++;
    if (state->next_z == 0) {
        state->next_z = 1;
    }
    if (client->z != state->next_z) {
        client->z = state->next_z;
        mark_client_frame_dirty(root, client);
    }
}

static void focus_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    if (client != 0 && (!client->used || client->surface_id == 0)) {
        client = 0;
    }
    uint64_t next_id = client != 0 ? client->surface_id : 0;
    if (client != 0 && state->desktop_focus_kind != DISPLAY_DESKTOP_FOCUS_NONE) {
        set_desktop_focus(root, state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
    }
    if (state->focused_surface_id == next_id) {
        return;
    }

    struct display_client *old = find_client_by_surface(state, state->focused_surface_id);
    if (old != 0) {
        old->focused = 0;
        mark_client_frame_dirty(root, old);
        mark_taskbar_dirty(root, state);
        send_client_event(old, GUI_MSG_V2_EVENT_FOCUS, 0, 0,
            old->width, old->height, 0, "");
    }

    state->focused_surface_id = next_id;
    if (client != 0) {
        raise_client(root, state, client);
        client->focused = 1;
        mark_client_frame_dirty(root, client);
        mark_taskbar_dirty(root, state);
        srv_puts("displayd: focus ");
        srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
        srv_puts("\n");
        request_chrome_scrub(state);
        send_client_event(client, GUI_MSG_V2_EVENT_FOCUS, 0, 0,
            client->width, client->height, 1, "");
    }
}

static struct display_client *client_at(struct display_state *state,
    int64_t x, int64_t y, int64_t *local_x, int64_t *local_y) {
    struct display_client *top = 0;
    uint64_t top_z = 0;
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *client = &state->clients[i];
        if (!client->used || client->minimized) {
            continue;
        }
        int64_t content_x = client->x + 1;
        int64_t content_y = client->y + WINDOW_TITLE_H + 1;
        if (x >= content_x && y >= content_y &&
            (uint64_t)(x - content_x) < client->width &&
            (uint64_t)(y - content_y) < client->height &&
            (top == 0 || client->z >= top_z)) {
            top = client;
            top_z = client->z;
        }
    }
    if (top != 0) {
        if (local_x != 0) {
            *local_x = x - (top->x + 1);
        }
        if (local_y != 0) {
            *local_y = y - (top->y + WINDOW_TITLE_H + 1);
        }
    }
    return top;
}

static struct display_client *client_frame_at(struct display_state *state, int64_t x, int64_t y) {
    struct display_client *top = 0;
    uint64_t top_z = 0;
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *client = &state->clients[i];
        if (!client->used) {
            continue;
        }
        if (client_frame_hit(client, x, y) && (top == 0 || client->z >= top_z)) {
            top = client;
            top_z = client->z;
        }
    }
    return top;
}

static int64_t clamp_i64(int64_t value, int64_t low, int64_t high) {
    if (value < low) {
        return low;
    }
    if (value > high) {
        return high;
    }
    return value;
}

static void clamp_client_position(const struct ui_element *root, struct display_client *client) {
    int64_t max_x;
    int64_t max_y;
    const struct display_state *state;
    const struct display_metrics *m;
    int64_t min_x;
    int64_t min_y;
    if (root == 0 || client == 0) {
        return;
    }
    state = (const struct display_state *)root->userdata;
    m = state != 0 ? &state->metrics : 0;
    min_x = m != 0 ? work_x(m) : 0;
    min_y = m != 0 ? work_y(m) : 0;
    max_x = m != 0 ? min_x + (int64_t)work_width(m) -
        (int64_t)min_u64(client_frame_width(client), work_width(m)) :
        (int64_t)root->width - (int64_t)min_u64(client_frame_width(client), root->width);
    max_y = m != 0 ? min_y + (int64_t)work_height(m) -
        (int64_t)min_u64(client_frame_height(client), work_height(m)) :
        (int64_t)root->height - (int64_t)min_u64(client_frame_height(client), root->height);
    client->x = clamp_i64(client->x, min_x, max_x);
    client->y = clamp_i64(client->y, min_y, max_y);
}

static int client_frames_overlap(const struct display_client *a, const struct display_client *b) {
    int64_t ax2;
    int64_t ay2;
    int64_t bx2;
    int64_t by2;
    if (a == 0 || b == 0 || !a->used || !b->used || a == b) {
        return 0;
    }
    ax2 = a->x + (int64_t)client_frame_width(a);
    ay2 = a->y + (int64_t)client_frame_height(a);
    bx2 = b->x + (int64_t)client_frame_width(b);
    by2 = b->y + (int64_t)client_frame_height(b);
    return a->x < bx2 && ax2 > b->x && a->y < by2 && ay2 > b->y;
}

static int client_overlaps_any(const struct display_state *state,
    const struct display_client *client) {
    if (state == 0 || client == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (client_frames_overlap(client, &state->clients[i])) {
            return 1;
        }
    }
    return 0;
}

static int64_t position_between(int64_t low, int64_t high, uint64_t numerator,
    uint64_t denominator) {
    if (denominator == 0 || high <= low) {
        return low;
    }
    return low + (int64_t)(((uint64_t)(high - low) * numerator) / denominator);
}

static void choose_managed_client_position(const struct display_metrics *m,
    const struct display_client *client, int64_t *x, int64_t *y) {
    const struct display_app *app = client_app(client);
    uint64_t frame_w;
    uint64_t frame_h;
    int64_t min_x;
    int64_t min_y;
    int64_t max_x;
    int64_t max_y;
    if (m == 0 || client == 0 || x == 0 || y == 0) {
        return;
    }

    frame_w = min_u64(client_frame_width(client), work_width(m));
    frame_h = min_u64(client_frame_height(client), work_height(m));
    min_x = work_x(m) + (int64_t)m->gap;
    min_y = work_y(m) + (int64_t)m->gap;
    max_x = work_x(m) + (int64_t)work_width(m) - (int64_t)frame_w -
        (int64_t)m->gap;
    max_y = work_y(m) + (int64_t)work_height(m) - (int64_t)frame_h -
        (int64_t)m->gap;
    if (max_x < min_x) {
        max_x = min_x;
    }
    if (max_y < min_y) {
        max_y = min_y;
    }

    if (app != 0 && streq(app->id, "notes")) {
        *x = position_between(min_x, max_x, 0, 1);
        *y = position_between(min_y, max_y, 4, 5);
    } else if (app != 0 && streq(app->id, "fileman")) {
        *x = position_between(min_x, max_x, 1, 1);
        *y = position_between(min_y, max_y, 0, 1);
    } else if (app != 0 && streq(app->id, "textedit")) {
        *x = position_between(min_x, max_x, 1, 6);
        *y = position_between(min_y, max_y, 1, 8);
    } else if (app != 0 && streq(app->id, "paint")) {
        *x = position_between(min_x, max_x, 1, 2);
        *y = position_between(min_y, max_y, 3, 4);
    } else if (app != 0 && streq(app->id, "gui2demo")) {
        *x = position_between(min_x, max_x, 1, 1);
        *y = position_between(min_y, max_y, 1, 5);
    } else if (app != 0 && streq(app->id, "surfacedemo")) {
        *x = position_between(min_x, max_x, 1, 1);
        *y = position_between(min_y, max_y, 3, 5);
    } else if (app != 0 && streq(app->id, "calc")) {
        *x = position_between(min_x, max_x, 1, 1);
        *y = position_between(min_y, max_y, 1, 1);
    } else {
        uint64_t app_index = client->app_index < DISPLAY_APP_COUNT ? client->app_index : 0;
        *x = position_between(min_x, max_x, app_index % 3, 2);
        *y = position_between(min_y, max_y, (app_index / 3) % 2, 1);
    }
}

static int choose_default_client_position(const struct ui_element *root,
    const struct display_state *state, struct display_client *client, int force_managed) {
    const struct display_metrics *m;
    uint64_t app_index;
    uint64_t work_w;
    uint64_t work_h;
    uint64_t frame_w;
    uint64_t frame_h;
    int64_t x;
    int64_t y;
    if (root == 0 || state == 0 || client == 0) {
        return 0;
    }
    m = &state->metrics;
    x = client->x;
    y = client->y;
    if (!force_managed && x >= work_x(m) && y >= work_y(m) &&
        x < work_x(m) + (int64_t)work_width(m) &&
        y < work_y(m) + (int64_t)work_height(m)) {
        return 0;
    }
    if (force_managed) {
        choose_managed_client_position(m, client, &client->x, &client->y);
        clamp_client_position(root, client);
        return 1;
    }
    app_index = client->app_index < DISPLAY_APP_COUNT ? client->app_index : 0;
    work_w = work_width(m);
    work_h = work_height(m);
    frame_w = min_u64(client_frame_width(client), work_w);
    frame_h = min_u64(client_frame_height(client), work_h);
    client->x = work_x(m) + (int64_t)m->gap +
        (int64_t)((work_w > frame_w + 2 * m->gap) ?
            ((work_w - frame_w - 2 * m->gap) * (app_index % 3)) / 2 : 0);
    client->y = work_y(m) + (int64_t)m->gap +
        (int64_t)((work_h > frame_h + 2 * m->gap) ?
            ((work_h - frame_h - 2 * m->gap) * ((app_index / 3) % 2)) : 0);
    clamp_client_position(root, client);
    return 0;
}

static void place_new_client(struct ui_element *root, struct display_state *state,
    struct display_client *client, int force_managed) {
    uint64_t same_title = 0;
    int managed;
    if (root == 0 || state == 0 || client == 0) {
        return;
    }
    managed = choose_default_client_position(root, state, client, force_managed);
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *other = &state->clients[i];
        if (other != client && other->used && streq(other->title, client->title)) {
            same_title++;
        }
    }
    if (same_title != 0) {
        int64_t offset = (int64_t)(same_title * 28);
        client->x += offset;
        client->y += offset;
    }
    clamp_client_position(root, client);
    if (managed) {
        goto done;
    }
    for (uint64_t attempts = 0; attempts < DISPLAY_CLIENT_MAX * 2 &&
        client_overlaps_any(state, client); attempts++) {
        int64_t old_x = client->x;
        int64_t old_y = client->y;
        client->x += 32;
        client->y += 28;
        clamp_client_position(root, client);
        if (client->x == old_x && client->y == old_y && client_overlaps_any(state, client)) {
            const struct display_metrics *m = &state->metrics;
            uint64_t frame_w = min_u64(client_frame_width(client), work_width(m));
            uint64_t frame_h = min_u64(client_frame_height(client), work_height(m));
            uint64_t span_x = work_width(m) > frame_w + 2 * m->gap ?
                work_width(m) - frame_w - 2 * m->gap : 1;
            uint64_t span_y = work_height(m) > frame_h + 2 * m->gap ?
                work_height(m) - frame_h - 2 * m->gap : 1;
            uint64_t seed = (client->app_index < DISPLAY_APP_COUNT ? client->app_index : 0) +
                attempts + 1;
            client->x = work_x(m) + (int64_t)m->gap + (int64_t)((seed * 47) % span_x);
            client->y = work_y(m) + (int64_t)m->gap + (int64_t)((seed * 31) % span_y);
            clamp_client_position(root, client);
        }
    }
done:
    srv_puts("displayd: place ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts(" x=");
    print_i64(client->x);
    srv_puts(" y=");
    print_i64(client->y);
    srv_puts("\n");
}

static void launch_app(struct ui_element *root, struct display_state *state, uint64_t index) {
    long pid;
    const struct display_app *app;
    if (root == 0 || state == 0 || index >= DISPLAY_APP_COUNT) {
        return;
    }
    app = app_at(index);
    if (!app_is_launch_enabled(app)) {
        srv_puts("displayd: launcher ");
        srv_puts(app != 0 ? app->label : "UNKNOWN");
        srv_puts(" unavailable\n");
        return;
    }
    if (launch_reservation_count(state) >= DISPLAY_CLIENT_MAX) {
        char notice[DISPLAY_NOTICE_TEXT_MAX] = {0};
        uint64_t length = 0;
        append_text(notice, sizeof(notice), &length, "workspace full: ");
        append_text(notice, sizeof(notice), &length, app->label);
        srv_puts("displayd: launcher ");
        srv_puts(app->label);
        srv_puts(" full\n");
        add_notice(root, state, notice, COLOR_FOCUS);
        ui_mark_dirty_rect(root, 0, state->metrics.top_h, state->metrics.dock_w,
            state->metrics.height - state->metrics.top_h);
        request_chrome_scrub(state);
        return;
    }
    pid = srv_spawn_bg(app->path);
    srv_puts("displayd: launch ");
    srv_puts(app->label);
    srv_puts(" ");
    srv_puts(app->path);
    srv_puts(" pid=");
    if (pid < 0) {
        srv_puts("failed\n");
    } else {
        print_u64((uint64_t)pid);
        srv_puts("\n");
        remember_launch(state, (uint64_t)pid, index);
        state->launch_count++;
    }
    ui_mark_dirty_rect(root, 0, state->metrics.top_h, state->metrics.dock_w,
        state->metrics.height - state->metrics.top_h);
    request_chrome_scrub(state);
}

static void move_client_to(struct ui_element *root, struct display_client *client,
    int64_t x, int64_t y) {
    if (root == 0 || client == 0 || !client->used) {
        return;
    }
    mark_client_frame_dirty(root, client);
    client->x = x;
    client->y = y;
    clamp_client_position(root, client);
    mark_client_frame_dirty(root, client);
    srv_puts("displayd: drag ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts(" x=");
    print_i64(client->x);
    srv_puts(" y=");
    print_i64(client->y);
    srv_puts("\n");
    request_chrome_scrub((struct display_state *)root->userdata);
}

static void resize_client_to(struct ui_element *root, struct display_state *state,
    struct display_client *client, uint64_t width, uint64_t height) {
    const struct display_metrics *m;
    uint64_t max_width;
    uint64_t max_height;
    if (root == 0 || state == 0 || client == 0 || !client->used || client->minimized) {
        return;
    }
    m = &state->metrics;
    max_width = work_width(m);
    max_height = work_height(m) > WINDOW_TITLE_H + 2 ?
        work_height(m) - WINDOW_TITLE_H - 2 : WINDOW_MIN_HEIGHT;
    if (client->x > work_x(m)) {
        uint64_t used_x = (uint64_t)(client->x - work_x(m));
        if (used_x < max_width) {
            max_width -= used_x;
        }
    }
    if (client->y > work_y(m)) {
        uint64_t used_y = (uint64_t)(client->y - work_y(m));
        if (used_y < max_height) {
            max_height -= used_y;
        }
    }
    max_width = max_u64(WINDOW_MIN_WIDTH, max_width > 2 ? max_width - 2 : max_width);
    max_height = max_u64(WINDOW_MIN_HEIGHT, max_height);
    width = clamp_u64(width, WINDOW_MIN_WIDTH, max_width);
    height = clamp_u64(height, WINDOW_MIN_HEIGHT, max_height);
    if (client->width == width && client->height == height) {
        return;
    }
    mark_client_frame_dirty(root, client);
    client->width = width;
    client->height = height;
    clamp_client_position(root, client);
    mark_client_frame_dirty(root, client);
    srv_puts("displayd: resize ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts(" ");
    print_u64(client->width);
    srv_puts("x");
    print_u64(client->height);
    srv_puts("\n");
    request_chrome_scrub(state);
    send_client_event(client, GUI_MSG_V2_EVENT_CONFIGURE,
        client->x,
        client->y,
        client->width,
        client->height,
        0,
        "");
}

static void toggle_client_minimized(struct ui_element *root, struct display_client *client) {
    if (root == 0 || client == 0 || !client->used) {
        return;
    }
    mark_client_frame_dirty(root, client);
    client->minimized = !client->minimized;
    clamp_client_position(root, client);
    mark_client_frame_dirty(root, client);
    mark_taskbar_dirty(root, (const struct display_state *)root->userdata);
    srv_puts("displayd: minimize ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts(" state=");
    print_u64(client->minimized ? 1 : 0);
    srv_puts("\n");
}

static void activate_taskbar_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    if (root == 0 || state == 0 || client == 0 || !client->used) {
        return;
    }
    if (client->minimized) {
        mark_client_frame_dirty(root, client);
        client->minimized = 0;
        clamp_client_position(root, client);
        mark_client_frame_dirty(root, client);
        srv_puts("displayd: taskbar restore ");
    } else {
        srv_puts("displayd: taskbar focus ");
    }
    log_client_title(client);
    srv_puts("\n");
    focus_client(root, state, client);
}

static void close_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    if (client == 0 || !client->used) {
        return;
    }
    if (client->closing) {
        return;
    }
    client->closing = 1;
    client->close_tick = (uint64_t)srv_ticks();
    send_client_event(client, GUI_MSG_EVENT_CLOSE, 0, 0, client->width, client->height, 0, "");
    srv_puts("displayd: close ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts("\n");
    if (state->dragging_surface_id == client->surface_id) {
        state->dragging_surface_id = 0;
    }
    if (state->resizing_surface_id == client->surface_id) {
        state->resizing_surface_id = 0;
    }
    mark_client_frame_dirty(root, client);
}

static struct display_client *soak_client_at(struct display_state *state, uint64_t ordinal) {
    uint64_t seen = 0;
    if (state == 0) {
        return 0;
    }
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *client = &state->clients[i];
        if (!client->used || client->closing) {
            continue;
        }
        if (seen == ordinal) {
            return client;
        }
        seen++;
    }
    return 0;
}

static void soak_launch_app(struct ui_element *root, struct display_state *state, const char *id) {
    uint64_t app_index = 0;
    if (find_app_by_id(id, &app_index)) {
        launch_app(root, state, app_index);
    } else {
        srv_puts("displayd: soak missing ");
        srv_puts(id);
        srv_puts("\n");
    }
}

static void soak_launch_set(struct ui_element *root, struct display_state *state) {
    srv_puts("displayd: soak launch phase\n");
    soak_launch_app(root, state, "notes");
    soak_launch_app(root, state, "calc");
    soak_launch_app(root, state, "gui2demo");
}

static void soak_close_all(struct ui_element *root, struct display_state *state) {
    srv_puts("displayd: soak close phase\n");
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used) {
            close_client(root, state, &state->clients[i]);
        }
    }
}

static int soak_step(struct ui_element *root, struct display_state *state,
    uint64_t *step, uint64_t *last_tick) {
    uint64_t now = (uint64_t)srv_ticks();
    struct display_client *client;
    if (root == 0 || state == 0 || step == 0 || last_tick == 0) {
        return 0;
    }
    if (*last_tick != 0 && now - *last_tick < 24) {
        return 0;
    }

    switch (*step) {
    case 0:
        soak_launch_set(root, state);
        *last_tick = now;
        *step = 1;
        return 0;
    case 1:
        if (mapped_client_count(state) < 3) {
            return 0;
        }
        client = soak_client_at(state, 0);
        if (client != 0) {
            move_client_to(root, client,
                work_x(&state->metrics) + (int64_t)state->metrics.gap,
                work_y(&state->metrics) + (int64_t)state->metrics.gap);
        }
        *last_tick = now;
        *step = 2;
        return 0;
    case 2:
        client = soak_client_at(state, 1);
        if (client != 0) {
            resize_client_to(root, state, client, client->width + 48, client->height + 32);
        }
        *last_tick = now;
        *step = 3;
        return 0;
    case 3:
        client = soak_client_at(state, 2);
        if (client != 0) {
            toggle_client_minimized(root, client);
        }
        *last_tick = now;
        *step = 4;
        return 0;
    case 4:
        client = soak_client_at(state, 2);
        if (client != 0) {
            activate_taskbar_client(root, state, client);
        }
        *last_tick = now;
        *step = 5;
        return 0;
    case 5:
        soak_close_all(root, state);
        *last_tick = now;
        *step = 6;
        return 0;
    case 6:
        if (mapped_client_count(state) != 0) {
            return 0;
        }
        soak_launch_set(root, state);
        *last_tick = now;
        *step = 7;
        return 0;
    case 7:
        if (mapped_client_count(state) < 2) {
            return 0;
        }
        client = soak_client_at(state, 0);
        if (client != 0) {
            move_client_to(root, client,
                work_x(&state->metrics) + (int64_t)state->metrics.dock_w + (int64_t)state->metrics.gap,
                work_y(&state->metrics) + 2 * (int64_t)state->metrics.gap);
            resize_client_to(root, state, client, client->width + 24, client->height + 24);
        }
        client = soak_client_at(state, 1);
        if (client != 0) {
            toggle_client_minimized(root, client);
            activate_taskbar_client(root, state, client);
        }
        *last_tick = now;
        *step = 8;
        return 0;
    case 8:
        soak_close_all(root, state);
        *last_tick = now;
        *step = 9;
        return 0;
    case 9:
        if (mapped_client_count(state) != 0) {
            return 0;
        }
        srv_puts("displayd: soak smoke ok\n");
        *last_tick = now;
        *step = 10;
        return 1;
    default:
        return 1;
    }
}

static void request_session_shutdown(struct ui_element *root, struct display_state *state) {
    uint64_t now;
    if (root == 0 || state == 0) {
        return;
    }
    now = (uint64_t)srv_ticks();
    if (!state->shutdown_requested) {
        state->shutdown_requested = 1;
        state->shutdown_tick = now;
        srv_puts("displayd: shutdown requested\n");
    }
    focus_client(root, state, 0);
    set_desktop_focus(root, state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
    state->dragging_surface_id = 0;
    state->resizing_surface_id = 0;
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        if (state->clients[i].used) {
            close_client(root, state, &state->clients[i]);
        }
    }
}

static void cancel_pointer_operation(struct ui_element *root, struct display_state *state) {
    struct display_client *client;
    if (root == 0 || state == 0) {
        return;
    }
    if (state->dragging_surface_id != 0) {
        client = find_client_by_surface(state, state->dragging_surface_id);
        if (client != 0) {
            mark_client_frame_dirty(root, client);
        }
        state->dragging_surface_id = 0;
        srv_puts("displayd: drag cancel\n");
    }
    if (state->resizing_surface_id != 0) {
        client = find_client_by_surface(state, state->resizing_surface_id);
        if (client != 0) {
            mark_client_frame_dirty(root, client);
        }
        state->resizing_surface_id = 0;
        srv_puts("displayd: resize cancel\n");
    }
}

static void activate_desktop_focus(struct ui_element *root, struct display_state *state) {
    int kind;
    uint64_t index;
    if (root == 0 || state == 0) {
        return;
    }
    kind = state->desktop_focus_kind;
    index = state->desktop_focus_index;
    if (!desktop_focus_valid(state, kind, index)) {
        set_desktop_focus(root, state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
        return;
    }
    if (kind == DISPLAY_DESKTOP_FOCUS_LAUNCHER) {
        uint64_t app_index = 0;
        if (launcher_app_index(index, &app_index)) {
            launch_app(root, state, app_index);
        }
    } else if (kind == DISPLAY_DESKTOP_FOCUS_EXIT) {
        request_session_shutdown(root, state);
    } else if (kind == DISPLAY_DESKTOP_FOCUS_TASKBAR) {
        activate_taskbar_client(root, state, &state->clients[index]);
    }
}

static int handle_desktop_key(struct ui_element *root, struct display_state *state, int key) {
    if (root == 0 || state == 0 || key == 0) {
        return 0;
    }
    if (key == '\t' || key == GUI_KEY_BACKTAB) {
        desktop_focus_step(root, state, key == GUI_KEY_BACKTAB);
        return 1;
    }
    if (key == '\r' || key == '\n' || key == ' ') {
        activate_desktop_focus(root, state);
        return 1;
    }
    if (key == 27) {
        set_desktop_focus(root, state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
        return 1;
    }
    return 0;
}

static void sweep_client_lifecycle(struct ui_element *root, struct display_state *state) {
    uint64_t now;
    if (root == 0 || state == 0) {
        return;
    }
    now = (uint64_t)srv_ticks();
    if (state->last_lifecycle_sweep_ticks != 0 &&
        now - state->last_lifecycle_sweep_ticks < DISPLAY_LIFECYCLE_SWEEP_TICKS) {
        return;
    }
    state->last_lifecycle_sweep_ticks = now;
    sweep_unmapped_launches(state);

    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        struct display_client *client = &state->clients[i];
        uint64_t status = 0;
        long waited;
        if (!client->used || client->pid == 0) {
            continue;
        }
        waited = srv_wait(client->pid, &status, SRV_WAIT_NOHANG);
        if (waited == (long)client->pid) {
            forget_launch_pid(state, client->pid);
            remove_client(root, state, client, "exit", status, 1);
            continue;
        }
        if (waited < 0 && !process_is_running(client->pid)) {
            forget_launch_pid(state, client->pid);
            if (srv_proc_exit_status(client->pid, &status) == 0) {
                remove_client(root, state, client, "exit", status, 1);
            } else {
                remove_client(root, state, client, "gone", 0, 0);
            }
            continue;
        }
        if (client->closing && now - client->close_tick > DISPLAY_CLOSE_KILL_TICKS) {
            srv_puts("displayd: close timeout ");
            log_client_title(client);
            srv_puts(" pid=");
            print_u64(client->pid);
            srv_puts("\n");
            (void)srv_kill((int64_t)client->pid);
            client->close_tick = now;
        }
    }
}

static int session_shutdown_complete(struct ui_element *root, struct display_state *state) {
    uint64_t now;
    if (root == 0 || state == 0 || !state->shutdown_requested) {
        return 0;
    }
    if (mapped_client_count(state) == 0) {
        srv_puts("displayd: shutdown complete\n");
        return 1;
    }
    now = (uint64_t)srv_ticks();
    if (now - state->shutdown_tick > DISPLAY_SESSION_SHUTDOWN_TICKS) {
        for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
            if (state->clients[i].used && state->clients[i].pid != 0) {
                srv_puts("displayd: shutdown kill ");
                log_client_title(&state->clients[i]);
                srv_puts(" pid=");
                print_u64(state->clients[i].pid);
                srv_puts("\n");
                (void)srv_kill((int64_t)state->clients[i].pid);
                state->clients[i].close_tick = now;
            }
        }
        state->shutdown_tick = now;
    }
    return 0;
}

static void draw_surface_client(struct ui_element *root, const struct display_client *client) {
    uint64_t frame_w = client_frame_width(client);
    uint64_t frame_h = client_frame_height(client);
    int64_t frame_x = client->x;
    int64_t frame_y = client->y;
    int64_t content_x = frame_x + 1;
    int64_t content_y = frame_y + WINDOW_TITLE_H + 1;
    int64_t close_x;
    int64_t close_y;
    uint64_t close_w;
    uint64_t close_h;
    int64_t min_x;
    int64_t min_y;
    uint64_t min_w;
    uint64_t min_h;

    draw_panel(root, frame_x, frame_y, frame_w, frame_h, COLOR_FRAME_BG,
        client->focused ? COLOR_FOCUS : COLOR_PANEL_BORDER);
    if (frame_w > 4 && frame_h > 4) {
        ui_draw_rect(root, frame_x + 2, frame_y + 2, frame_w - 4, 1,
            client->focused ? 0x88bdb3 : COLOR_PANEL_BORDER_DIM);
    }
    ui_draw_rect(root, frame_x + 1, frame_y + 1, frame_w - 2, WINDOW_TITLE_H - 1,
        client->focused ? client_accent_color(client) : COLOR_FRAME_INACTIVE);
    if (close_button_rect(client, &close_x, &close_y, &close_w, &close_h)) {
        ui_draw_rect(root, close_x, close_y, close_w, close_h,
            client->focused ? COLOR_DANGER : 0x5d3b3f);
        ui_draw_rect(root, close_x + 3, close_y + 3, close_w - 6, close_h - 6,
            client->focused ? 0xb96066 : COLOR_DANGER);
    }
    if (minimize_button_rect(client, &min_x, &min_y, &min_w, &min_h)) {
        ui_draw_rect(root, min_x, min_y, min_w, min_h,
            client->focused ? COLOR_FOCUS : 0x7b693e);
        ui_draw_rect(root, min_x + 3, min_y + (int64_t)min_h / 2,
            min_w - 6, 2, COLOR_FIELD);
        if (client->minimized && min_h > 8) {
            ui_draw_rect(root, min_x + (int64_t)min_w / 2, min_y + 3,
                2, min_h - 6, COLOR_FIELD);
        }
    }
    ui_draw_text(root, frame_x + 44, frame_y + 8,
        client->title[0] != '\0' ? client->title : "SURFACE",
        client->focused ? COLOR_TEXT : COLOR_TEXT_MUTED);
    if (client->minimized) {
        return;
    }
    ui_draw_rect(root, content_x, content_y, client->width, client->height, COLOR_FIELD);
    if (client->surface_id != 0 && root->surface.pixels != 0 &&
        content_x >= 0 && content_y >= 0 &&
        (uint64_t)content_x < root->surface.width &&
        (uint64_t)content_y < root->surface.height) {
        uint64_t width = client->width;
        uint64_t height = client->height;
        if ((uint64_t)content_x + width > root->surface.width) {
            width = root->surface.width - (uint64_t)content_x;
        }
        if ((uint64_t)content_y + height > root->surface.height) {
            height = root->surface.height - (uint64_t)content_y;
        }
        if (width != 0 && height != 0) {
            uint32_t *target = root->surface.pixels +
                (uint64_t)content_y * root->surface.stride + (uint64_t)content_x;
            if (gui_surface_copy(client->surface_id, 0, 0, width, height,
                    target, root->surface.stride) != 0) {
                ui_draw_text(root, content_x + 12, content_y + 16,
                    "SURFACE UNAVAILABLE", 0xf87171);
            }
        }
    }
    if (client->width >= WINDOW_RESIZE_GRIP && client->height >= WINDOW_RESIZE_GRIP) {
        int64_t grip_x = frame_x + (int64_t)frame_w - WINDOW_RESIZE_GRIP;
        int64_t grip_y = frame_y + (int64_t)frame_h - WINDOW_RESIZE_GRIP;
        ui_draw_rect(root, grip_x + WINDOW_RESIZE_GRIP - 2, grip_y + 4, 1, WINDOW_RESIZE_GRIP - 5,
            COLOR_PANEL_BORDER);
        ui_draw_rect(root, grip_x + 4, grip_y + WINDOW_RESIZE_GRIP - 2, WINDOW_RESIZE_GRIP - 5, 1,
            COLOR_PANEL_BORDER);
        ui_draw_rect(root, grip_x + WINDOW_RESIZE_GRIP - 6, grip_y + 8, 1, WINDOW_RESIZE_GRIP - 9,
            COLOR_PANEL_BORDER_DIM);
        ui_draw_rect(root, grip_x + 8, grip_y + WINDOW_RESIZE_GRIP - 6, WINDOW_RESIZE_GRIP - 9, 1,
            COLOR_PANEL_BORDER_DIM);
    }
}

static void draw_clients(struct ui_element *root, const struct display_state *state) {
    uint64_t drawn = 0;
    uint64_t last_z = 0;
    while (drawn < DISPLAY_CLIENT_MAX) {
        const struct display_client *next = 0;
        uint64_t next_z = 0;
        for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
            const struct display_client *client = &state->clients[i];
            if (!client->used || client->z <= last_z) {
                continue;
            }
            if (next == 0 || client->z < next_z) {
                next = client;
                next_z = client->z;
            }
        }
        if (next == 0) {
            break;
        }
        draw_surface_client(root, next);
        last_z = next_z;
        drawn++;
    }
}

static void draw_dock_button(struct ui_element *root, const struct display_metrics *m,
    uint64_t index, const struct display_app *app, int focused) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    int enabled = app_is_launch_enabled(app);
    uint32_t fill = enabled && app != 0 ? app->color : COLOR_PANEL_ALT;
    uint32_t border = focused ? COLOR_FOCUS : enabled ? COLOR_PANEL_BORDER : COLOR_PANEL_BORDER_DIM;
    uint32_t text = enabled ? COLOR_TEXT : COLOR_TEXT_MUTED;
    if (!launcher_rect(m, index, &x, &y, &width, &height)) {
        return;
    }
    draw_panel(root, x, y, width, m->button_h, fill, border);
    if (focused && width > 6 && m->button_h > 6) {
        ui_draw_rect(root, x + 2, y + 2, width - 4, 1, COLOR_FOCUS);
        ui_draw_rect(root, x + 2, y + (int64_t)m->button_h - 3, width - 4, 1, COLOR_FOCUS);
        ui_draw_rect(root, x + 2, y + 2, 1, m->button_h - 4, COLOR_FOCUS);
        ui_draw_rect(root, x + (int64_t)width - 3, y + 2, 1, m->button_h - 4, COLOR_FOCUS);
    }
    if (height >= 22 && width >= 42 && app != 0) {
        int64_t icon_x = x + 8;
        int64_t icon_y = y + (int64_t)((height - 16) / 2);
        ui_draw_rect(root, icon_x, icon_y, 16, 16, COLOR_FIELD);
        ui_draw_rect(root, icon_x, icon_y, 16, 1, 0xbfd3cc);
        ui_draw_rect(root, icon_x, icon_y + 15, 16, 1, COLOR_PANEL_BORDER_DIM);
        ui_draw_text(root, icon_x + 4, icon_y + 5,
            app->icon[0] != '\0' ? app->icon : app->label, text);
        ui_draw_text(root, x + 32, y + (int64_t)((m->button_h - 7) / 2), app->label, text);
    } else if (app != 0) {
        ui_draw_text(root, x + 10, y + (int64_t)((m->button_h - 7) / 2), app->label, text);
    }
}

static void draw_exit_button(struct ui_element *root, const struct display_metrics *m, int focused) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (!exit_rect(m, &x, &y, &width, &height)) {
        return;
    }
    draw_panel(root, x, y, width, height, 0x70485f, COLOR_FOCUS);
    if (focused && width > 6 && height > 6) {
        ui_draw_rect(root, x + 2, y + 2, width - 4, 1, COLOR_TEXT);
        ui_draw_rect(root, x + 2, y + (int64_t)height - 3, width - 4, 1, COLOR_TEXT);
        ui_draw_rect(root, x + 2, y + 2, 1, height - 4, COLOR_TEXT);
        ui_draw_rect(root, x + (int64_t)width - 3, y + 2, 1, height - 4, COLOR_TEXT);
    }
    ui_draw_text(root, x + 10, y + (int64_t)((height - 7) / 2), "EXIT", COLOR_TEXT);
}

static void make_taskbar_label(const struct display_client *client,
    char *out, uint64_t capacity, uint64_t button_width) {
    uint64_t len = 0;
    uint64_t max_chars;
    const char *title;
    if (out == 0 || capacity == 0) {
        return;
    }
    max_chars = button_width > 24 ? (button_width - 16) / 8 : 1;
    max_chars = min_u64(max_chars, capacity - 1);
    title = client != 0 && client->title[0] != '\0' ? client->title : "SURFACE";
    if (client != 0 && client->minimized && len < max_chars) {
        append_char(out, capacity, &len, '+');
    }
    for (uint64_t i = 0; title[i] != '\0' && len < max_chars; i++) {
        append_char(out, capacity, &len, title[i]);
    }
    out[len] = '\0';
}

static void draw_taskbar(struct ui_element *root, const struct display_state *state) {
    const struct display_metrics *m;
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (root == 0 || state == 0) {
        return;
    }
    m = &state->metrics;
    if (!taskbar_rect(m, &x, &y, &width, &height)) {
        return;
    }
    draw_panel(root, x, y, width, height, COLOR_PANEL_BG, COLOR_PANEL_BORDER_DIM);
    ui_draw_text(root, x + 14, y + 12, "TASKS", COLOR_TEXT);
    ui_draw_text(root, x + 14, y + 30, "GUI IPC ONLINE", COLOR_TEXT_MUTED);
    for (uint64_t i = 0; i < DISPLAY_CLIENT_MAX; i++) {
        const struct display_client *client = &state->clients[i];
        int64_t bx;
        int64_t by;
        uint64_t bw;
        uint64_t bh;
        char label[GUI_TEXT_MAX];
        uint32_t fill;
        uint32_t border;
        int desktop_focused = state->desktop_focus_kind == DISPLAY_DESKTOP_FOCUS_TASKBAR &&
            state->desktop_focus_index == i;
        if (!client->used ||
            !taskbar_button_rect(state, i, &bx, &by, &bw, &bh)) {
            continue;
        }
        fill = client->focused ? client_accent_color(client) :
            client->minimized ? 0x151d20 : COLOR_PANEL_ALT;
        border = (client->focused || desktop_focused) ? COLOR_FOCUS : COLOR_PANEL_BORDER_DIM;
        draw_panel(root, bx, by, bw, bh, fill, border);
        if (desktop_focused && bw > 6 && bh > 6) {
            ui_draw_rect(root, bx + 2, by + 2, bw - 4, 1, COLOR_FOCUS);
            ui_draw_rect(root, bx + 2, by + (int64_t)bh - 3, bw - 4, 1, COLOR_FOCUS);
            ui_draw_rect(root, bx + 2, by + 2, 1, bh - 4, COLOR_FOCUS);
            ui_draw_rect(root, bx + (int64_t)bw - 3, by + 2, 1, bh - 4, COLOR_FOCUS);
        }
        if (client->focused && bw > 10) {
            ui_draw_rect(root, bx + 5, by + (int64_t)bh - 4, bw - 10, 2, COLOR_FOCUS);
        }
        make_taskbar_label(client, label, sizeof(label), bw);
        ui_draw_text(root, bx + 8, by + (int64_t)((bh > 8 ? bh - 8 : 0) / 2), label,
            client->minimized ? COLOR_FOCUS : COLOR_TEXT);
    }
}

static void draw_workspace_backdrop(struct ui_element *root, const struct display_metrics *m) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (root == 0 || m == 0) {
        return;
    }
    x = work_x(m);
    y = work_y(m);
    width = work_width(m);
    height = work_height(m);
    ui_draw_rect(root, x, y, width, height, COLOR_DESKTOP_BG);
    if (width > 4 && height > 4) {
        ui_draw_rect(root, x, y, width, 1, 0x172023);
        ui_draw_rect(root, x, y, 1, height, 0x172023);
    }
    for (uint64_t gx = 96; gx + 1 < width; gx += 96) {
        ui_draw_rect(root, x + (int64_t)gx, y, 1, height, 0x11191c);
    }
    for (uint64_t gy = 80; gy + 1 < height; gy += 80) {
        ui_draw_rect(root, x, y + (int64_t)gy, width, 1, 0x11191c);
    }
}

static void draw_notifications(struct ui_element *root, const struct display_state *state) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    uint64_t visible = 0;
    uint64_t now;
    if (root == 0 || state == 0 ||
        !notices_rect(&state->metrics, &x, &y, &width, &height)) {
        return;
    }
    now = (uint64_t)srv_ticks();
    for (uint64_t i = 0; i < DISPLAY_NOTICE_MAX; i++) {
        const struct display_notice *notice = &state->notices[i];
        int64_t ny;
        if (!notice->used || notice->until_ticks <= now) {
            continue;
        }
        ny = y + (int64_t)(visible * 34);
        draw_panel(root, x, ny, width, 28, COLOR_PANEL_BG, notice->color);
        ui_draw_rect(root, x + 1, ny + 1, 4, 26, notice->color);
        ui_draw_text(root, x + 12, ny + 10, notice->text, COLOR_TEXT);
        visible++;
    }
}

static void draw_scene(struct ui_element *root) {
    struct display_state *state = (struct display_state *)root->userdata;
    const struct display_metrics *m = &state->metrics;
    char text[48];
    uint64_t text_len = 0;

    ui_clear(root, COLOR_DESKTOP_BG);
    ui_draw_rect(root, 0, 0, m->width, m->top_h, COLOR_TOP_BAR);
    ui_draw_rect(root, 0, (int64_t)m->top_h - 1, m->width, 1, COLOR_PANEL_BORDER_DIM);
    ui_draw_text(root, 16, (int64_t)((m->top_h - 7) / 2), "SRVROS", COLOR_TEXT);
    append_u64(text, sizeof(text), &text_len, m->width);
    append_char(text, sizeof(text), &text_len, 'X');
    append_u64(text, sizeof(text), &text_len, m->height);
    append_text(text, sizeof(text), &text_len, "  WINDOWS ");
    append_u64(text, sizeof(text), &text_len, visible_client_count(state));
    ui_draw_text(root, (int64_t)(m->width > 230 ? m->width - 230 : 16),
        (int64_t)((m->top_h - 7) / 2), text, COLOR_TEXT_MUTED);

    ui_draw_rect(root, 0, m->top_h, m->dock_w, m->height - m->top_h, COLOR_DOCK_BG);
    ui_draw_rect(root, (int64_t)m->dock_w - 1, (int64_t)m->top_h, 1,
        m->height - m->top_h, COLOR_PANEL_BORDER_DIM);
    for (uint64_t i = 0; i < display_launcher_count(); i++) {
        uint64_t app_index = 0;
        if (launcher_app_index(i, &app_index)) {
            draw_dock_button(root, m, i, &apps[app_index],
                state->desktop_focus_kind == DISPLAY_DESKTOP_FOCUS_LAUNCHER &&
                state->desktop_focus_index == i);
        }
    }
    draw_exit_button(root, m, state->desktop_focus_kind == DISPLAY_DESKTOP_FOCUS_EXIT);

    draw_workspace_backdrop(root, m);
    draw_taskbar(root, state);
    draw_clients(root, state);
    draw_notifications(root, state);
}

static void draw_debug_outline(int64_t x, int64_t y, uint64_t width, uint64_t height,
    uint32_t color) {
    if (width == 0 || height == 0 || x < 0 || y < 0) {
        return;
    }
    fillrect((uint64_t)x, (uint64_t)y, width, 1, color);
    fillrect((uint64_t)x, (uint64_t)(y + (int64_t)height - 1), width, 1, color);
    fillrect((uint64_t)x, (uint64_t)y, 1, height, color);
    fillrect((uint64_t)(x + (int64_t)width - 1), (uint64_t)y, 1, height, color);
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
    struct display_state *state = root != 0 ? (struct display_state *)root->userdata : 0;
    int64_t dirty_x = 0;
    int64_t dirty_y = 0;
    uint64_t dirty_width = 0;
    uint64_t dirty_height = 0;
    if (root == 0) {
        return;
    }
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
    if (state != 0 && state->damage_debug && (has_dirty || cursor_dirty)) {
        if (has_dirty) {
            state->last_damage_x = dirty_x;
            state->last_damage_y = dirty_y;
            state->last_damage_width = dirty_width;
            state->last_damage_height = dirty_height;
            state->damage_debug_pulse = DISPLAY_DAMAGE_DEBUG_TICKS;
            draw_debug_outline(dirty_x, dirty_y, dirty_width, dirty_height, 0xf5d76e);
        }
        if (cursor_dirty) {
            draw_debug_outline((int64_t)old_mouse_x, (int64_t)old_mouse_y,
                CURSOR_W, CURSOR_H, 0x6ee7ff);
            draw_debug_outline((int64_t)mouse_x, (int64_t)mouse_y,
                CURSOR_W, CURSOR_H, 0x7cff9a);
        }
    }
}

static void handle_gui_messages(struct ui_element *root, struct display_state *state) {
    struct gui_message msg;
    while (gui_recv(&msg) > 0) {
        state->gui_messages++;
        request_chrome_scrub(state);
        if (msg.type == GUI_MSG_CREATE_WINDOW) {
            srv_puts("displayd: client window ");
            srv_puts(msg.text);
            srv_puts(" pid=");
            print_u64(msg.source_pid);
            srv_puts("\n");
        } else if (msg.type == GUI_MSG_V2_CREATE_SURFACE_WINDOW) {
            struct display_client *client = find_client_for_surface_create(state,
                msg.source_pid,
                msg.window_id,
                (uint64_t)msg.value);
            int was_used = client != 0 && client->used;
            if (client == 0) {
                client = alloc_client(state);
            }
            if (client != 0) {
                uint64_t app_index = DISPLAY_APP_NONE;
                uint64_t old_surface_id = client->used ? client->surface_id : 0;
                int launched_by_displayd = 0;
                if (client->used) {
                    mark_client_frame_dirty(root, client);
                }
                client->used = 1;
                client->pid = msg.source_pid;
                client->window_id = msg.window_id;
                client->surface_id = (uint64_t)msg.value;
                client->x = msg.x;
                client->y = msg.y;
                client->width = msg.width;
                client->height = msg.height;
                client->closing = 0;
                client->close_tick = 0;
                if (was_used && old_surface_id != 0 && old_surface_id != client->surface_id) {
                    if (state->focused_surface_id == old_surface_id) {
                        state->focused_surface_id = client->surface_id;
                    }
                    if (state->hovered_surface_id == old_surface_id) {
                        state->hovered_surface_id = client->surface_id;
                    }
                    if (state->dragging_surface_id == old_surface_id) {
                        state->dragging_surface_id = client->surface_id;
                    }
                    if (state->resizing_surface_id == old_surface_id) {
                        state->resizing_surface_id = client->surface_id;
                    }
                    srv_puts("displayd: remap surface window ");
                    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
                    srv_puts(" old=");
                    print_u64(old_surface_id);
                    srv_puts(" new=");
                    print_u64(client->surface_id);
                    srv_puts("\n");
                }
                if (!was_used) {
                    client->focused = 0;
                    client->minimized = 0;
                    client->app_index = DISPLAY_APP_NONE;
                    state->next_z++;
                    if (state->next_z == 0) {
                        state->next_z = 1;
                    }
                    client->z = state->next_z;
                }
                if (find_app_by_pid(state, msg.source_pid, &app_index)) {
                    launched_by_displayd = 1;
                    client->app_index = app_index;
                } else if (find_app_by_title(msg.text, &app_index)) {
                    client->app_index = app_index;
                }
                if (msg.text[0] != '\0') {
                    copy_text(client->title, msg.text);
                } else if (client->title[0] == '\0' && client->app_index < DISPLAY_APP_COUNT) {
                    copy_text(client->title, apps[client->app_index].title);
                }
                if (client->app_index < DISPLAY_APP_COUNT &&
                    (client->width < WINDOW_MIN_WIDTH || client->height < WINDOW_MIN_HEIGHT)) {
                    const struct display_app *app = app_at(client->app_index);
                    uint64_t max_width = max_u64(WINDOW_MIN_WIDTH, work_width(&state->metrics));
                    uint64_t max_height = max_u64(WINDOW_MIN_HEIGHT, work_height(&state->metrics));
                    if (app != 0) {
                        client->width = min_u64(max_u64(app->default_width, WINDOW_MIN_WIDTH), max_width);
                        client->height = min_u64(max_u64(app->default_height, WINDOW_MIN_HEIGHT), max_height);
                    }
                }
                if (!was_used) {
                    place_new_client(root, state, client, launched_by_displayd);
                } else {
                    clamp_client_position(root, client);
                }
                mark_client_frame_dirty(root, client);
                mark_taskbar_dirty(root, state);
                srv_puts("displayd: mapped surface window ");
                srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
                srv_puts(" surface=");
                print_u64(client->surface_id);
                srv_puts(" pid=");
                print_u64(client->pid);
                srv_puts("\n");
                send_client_event(client, GUI_MSG_V2_EVENT_CONFIGURE,
                    client->x,
                    client->y,
                    client->width,
                    client->height,
                    0,
                    "");
                focus_client(root, state, client);
            }
        } else if (msg.type == GUI_MSG_V2_DAMAGE_SURFACE) {
            struct display_client *client = find_client_by_surface(state, (uint64_t)msg.value);
            if (client != 0) {
                if (client->minimized) {
                    continue;
                }
                ui_mark_dirty_rect(root,
                    client->x + 1 + msg.x,
                    client->y + WINDOW_TITLE_H + 1 + msg.y,
                    msg.width,
                    msg.height);
            }
        } else if (msg.type == GUI_MSG_V2_DESTROY_SURFACE) {
            struct display_client *client = find_client_by_surface(state, (uint64_t)msg.value);
            if (client != 0) {
                remove_client(root, state, client, "destroy", 0, 0);
            }
        }
    }
}

int main(int argc, char **argv) {
    struct gfx_info gfx = {0};
    struct mouse_event mouse = {0};
    struct ui_surface root_surface;
    struct ui_element root;
    struct display_state state;
    struct display_key_state key_state = {0};
    uint32_t *pixels;
    uint64_t pixel_count;
    uint64_t mouse_x = 96;
    uint64_t mouse_y = 96;
    uint64_t old_mouse_x = 96;
    uint64_t old_mouse_y = 96;
    uint8_t buttons = 0;
    uint64_t start_ticks;
    int smoke = 0;
    int smoke_autostart = 0;
    int frame_smoke = 0;
    int launcher_smoke = 0;
    int textedit_dialog_smoke = 0;
    int paint_dialog_smoke = 0;
    int fileman_open_smoke = 0;
    int app_exit_smoke = 0;
    int owner_crash_smoke = 0;
    int soak_smoke = 0;
    int console_muted = 0;
    int damage_debug = 0;
    uint64_t soak_smoke_step = 0;
    uint64_t soak_smoke_last_tick = 0;

    for (int i = 1; i < argc; i++) {
        if (streq(argv[i], "--smoke")) {
            smoke = 1;
        } else if (streq(argv[i], "--smoke-autostart")) {
            smoke = 1;
            smoke_autostart = 1;
        } else if (streq(argv[i], "--frame-smoke-autostart")) {
            smoke = 1;
            smoke_autostart = 1;
            frame_smoke = 1;
        } else if (streq(argv[i], "--frame-keyboard-autostart")) {
            smoke_autostart = 1;
            frame_smoke = 1;
        } else if (streq(argv[i], "--launcher-smoke")) {
            smoke = 1;
            launcher_smoke = 1;
        } else if (streq(argv[i], "--textedit-dialog-smoke")) {
            textedit_dialog_smoke = 1;
        } else if (streq(argv[i], "--paint-dialog-smoke")) {
            paint_dialog_smoke = 1;
        } else if (streq(argv[i], "--fileman-open-smoke")) {
            fileman_open_smoke = 1;
        } else if (streq(argv[i], "--app-exit-smoke")) {
            smoke = 1;
            app_exit_smoke = 1;
        } else if (streq(argv[i], "--owner-crash-smoke")) {
            owner_crash_smoke = 1;
        } else if (streq(argv[i], "--soak-smoke")) {
            soak_smoke = 1;
        } else if (streq(argv[i], "--damage-debug")) {
            damage_debug = 1;
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
    load_app_registry();
    state.damage_debug = damage_debug;
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

    ui_mark_dirty(&root);
    ui_render_tree(&root);
    ui_present(&root);
    draw_cursor(mouse_x, mouse_y, gfx.width, gfx.height, 0xffffff);
    if (gfx_console_mute(1) == 0) {
        console_muted = 1;
        srv_puts("displayd: framebuffer console muted\n");
    }
    srv_puts("displayd: root backbuffer ready\n");
    if (owner_crash_smoke) {
        srv_puts("displayd: owner crash smoke exiting\n");
        return 42;
    }
    if (launcher_smoke) {
        for (uint64_t i = 0; i < DISPLAY_APP_COUNT; i++) {
            launch_app(&root, &state, i);
        }
    } else if (textedit_dialog_smoke) {
        long pid = srv_spawn_bg_args("/fat/bin/textedit",
            "/fat/home/dialog-saveas-target.txt");
        srv_puts("displayd: launched textedit-dialog pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
    } else if (paint_dialog_smoke) {
        long pid = srv_spawn_bg_args("/fat/bin/paint",
            "/fat/home/paint-dialog.bmp");
        srv_puts("displayd: launched paint-dialog pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
    } else if (fileman_open_smoke) {
        uint64_t app_index = 0;
        long pid = srv_spawn_bg_args("/fat/bin/fileman", "/fat/home/open-smoke");
        srv_puts("displayd: launched fileman-open pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
            if (find_app_by_id("fileman", &app_index)) {
                remember_launch(&state, (uint64_t)pid, app_index);
            }
        }
    } else if (app_exit_smoke) {
        long pid = srv_spawn_bg_args("/fat/bin/guifail", "--exit");
        srv_puts("displayd: launched guifail pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
    } else if (smoke_autostart) {
        long pid = srv_spawn_bg("/fat/bin/surfacedemo");
        srv_puts("displayd: launched surfacedemo pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
        pid = srv_spawn_bg("/fat/bin/gui2demo");
        srv_puts("displayd: launched gui2demo pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
        pid = srv_spawn_bg("/fat/bin/notes");
        srv_puts("displayd: launched notes pid=");
        if (pid < 0) {
            srv_puts("failed\n");
        } else {
            print_u64((uint64_t)pid);
            srv_puts("\n");
        }
    }

    start_ticks = (uint64_t)srv_ticks();
    for (;;) {
        int key = display_read_key(&key_state);
        int cursor_dirty = 0;
        struct display_client *focused_client;
        old_mouse_x = mouse_x;
        old_mouse_y = mouse_y;

        handle_gui_messages(&root, &state);
        sweep_client_lifecycle(&root, &state);
        sweep_notices(&root, &state);
        focused_client = find_client_by_surface(&state, state.focused_surface_id);
        if (key != 0) {
            if (key == 27 && (state.dragging_surface_id != 0 || state.resizing_surface_id != 0)) {
                cancel_pointer_operation(&root, &state);
            } else if (focused_client != 0) {
                send_client_event(focused_client, GUI_MSG_V2_EVENT_KEY_DOWN, 0, 0,
                    focused_client->width, focused_client->height, key, "");
            } else {
                (void)handle_desktop_key(&root, &state, key);
            }
        }

        if (mouse_scan(&mouse) > 0) {
            uint8_t old_buttons = buttons;
            int64_t local_x = 0;
            int64_t local_y = 0;
            struct display_client *client = 0;
            struct display_client *frame_client = 0;
            int was_dragging = state.dragging_surface_id != 0 || state.resizing_surface_id != 0;
            if (gfx.width > CURSOR_W) {
                mouse_x = clamp_add(mouse_x, mouse.dx, 0, gfx.width - CURSOR_W);
            }
            if (gfx.height > CURSOR_H) {
                mouse_y = clamp_add(mouse_y, mouse.dy, 0, gfx.height - CURSOR_H);
            }
            buttons = mouse.buttons;
            cursor_dirty = mouse_x != old_mouse_x || mouse_y != old_mouse_y;
            state.mouse_events++;

            if (state.dragging_surface_id != 0) {
                client = find_client_by_surface(&state, state.dragging_surface_id);
                if (client == 0 || (buttons & 1) == 0) {
                    state.dragging_surface_id = 0;
                } else if (cursor_dirty) {
                    move_client_to(&root, client,
                        (int64_t)mouse_x - state.drag_offset_x,
                        (int64_t)mouse_y - state.drag_offset_y);
                }
            }

            if (state.resizing_surface_id != 0) {
                client = find_client_by_surface(&state, state.resizing_surface_id);
                if (client == 0 || (buttons & 1) == 0) {
                    state.resizing_surface_id = 0;
                } else if (cursor_dirty) {
                    int64_t frame_width = (int64_t)mouse_x + state.resize_offset_x - client->x;
                    int64_t frame_height = (int64_t)mouse_y + state.resize_offset_y - client->y;
                    uint64_t next_width = frame_width > 2 ? (uint64_t)(frame_width - 2) : 1;
                    uint64_t next_height = frame_height > WINDOW_TITLE_H + 2 ?
                        (uint64_t)(frame_height - WINDOW_TITLE_H - 2) : 1;
                    resize_client_to(&root, &state, client, next_width, next_height);
                }
            }

            if (state.dragging_surface_id == 0 && state.resizing_surface_id == 0) {
                client = client_at(&state, (int64_t)mouse_x, (int64_t)mouse_y, &local_x, &local_y);
                state.hovered_surface_id = client != 0 ? client->surface_id : 0;
                if (client != 0 && cursor_dirty) {
                    send_client_event(client, GUI_MSG_V2_EVENT_POINTER_MOVE,
                        local_x, local_y, client->width, client->height, buttons, "");
                }
            }

            if (old_buttons != buttons) {
                if ((buttons & 1) != 0 && (old_buttons & 1) == 0) {
                    int launcher_index = launcher_at(&state, (int64_t)mouse_x, (int64_t)mouse_y);
                    if (launcher_index >= 0) {
                        focus_client(&root, &state, 0);
                        set_desktop_focus(&root, &state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
                        launch_app(&root, &state, (uint64_t)launcher_index);
                    } else if (exit_hit(&state, (int64_t)mouse_x, (int64_t)mouse_y)) {
                        set_desktop_focus(&root, &state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
                        request_session_shutdown(&root, &state);
                    } else {
                        struct display_client *task_client =
                            taskbar_client_at(&state, (int64_t)mouse_x, (int64_t)mouse_y);
                        if (task_client != 0) {
                            activate_taskbar_client(&root, &state, task_client);
                        } else if ((frame_client =
                                client_frame_at(&state, (int64_t)mouse_x, (int64_t)mouse_y)) != 0) {
                            focus_client(&root, &state, frame_client);
                            if (client_close_hit(frame_client, (int64_t)mouse_x, (int64_t)mouse_y)) {
                                close_client(&root, &state, frame_client);
                            } else if (client_minimize_hit(frame_client, (int64_t)mouse_x, (int64_t)mouse_y)) {
                                toggle_client_minimized(&root, frame_client);
                            } else if (client_resize_hit(frame_client, (int64_t)mouse_x, (int64_t)mouse_y)) {
                                state.resizing_surface_id = frame_client->surface_id;
                                state.resize_offset_x = frame_client->x +
                                    (int64_t)client_frame_width(frame_client) - (int64_t)mouse_x;
                                state.resize_offset_y = frame_client->y +
                                    (int64_t)client_frame_height(frame_client) - (int64_t)mouse_y;
                            } else if (client_title_hit(frame_client, (int64_t)mouse_x, (int64_t)mouse_y)) {
                                state.dragging_surface_id = frame_client->surface_id;
                                state.drag_offset_x = (int64_t)mouse_x - frame_client->x;
                                state.drag_offset_y = (int64_t)mouse_y - frame_client->y;
                            } else if (client == frame_client && !frame_client->minimized) {
                                send_client_event(client, GUI_MSG_V2_EVENT_POINTER_BUTTON,
                                    local_x, local_y, client->width, client->height, buttons, "");
                            }
                        } else {
                            focus_client(&root, &state, 0);
                            set_desktop_focus(&root, &state, DISPLAY_DESKTOP_FOCUS_NONE, 0);
                        }
                    }
                } else if (!was_dragging) {
                    client = client_at(&state, (int64_t)mouse_x, (int64_t)mouse_y, &local_x, &local_y);
                    if (client != 0) {
                        send_client_event(client, GUI_MSG_V2_EVENT_POINTER_BUTTON,
                            local_x, local_y, client->width, client->height, buttons, "");
                    }
                }
            }
        }

        scrub_chrome_if_needed(&root, &state);
        present_dirty(&root, mouse_x, mouse_y, old_mouse_x, old_mouse_y,
            cursor_dirty, buttons != 0 ? 0xf87171 : 0xffffff);

        if (soak_smoke && soak_step(&root, &state, &soak_smoke_step, &soak_smoke_last_tick)) {
            break;
        }

        if (session_shutdown_complete(&root, &state)) {
            break;
        }

        if (!state.shutdown_requested && smoke && ((launcher_smoke &&
                state.launch_count >= DISPLAY_APP_COUNT &&
                mapped_client_count(&state) >= DISPLAY_APP_COUNT) ||
            (app_exit_smoke && state.notice_count > 0) ||
            (uint64_t)srv_ticks() - start_ticks >
                (frame_smoke ? 220 : launcher_smoke ? 520 : smoke_autostart ? 80 : 20))) {
            if (app_exit_smoke && state.notice_count > 0) {
                srv_puts("displayd: app-exit smoke ok\n");
            } else {
                srv_puts("displayd: smoke ok\n");
            }
            break;
        }
        srv_yield();
    }

    free(pixels);
    if (console_muted) {
        gfx_console_mute(0);
    }
    srv_puts("displayd: exited\n");
    return 0;
}
