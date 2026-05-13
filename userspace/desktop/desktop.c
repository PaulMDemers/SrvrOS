#include <srvros/conio.h>
#include <srvros/gfx.h>
#include <srvros/gui.h>
#include <srvros/mouse.h>
#include <srvros/sys.h>
#include <srvros/ui.h>

#include <stdint.h>

#define CURSOR_W 16
#define CURSOR_H 16
#define TITLE_H 24

#define CLIENT_MAX 8
#define ROOT_CHILDREN (5 + CLIENT_MAX)
#define CLIENT_CHILDREN 32
#define CLIENT_BUTTONS 20
#define CLIENT_LABELS 2
#define CLIENT_TEXT_ENTRIES 2
#define CLIENT_IMAGES 1
#define CLIENT_W 320
#define CLIENT_H 240
#define LABEL_W 280
#define LABEL_H 112
#define TEXT_ENTRY_W 256
#define TEXT_ENTRY_H 28
#define IMAGE_W 160
#define IMAGE_H 120
#define BUTTON_W 104
#define BUTTON_H 28
#define SMALL_BUTTON_W 72
#define SMALL_BUTTON_H 24
#define CLIENT_SLOT_INVALID ((uint64_t)-1)

static uint32_t launch_calc_pixels[BUTTON_W * BUTTON_H];
static uint32_t launch_notes_pixels[BUTTON_W * BUTTON_H];
static uint32_t launch_edit_pixels[BUTTON_W * BUTTON_H];
static uint32_t launch_paint_pixels[BUTTON_W * BUTTON_H];
static uint32_t exit_pixels[BUTTON_W * BUTTON_H];
static uint32_t window_pixels[CLIENT_MAX][CLIENT_W * CLIENT_H];
static uint32_t label_pixels[CLIENT_MAX][CLIENT_LABELS][LABEL_W * LABEL_H];
static uint32_t text_entry_pixels[CLIENT_MAX][CLIENT_TEXT_ENTRIES][TEXT_ENTRY_W * TEXT_ENTRY_H];
static uint32_t image_surface_pixels[CLIENT_MAX][CLIENT_IMAGES][IMAGE_W * IMAGE_H];
static uint32_t image_source_pixels[CLIENT_MAX][CLIENT_IMAGES][IMAGE_W * IMAGE_H];
static uint32_t button_pixels[CLIENT_MAX][CLIENT_BUTTONS][SMALL_BUTTON_W * SMALL_BUTTON_H];
static struct ui_element root;
static struct ui_element *root_children[ROOT_CHILDREN];
static struct ui_element launch_calc;
static struct ui_element launch_notes;
static struct ui_element launch_edit;
static struct ui_element launch_paint;
static struct ui_element exit_button;

struct desktop_state {
    int running;
};

struct client_window;

struct button_state {
    const char *label;
    char label_storage[GUI_TEXT_MAX];
    int pressed;
    uint32_t face;
    void (*action)(struct ui_element *button);
    void *target;
    uint64_t control_id;
};

static struct button_state launch_calc_state;
static struct button_state launch_notes_state;
static struct button_state launch_edit_state;
static struct button_state launch_paint_state;
static struct button_state exit_button_state;
static struct ui_event event_scratch;
static char calc_label[5];
static char calc_path[17];
static char notes_label[6];
static char notes_path[18];

struct label_state {
    char text[GUI_TEXT_MAX];
};

struct client_window {
    struct ui_element element;
    struct ui_element *children[CLIENT_CHILDREN];
    struct ui_element labels[CLIENT_LABELS];
    struct ui_element text_entries[CLIENT_TEXT_ENTRIES];
    struct ui_element images[CLIENT_IMAGES];
    struct ui_element buttons[CLIENT_BUTTONS];
    struct label_state label_states[CLIENT_LABELS];
    uint64_t label_ids[CLIENT_LABELS];
    struct ui_text_entry text_entry_states[CLIENT_TEXT_ENTRIES];
    char text_entry_buffers[CLIENT_TEXT_ENTRIES][GUI_TEXT_MAX];
    uint64_t text_entry_ids[CLIENT_TEXT_ENTRIES];
    struct ui_image image_states[CLIENT_IMAGES];
    uint64_t image_ids[CLIENT_IMAGES];
    uint64_t image_widths[CLIENT_IMAGES];
    uint64_t image_heights[CLIENT_IMAGES];
    struct button_state button_states[CLIENT_BUTTONS];
    uint64_t label_count;
    uint64_t text_entry_count;
    uint64_t image_count;
    uint64_t button_count;
    uint64_t pid;
    uint64_t app_window_id;
    char title[GUI_TEXT_MAX];
    uint64_t screen_width;
    uint64_t screen_height;
    int dragging;
    int minimized;
    int open;
};

static struct client_window clients[CLIENT_MAX];
static uint64_t next_window_slot;

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
        to[i] = from[i];
        i++;
    }
    to[i] = '\0';
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

static void delay(void) {
    srv_yield();
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

static void launch_app(const char *label, const char *path) {
    long pid = spawn_bg(path);
    srv_puts("desktop: launched ");
    srv_puts(label);
    srv_puts(" ");
    srv_puts(path);
    srv_puts(" pid=");
    if (pid < 0) {
        srv_puts("failed\n");
        return;
    }
    print_u64((uint64_t)pid);
    srv_puts("\n");
}

static void launch_calc_app(void) {
    calc_label[0] = 'C';
    calc_label[1] = 'A';
    calc_label[2] = 'L';
    calc_label[3] = 'C';
    calc_label[4] = '\0';
    calc_path[0] = '/';
    calc_path[1] = 'f';
    calc_path[2] = 'a';
    calc_path[3] = 't';
    calc_path[4] = '/';
    calc_path[5] = 'b';
    calc_path[6] = 'i';
    calc_path[7] = 'n';
    calc_path[8] = '/';
    calc_path[9] = 'c';
    calc_path[10] = 'a';
    calc_path[11] = 'l';
    calc_path[12] = 'c';
    calc_path[13] = 'g';
    calc_path[14] = 'u';
    calc_path[15] = 'i';
    calc_path[16] = '\0';
    launch_app(calc_label, calc_path);
}

static void launch_notes_app(void) {
    notes_label[0] = 'N';
    notes_label[1] = 'O';
    notes_label[2] = 'T';
    notes_label[3] = 'E';
    notes_label[4] = 'S';
    notes_label[5] = '\0';
    notes_path[0] = '/';
    notes_path[1] = 'f';
    notes_path[2] = 'a';
    notes_path[3] = 't';
    notes_path[4] = '/';
    notes_path[5] = 'b';
    notes_path[6] = 'i';
    notes_path[7] = 'n';
    notes_path[8] = '/';
    notes_path[9] = 'n';
    notes_path[10] = 'o';
    notes_path[11] = 't';
    notes_path[12] = 'e';
    notes_path[13] = 's';
    notes_path[14] = 'g';
    notes_path[15] = 'u';
    notes_path[16] = 'i';
    notes_path[17] = '\0';
    launch_app(notes_label, notes_path);
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

static void draw_cursor(uint64_t x, uint64_t y, uint64_t screen_width,
    uint64_t screen_height, uint32_t color) {
    if (x >= screen_width || y >= screen_height) {
        return;
    }
    uint64_t width = screen_width - x;
    uint64_t height = screen_height - y;
    if (width > CURSOR_W) {
        width = CURSOR_W;
    }
    if (height > CURSOR_H) {
        height = CURSOR_H;
    }
    if (height > 0) {
        fillrect(x, y, width > 2 ? 2 : width, height, color);
    }
    if (width > 0) {
        fillrect(x, y, width, height > 2 ? 2 : height, color);
    }
    if (width > 5 && height > 11) {
        fillrect(x + 3, y + 3, 2, 8, color);
    }
    if (width > 8 && height > 12) {
        fillrect(x + 6, y + 6, 2, 6, color);
    }
    if (width > 11 && height > 13) {
        fillrect(x + 9, y + 9, 2, 4, color);
    }
}

static void draw_label(struct ui_element *element) {
    struct label_state *state = (struct label_state *)element->userdata;
    ui_clear(element, 0x17212b);
    ui_draw_rect(element, 0, 0, element->width, 1, 0x425466);
    ui_draw_text(element, 8, 12, state != 0 ? state->text : "", 0xd7e6ee);
}

static void send_client_event(struct client_window *client, uint64_t type, uint64_t control_id) {
    struct gui_message event = {
        .type = type,
        .target_pid = client != 0 ? client->pid : 0,
        .window_id = client != 0 ? client->app_window_id : 0,
        .control_id = control_id,
    };
    if (client != 0 && client->pid != 0) {
        gui_send(&event);
    }
}

static void send_client_text_event(struct client_window *client, uint64_t control_id,
    const char *text) {
    struct gui_message event = {
        .type = GUI_MSG_EVENT_CHANGE,
        .target_pid = client != 0 ? client->pid : 0,
        .window_id = client != 0 ? client->app_window_id : 0,
        .control_id = control_id,
    };
    if (client != 0 && client->pid != 0) {
        copy_text(event.text, text != 0 ? text : "");
        gui_send(&event);
    }
}

static void send_client_xy_event(struct client_window *client, uint64_t type,
    uint64_t control_id, int64_t x, int64_t y) {
    struct gui_message event = {
        .type = type,
        .target_pid = client != 0 ? client->pid : 0,
        .window_id = client != 0 ? client->app_window_id : 0,
        .control_id = control_id,
        .x = x,
        .y = y,
    };
    if (client != 0 && client->pid != 0) {
        gui_send(&event);
    }
}

static void draw_client_window(struct ui_element *element) {
    struct client_window *client = (struct client_window *)element->userdata;
    const char *title = client != 0 ? client->title : "APP";

    ui_clear(element, UI_TRANSPARENT);
    ui_draw_rect(element, 0, 0, element->width, TITLE_H, 0x2f6f73);
    ui_draw_rect(element, 0, TITLE_H, element->width, 1, 0x8fd0d4);
    ui_draw_rect(element, 0, 0, element->width, 1, 0x9fd8db);
    ui_draw_rect(element, 0, 0, 1, TITLE_H, 0x9fd8db);
    ui_draw_rect(element, element->width - 1, 0, 1, TITLE_H, 0x0b1014);
    ui_draw_rect(element, 7, 7, 10, 10, 0xd9534f);
    ui_draw_rect(element, 23, 7, 10, 10, 0xe0b84f);
    ui_draw_text(element, 42, 8, title, 0xffffff);

    if (client != 0 && client->minimized) {
        return;
    }

    ui_draw_rect(element, 0, TITLE_H + 1, element->width, element->height - TITLE_H - 1, 0x23303a);
    ui_draw_rect(element, 0, element->height - 1, element->width, 1, 0x0b1014);
    ui_draw_rect(element, 0, TITLE_H, 1, element->height - TITLE_H, 0x9fd8db);
    ui_draw_rect(element, element->width - 1, TITLE_H, 1, element->height - TITLE_H, 0x0b1014);
}

static void set_client_children_visible(struct client_window *client, int visible) {
    if (client == 0) {
        return;
    }
    for (size_t i = 0; i < client->element.child_count; i++) {
        client->element.children[i]->visible = visible;
    }
}

static int client_window_event(struct ui_element *element, const struct ui_event *event) {
    struct client_window *client = (struct client_window *)element->userdata;
    if (client == 0) {
        return 0;
    }
    if (event->type == UI_EVENT_MOUSE_DOWN && (event->changed_buttons & MOUSE_LEFT) != 0) {
        ui_element_bring_to_front(element);
        if (event->local_y < TITLE_H) {
            if (event->local_x >= 5 && event->local_x < 19) {
                send_client_event(client, GUI_MSG_EVENT_CLOSE, 0);
                client->open = 0;
                client->pid = 0;
                client->element.visible = 0;
                ui_mark_dirty(element);
                return 1;
            }
            if (event->local_x >= 21 && event->local_x < 35) {
                client->minimized = !client->minimized;
                set_client_children_visible(client, !client->minimized);
                ui_mark_dirty(element);
                return 1;
            }
            client->dragging = 1;
            return 1;
        }
    }
    if (event->type == UI_EVENT_MOUSE_MOVE && client->dragging) {
        int64_t old_x = element->x;
        int64_t old_y = element->y;
        int64_t next_x = element->x + event->dx;
        int64_t next_y = element->y + event->dy;
        int64_t max_x = (int64_t)client->screen_width - (int64_t)TITLE_H;
        int64_t max_y = (int64_t)client->screen_height - (int64_t)TITLE_H;
        if (next_x < 0) {
            next_x = 0;
        }
        if (next_y < 0) {
            next_y = 0;
        }
        if (next_x > max_x) {
            next_x = max_x;
        }
        if (next_y > max_y) {
            next_y = max_y;
        }
        element->x = next_x;
        element->y = next_y;
        if (element->parent != 0) {
            ui_mark_dirty_rect(element->parent, old_x, old_y, element->width, element->height);
            ui_mark_dirty_rect(element->parent, element->x, element->y, element->width, element->height);
        }
        return 1;
    }
    if (event->type == UI_EVENT_MOUSE_UP && client->dragging) {
        client->dragging = 0;
        return 1;
    }
    return 0;
}

static void draw_button(struct ui_element *element) {
    struct button_state *state = (struct button_state *)element->userdata;
    uint32_t face = state != 0 ? state->face : 0x5b7cfa;
    const char *label = state != 0 ? state->label : "BTN";
    if (state != 0 && state->pressed) {
        face = 0x3254b8;
    }
    ui_clear(element, face);
    ui_draw_rect(element, 0, 0, element->width, 1, 0xb5c4ff);
    ui_draw_rect(element, 0, 0, 1, element->height, 0xb5c4ff);
    ui_draw_rect(element, 0, element->height - 1, element->width, 1, 0x142057);
    ui_draw_rect(element, element->width - 1, 0, 1, element->height, 0x142057);
    ui_draw_text(element, 8, 8, label, 0xffffff);
}

static int button_event(struct ui_element *element, const struct ui_event *event) {
    struct button_state *state = (struct button_state *)element->userdata;
    if (state == 0) {
        return 0;
    }
    if (event->type == UI_EVENT_MOUSE_DOWN && (event->changed_buttons & MOUSE_LEFT) != 0) {
        state->pressed = 1;
        ui_mark_dirty(element);
        return 1;
    }
    if (event->type == UI_EVENT_MOUSE_UP && state->pressed) {
        state->pressed = 0;
        ui_mark_dirty(element);
        if (state->action != 0) {
            state->action(element);
        }
        return 1;
    }
    return 0;
}

static void launch_action(struct ui_element *button) {
    struct button_state *state = (struct button_state *)button->userdata;
    if (state != 0 && state->target != 0) {
        launch_app(state->label, (const char *)state->target);
    }
}

static void exit_action(struct ui_element *button) {
    struct button_state *state = (struct button_state *)button->userdata;
    struct desktop_state *desktop = state != 0 ? (struct desktop_state *)state->target : 0;
    if (desktop != 0) {
        desktop->running = 0;
    }
}

static void client_button_action(struct ui_element *button) {
    struct button_state *state = (struct button_state *)button->userdata;
    if (state != 0) {
        send_client_event((struct client_window *)state->target,
            GUI_MSG_EVENT_CLICK,
            state->control_id);
    }
}

static int image_control_event(struct ui_element *element, const struct ui_event *event) {
    struct ui_image *image = (struct ui_image *)element->userdata;
    struct client_window *client = 0;
    uint64_t control_id = 0;
    if (image == 0 || event == 0 || event->type != UI_EVENT_MOUSE_DOWN) {
        return 0;
    }

    for (uint64_t slot = 0; slot < CLIENT_MAX; slot++) {
        for (uint64_t i = 0; i < clients[slot].image_count; i++) {
            if (&clients[slot].image_states[i] == image) {
                client = &clients[slot];
                control_id = clients[slot].image_ids[i];
                break;
            }
        }
        if (client != 0) {
            break;
        }
    }

    if (client == 0 || element->width == 0 || element->height == 0) {
        return 0;
    }

    int64_t ix = event->local_x;
    int64_t iy = event->local_y;
    if (image->width > 0) {
        ix = (event->local_x * (int64_t)image->width) / (int64_t)element->width;
    }
    if (image->height > 0) {
        iy = (event->local_y * (int64_t)image->height) / (int64_t)element->height;
    }
    send_client_xy_event(client, GUI_MSG_EVENT_IMAGE_CLICK, control_id, ix, iy);
    return 1;
}

static struct client_window *find_text_entry_client(struct ui_element *element,
    uint64_t *control_id, const char **text) {
    for (uint64_t slot = 0; slot < CLIENT_MAX; slot++) {
        for (uint64_t i = 0; i < clients[slot].text_entry_count; i++) {
            if (&clients[slot].text_entries[i] == element) {
                if (control_id != 0) {
                    *control_id = clients[slot].text_entry_ids[i];
                }
                if (text != 0) {
                    *text = clients[slot].text_entry_buffers[i];
                }
                return &clients[slot];
            }
        }
    }
    return 0;
}

static void init_button(struct ui_element *button, struct ui_surface surface,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    struct button_state *state, const char *label, uint32_t face,
    void (*action)(struct ui_element *button), void *target, uint64_t control_id) {
    ui_element_init(button, x, y, width, height, surface);
    state->label = label;
    state->pressed = 0;
    state->face = face;
    state->action = action;
    state->target = target;
    state->control_id = control_id;
    button->draw = draw_button;
    button->event = button_event;
    button->userdata = state;
}

static uint64_t find_client_slot(uint64_t pid, uint64_t window_id) {
    for (uint64_t i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].open && clients[i].pid == pid && clients[i].app_window_id == window_id) {
            return i;
        }
    }
    return CLIENT_SLOT_INVALID;
}

static struct client_window *find_client(uint64_t pid, uint64_t window_id) {
    uint64_t slot = find_client_slot(pid, window_id);
    if (slot == CLIENT_SLOT_INVALID) {
        return 0;
    }
    return &clients[slot];
}

static uint64_t alloc_client_slot(uint64_t pid, uint64_t window_id) {
    uint64_t slot = find_client_slot(pid, window_id);
    if (slot != CLIENT_SLOT_INVALID) {
        return slot;
    }
    for (uint64_t count = 0; count < CLIENT_MAX; count++) {
        uint64_t i = (next_window_slot + count) % CLIENT_MAX;
        if (!clients[i].open) {
            clients[i].pid = pid;
            clients[i].app_window_id = window_id;
            clients[i].open = 1;
            clients[i].dragging = 0;
            clients[i].minimized = 0;
            clients[i].label_count = 0;
            clients[i].text_entry_count = 0;
            clients[i].image_count = 0;
            clients[i].button_count = 0;
            clients[i].element.child_count = 0;
            next_window_slot = (i + 1) % CLIENT_MAX;
            return i;
        }
    }
    return CLIENT_SLOT_INVALID;
}

static int64_t cascaded_window_x(uint64_t slot, uint64_t requested_x,
    uint64_t width, uint64_t screen_width) {
    int64_t x = (int64_t)requested_x + (int64_t)((slot % 4) * 28);
    int64_t max_x = (int64_t)screen_width - (int64_t)width;
    if (max_x < 0) {
        max_x = 0;
    }
    while (x > max_x && x > 16) {
        x -= 112;
    }
    if (x < 0) {
        x = 0;
    }
    return x;
}

static int64_t cascaded_window_y(uint64_t slot, uint64_t requested_y,
    uint64_t height, uint64_t screen_height) {
    int64_t y = (int64_t)requested_y + (int64_t)((slot % 4) * 28) + (int64_t)((slot / 4) * 36);
    int64_t max_y = (int64_t)screen_height - (int64_t)height;
    if (max_y < 0) {
        max_y = 0;
    }
    while (y > max_y && y > 28) {
        y -= 96;
    }
    if (y < 0) {
        y = 0;
    }
    return y;
}

static void handle_gui_message(struct ui_element *root, const struct gui_message *message,
    uint64_t screen_width, uint64_t screen_height) {
    struct client_window *client = find_client(message->source_pid, message->window_id);
    struct ui_surface surface;
    (void)root;

    if (message->type == GUI_MSG_CREATE_WINDOW) {
        uint64_t slot = alloc_client_slot(message->source_pid, message->window_id);
        if (slot == CLIENT_SLOT_INVALID) {
            srv_puts("desktop: no free client window slots\n");
            return;
        }
        client = &clients[slot];
        uint64_t width = message->width <= CLIENT_W ? message->width : CLIENT_W;
        uint64_t height = message->height <= CLIENT_H ? message->height : CLIENT_H;
        ui_surface_init(&surface, CLIENT_W, CLIENT_H, CLIENT_W, window_pixels[slot]);
        ui_element_init(&client->element,
            cascaded_window_x(slot, (uint64_t)message->x, width, screen_width),
            cascaded_window_y(slot, (uint64_t)message->y, height, screen_height),
            width,
            height,
            surface);
        client->element.children = client->children;
        client->element.child_capacity = CLIENT_CHILDREN;
        client->element.draw = draw_client_window;
        client->element.event = client_window_event;
        client->element.userdata = client;
        client->element.parent = root;
        client->screen_width = screen_width;
        client->screen_height = screen_height;
        copy_text(client->title, message->text);
        client->element.visible = 1;
        ui_mark_dirty(&client->element);
        ui_element_bring_to_front(&client->element);
        srv_puts("desktop: mapped client window ");
        srv_puts(client->title);
        srv_puts("\n");
        return;
    }

    if (client == 0) {
        return;
    }

    uint64_t slot = (uint64_t)(client - clients);
    if (message->type == GUI_MSG_ADD_LABEL && client->label_count < CLIENT_LABELS) {
        uint64_t i = client->label_count++;
        uint64_t width = message->width <= LABEL_W ? message->width : LABEL_W;
        uint64_t height = message->height <= LABEL_H ? message->height : LABEL_H;
        ui_surface_init(&surface, LABEL_W, LABEL_H, LABEL_W, label_pixels[slot][i]);
        ui_element_init(&client->labels[i], message->x, message->y, width, height, surface);
        copy_text(client->label_states[i].text, message->text);
        client->label_ids[i] = message->control_id;
        client->labels[i].draw = draw_label;
        client->label_states[i].text[GUI_TEXT_MAX - 1] = '\0';
        client->labels[i].event = 0;
        client->labels[i].userdata = &client->label_states[i];
        ui_element_add(&client->element, &client->labels[i]);
        return;
    }

    if (message->type == GUI_MSG_ADD_TEXT_ENTRY && client->text_entry_count < CLIENT_TEXT_ENTRIES) {
        uint64_t i = client->text_entry_count++;
        uint64_t width = message->width <= TEXT_ENTRY_W ? message->width : TEXT_ENTRY_W;
        uint64_t height = message->height <= TEXT_ENTRY_H ? message->height : TEXT_ENTRY_H;
        ui_surface_init(&surface, TEXT_ENTRY_W, TEXT_ENTRY_H, TEXT_ENTRY_W, text_entry_pixels[slot][i]);
        copy_text(client->text_entry_buffers[i], message->text);
        client->text_entry_ids[i] = message->control_id;
        ui_text_entry_init(&client->text_entries[i],
            &client->text_entry_states[i],
            message->x,
            message->y,
            width,
            height,
            surface,
            client->text_entry_buffers[i],
            GUI_TEXT_MAX);
        ui_element_add(&client->element, &client->text_entries[i]);
        return;
    }

    if (message->type == GUI_MSG_ADD_IMAGE && client->image_count < CLIENT_IMAGES) {
        uint64_t i = client->image_count++;
        uint64_t width = message->width <= IMAGE_W ? message->width : IMAGE_W;
        uint64_t height = message->height <= IMAGE_H ? message->height : IMAGE_H;
        uint32_t color = message->value != 0 ? (uint32_t)message->value : 0xffffff;
        for (uint64_t p = 0; p < IMAGE_W * IMAGE_H; p++) {
            image_source_pixels[slot][i][p] = color;
        }
        client->image_ids[i] = message->control_id;
        client->image_widths[i] = width;
        client->image_heights[i] = height;
        ui_surface_init(&surface, IMAGE_W, IMAGE_H, IMAGE_W, image_surface_pixels[slot][i]);
        ui_image_init(&client->images[i],
            &client->image_states[i],
            message->x,
            message->y,
            width,
            height,
            surface,
            image_source_pixels[slot][i],
            width,
            height,
            IMAGE_W);
        client->images[i].event = image_control_event;
        ui_element_add(&client->element, &client->images[i]);
        return;
    }

    if (message->type == GUI_MSG_ADD_BUTTON && client->button_count < CLIENT_BUTTONS) {
        uint64_t i = client->button_count++;
        uint64_t width = message->width <= SMALL_BUTTON_W ? message->width : SMALL_BUTTON_W;
        uint64_t height = message->height <= SMALL_BUTTON_H ? message->height : SMALL_BUTTON_H;
        ui_surface_init(&surface, SMALL_BUTTON_W, SMALL_BUTTON_H, SMALL_BUTTON_W, button_pixels[slot][i]);
        copy_text(client->button_states[i].label_storage, message->text);
        init_button(&client->buttons[i], surface, message->x, message->y, width, height,
            &client->button_states[i], client->button_states[i].label_storage,
            0x5b7cfa, client_button_action, client, message->control_id);
        ui_element_add(&client->element, &client->buttons[i]);
        return;
    }

    if (message->type == GUI_MSG_SET_TEXT) {
        for (uint64_t i = 0; i < client->label_count; i++) {
            if (client->label_ids[i] == message->control_id) {
                copy_text(client->label_states[i].text, message->text);
                ui_mark_dirty(&client->labels[i]);
                return;
            }
        }
        for (uint64_t i = 0; i < client->text_entry_count; i++) {
            if (client->text_entry_ids[i] == message->control_id) {
                ui_text_entry_set_text(&client->text_entries[i], message->text);
                return;
            }
        }
    }

    if (message->type == GUI_MSG_SET_IMAGE_PIXEL) {
        for (uint64_t i = 0; i < client->image_count; i++) {
            if (client->image_ids[i] != message->control_id ||
                message->x < 0 || message->y < 0 ||
                message->x >= (int64_t)client->image_widths[i] ||
                message->y >= (int64_t)client->image_heights[i]) {
                continue;
            }
            uint64_t index = (uint64_t)message->y * IMAGE_W + (uint64_t)message->x;
            image_source_pixels[slot][i][index] = (uint32_t)message->value;
            ui_mark_dirty(&client->images[i]);
            return;
        }
    }

    if (message->type == GUI_MSG_FILL_IMAGE) {
        for (uint64_t i = 0; i < client->image_count; i++) {
            if (client->image_ids[i] == message->control_id) {
                uint32_t color = (uint32_t)message->value;
                for (uint64_t y = 0; y < client->image_heights[i]; y++) {
                    for (uint64_t x = 0; x < client->image_widths[i]; x++) {
                        image_source_pixels[slot][i][y * IMAGE_W + x] = color;
                    }
                }
                ui_mark_dirty(&client->images[i]);
                return;
            }
        }
    }
}

static void present_dirty(struct ui_element *root, uint64_t mouse_x, uint64_t mouse_y,
    uint8_t buttons, int64_t old_x0, int64_t old_y0, uint64_t old_w,
    uint64_t old_h, int old_rect_valid, int need_cursor_present,
    uint64_t old_mouse_x, uint64_t old_mouse_y, uint64_t screen_width,
    uint64_t screen_height) {
    int64_t dirty_x = 0;
    int64_t dirty_y = 0;
    uint64_t dirty_width = 0;
    uint64_t dirty_height = 0;
    if (!ui_dirty_rect(root, &dirty_x, &dirty_y, &dirty_width, &dirty_height)) {
        if (need_cursor_present) {
            ui_present_rect(root, (int64_t)old_mouse_x, (int64_t)old_mouse_y, CURSOR_W, CURSOR_H);
            ui_present_rect(root, (int64_t)mouse_x, (int64_t)mouse_y, CURSOR_W, CURSOR_H);
            draw_cursor(mouse_x, mouse_y, screen_width, screen_height, buttons != 0 ? 0xf87171 : 0xffffff);
        }
        return;
    }

    ui_render_tree(root);
    if (old_rect_valid) {
        ui_present_rect(root, old_x0, old_y0, old_w, old_h);
    }
    ui_present_rect(root, dirty_x, dirty_y, dirty_width, dirty_height);
    if (need_cursor_present) {
        ui_present_rect(root, (int64_t)old_mouse_x, (int64_t)old_mouse_y, CURSOR_W, CURSOR_H);
        ui_present_rect(root, (int64_t)mouse_x, (int64_t)mouse_y, CURSOR_W, CURSOR_H);
    }
    draw_cursor(mouse_x, mouse_y, screen_width, screen_height, buttons != 0 ? 0xf87171 : 0xffffff);
}

int main(int argc, char **argv) {
    struct gfx_info gfx = { 0, 0, 0 };
    struct mouse_event mouse = { 0, 0, 0, 0, 0 };
    struct desktop_state desktop_state = { 1 };
    struct ui_surface root_surface;
    struct ui_surface surface;
    struct ui_element *captured = 0;
    uint64_t mouse_x = 120;
    uint64_t mouse_y = 120;
    uint64_t old_mouse_x = 120;
    uint64_t old_mouse_y = 120;
    uint8_t old_buttons = 0;
    int smoke_autostart = argc > 1 && streq(argv[1], "--smoke-autostart");

    clrscr();
    gfx_info(&gfx);
    if (gfx.width == 0 || gfx.height == 0) {
        gfx.width = 640;
        gfx.height = 480;
    }
    gui_register_server();

    ui_surface_init(&root_surface, gfx.width, gfx.height, gfx.width, 0);
    ui_element_init(&root, 0, 0, gfx.width, gfx.height, root_surface);
    root.background = 0x0e141b;
    root.children = root_children;
    root.child_capacity = ROOT_CHILDREN;
    root.event = 0;
    root.userdata = &desktop_state;

    ui_surface_init(&surface, BUTTON_W, BUTTON_H, BUTTON_W, launch_calc_pixels);
    init_button(&launch_calc, surface, 16, 40, BUTTON_W, BUTTON_H,
        &launch_calc_state, "CALC", 0x397367, launch_action, "/fat/bin/calcgui", 0);
    ui_surface_init(&surface, BUTTON_W, BUTTON_H, BUTTON_W, launch_notes_pixels);
    init_button(&launch_notes, surface, 16, 76, BUTTON_W, BUTTON_H,
        &launch_notes_state, "NOTES", 0x397367, launch_action, "/fat/bin/notesgui", 0);
    ui_surface_init(&surface, BUTTON_W, BUTTON_H, BUTTON_W, launch_edit_pixels);
    init_button(&launch_edit, surface, 16, 112, BUTTON_W, BUTTON_H,
        &launch_edit_state, "EDIT", 0x397367, launch_action, "/fat/bin/textedit", 0);
    ui_surface_init(&surface, BUTTON_W, BUTTON_H, BUTTON_W, launch_paint_pixels);
    init_button(&launch_paint, surface, 16, 148, BUTTON_W, BUTTON_H,
        &launch_paint_state, "PAINT", 0x397367, launch_action, "/fat/bin/imgedit", 0);
    ui_surface_init(&surface, BUTTON_W, BUTTON_H, BUTTON_W, exit_pixels);
    init_button(&exit_button, surface, 16, 184, BUTTON_W, BUTTON_H,
        &exit_button_state, "EXIT", 0x6f3939, exit_action, &desktop_state, 0);
    ui_element_add(&root, &launch_calc);
    ui_element_add(&root, &launch_notes);
    ui_element_add(&root, &launch_edit);
    ui_element_add(&root, &launch_paint);
    ui_element_add(&root, &exit_button);

    for (uint64_t i = 0; i < CLIENT_MAX; i++) {
        ui_surface_init(&surface, CLIENT_W, CLIENT_H, CLIENT_W, window_pixels[i]);
        ui_element_init(&clients[i].element, 160 + (int64_t)(i * 240), 72,
            CLIENT_W, CLIENT_H, surface);
        clients[i].element.children = clients[i].children;
        clients[i].element.child_capacity = CLIENT_CHILDREN;
        clients[i].element.draw = draw_client_window;
        clients[i].element.event = client_window_event;
        clients[i].element.userdata = &clients[i];
        clients[i].element.visible = 0;
        clients[i].screen_width = gfx.width;
        clients[i].screen_height = gfx.height;
        clients[i].open = 0;
        ui_element_add(&root, &clients[i].element);
    }

    ui_render_tree(&root);
    ui_present(&root);
    draw_cursor(mouse_x, mouse_y, gfx.width, gfx.height, 0xffffff);
    gotoxy(2, 1);
    cputs("desktop: freestanding gui apps. Use the EXIT button to quit.");
    if (smoke_autostart) {
        launch_calc_app();
        launch_notes_app();
        launch_app("EDIT", "/fat/bin/textedit");
        launch_app("PAINT", "/fat/bin/imgedit");
    }

    while (desktop_state.running) {
        int key = kbhit();
        int need_cursor_present = 0;
        int old_rect_valid = 0;
        int64_t old_x0 = 0;
        int64_t old_y0 = 0;
        uint64_t old_w = 0;
        uint64_t old_h = 0;
        struct gui_message msg;

        while (gui_recv(&msg) > 0) {
            handle_gui_message(&root, &msg, gfx.width, gfx.height);
        }

        if (key != 0) {
            event_scratch = (struct ui_event) {
                .type = UI_EVENT_KEY_DOWN,
                .x = (int64_t)mouse_x,
                .y = (int64_t)mouse_y,
                .key = key,
            };
            struct ui_element *consumer = ui_dispatch_event(&root, &event_scratch);
            uint64_t control_id = 0;
            const char *text = 0;
            struct client_window *client = find_text_entry_client(consumer, &control_id, &text);
            if (client != 0) {
                send_client_text_event(client, control_id, text);
            }
        }

        if (mouse_scan(&mouse) > 0) {
            uint8_t changed = (uint8_t)(old_buttons ^ mouse.buttons);
            old_mouse_x = mouse_x;
            old_mouse_y = mouse_y;
            if (captured != 0) {
                old_x0 = captured->x;
                old_y0 = captured->y;
                old_w = captured->width;
                old_h = captured->height;
                old_rect_valid = captured->parent == &root;
            }
            if (gfx.width > CURSOR_W) {
                mouse_x = clamp_add(mouse_x, mouse.dx, 0, gfx.width - CURSOR_W);
            }
            if (gfx.height > CURSOR_H) {
                mouse_y = clamp_add(mouse_y, mouse.dy, 0, gfx.height - CURSOR_H);
            }
            if (mouse_x != old_mouse_x || mouse_y != old_mouse_y) {
                event_scratch = (struct ui_event) {
                    .type = UI_EVENT_MOUSE_MOVE,
                    .x = (int64_t)mouse_x,
                    .y = (int64_t)mouse_y,
                    .dx = mouse.dx,
                    .dy = mouse.dy,
                    .buttons = mouse.buttons,
                };
                if (captured != 0) {
                    ui_dispatch_to(captured, &event_scratch);
                } else {
                    ui_dispatch_event(&root, &event_scratch);
                }
                need_cursor_present = 1;
            }
            if ((changed & MOUSE_LEFT) != 0) {
                event_scratch = (struct ui_event) {
                    .type = (mouse.buttons & MOUSE_LEFT) != 0 ? UI_EVENT_MOUSE_DOWN : UI_EVENT_MOUSE_UP,
                    .x = (int64_t)mouse_x,
                    .y = (int64_t)mouse_y,
                    .buttons = mouse.buttons,
                    .changed_buttons = MOUSE_LEFT,
                };
                if (event_scratch.type == UI_EVENT_MOUSE_DOWN) {
                    captured = ui_dispatch_event(&root, &event_scratch);
                } else if (captured != 0) {
                    ui_dispatch_to(captured, &event_scratch);
                    captured = 0;
                } else {
                    ui_dispatch_event(&root, &event_scratch);
                }
            }
            old_buttons = mouse.buttons;
        }

        present_dirty(&root, mouse_x, mouse_y, old_buttons, old_x0, old_y0,
            old_w, old_h, old_rect_valid, need_cursor_present, old_mouse_x,
            old_mouse_y, gfx.width, gfx.height);
        delay();
    }

    for (uint64_t i = 0; i < CLIENT_MAX; i++) {
        if (clients[i].pid != 0) {
            send_client_event(&clients[i], GUI_MSG_EVENT_CLOSE, 0);
        }
    }

    gotoxy(2, 3);
    cputs("desktop exited\n");
    return 0;
}
