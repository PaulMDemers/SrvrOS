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

struct display_launcher {
    const char *label;
    const char *path;
    uint32_t color;
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
    uint64_t z;
    char title[GUI_TEXT_MAX];
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
    uint8_t last_buttons;
    struct display_client clients[DISPLAY_CLIENT_MAX];
};

static const struct display_launcher launchers[] = {
    { "NOTES", "/fat/bin/notes", 0x2f6f68 },
    { "GUI2", "/fat/bin/gui2demo", 0x335b7a },
    { "SURFACE", "/fat/bin/surfacedemo", 0x60548d },
    { "CALC", "/fat/bin/calc", 0x70485f },
};

#define DISPLAY_LAUNCHER_COUNT (sizeof(launchers) / sizeof(launchers[0]))

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
    if (m == 0 || index >= DISPLAY_LAUNCHER_COUNT || m->dock_w <= 2 * m->margin) {
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
    for (uint64_t i = 0; i < DISPLAY_LAUNCHER_COUNT; i++) {
        int64_t bx;
        int64_t by;
        uint64_t bw;
        uint64_t bh;
        if (launcher_rect(m, i, &bx, &by, &bw, &bh) && rect_hit(bx, by, bw, bh, x, y)) {
            return (int)i;
        }
    }
    return -1;
}

static struct display_client *find_client(struct display_state *state,
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
    if (state->focused_surface_id == next_id) {
        return;
    }

    struct display_client *old = find_client_by_surface(state, state->focused_surface_id);
    if (old != 0) {
        old->focused = 0;
        mark_client_frame_dirty(root, old);
        send_client_event(old, GUI_MSG_V2_EVENT_FOCUS, 0, 0,
            old->width, old->height, 0, "");
    }

    state->focused_surface_id = next_id;
    if (client != 0) {
        raise_client(root, state, client);
        client->focused = 1;
        mark_client_frame_dirty(root, client);
        srv_puts("displayd: focus ");
        srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
        srv_puts("\n");
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
        (int64_t)min_u64(WINDOW_TITLE_H, work_height(m)) :
        (int64_t)root->height - (int64_t)min_u64(WINDOW_TITLE_H, root->height);
    client->x = clamp_i64(client->x, min_x, max_x);
    client->y = clamp_i64(client->y, min_y, max_y);
}

static void place_new_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    uint64_t same_title = 0;
    if (root == 0 || state == 0 || client == 0) {
        return;
    }
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
    const struct display_launcher *launcher;
    if (root == 0 || state == 0 || index >= DISPLAY_LAUNCHER_COUNT) {
        return;
    }
    launcher = &launchers[index];
    if (launcher->path == 0 || launcher->path[0] == '\0') {
        srv_puts("displayd: launcher ");
        srv_puts(launcher->label);
        srv_puts(" unavailable\n");
        return;
    }
    pid = srv_spawn_bg(launcher->path);
    srv_puts("displayd: launch ");
    srv_puts(launcher->label);
    srv_puts(" ");
    srv_puts(launcher->path);
    srv_puts(" pid=");
    if (pid < 0) {
        srv_puts("failed\n");
    } else {
        print_u64((uint64_t)pid);
        srv_puts("\n");
        state->launch_count++;
    }
    ui_mark_dirty_rect(root, 0, state->metrics.top_h, state->metrics.dock_w,
        state->metrics.height - state->metrics.top_h);
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
    srv_puts("displayd: minimize ");
    srv_puts(client->title[0] != '\0' ? client->title : "SURFACE");
    srv_puts(" state=");
    print_u64(client->minimized ? 1 : 0);
    srv_puts("\n");
}

static void close_client(struct ui_element *root, struct display_state *state,
    struct display_client *client) {
    if (client == 0 || !client->used) {
        return;
    }
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

    draw_panel(root, frame_x, frame_y, frame_w, frame_h, 0x17232d,
        client->focused ? 0xf5b84b : 0x8fd0d4);
    ui_draw_rect(root, frame_x + 1, frame_y + 1, frame_w - 2, WINDOW_TITLE_H - 1, 0x2f6f68);
    if (close_button_rect(client, &close_x, &close_y, &close_w, &close_h)) {
        ui_draw_rect(root, close_x, close_y, close_w, close_h, 0x8a3f48);
        ui_draw_text(root, close_x + 3, close_y + 2, "X", 0xffffff);
    }
    if (minimize_button_rect(client, &min_x, &min_y, &min_w, &min_h)) {
        ui_draw_rect(root, min_x, min_y, min_w, min_h, 0xf5b84b);
        ui_draw_text(root, min_x + 3, min_y + 2, client->minimized ? "+" : "-", 0x111b24);
    }
    ui_draw_text(root, frame_x + 44, frame_y + 8,
        client->title[0] != '\0' ? client->title : "SURFACE", 0xffffff);
    if (client->minimized) {
        return;
    }
    ui_draw_rect(root, content_x, content_y, client->width, client->height, 0x0f1720);
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
            0x8fd0d4);
        ui_draw_rect(root, grip_x + 4, grip_y + WINDOW_RESIZE_GRIP - 2, WINDOW_RESIZE_GRIP - 5, 1,
            0x8fd0d4);
        ui_draw_rect(root, grip_x + WINDOW_RESIZE_GRIP - 6, grip_y + 8, 1, WINDOW_RESIZE_GRIP - 9,
            0x486476);
        ui_draw_rect(root, grip_x + 8, grip_y + WINDOW_RESIZE_GRIP - 6, WINDOW_RESIZE_GRIP - 9, 1,
            0x486476);
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
    uint64_t index, const char *label, uint32_t color) {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    if (!launcher_rect(m, index, &x, &y, &width, &height)) {
        return;
    }
    draw_panel(root, x, y, width, m->button_h, color, 0x8fd0d4);
    ui_draw_text(root, x + 10, y + (int64_t)((m->button_h - 7) / 2), label, 0xf2f7ff);
}

static void draw_scene(struct ui_element *root) {
    struct display_state *state = (struct display_state *)root->userdata;
    const struct display_metrics *m = &state->metrics;
    uint64_t scene_work_x = (uint64_t)work_x(m);
    uint64_t scene_work_y = (uint64_t)work_y(m);
    uint64_t work_w = work_width(m);
    uint64_t work_h = work_height(m);
    uint64_t card_w = work_w > 3 * m->gap ? (work_w - 2 * m->gap) / 3 : work_w;
    uint64_t card_h = clamp_u64(work_h / 3, 84, 180);
    char text[48];

    ui_clear(root, 0x0b1118);
    ui_draw_rect(root, 0, 0, m->width, m->top_h, 0x233640);
    ui_draw_text(root, 16, (int64_t)((m->top_h - 7) / 2), "DISPLAYD", 0xffffff);
    ui_draw_text(root, (int64_t)(m->width > 220 ? m->width - 220 : 16),
        (int64_t)((m->top_h - 7) / 2), "Q OR ESC EXITS", 0xb9d8df);

    ui_draw_rect(root, 0, m->top_h, m->dock_w, m->height - m->top_h, 0x101a22);
    for (uint64_t i = 0; i < DISPLAY_LAUNCHER_COUNT; i++) {
        draw_dock_button(root, m, i, launchers[i].label, launchers[i].color);
    }

    draw_panel(root, (int64_t)scene_work_x, (int64_t)scene_work_y, card_w, card_h,
        0x15232d, 0x486476);
    ui_draw_text(root, (int64_t)scene_work_x + 14, (int64_t)scene_work_y + 16,
        "COMPOSITOR READY", 0xffffff);
    ui_draw_text(root, (int64_t)scene_work_x + 14, (int64_t)scene_work_y + 34,
        "DOCK LAUNCHERS", 0xb9d8df);

    draw_panel(root, (int64_t)(scene_work_x + card_w + m->gap), (int64_t)scene_work_y,
        card_w, card_h, 0x142620, 0x50786c);
    ui_draw_text(root, (int64_t)(scene_work_x + card_w + m->gap + 14),
        (int64_t)scene_work_y + 16, "DISPLAY SCALE", 0xffffff);
    text[0] = '\0';
    uint64_t text_len = 0;
    append_text(text, sizeof(text), &text_len, "SIZE ");
    append_u64(text, sizeof(text), &text_len, m->width);
    append_char(text, sizeof(text), &text_len, 'X');
    append_u64(text, sizeof(text), &text_len, m->height);
    ui_draw_text(root, (int64_t)(scene_work_x + card_w + m->gap + 14),
        (int64_t)scene_work_y + 34, text, 0xb9d8df);

    draw_panel(root, (int64_t)(scene_work_x + 2 * (card_w + m->gap)), (int64_t)scene_work_y,
        card_w, card_h, 0x241c28, 0x7f668f);
    ui_draw_text(root, (int64_t)(scene_work_x + 2 * (card_w + m->gap) + 14),
        (int64_t)scene_work_y + 16, "INPUT PIPE", 0xffffff);
    ui_draw_text(root, (int64_t)(scene_work_x + 2 * (card_w + m->gap) + 14),
        (int64_t)scene_work_y + 34, "MOUSE AND KEYS", 0xb9d8df);

    draw_panel(root, (int64_t)scene_work_x, (int64_t)(m->height - m->status_h - m->margin),
        work_w, m->status_h, 0x111b24, 0x3f5d6b);
    ui_draw_text(root, (int64_t)scene_work_x + 14,
        (int64_t)(m->height - m->status_h - m->margin + 14),
        "STATUS", 0xffffff);
    ui_draw_text(root, (int64_t)scene_work_x + 14,
        (int64_t)(m->height - m->status_h - m->margin + 32),
        "GUI IPC SERVER ONLINE", 0xb9d8df);

    draw_clients(root, state);
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

static void handle_gui_messages(struct ui_element *root, struct display_state *state) {
    struct gui_message msg;
    while (gui_recv(&msg) > 0) {
        state->gui_messages++;
        if (msg.type == GUI_MSG_CREATE_WINDOW) {
            srv_puts("displayd: client window ");
            srv_puts(msg.text);
            srv_puts(" pid=");
            print_u64(msg.source_pid);
            srv_puts("\n");
        } else if (msg.type == GUI_MSG_V2_CREATE_SURFACE_WINDOW) {
            struct display_client *client = find_client(state,
                msg.source_pid,
                msg.window_id,
                (uint64_t)msg.value);
            int was_used = client != 0 && client->used;
            if (client == 0) {
                client = alloc_client(state);
            }
            if (client != 0) {
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
                if (!was_used) {
                    client->focused = 0;
                    client->minimized = 0;
                    state->next_z++;
                    if (state->next_z == 0) {
                        state->next_z = 1;
                    }
                    client->z = state->next_z;
                }
                if (msg.text[0] != '\0' || client->title[0] == '\0') {
                    copy_text(client->title, msg.text);
                }
                if (!was_used) {
                    place_new_client(root, state, client);
                } else {
                    clamp_client_position(root, client);
                }
                mark_client_frame_dirty(root, client);
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
            struct display_client *client = find_client(state,
                msg.source_pid,
                msg.window_id,
                (uint64_t)msg.value);
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
            struct display_client *client = find_client(state,
                msg.source_pid,
                msg.window_id,
                (uint64_t)msg.value);
            if (client != 0) {
                mark_client_frame_dirty(root, client);
                if (state->focused_surface_id == client->surface_id) {
                    focus_client(root, state, 0);
                }
                if (state->hovered_surface_id == client->surface_id) {
                    state->hovered_surface_id = 0;
                }
                client->used = 0;
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
        } else if (streq(argv[i], "--launcher-smoke")) {
            smoke = 1;
            launcher_smoke = 1;
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
    if (smoke_autostart) {
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
        int key = kbhit();
        int cursor_dirty = 0;
        struct display_client *focused_client;
        old_mouse_x = mouse_x;
        old_mouse_y = mouse_y;

        handle_gui_messages(&root, &state);
        focused_client = find_client_by_surface(&state, state.focused_surface_id);
        if (key != 0) {
            if (focused_client != 0) {
                send_client_event(focused_client, GUI_MSG_V2_EVENT_KEY_DOWN, 0, 0,
                    focused_client->width, focused_client->height, key, "");
            } else if (key == 'q' || key == 'Q' || key == 27) {
                break;
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
                        launch_app(&root, &state, (uint64_t)launcher_index);
                    } else {
                        frame_client = client_frame_at(&state, (int64_t)mouse_x, (int64_t)mouse_y);
                        if (frame_client != 0) {
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

        present_dirty(&root, mouse_x, mouse_y, old_mouse_x, old_mouse_y,
            cursor_dirty, buttons != 0 ? 0xf87171 : 0xffffff);

        if (smoke && (uint64_t)srv_ticks() - start_ticks >
            (frame_smoke ? 220 : launcher_smoke ? 180 : smoke_autostart ? 80 : 20)) {
            srv_puts("displayd: smoke ok\n");
            break;
        }
        srv_yield();
    }

    free(pixels);
    srv_puts("displayd: exited\n");
    return 0;
}
