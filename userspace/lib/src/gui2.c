#include <srvros/gui.h>
#include <srvros/gui2.h>

#include <stdlib.h>
#include <string.h>

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

static int64_t max_i64(int64_t a, int64_t b) {
    return a > b ? a : b;
}

static int64_t min_i64(int64_t a, int64_t b) {
    return a < b ? a : b;
}

static int event_hits(const struct gui2_event *event,
    int64_t x, int64_t y, uint64_t width, uint64_t height) {
    return event != 0 &&
        event->x >= x && event->y >= y &&
        (uint64_t)(event->x - x) < width &&
        (uint64_t)(event->y - y) < height;
}

static void send_window_msg(const struct gui2_window *window, uint64_t type,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    int64_t value, const char *text) {
    struct gui_message msg;
    if (window == 0 || window->surface_id == 0) {
        return;
    }
    memset(&msg, 0, sizeof(msg));
    msg.type = type;
    msg.window_id = window->window_id;
    msg.x = x;
    msg.y = y;
    msg.width = width;
    msg.height = height;
    msg.value = value;
    copy_text(msg.text, text);
    gui_send(&msg);
}

uint32_t gui2_rgb(uint8_t r, uint8_t g, uint8_t b) {
    return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

int gui2_window_open(struct gui2_window *window, uint64_t window_id,
    const char *title, int64_t x, int64_t y, uint64_t width,
    uint64_t height, uint32_t background) {
    if (window == 0 || width == 0 || height == 0 ||
        width > SIZE_MAX / height ||
        width * height > SIZE_MAX / sizeof(uint32_t)) {
        return -1;
    }
    memset(window, 0, sizeof(*window));
    window->pixels = (uint32_t *)malloc((size_t)(width * height * sizeof(uint32_t)));
    if (window->pixels == 0) {
        return -1;
    }
    if (gui_surface_create(width, height, 0, &window->surface_id) != 0 ||
        window->surface_id == 0) {
        free(window->pixels);
        memset(window, 0, sizeof(*window));
        return -1;
    }

    window->window_id = window_id;
    window->x = x;
    window->y = y;
    window->width = width;
    window->height = height;
    window->background = background;
    window->opened = 1;
    ui_surface_init(&window->surface, width, height, width, window->pixels);
    ui_element_init(&window->root, 0, 0, width, height, window->surface);
    window->root.background = background;
    gui2_clear(window, background);
    if (gui_surface_blit(window->surface_id, 0, 0, width, height,
            window->pixels, width) != 0) {
        gui_surface_destroy(window->surface_id);
        free(window->pixels);
        memset(window, 0, sizeof(*window));
        return -1;
    }
    window->dirty = 0;
    window->dirty_x = 0;
    window->dirty_y = 0;
    window->dirty_width = 0;
    window->dirty_height = 0;
    send_window_msg(window, GUI_MSG_V2_CREATE_SURFACE_WINDOW, x, y,
        width, height, (int64_t)window->surface_id, title);
    send_window_msg(window, GUI_MSG_V2_DAMAGE_SURFACE, 0, 0,
        width, height, (int64_t)window->surface_id, "");
    return 0;
}

void gui2_window_close(struct gui2_window *window) {
    if (window == 0 || !window->opened) {
        return;
    }
    send_window_msg(window, GUI_MSG_V2_DESTROY_SURFACE, 0, 0,
        window->width, window->height, (int64_t)window->surface_id, "");
    gui_surface_destroy(window->surface_id);
    free(window->pixels);
    memset(window, 0, sizeof(*window));
}

void gui2_window_mark_dirty(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    if (window == 0 || width == 0 || height == 0) {
        return;
    }
    int64_t x0 = max_i64(x, 0);
    int64_t y0 = max_i64(y, 0);
    int64_t x1 = min_i64(x + (int64_t)width, (int64_t)window->width);
    int64_t y1 = min_i64(y + (int64_t)height, (int64_t)window->height);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }
    if (!window->dirty || window->dirty_width == 0 || window->dirty_height == 0) {
        window->dirty_x = x0;
        window->dirty_y = y0;
        window->dirty_width = (uint64_t)(x1 - x0);
        window->dirty_height = (uint64_t)(y1 - y0);
        window->dirty = 1;
        return;
    }
    int64_t old_x1 = window->dirty_x + (int64_t)window->dirty_width;
    int64_t old_y1 = window->dirty_y + (int64_t)window->dirty_height;
    int64_t union_x0 = min_i64(window->dirty_x, x0);
    int64_t union_y0 = min_i64(window->dirty_y, y0);
    int64_t union_x1 = max_i64(old_x1, x1);
    int64_t union_y1 = max_i64(old_y1, y1);
    window->dirty_x = union_x0;
    window->dirty_y = union_y0;
    window->dirty_width = (uint64_t)(union_x1 - union_x0);
    window->dirty_height = (uint64_t)(union_y1 - union_y0);
    window->dirty = 1;
}

int gui2_window_present(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    if (window == 0 || window->surface_id == 0 || window->pixels == 0 ||
        width == 0 || height == 0 || x < 0 || y < 0 ||
        (uint64_t)x >= window->width || (uint64_t)y >= window->height) {
        return -1;
    }
    if ((uint64_t)x + width > window->width) {
        width = window->width - (uint64_t)x;
    }
    if ((uint64_t)y + height > window->height) {
        height = window->height - (uint64_t)y;
    }
    if (gui_surface_blit(window->surface_id, (uint64_t)x, (uint64_t)y,
            width, height, window->pixels + (uint64_t)y * window->width + (uint64_t)x,
            window->width) != 0) {
        return -1;
    }
    send_window_msg(window, GUI_MSG_V2_DAMAGE_SURFACE, x, y,
        width, height, (int64_t)window->surface_id, "");
    return 0;
}

int gui2_window_present_dirty(struct gui2_window *window) {
    if (window == 0 || !window->dirty) {
        return 0;
    }
    int result = gui2_window_present(window, window->dirty_x, window->dirty_y,
        window->dirty_width, window->dirty_height);
    if (result == 0) {
        window->dirty = 0;
        window->dirty_x = 0;
        window->dirty_y = 0;
        window->dirty_width = 0;
        window->dirty_height = 0;
    }
    return result;
}

int gui2_poll_event(struct gui2_window *window, struct gui2_event *event) {
    struct gui_message msg;
    if (event == 0) {
        return -1;
    }
    memset(event, 0, sizeof(*event));
    if (gui_recv(&msg) <= 0) {
        return 0;
    }
    event->x = msg.x;
    event->y = msg.y;
    event->width = msg.width;
    event->height = msg.height;
    event->value = msg.value;
    if (msg.type == GUI_MSG_V2_EVENT_CONFIGURE) {
        event->type = GUI2_EVENT_CONFIGURE;
    } else if (msg.type == GUI_MSG_V2_EVENT_FOCUS) {
        event->type = GUI2_EVENT_FOCUS;
        event->focused = msg.value != 0;
        if (window != 0) {
            window->focused = event->focused;
        }
    } else if (msg.type == GUI_MSG_V2_EVENT_POINTER_MOVE ||
        msg.type == GUI_MSG_V2_EVENT_POINTER_BUTTON) {
        uint8_t new_buttons = (uint8_t)msg.value;
        event->type = msg.type == GUI_MSG_V2_EVENT_POINTER_MOVE ?
            GUI2_EVENT_POINTER_MOVE : GUI2_EVENT_POINTER_BUTTON;
        event->buttons = new_buttons;
        if (window != 0) {
            event->changed_buttons = (uint8_t)(window->buttons ^ new_buttons);
            window->buttons = new_buttons;
        }
    } else if (msg.type == GUI_MSG_V2_EVENT_KEY_DOWN) {
        event->type = GUI2_EVENT_KEY_DOWN;
        event->key = (int)msg.value;
    }
    return event->type == GUI2_EVENT_NONE ? 0 : 1;
}

void gui2_clear(struct gui2_window *window, uint32_t color) {
    if (window == 0) {
        return;
    }
    ui_draw_rect(&window->root, 0, 0, window->width, window->height, color);
    gui2_window_mark_dirty(window, 0, 0, window->width, window->height);
}

void gui2_rect(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t color) {
    if (window == 0) {
        return;
    }
    ui_draw_rect(&window->root, x, y, width, height, color);
    gui2_window_mark_dirty(window, x, y, width, height);
}

void gui2_text(struct gui2_window *window, int64_t x, int64_t y,
    const char *text, uint32_t color) {
    if (window == 0) {
        return;
    }
    ui_draw_text(&window->root, x, y, text, color);
    gui2_window_mark_dirty(window, x, y, 8 * (text != 0 ? strlen(text) : 1), 9);
}

void gui2_button_init(struct gui2_button *button, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const char *label) {
    if (button == 0) {
        return;
    }
    memset(button, 0, sizeof(*button));
    button->x = x;
    button->y = y;
    button->width = width;
    button->height = height;
    button->label = label;
}

void gui2_button_draw(struct gui2_window *window, const struct gui2_button *button) {
    if (window == 0 || button == 0) {
        return;
    }
    uint32_t fill = button->pressed ? gui2_rgb(0x2f, 0x4b, 0x74) :
        button->hovered ? gui2_rgb(0x4f, 0x74, 0xb8) : gui2_rgb(0x3d, 0x63, 0xdf);
    gui2_rect(window, button->x, button->y, button->width, button->height, fill);
    gui2_rect(window, button->x, button->y, button->width, 1, gui2_rgb(0xb9, 0xd8, 0xdf));
    gui2_rect(window, button->x, button->y + (int64_t)button->height - 1,
        button->width, 1, gui2_rgb(0x18, 0x24, 0x34));
    gui2_text(window, button->x + 8,
        button->y + (int64_t)((button->height - 7) / 2), button->label, gui2_rgb(0xff, 0xff, 0xff));
}

int gui2_button_event(struct gui2_button *button, const struct gui2_event *event) {
    if (button == 0 || event == 0) {
        return 0;
    }
    int hit = event_hits(event, button->x, button->y, button->width, button->height);
    if (event->type == GUI2_EVENT_POINTER_MOVE) {
        int old = button->hovered;
        button->hovered = hit;
        return old != button->hovered;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0) {
        int old_pressed = button->pressed;
        button->pressed = hit && (event->buttons & 1) != 0;
        if (old_pressed && (event->buttons & 1) == 0 && hit) {
            button->clicks++;
            return 1;
        }
        return old_pressed != button->pressed;
    }
    return 0;
}

void gui2_textbox_init(struct gui2_textbox *textbox, int64_t x, int64_t y,
    uint64_t width, uint64_t height, char *buffer, size_t capacity) {
    if (textbox == 0) {
        return;
    }
    memset(textbox, 0, sizeof(*textbox));
    textbox->x = x;
    textbox->y = y;
    textbox->width = width;
    textbox->height = height;
    textbox->buffer = buffer;
    textbox->capacity = capacity;
    if (buffer != 0 && capacity != 0) {
        buffer[0] = '\0';
    }
}

void gui2_textbox_draw(struct gui2_window *window, const struct gui2_textbox *textbox) {
    if (window == 0 || textbox == 0) {
        return;
    }
    uint32_t border = textbox->focused ? gui2_rgb(0xf5, 0xb8, 0x4b) : gui2_rgb(0x6a, 0x86, 0x95);
    gui2_rect(window, textbox->x, textbox->y, textbox->width, textbox->height, gui2_rgb(0x0f, 0x17, 0x20));
    gui2_rect(window, textbox->x, textbox->y, textbox->width, 1, border);
    gui2_rect(window, textbox->x, textbox->y + (int64_t)textbox->height - 1, textbox->width, 1, border);
    gui2_rect(window, textbox->x, textbox->y, 1, textbox->height, border);
    gui2_rect(window, textbox->x + (int64_t)textbox->width - 1, textbox->y, 1, textbox->height, border);
    gui2_text(window, textbox->x + 8, textbox->y + 8,
        textbox->buffer != 0 && textbox->buffer[0] != '\0' ? textbox->buffer : "TYPE HERE",
        textbox->buffer != 0 && textbox->buffer[0] != '\0' ?
            gui2_rgb(0xff, 0xff, 0xff) : gui2_rgb(0x8f, 0xa9, 0xb6));
}

int gui2_textbox_event(struct gui2_textbox *textbox, const struct gui2_event *event) {
    if (textbox == 0 || event == 0) {
        return 0;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0 &&
        (event->buttons & 1) != 0) {
        int was_focused = textbox->focused;
        textbox->focused = event_hits(event, textbox->x, textbox->y,
            textbox->width, textbox->height);
        return was_focused != textbox->focused;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN && textbox->focused &&
        textbox->buffer != 0 && textbox->capacity != 0) {
        if ((event->key == 8 || event->key == 127) && textbox->length > 0) {
            textbox->buffer[--textbox->length] = '\0';
            return 1;
        }
        if (event->key >= 32 && event->key < 127 && textbox->length + 1 < textbox->capacity) {
            textbox->buffer[textbox->length++] = (char)event->key;
            textbox->buffer[textbox->length] = '\0';
            return 1;
        }
    }
    return 0;
}
