#include <srvros/cli.h>
#include <srvros/gui.h>
#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdlib.h>
#include <string.h>

static const struct gui2_theme default_theme = {
    .canvas = 0x0f1417,
    .panel = 0x192327,
    .panel_alt = 0x202b30,
    .field = 0x0c1114,
    .border = 0x50656a,
    .border_focus = 0xd6a64a,
    .text = 0xf3f5f0,
    .text_muted = 0xa2b0ac,
    .accent = 0x3f6f68,
    .accent_hover = 0x4a7c74,
    .accent_down = 0x2f554f,
    .danger = 0x8b4b50,
    .pad = 12,
    .gap = 8,
    .control_h = 30,
    .toolbar_h = 38,
    .status_h = 24,
    .list_row_h = 22,
};

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

static int rect_contains(int64_t x, int64_t y, uint64_t width, uint64_t height,
    int64_t px, int64_t py) {
    return px >= x && py >= y &&
        (uint64_t)(px - x) < width &&
        (uint64_t)(py - y) < height;
}

static void draw_control_border(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t color, int focused) {
    if (window == 0 || width == 0 || height == 0) {
        return;
    }
    gui2_rect(window, x, y, width, 1, color);
    gui2_rect(window, x, y + (int64_t)height - 1, width, 1, color);
    gui2_rect(window, x, y, 1, height, color);
    gui2_rect(window, x + (int64_t)width - 1, y, 1, height, color);
    if (focused && width > 4 && height > 4) {
        gui2_rect(window, x + 2, y + 2, width - 4, 1, color);
        gui2_rect(window, x + 2, y + (int64_t)height - 3, width - 4, 1, color);
        gui2_rect(window, x + 2, y + 2, 1, height - 4, color);
        gui2_rect(window, x + (int64_t)width - 3, y + 2, 1, height - 4, color);
    }
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

const struct gui2_theme *gui2_theme_default(void) {
    return &default_theme;
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

int gui2_window_resize(struct gui2_window *window, uint64_t width, uint64_t height) {
    uint32_t *pixels;
    uint32_t *old_pixels;
    uint64_t surface_id = 0;
    uint64_t old_surface_id;
    uint64_t old_width;
    uint64_t old_height;
    if (window == 0 || !window->opened || width == 0 || height == 0 ||
        width > SIZE_MAX / height ||
        width * height > SIZE_MAX / sizeof(uint32_t)) {
        return -1;
    }
    if (window->width == width && window->height == height) {
        return 0;
    }
    pixels = (uint32_t *)malloc((size_t)(width * height * sizeof(uint32_t)));
    if (pixels == 0) {
        return -1;
    }
    if (gui_surface_create(width, height, 0, &surface_id) != 0 || surface_id == 0) {
        free(pixels);
        return -1;
    }
    old_surface_id = window->surface_id;
    old_pixels = window->pixels;
    old_width = window->width;
    old_height = window->height;
    window->pixels = pixels;
    window->surface_id = surface_id;
    window->width = width;
    window->height = height;
    ui_surface_init(&window->surface, width, height, width, window->pixels);
    ui_element_init(&window->root, 0, 0, width, height, window->surface);
    window->root.background = window->background;
    gui2_clear(window, window->background);
    if (gui_surface_blit(window->surface_id, 0, 0, width, height,
            window->pixels, width) != 0) {
        gui_surface_destroy(window->surface_id);
        free(window->pixels);
        window->pixels = old_pixels;
        window->surface_id = old_surface_id;
        window->width = old_width;
        window->height = old_height;
        ui_surface_init(&window->surface, old_width, old_height, old_width, window->pixels);
        ui_element_init(&window->root, 0, 0, old_width, old_height, window->surface);
        window->root.background = window->background;
        return -1;
    }
    free(old_pixels);
    send_window_msg(window, GUI_MSG_V2_CREATE_SURFACE_WINDOW, window->x, window->y,
        width, height, (int64_t)window->surface_id, "");
    send_window_msg(window, GUI_MSG_V2_DAMAGE_SURFACE, 0, 0,
        width, height, (int64_t)window->surface_id, "");
    if (old_surface_id != 0) {
        gui_surface_destroy(old_surface_id);
    }
    return 0;
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
        if (window != 0) {
            window->x = msg.x;
            window->y = msg.y;
        }
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
    } else if (msg.type == GUI_MSG_EVENT_CLOSE) {
        event->type = GUI2_EVENT_CLOSE;
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

void gui2_panel(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t fill) {
    const struct gui2_theme *theme = gui2_theme_default();
    if (window == 0 || width == 0 || height == 0) {
        return;
    }
    gui2_rect(window, x, y, width, height, fill);
    gui2_rect(window, x, y, width, 1, theme->border);
    gui2_rect(window, x, y + (int64_t)height - 1, width, 1, theme->border);
    gui2_rect(window, x, y, 1, height, theme->border);
    gui2_rect(window, x + (int64_t)width - 1, y, 1, height, theme->border);
}

void gui2_label(struct gui2_window *window, int64_t x, int64_t y,
    const char *text) {
    gui2_text(window, x, y, text, gui2_theme_default()->text);
}

void gui2_app_header(struct gui2_window *window, const char *title,
    const char *subtitle) {
    const struct gui2_theme *theme = gui2_theme_default();
    if (window == 0) {
        return;
    }
    gui2_rect(window, 0, 0, window->width, theme->toolbar_h, theme->panel);
    gui2_rect(window, 0, theme->toolbar_h - 1, window->width, 1, theme->border);
    if (window->width > theme->pad * 2) {
        gui2_rect(window, (int64_t)theme->pad, theme->toolbar_h - 4,
            window->width - theme->pad * 2, 1, theme->panel_alt);
    }
    gui2_text(window, (int64_t)theme->pad, 9, title != 0 ? title : "", theme->text);
    if (subtitle != 0 && subtitle[0] != '\0') {
        gui2_text(window, (int64_t)(theme->pad + 88), 9, subtitle, theme->text_muted);
    }
}

void gui2_status_bar(struct gui2_window *window, const char *left,
    const char *right) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t y;
    if (window == 0 || window->height < theme->status_h) {
        return;
    }
    y = window->height - theme->status_h;
    gui2_rect(window, 0, (int64_t)y, window->width, theme->status_h, theme->panel);
    gui2_rect(window, 0, (int64_t)y, window->width, 1, theme->border);
    if (window->width > theme->pad * 2 && theme->status_h > 6) {
        gui2_rect(window, (int64_t)theme->pad, (int64_t)y + 4,
            window->width - theme->pad * 2, 1, theme->panel_alt);
    }
    gui2_text(window, (int64_t)theme->pad, (int64_t)y + 8,
        left != 0 ? left : "", theme->text_muted);
    if (right != 0 && right[0] != '\0' && window->width > 180) {
        int64_t x = (int64_t)window->width - (int64_t)theme->pad - 132;
        if (x < (int64_t)theme->pad) {
            x = (int64_t)theme->pad;
        }
        gui2_text(window, x, (int64_t)y + 8, right, theme->text_muted);
    }
}

void gui2_layout_button_row(struct gui2_button *buttons, size_t count,
    int64_t x, int64_t y, uint64_t width, uint64_t height, uint64_t gap) {
    uint64_t button_w;
    if (buttons == 0 || count == 0) {
        return;
    }
    if (height == 0) {
        height = gui2_theme_default()->control_h;
    }
    if (count > 1 && width > gap * (count - 1)) {
        button_w = (width - gap * (count - 1)) / count;
    } else {
        button_w = width / count;
    }
    if (button_w == 0) {
        button_w = 1;
    }
    for (size_t i = 0; i < count; i++) {
        buttons[i].x = x + (int64_t)(i * (button_w + gap));
        buttons[i].y = y;
        buttons[i].width = button_w;
        buttons[i].height = height;
    }
}

void gui2_layout_begin(struct gui2_layout *layout, int64_t x, int64_t y,
    uint64_t width) {
    const struct gui2_theme *theme = gui2_theme_default();
    if (layout == 0) {
        return;
    }
    layout->x = x;
    layout->y = y;
    layout->width = width;
    layout->gap = theme->gap;
    layout->control_h = theme->control_h;
}

struct gui2_rect gui2_layout_next(struct gui2_layout *layout, uint64_t height) {
    struct gui2_rect rect = {0};
    if (layout == 0) {
        return rect;
    }
    if (height == 0) {
        height = layout->control_h;
    }
    rect.x = layout->x;
    rect.y = layout->y;
    rect.width = layout->width;
    rect.height = height;
    layout->y += (int64_t)(height + layout->gap);
    return rect;
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
    const struct gui2_theme *theme = gui2_theme_default();
    if (window == 0 || button == 0) {
        return;
    }
    uint32_t fill = button->pressed ? theme->accent_down :
        button->hovered ? theme->accent_hover : theme->accent;
    uint32_t border = button->focused ? theme->border_focus : theme->border;
    gui2_rect(window, button->x, button->y, button->width, button->height, fill);
    if (button->width > 4 && button->height > 4) {
        gui2_rect(window, button->x + 1, button->y + 1, button->width - 2, 1,
            button->pressed ? theme->accent_down : theme->accent_hover);
    }
    draw_control_border(window, button->x, button->y, button->width,
        button->height, border, button->focused);
    gui2_text(window, button->x + 8,
        button->y + (int64_t)((button->height - 7) / 2), button->label, theme->text);
}

int gui2_button_event(struct gui2_button *button, const struct gui2_event *event) {
    if (button == 0 || event == 0) {
        return 0;
    }
    int hit = event_hits(event, button->x, button->y, button->width, button->height);
    if (event->type == GUI2_EVENT_POINTER_MOVE) {
        int old = button->hovered;
        button->hovered = hit;
        return old != button->hovered ? GUI2_WIDGET_DIRTY : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0) {
        int old_pressed = button->pressed;
        button->pressed = hit && (event->buttons & 1) != 0;
        if (old_pressed && (event->buttons & 1) == 0 && hit) {
            button->clicks++;
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
        }
        return old_pressed != button->pressed ? GUI2_WIDGET_DIRTY : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN && button->focused &&
        (event->key == '\n' || event->key == '\r' || event->key == ' ')) {
        button->clicks++;
        return GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
    }
    return GUI2_WIDGET_NONE;
}

int gui2_button_contains(const struct gui2_button *button, int64_t x, int64_t y) {
    return button != 0 && rect_contains(button->x, button->y,
        button->width, button->height, x, y);
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
    textbox->length = 0;
    textbox->cursor = 0;
    textbox->placeholder = "TYPE HERE";
}

void gui2_textbox_set_placeholder(struct gui2_textbox *textbox, const char *placeholder) {
    if (textbox != 0) {
        textbox->placeholder = placeholder;
    }
}

void gui2_textbox_draw(struct gui2_window *window, const struct gui2_textbox *textbox) {
    const struct gui2_theme *theme = gui2_theme_default();
    if (window == 0 || textbox == 0) {
        return;
    }
    uint32_t border = textbox->focused ? theme->border_focus : theme->border;
    int has_text = textbox->buffer != 0 && textbox->buffer[0] != '\0';
    gui2_rect(window, textbox->x, textbox->y, textbox->width, textbox->height, theme->field);
    draw_control_border(window, textbox->x, textbox->y, textbox->width,
        textbox->height, border, textbox->focused);
    gui2_text(window, textbox->x + 8, textbox->y + 8,
        has_text ? textbox->buffer : textbox->placeholder,
        has_text ? theme->text : theme->text_muted);
    if (textbox->focused) {
        int64_t cursor_x = textbox->x + 8 + (int64_t)(textbox->cursor * 6);
        if (cursor_x < textbox->x + (int64_t)textbox->width - 5) {
            gui2_rect(window, cursor_x, textbox->y + 6, 1, textbox->height - 12,
                theme->border_focus);
        }
    }
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
        if (textbox->focused) {
            textbox->cursor = textbox->length;
        }
        return was_focused != textbox->focused ? GUI2_WIDGET_DIRTY | GUI2_WIDGET_FOCUS : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN && textbox->focused &&
        textbox->buffer != 0 && textbox->capacity != 0) {
        if ((event->key == 8 || event->key == 127) && textbox->cursor > 0) {
            for (size_t i = textbox->cursor - 1; i < textbox->length; i++) {
                textbox->buffer[i] = textbox->buffer[i + 1];
            }
            textbox->cursor--;
            textbox->length--;
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 4 && textbox->cursor < textbox->length) {
            for (size_t i = textbox->cursor; i < textbox->length; i++) {
                textbox->buffer[i] = textbox->buffer[i + 1];
            }
            textbox->length--;
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 2 && textbox->cursor > 0) {
            textbox->cursor--;
            return GUI2_WIDGET_DIRTY;
        }
        if (event->key == 6 && textbox->cursor < textbox->length) {
            textbox->cursor++;
            return GUI2_WIDGET_DIRTY;
        }
        if (event->key >= 32 && event->key < 127 && textbox->length + 1 < textbox->capacity) {
            for (size_t i = textbox->length + 1; i > textbox->cursor; i--) {
                textbox->buffer[i] = textbox->buffer[i - 1];
            }
            textbox->buffer[textbox->cursor++] = (char)event->key;
            textbox->length++;
            textbox->buffer[textbox->length] = '\0';
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
    }
    return GUI2_WIDGET_NONE;
}

int gui2_textbox_contains(const struct gui2_textbox *textbox, int64_t x, int64_t y) {
    return textbox != 0 && rect_contains(textbox->x, textbox->y,
        textbox->width, textbox->height, x, y);
}

static size_t text_length_bounded(const char *text, size_t capacity) {
    size_t length = 0;
    if (text == 0 || capacity == 0) {
        return 0;
    }
    while (length + 1 < capacity && text[length] != '\0') {
        length++;
    }
    return length;
}

static size_t textarea_line_for_index(const struct gui2_textarea *textarea, size_t index) {
    size_t line = 0;
    if (textarea == 0 || textarea->buffer == 0) {
        return 0;
    }
    if (index > textarea->length) {
        index = textarea->length;
    }
    for (size_t i = 0; i < index; i++) {
        if (textarea->buffer[i] == '\n') {
            line++;
        }
    }
    return line;
}

static size_t textarea_line_start(const struct gui2_textarea *textarea, size_t line) {
    size_t current_line = 0;
    if (textarea == 0 || textarea->buffer == 0) {
        return 0;
    }
    if (line == 0) {
        return 0;
    }
    for (size_t i = 0; i < textarea->length; i++) {
        if (textarea->buffer[i] == '\n') {
            current_line++;
            if (current_line == line) {
                return i + 1;
            }
        }
    }
    return textarea->length;
}

static size_t textarea_line_end(const struct gui2_textarea *textarea, size_t start) {
    size_t i = start;
    if (textarea == 0 || textarea->buffer == 0) {
        return 0;
    }
    while (i < textarea->length && textarea->buffer[i] != '\n') {
        i++;
    }
    return i;
}

static void textarea_keep_cursor_visible(struct gui2_textarea *textarea) {
    size_t cursor_line;
    size_t visible_lines;
    if (textarea == 0) {
        return;
    }
    visible_lines = textarea->height > 18 ? (textarea->height - 16) / 12 : 1;
    if (visible_lines == 0) {
        visible_lines = 1;
    }
    cursor_line = textarea_line_for_index(textarea, textarea->cursor);
    if (cursor_line < textarea->scroll_line) {
        textarea->scroll_line = cursor_line;
    } else if (cursor_line >= textarea->scroll_line + visible_lines) {
        textarea->scroll_line = cursor_line - visible_lines + 1;
    }
}

static void textarea_delete_range(struct gui2_textarea *textarea, size_t start, size_t count) {
    if (textarea == 0 || textarea->buffer == 0 || count == 0 ||
        start >= textarea->length) {
        return;
    }
    if (start + count > textarea->length) {
        count = textarea->length - start;
    }
    for (size_t i = start; i + count <= textarea->length; i++) {
        textarea->buffer[i] = textarea->buffer[i + count];
    }
    textarea->length -= count;
    if (textarea->cursor > textarea->length) {
        textarea->cursor = textarea->length;
    }
}

static int textarea_insert_char(struct gui2_textarea *textarea, char c) {
    if (textarea == 0 || textarea->buffer == 0 ||
        textarea->capacity == 0 || textarea->length + 1 >= textarea->capacity) {
        return 0;
    }
    for (size_t i = textarea->length + 1; i > textarea->cursor; i--) {
        textarea->buffer[i] = textarea->buffer[i - 1];
    }
    textarea->buffer[textarea->cursor++] = c;
    textarea->length++;
    textarea->buffer[textarea->length] = '\0';
    textarea_keep_cursor_visible(textarea);
    return 1;
}

static void draw_text_line_clipped(struct gui2_window *window, int64_t x, int64_t y,
    const char *text, size_t length, uint64_t max_chars, uint32_t color) {
    char line[96];
    size_t out = 0;
    if (max_chars >= sizeof(line)) {
        max_chars = sizeof(line) - 1;
    }
    while (text != 0 && out < length && out < max_chars) {
        line[out] = text[out];
        out++;
    }
    line[out] = '\0';
    gui2_text(window, x, y, line, color);
}

void gui2_textarea_init(struct gui2_textarea *textarea, int64_t x, int64_t y,
    uint64_t width, uint64_t height, char *buffer, size_t capacity) {
    if (textarea == 0) {
        return;
    }
    memset(textarea, 0, sizeof(*textarea));
    textarea->x = x;
    textarea->y = y;
    textarea->width = width;
    textarea->height = height;
    textarea->buffer = buffer;
    textarea->capacity = capacity;
    if (buffer != 0 && capacity != 0) {
        buffer[0] = '\0';
    }
    textarea->placeholder = "TYPE HERE";
}

void gui2_textarea_set_placeholder(struct gui2_textarea *textarea, const char *placeholder) {
    if (textarea != 0) {
        textarea->placeholder = placeholder;
    }
}

void gui2_textarea_sync(struct gui2_textarea *textarea) {
    if (textarea == 0) {
        return;
    }
    textarea->length = text_length_bounded(textarea->buffer, textarea->capacity);
    textarea->cursor = textarea->length;
    if (textarea->buffer != 0 && textarea->capacity != 0) {
        textarea->buffer[textarea->length] = '\0';
    }
    textarea_keep_cursor_visible(textarea);
}

void gui2_textarea_set_cursor(struct gui2_textarea *textarea, size_t cursor) {
    if (textarea == 0) {
        return;
    }
    if (cursor > textarea->length) {
        cursor = textarea->length;
    }
    textarea->cursor = cursor;
    textarea_keep_cursor_visible(textarea);
}

void gui2_textarea_draw(struct gui2_window *window, const struct gui2_textarea *textarea) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t max_chars;
    uint64_t visible_lines;
    uint32_t border;
    if (window == 0 || textarea == 0) {
        return;
    }
    border = textarea->focused ? theme->border_focus : theme->border;
    max_chars = textarea->width > 18 ? (textarea->width - 18) / 8 : 1;
    visible_lines = textarea->height > 18 ? (textarea->height - 16) / 12 : 1;
    if (visible_lines == 0) {
        visible_lines = 1;
    }
    gui2_rect(window, textarea->x, textarea->y,
        textarea->width, textarea->height, theme->field);
    draw_control_border(window, textarea->x, textarea->y, textarea->width,
        textarea->height, border, textarea->focused);
    if (textarea->buffer == 0 || textarea->buffer[0] == '\0') {
        gui2_text(window, textarea->x + 8, textarea->y + 8,
            textarea->placeholder, theme->text_muted);
    } else {
        for (uint64_t row = 0; row < visible_lines; row++) {
            size_t line = textarea->scroll_line + row;
            size_t start = textarea_line_start(textarea, line);
            size_t end = textarea_line_end(textarea, start);
            if (start >= textarea->length && line > textarea_line_for_index(textarea, textarea->length)) {
                break;
            }
            draw_text_line_clipped(window, textarea->x + 8,
                textarea->y + 8 + (int64_t)(row * 12),
                textarea->buffer + start, end - start, max_chars, theme->text);
        }
    }
    if (textarea->focused) {
        size_t cursor_line = textarea_line_for_index(textarea, textarea->cursor);
        if (cursor_line >= textarea->scroll_line &&
            cursor_line < textarea->scroll_line + visible_lines) {
            size_t line_start = textarea_line_start(textarea, cursor_line);
            uint64_t col = textarea->cursor > line_start ?
                (uint64_t)(textarea->cursor - line_start) : 0;
            if (col > max_chars) {
                col = max_chars;
            }
            gui2_rect(window, textarea->x + 8 + (int64_t)(col * 8),
                textarea->y + 6 + (int64_t)((cursor_line - textarea->scroll_line) * 12),
                1, 11, theme->border_focus);
        }
    }
}

int gui2_textarea_event(struct gui2_textarea *textarea, const struct gui2_event *event) {
    if (textarea == 0 || event == 0) {
        return GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0 &&
        (event->buttons & 1) != 0) {
        int was_focused = textarea->focused;
        textarea->focused = event_hits(event, textarea->x, textarea->y,
            textarea->width, textarea->height);
        if (textarea->focused) {
            size_t line = textarea->scroll_line;
            size_t col = 0;
            size_t start;
            size_t end;
            if (event->y > textarea->y + 8) {
                line += (size_t)((event->y - textarea->y - 8) / 12);
            }
            if (event->x > textarea->x + 8) {
                col = (size_t)((event->x - textarea->x - 8) / 8);
            }
            start = textarea_line_start(textarea, line);
            end = textarea_line_end(textarea, start);
            textarea->cursor = start + col;
            if (textarea->cursor > end) {
                textarea->cursor = end;
            }
        }
        return was_focused != textarea->focused ? GUI2_WIDGET_DIRTY | GUI2_WIDGET_FOCUS : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN && textarea->focused &&
        textarea->buffer != 0 && textarea->capacity != 0) {
        if ((event->key == 8 || event->key == 127) && textarea->cursor > 0) {
            textarea_delete_range(textarea, textarea->cursor - 1, 1);
            textarea->cursor--;
            textarea_keep_cursor_visible(textarea);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 4 && textarea->cursor < textarea->length) {
            textarea_delete_range(textarea, textarea->cursor, 1);
            textarea_keep_cursor_visible(textarea);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 2 && textarea->cursor > 0) {
            textarea->cursor--;
            textarea_keep_cursor_visible(textarea);
            return GUI2_WIDGET_DIRTY;
        }
        if (event->key == 6 && textarea->cursor < textarea->length) {
            textarea->cursor++;
            textarea_keep_cursor_visible(textarea);
            return GUI2_WIDGET_DIRTY;
        }
        if ((event->key == '\n' || event->key == '\r') &&
            textarea_insert_char(textarea, '\n')) {
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key >= 32 && event->key < 127 &&
            textarea_insert_char(textarea, (char)event->key)) {
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
    }
    return GUI2_WIDGET_NONE;
}

int gui2_textarea_contains(const struct gui2_textarea *textarea, int64_t x, int64_t y) {
    return textarea != 0 && rect_contains(textarea->x, textarea->y,
        textarea->width, textarea->height, x, y);
}

void gui2_list_init(struct gui2_list *list, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    if (list == 0) {
        return;
    }
    memset(list, 0, sizeof(*list));
    list->x = x;
    list->y = y;
    list->width = width;
    list->height = height;
    list->row_height = gui2_theme_default()->list_row_h;
    list->header_height = 20;
    list->hovered = -1;
    list->empty_text = "EMPTY";
}

static uint64_t list_header_height(const struct gui2_list *list) {
    return list != 0 && list->columns != 0 && list->column_count != 0 ?
        list->header_height : 0;
}

static size_t list_visible_rows(const struct gui2_list *list) {
    uint64_t header_h;
    uint64_t content_h;
    if (list == 0) {
        return 1;
    }
    if (list->row_height == 0) {
        return 1;
    }
    header_h = list_header_height(list);
    content_h = list->height > header_h + 8 ? list->height - header_h - 8 : 0;
    if (content_h < list->row_height) {
        return 1;
    }
    return (size_t)(content_h / list->row_height);
}

static int list_has_scrollbar(const struct gui2_list *list) {
    return list != 0 && list->count > list_visible_rows(list);
}

static int64_t list_content_y(const struct gui2_list *list) {
    return list->y + (int64_t)list_header_height(list) + 4;
}

static uint64_t list_content_height(const struct gui2_list *list) {
    uint64_t header_h = list_header_height(list);
    if (list == 0 || list->height <= header_h + 8) {
        return 1;
    }
    return list->height - header_h - 8;
}

void gui2_list_keep_selected_visible(struct gui2_list *list) {
    size_t visible_rows;
    if (list == 0) {
        return;
    }
    if (list->row_height == 0) {
        list->row_height = 22;
    }
    visible_rows = list_visible_rows(list);
    if (visible_rows == 0) {
        visible_rows = 1;
    }
    if (list->count == 0) {
        list->selected = 0;
        list->scroll = 0;
        return;
    }
    if (list->selected >= list->count) {
        list->selected = list->count - 1;
    }
    if (list->selected < list->scroll) {
        list->scroll = list->selected;
    } else if (list->selected >= list->scroll + visible_rows) {
        list->scroll = list->selected - visible_rows + 1;
    }
    if (list->scroll + visible_rows > list->count && list->count > visible_rows) {
        list->scroll = list->count - visible_rows;
    }
}

void gui2_list_set_items(struct gui2_list *list,
    const struct gui2_list_item *items, size_t count) {
    if (list == 0) {
        return;
    }
    list->items = items;
    list->count = count;
    if (list->count == 0) {
        list->selected = 0;
        list->scroll = 0;
    } else if (list->selected >= list->count) {
        list->selected = list->count - 1;
    }
    gui2_list_keep_selected_visible(list);
}

void gui2_list_set_columns(struct gui2_list *list,
    const struct gui2_list_column *columns, size_t count) {
    if (list == 0) {
        return;
    }
    list->columns = columns;
    list->column_count = count;
    if (list->sort_column >= count) {
        list->sort_column = 0;
    }
    gui2_list_keep_selected_visible(list);
}

static void gui2_list_draw_item_text(struct gui2_window *window,
    const struct gui2_list_item *item, int64_t x, int64_t y,
    uint64_t width, uint32_t color, uint32_t muted) {
    char line[96];
    size_t out = 0;
    const char *prefix = (item->flags & GUI2_LIST_ITEM_DIR) != 0 ? "[D] " : "[F] ";
    while (*prefix != '\0' && out + 1 < sizeof(line)) {
        line[out++] = *prefix++;
    }
    for (const char *p = item->label; p != 0 && *p != '\0' && out + 1 < sizeof(line); p++) {
        line[out++] = *p;
    }
    line[out] = '\0';
    if (width > 22) {
        size_t max = (size_t)((width - 22) / 8);
        if (max + 1 < sizeof(line)) {
            line[max] = '\0';
        }
    }
    gui2_text(window, x, y, line, color);
    if (item->detail != 0 && item->detail[0] != '\0' && width > 180) {
        int64_t detail_x = x + (int64_t)width - 132;
        gui2_text(window, detail_x, y, item->detail, muted);
    }
}

static void gui2_list_draw_header(struct gui2_window *window,
    const struct gui2_list *list) {
    const struct gui2_theme *theme = gui2_theme_default();
    int64_t x;
    uint64_t remaining;
    if (window == 0 || list == 0 || list->columns == 0 ||
        list->column_count == 0 || list->header_height == 0) {
        return;
    }
    gui2_rect(window, list->x + 1, list->y + 1,
        list->width > 2 ? list->width - 2 : 1, list->header_height, theme->panel);
    gui2_rect(window, list->x + 1, list->y + (int64_t)list->header_height,
        list->width > 2 ? list->width - 2 : 1, 1, theme->border);
    x = list->x + 8;
    remaining = list->width > 16 ? list->width - 16 : 1;
    for (size_t i = 0; i < list->column_count && remaining > 0; i++) {
        char label[40];
        uint64_t col_w = list->columns[i].width;
        size_t len = 0;
        const char *text = list->columns[i].label != 0 ? list->columns[i].label : "";
        if (i + 1 == list->column_count || col_w == 0 || col_w > remaining) {
            col_w = remaining;
        }
        while (text[len] != '\0' && len + 4 < sizeof(label)) {
            label[len] = text[len];
            len++;
        }
        if (i == list->sort_column && len + 2 < sizeof(label)) {
            label[len++] = list->sort_desc ? 'v' : '^';
        }
        label[len] = '\0';
        gui2_text(window, x, list->y + 7, label, theme->text_muted);
        if (i + 1 < list->column_count) {
            gui2_rect(window, x + (int64_t)col_w - 8, list->y + 4,
                1, list->header_height > 8 ? list->header_height - 8 : 1, theme->border);
        }
        x += (int64_t)col_w;
        remaining = remaining > col_w ? remaining - col_w : 0;
    }
}

void gui2_list_draw(struct gui2_window *window, const struct gui2_list *list) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint32_t border;
    size_t visible_rows;
    uint64_t header_h;
    uint64_t text_w;
    if (window == 0 || list == 0) {
        return;
    }
    border = list->focused ? theme->border_focus : theme->border;
    header_h = list_header_height(list);
    visible_rows = list_visible_rows(list);
    if (visible_rows == 0) {
        visible_rows = 1;
    }
    text_w = list->width > 16 ? list->width - 16 : 1;
    if (list_has_scrollbar(list) && text_w > 14) {
        text_w -= 14;
    }
    gui2_rect(window, list->x, list->y, list->width, list->height, theme->field);
    draw_control_border(window, list->x, list->y, list->width, list->height,
        border, list->focused);
    gui2_list_draw_header(window, list);
    if (list->count == 0 || list->items == 0) {
        gui2_text(window, list->x + 8, list->y + (int64_t)header_h + 8,
            list->empty_text != 0 ? list->empty_text : "EMPTY", theme->text_muted);
        return;
    }
    for (size_t row = 0; row < visible_rows; row++) {
        size_t index = list->scroll + row;
        int64_t row_y = list->y + (int64_t)header_h + 4 +
            (int64_t)(row * list->row_height);
        if (index >= list->count) {
            break;
        }
        if (index == list->selected) {
            gui2_rect(window, list->x + 2, row_y, list->width > 4 ? list->width - 4 : 1,
                list->row_height, list->focused ? theme->accent_down : theme->panel_alt);
        } else if ((int)index == list->hovered) {
            gui2_rect(window, list->x + 2, row_y, list->width > 4 ? list->width - 4 : 1,
                list->row_height, theme->panel);
        }
        gui2_list_draw_item_text(window, &list->items[index], list->x + 8, row_y + 6,
            text_w, theme->text, theme->text_muted);
    }
    if (list_has_scrollbar(list)) {
        uint64_t content_h = list_content_height(list);
        uint64_t thumb_h = (uint64_t)visible_rows * content_h / list->count;
        uint64_t max_scroll = list->count > visible_rows ? list->count - visible_rows : 1;
        uint64_t thumb_y;
        int64_t sx = list->x + (int64_t)list->width - 10;
        int64_t sy = list_content_y(list);
        if (thumb_h < 14) {
            thumb_h = 14;
        }
        if (thumb_h > content_h) {
            thumb_h = content_h;
        }
        thumb_y = max_scroll != 0 && content_h > thumb_h ?
            (uint64_t)list->scroll * (content_h - thumb_h) / max_scroll : 0;
        gui2_rect(window, sx, sy, 6, content_h, theme->panel);
        gui2_rect(window, sx, sy + (int64_t)thumb_y, 6, thumb_h,
            list->focused ? theme->accent : theme->border);
    }
}

int gui2_list_event(struct gui2_list *list, const struct gui2_event *event) {
    int hit;
    uint64_t header_h;
    if (list == 0 || event == 0) {
        return GUI2_WIDGET_NONE;
    }
    hit = gui2_list_contains(list, event->x, event->y);
    header_h = list_header_height(list);
    if (event->type == GUI2_EVENT_POINTER_MOVE) {
        int old = list->hovered;
        list->hovered = -1;
        if (hit && list->row_height != 0 &&
            event->y >= list->y + (int64_t)header_h + 4) {
            size_t row = (size_t)((event->y - list->y - (int64_t)header_h - 4) /
                (int64_t)list->row_height);
            size_t index = list->scroll + row;
            if (index < list->count) {
                list->hovered = (int)index;
            }
        }
        return old != list->hovered ? GUI2_WIDGET_DIRTY : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0 &&
        (event->buttons & 1) != 0) {
        if (hit && list_has_scrollbar(list) &&
            event->x >= list->x + (int64_t)list->width - 14) {
            size_t visible_rows = list_visible_rows(list);
            int64_t content_y = list_content_y(list);
            uint64_t content_h = list_content_height(list);
            if (event->y < content_y + (int64_t)(content_h / 2)) {
                if (list->selected > visible_rows) {
                    list->selected -= visible_rows;
                } else {
                    list->selected = 0;
                }
            } else if (list->count != 0) {
                list->selected += visible_rows;
                if (list->selected >= list->count) {
                    list->selected = list->count - 1;
                }
            }
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (hit && header_h != 0 && event->y < list->y + (int64_t)header_h) {
            uint64_t rel_x = event->x > list->x + 8 ?
                (uint64_t)(event->x - list->x - 8) : 0;
            uint64_t at = 0;
            uint64_t remaining = list->width > 16 ? list->width - 16 : 1;
            for (size_t i = 0; i < list->column_count; i++) {
                uint64_t col_w = list->columns[i].width;
                if (i + 1 == list->column_count || col_w == 0 || col_w > remaining) {
                    col_w = remaining;
                }
                if (rel_x >= at && rel_x < at + col_w) {
                    list->clicked_column = i;
                    list->header_clicks++;
                    return GUI2_WIDGET_CLICK | GUI2_WIDGET_VALUE | GUI2_WIDGET_DIRTY;
                }
                at += col_w;
                remaining = remaining > col_w ? remaining - col_w : 0;
            }
        }
        if (hit && list->row_height != 0) {
            if (event->y < list->y + (int64_t)header_h + 4) {
                return GUI2_WIDGET_NONE;
            }
            size_t row = (size_t)((event->y - list->y - (int64_t)header_h - 4) /
                (int64_t)list->row_height);
            size_t index = list->scroll + row;
            if (index < list->count) {
                if (index == list->selected) {
                    list->clicks++;
                }
                list->selected = index;
                gui2_list_keep_selected_visible(list);
                return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE | GUI2_WIDGET_CLICK;
            }
        }
        return GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN && list->focused) {
        size_t visible_rows = list_visible_rows(list);
        if ((event->key == 'k' || event->key == 'K') && list->selected > 0) {
            list->selected--;
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if ((event->key == 'j' || event->key == 'J') && list->selected + 1 < list->count) {
            list->selected++;
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if ((event->key == 'p' || event->key == 'P') && list->selected > 0) {
            if (list->selected > visible_rows) {
                list->selected -= visible_rows;
            } else {
                list->selected = 0;
            }
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if ((event->key == 'n' || event->key == 'N') && list->selected + 1 < list->count) {
            list->selected += visible_rows;
            if (list->selected >= list->count) {
                list->selected = list->count - 1;
            }
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 'g' && list->count != 0) {
            list->selected = 0;
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if (event->key == 'G' && list->count != 0) {
            list->selected = list->count - 1;
            gui2_list_keep_selected_visible(list);
            return GUI2_WIDGET_DIRTY | GUI2_WIDGET_VALUE;
        }
        if ((event->key == '\n' || event->key == '\r' || event->key == ' ') &&
            list->count != 0) {
            list->activations++;
            return GUI2_WIDGET_CLICK;
        }
    }
    return GUI2_WIDGET_NONE;
}

int gui2_list_contains(const struct gui2_list *list, int64_t x, int64_t y) {
    return list != 0 && rect_contains(list->x, list->y,
        list->width, list->height, x, y);
}

void gui2_dialog_init(struct gui2_dialog *dialog, const char *title,
    const char *message, const char *primary, const char *secondary) {
    if (dialog == 0) {
        return;
    }
    memset(dialog, 0, sizeof(*dialog));
    dialog->title = title;
    dialog->message = message;
    gui2_button_init(&dialog->primary, 0, 0, 88, 30, primary != 0 ? primary : "OK");
    gui2_button_init(&dialog->secondary, 0, 0, 88, 30, secondary != 0 ? secondary : "CANCEL");
}

void gui2_dialog_open(struct gui2_dialog *dialog, const char *title,
    const char *message) {
    if (dialog == 0) {
        return;
    }
    dialog->active = 1;
    if (title != 0) {
        dialog->title = title;
    }
    if (message != 0) {
        dialog->message = message;
    }
    dialog->primary_clicks = 0;
    dialog->secondary_clicks = 0;
    dialog->primary.clicks = 0;
    dialog->secondary.clicks = 0;
    dialog->progress_active = 0;
    dialog->progress_value = 0;
    dialog->progress_max = 0;
    dialog->progress_text = 0;
}

void gui2_dialog_set_progress(struct gui2_dialog *dialog, uint64_t value,
    uint64_t max, const char *text) {
    if (dialog == 0) {
        return;
    }
    dialog->progress_active = max != 0;
    dialog->progress_value = value > max && max != 0 ? max : value;
    dialog->progress_max = max;
    dialog->progress_text = text;
}

void gui2_dialog_close(struct gui2_dialog *dialog) {
    if (dialog != 0) {
        dialog->active = 0;
    }
}

void gui2_dialog_draw(struct gui2_window *window, struct gui2_dialog *dialog) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t w;
    uint64_t h;
    int64_t x;
    int64_t y;
    uint64_t bar_w;
    uint64_t fill_w;
    if (window == 0 || dialog == 0 || !dialog->active) {
        return;
    }
    gui2_rect(window, 0, 0, window->width, window->height, 0x05080b);
    w = window->width > 360 ? 340 : window->width > 2 * theme->pad ?
        window->width - 2 * theme->pad : window->width;
    h = dialog->progress_active ? 164 : 136;
    x = (int64_t)((window->width - w) / 2);
    y = (int64_t)((window->height - h) / 2);
    gui2_panel(window, x, y, w, h, theme->panel);
    gui2_text(window, x + 14, y + 14, dialog->title != 0 ? dialog->title : "",
        theme->text);
    gui2_text(window, x + 14, y + 42, dialog->message != 0 ? dialog->message : "",
        theme->text_muted);
    if (dialog->progress_active) {
        bar_w = w > 28 ? w - 28 : w;
        fill_w = dialog->progress_max != 0 ?
            (bar_w * dialog->progress_value) / dialog->progress_max : 0;
        gui2_panel(window, x + 14, y + 68, bar_w, 16, theme->field);
        if (fill_w != 0) {
            gui2_rect(window, x + 15, y + 69,
                fill_w > 2 ? fill_w - 2 : fill_w, 14, theme->accent);
        }
        gui2_text(window, x + 14, y + 94,
            dialog->progress_text != 0 ? dialog->progress_text : "", theme->text);
    }
    dialog->primary.x = x + (int64_t)w - 196;
    dialog->primary.y = y + (int64_t)h - 44;
    dialog->primary.width = 84;
    dialog->primary.height = theme->control_h;
    dialog->secondary.x = x + (int64_t)w - 104;
    dialog->secondary.y = y + (int64_t)h - 44;
    dialog->secondary.width = 90;
    dialog->secondary.height = theme->control_h;
    if (!dialog->progress_active) {
        gui2_button_draw(window, &dialog->primary);
    }
    gui2_button_draw(window, &dialog->secondary);
}

int gui2_dialog_event(struct gui2_dialog *dialog, const struct gui2_event *event) {
    int result = GUI2_WIDGET_NONE;
    if (dialog == 0 || event == 0 || !dialog->active) {
        return result;
    }
    if (!dialog->progress_active) {
        result |= gui2_button_event(&dialog->primary, event);
    }
    result |= gui2_button_event(&dialog->secondary, event);
    if (!dialog->progress_active && dialog->primary.clicks != 0) {
        dialog->primary.clicks = 0;
        dialog->primary_clicks++;
        result |= GUI2_WIDGET_CLICK;
    }
    if (dialog->secondary.clicks != 0) {
        dialog->secondary.clicks = 0;
        dialog->secondary_clicks++;
        result |= GUI2_WIDGET_CLICK;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN) {
        if (!dialog->progress_active &&
            (event->key == '\n' || event->key == '\r')) {
            dialog->primary_clicks++;
            result |= GUI2_WIDGET_CLICK | GUI2_WIDGET_DIRTY;
        } else if (event->key == 27) {
            dialog->secondary_clicks++;
            result |= GUI2_WIDGET_CLICK | GUI2_WIDGET_DIRTY;
        }
    }
    return result;
}

static int gui2_fd_path_is_dir(const char *path) {
    struct srv_stat info;
    return srv_stat(path, &info) == 0 && info.type == 1;
}

static int gui2_fd_name_is_less(const struct gui2_file_dialog_entry *a,
    const struct gui2_file_dialog_entry *b) {
    const char *left = a->name;
    const char *right = b->name;
    if (a->dir != b->dir) {
        return a->dir > b->dir;
    }
    while (*left != '\0' && *right != '\0') {
        char lc = *left >= 'A' && *left <= 'Z' ? (char)(*left + 32) : *left;
        char rc = *right >= 'A' && *right <= 'Z' ? (char)(*right + 32) : *right;
        if (lc != rc) {
            return lc < rc;
        }
        left++;
        right++;
    }
    return *left == '\0' && *right != '\0';
}

static void gui2_fd_sort(struct gui2_file_dialog_entry *entries, size_t count) {
    for (size_t i = 1; i < count; i++) {
        struct gui2_file_dialog_entry value = entries[i];
        size_t j = i;
        while (j > 0 && gui2_fd_name_is_less(&value, &entries[j - 1])) {
            entries[j] = entries[j - 1];
            j--;
        }
        entries[j] = value;
    }
}

static void gui2_fd_bind_items(struct gui2_file_dialog *dialog) {
    for (size_t i = 0; i < dialog->count; i++) {
        dialog->items[i].label = dialog->entries[i].name;
        dialog->items[i].detail = dialog->entries[i].detail;
        dialog->items[i].flags = dialog->entries[i].dir ? GUI2_LIST_ITEM_DIR : 0;
    }
    gui2_list_set_items(&dialog->list, dialog->items, dialog->count);
}

static void gui2_fd_set_textbox(struct gui2_textbox *textbox, const char *text) {
    cli_copy(textbox->buffer, textbox->capacity, text != 0 ? text : "");
    textbox->length = cli_strlen(textbox->buffer);
    textbox->cursor = textbox->length;
}

static void gui2_fd_basename(char *out, size_t capacity, const char *path) {
    cli_basename(out, capacity, path);
    if (out[0] == '\0') {
        cli_copy(out, capacity, "/");
    }
}

static void gui2_fd_parent(char *out, size_t capacity, const char *path) {
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
    if (length == 0) {
        length = 1;
    }
    if (length >= capacity) {
        length = capacity - 1;
    }
    for (size_t i = 0; i < length; i++) {
        out[i] = path[i];
    }
    out[length] = '\0';
}

static void gui2_fd_make_result(struct gui2_file_dialog *dialog) {
    if (dialog->filename[0] == '/') {
        cli_normalize_path(dialog->result_path, sizeof(dialog->result_path),
            "/", dialog->filename);
    } else {
        cli_normalize_path(dialog->result_path, sizeof(dialog->result_path),
            dialog->cwd, dialog->filename);
    }
}

static void gui2_fd_set_cwd(struct gui2_file_dialog *dialog, const char *cwd,
    const char *filename) {
    char normalized[CLI_PATH_MAX];
    cli_normalize_path(normalized, sizeof(normalized), "/", cwd);
    if (!gui2_fd_path_is_dir(normalized)) {
        cli_copy(normalized, sizeof(normalized), "/");
    }
    cli_copy(dialog->cwd, sizeof(dialog->cwd), normalized);
    gui2_fd_set_textbox(&dialog->path_box, filename != 0 ? filename : "");
}

static void gui2_fd_refresh(struct gui2_file_dialog *dialog) {
    char prefix[CLI_PATH_MAX];
    char listed[CLI_PATH_MAX];
    uint64_t size = 0;
    dialog->count = 0;
    if (!gui2_fd_path_is_dir(dialog->cwd)) {
        cli_copy(dialog->cwd, sizeof(dialog->cwd), "/");
    }
    cli_join_path(prefix, sizeof(prefix), dialog->cwd, "");
    for (uint64_t i = 0;; i++) {
        struct srv_stat info;
        char child[GUI2_FILE_NAME_MAX];
        const char *rest;
        int nested = 0;
        long result = srv_list(i, listed, sizeof(listed), &size);
        if (result <= 0) {
            break;
        }
        if (!cli_starts_with(listed, prefix) || cli_streq(listed, dialog->cwd)) {
            continue;
        }
        rest = listed + cli_strlen(prefix);
        if (rest[0] == '\0') {
            continue;
        }
        for (size_t j = 0; rest[j] != '\0'; j++) {
            if (rest[j] == '/') {
                nested = 1;
                break;
            }
        }
        if (nested || dialog->count >= GUI2_FILE_DIALOG_MAX) {
            continue;
        }
        cli_copy(child, sizeof(child), rest);
        if (srv_stat(listed, &info) != 0) {
            continue;
        }
        cli_copy(dialog->entries[dialog->count].name,
            sizeof(dialog->entries[dialog->count].name), child);
        cli_copy(dialog->entries[dialog->count].path,
            sizeof(dialog->entries[dialog->count].path), listed);
        dialog->entries[dialog->count].dir = info.type == 1;
        cli_copy(dialog->entries[dialog->count].detail,
            sizeof(dialog->entries[dialog->count].detail),
            info.type == 1 ? "DIR" : "FILE");
        dialog->count++;
    }
    gui2_fd_sort(dialog->entries, dialog->count);
    gui2_fd_bind_items(dialog);
    if (dialog->list.selected >= dialog->count && dialog->count != 0) {
        dialog->list.selected = dialog->count - 1;
    }
    gui2_list_keep_selected_visible(&dialog->list);
    cli_copy(dialog->status, sizeof(dialog->status), dialog->cwd);
}

void gui2_file_dialog_init(struct gui2_file_dialog *dialog, const char *title,
    const char *primary_label, int save_mode) {
    if (dialog == 0) {
        return;
    }
    memset(dialog, 0, sizeof(*dialog));
    dialog->save_mode = save_mode;
    dialog->title = title != 0 ? title : "FILE";
    dialog->primary_label = primary_label != 0 ? primary_label :
        (save_mode ? "SAVE" : "OPEN");
    gui2_context_init(&dialog->context);
    gui2_list_init(&dialog->list, 0, 0, 320, 180);
    gui2_textbox_init(&dialog->path_box, 0, 0, 320, 30,
        dialog->filename, sizeof(dialog->filename));
    gui2_textbox_set_placeholder(&dialog->path_box,
        save_mode ? "FILENAME OR PATH" : "PATH");
    gui2_button_init(&dialog->up, 0, 0, 72, 30, "UP");
    gui2_button_init(&dialog->primary, 0, 0, 88, 30, dialog->primary_label);
    gui2_button_init(&dialog->cancel, 0, 0, 88, 30, "CANCEL");
    dialog->list.empty_text = "EMPTY";
}

void gui2_file_dialog_open(struct gui2_file_dialog *dialog, const char *cwd,
    const char *filename) {
    if (dialog == 0) {
        return;
    }
    dialog->active = 1;
    dialog->primary_clicks = 0;
    dialog->cancel_clicks = 0;
    dialog->result_path[0] = '\0';
    dialog->list.selected = 0;
    dialog->list.scroll = 0;
    dialog->list.clicks = 0;
    dialog->list.activations = 0;
    dialog->primary.clicks = 0;
    dialog->cancel.clicks = 0;
    dialog->up.clicks = 0;
    gui2_context_init(&dialog->context);
    gui2_fd_set_cwd(dialog, cwd != 0 ? cwd : "/fat", filename);
    gui2_fd_refresh(dialog);
}

void gui2_file_dialog_close(struct gui2_file_dialog *dialog) {
    if (dialog != 0) {
        dialog->active = 0;
    }
}

void gui2_file_dialog_draw(struct gui2_window *window,
    struct gui2_file_dialog *dialog) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t w;
    uint64_t h;
    int64_t x;
    int64_t y;
    uint64_t pad;
    uint64_t gap;
    if (window == 0 || dialog == 0 || !dialog->active) {
        return;
    }
    pad = theme->pad;
    gap = theme->gap;
    gui2_rect(window, 0, 0, window->width, window->height, 0x05080b);
    w = window->width > 520 ? 500 :
        window->width > 2 * pad ? window->width - 2 * pad : window->width;
    h = window->height > 380 ? 360 :
        window->height > 2 * pad ? window->height - 2 * pad : window->height;
    x = (int64_t)((window->width - w) / 2);
    y = (int64_t)((window->height - h) / 2);
    gui2_panel(window, x, y, w, h, theme->panel);
    gui2_text(window, x + 14, y + 14,
        dialog->title != 0 ? dialog->title : "FILE", theme->text);
    gui2_text(window, x + 14, y + 34, dialog->cwd, theme->text_muted);

    dialog->list.x = x + (int64_t)pad;
    dialog->list.y = y + 58;
    dialog->list.width = w > 2 * pad ? w - 2 * pad : 1;
    dialog->list.height = h > 150 ? h - 150 : 80;
    gui2_list_draw(window, &dialog->list);

    dialog->path_box.x = x + (int64_t)pad;
    dialog->path_box.y = dialog->list.y + (int64_t)dialog->list.height +
        (int64_t)gap;
    dialog->path_box.width = dialog->list.width;
    dialog->path_box.height = theme->control_h;
    gui2_textbox_draw(window, &dialog->path_box);

    struct gui2_button buttons[] = {
        dialog->up, dialog->primary, dialog->cancel
    };
    gui2_layout_button_row(buttons, 3, x + (int64_t)pad,
        dialog->path_box.y + (int64_t)theme->control_h + (int64_t)gap,
        dialog->list.width, theme->control_h, gap);
    dialog->up = buttons[0];
    dialog->primary = buttons[1];
    dialog->cancel = buttons[2];
    gui2_button_draw(window, &dialog->up);
    gui2_button_draw(window, &dialog->primary);
    gui2_button_draw(window, &dialog->cancel);
    gui2_text(window, x + 14, y + (int64_t)h - 20, dialog->status,
        theme->text_muted);
}

static void gui2_fd_accept(struct gui2_file_dialog *dialog) {
    if (dialog->filename[0] == '\0' && dialog->count != 0 &&
        dialog->list.selected < dialog->count) {
        cli_copy(dialog->filename, sizeof(dialog->filename),
            dialog->entries[dialog->list.selected].name);
        dialog->path_box.length = cli_strlen(dialog->filename);
        dialog->path_box.cursor = dialog->path_box.length;
    }
    gui2_fd_make_result(dialog);
    if (!dialog->save_mode && gui2_fd_path_is_dir(dialog->result_path)) {
        cli_copy(dialog->cwd, sizeof(dialog->cwd), dialog->result_path);
        gui2_fd_set_textbox(&dialog->path_box, "");
        dialog->list.selected = 0;
        dialog->list.scroll = 0;
        gui2_fd_refresh(dialog);
        return;
    }
    dialog->primary_clicks++;
}

int gui2_file_dialog_event(struct gui2_file_dialog *dialog,
    const struct gui2_event *event) {
    int result;
    int accepted_list_activation;
    if (dialog == 0 || event == 0 || !dialog->active) {
        return GUI2_WIDGET_NONE;
    }
    struct gui2_control controls[] = {
        { GUI2_CONTROL_LIST, &dialog->list },
        { GUI2_CONTROL_TEXTBOX, &dialog->path_box },
        { GUI2_CONTROL_BUTTON, &dialog->up },
        { GUI2_CONTROL_BUTTON, &dialog->primary },
        { GUI2_CONTROL_BUTTON, &dialog->cancel },
    };
    result = gui2_dispatch_event(&dialog->context, event,
        controls, sizeof(controls) / sizeof(controls[0]));
    accepted_list_activation = 0;
    if ((result & GUI2_WIDGET_VALUE) != 0 && dialog->list.focused &&
        dialog->list.selected < dialog->count) {
        cli_copy(dialog->filename, sizeof(dialog->filename),
            dialog->entries[dialog->list.selected].name);
        dialog->path_box.length = cli_strlen(dialog->filename);
        dialog->path_box.cursor = dialog->path_box.length;
    }
    if (dialog->list.activations != 0) {
        dialog->list.activations = 0;
        accepted_list_activation = 1;
        if (dialog->list.selected < dialog->count) {
            if (dialog->entries[dialog->list.selected].dir) {
                cli_copy(dialog->cwd, sizeof(dialog->cwd),
                    dialog->entries[dialog->list.selected].path);
                gui2_fd_set_textbox(&dialog->path_box, "");
                dialog->list.selected = 0;
                dialog->list.scroll = 0;
                gui2_fd_refresh(dialog);
            } else {
                cli_copy(dialog->filename, sizeof(dialog->filename),
                    dialog->entries[dialog->list.selected].name);
                gui2_fd_accept(dialog);
            }
        }
        result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
    }
    if (dialog->up.clicks != 0) {
        char parent[CLI_PATH_MAX];
        dialog->up.clicks = 0;
        gui2_fd_parent(parent, sizeof(parent), dialog->cwd);
        cli_copy(dialog->cwd, sizeof(dialog->cwd), parent);
        gui2_fd_set_textbox(&dialog->path_box, "");
        dialog->list.selected = 0;
        dialog->list.scroll = 0;
        gui2_fd_refresh(dialog);
        result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
    }
    if (dialog->primary.clicks != 0) {
        dialog->primary.clicks = 0;
        gui2_fd_accept(dialog);
        result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
    }
    if (dialog->cancel.clicks != 0) {
        dialog->cancel.clicks = 0;
        dialog->cancel_clicks++;
        result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN) {
        if (!accepted_list_activation &&
            (event->key == '\n' || event->key == '\r')) {
            gui2_fd_accept(dialog);
            result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
        } else if (event->key == 27) {
            dialog->cancel_clicks++;
            result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_CLICK;
        }
    }
    if (dialog->result_path[0] != '\0') {
        gui2_fd_basename(dialog->status, sizeof(dialog->status),
            dialog->result_path);
    }
    return result;
}

void gui2_canvas_init(struct gui2_canvas *canvas, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const uint32_t *pixels,
    uint64_t pixel_width, uint64_t pixel_height, uint32_t background) {
    if (canvas == 0) {
        return;
    }
    memset(canvas, 0, sizeof(*canvas));
    canvas->x = x;
    canvas->y = y;
    canvas->width = width;
    canvas->height = height;
    canvas->pixels = pixels;
    canvas->pixel_width = pixel_width;
    canvas->pixel_height = pixel_height;
    canvas->view_x = 0;
    canvas->view_y = 0;
    canvas->view_width = pixel_width;
    canvas->view_height = pixel_height;
    canvas->background = background;
}

void gui2_canvas_set_view(struct gui2_canvas *canvas, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height) {
    if (canvas == 0) {
        return;
    }
    if (width == 0 || width > canvas->pixel_width) {
        width = canvas->pixel_width;
    }
    if (height == 0 || height > canvas->pixel_height) {
        height = canvas->pixel_height;
    }
    if (x + width > canvas->pixel_width) {
        x = canvas->pixel_width > width ? canvas->pixel_width - width : 0;
    }
    if (y + height > canvas->pixel_height) {
        y = canvas->pixel_height > height ? canvas->pixel_height - height : 0;
    }
    canvas->view_x = x;
    canvas->view_y = y;
    canvas->view_width = width;
    canvas->view_height = height;
}

void gui2_canvas_draw(struct gui2_window *window, const struct gui2_canvas *canvas) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t view_x;
    uint64_t view_y;
    uint64_t view_w;
    uint64_t view_h;
    if (window == 0 || canvas == 0 || canvas->width == 0 || canvas->height == 0) {
        return;
    }
    gui2_panel(window, canvas->x - 1, canvas->y - 1,
        canvas->width + 2, canvas->height + 2, theme->field);
    if (canvas->pixels == 0 || canvas->pixel_width == 0 || canvas->pixel_height == 0) {
        gui2_rect(window, canvas->x, canvas->y,
            canvas->width, canvas->height, canvas->background);
        return;
    }
    view_x = canvas->view_width != 0 ? canvas->view_x : 0;
    view_y = canvas->view_height != 0 ? canvas->view_y : 0;
    view_w = canvas->view_width != 0 ? canvas->view_width : canvas->pixel_width;
    view_h = canvas->view_height != 0 ? canvas->view_height : canvas->pixel_height;
    if (view_x >= canvas->pixel_width) {
        view_x = 0;
    }
    if (view_y >= canvas->pixel_height) {
        view_y = 0;
    }
    if (view_x + view_w > canvas->pixel_width) {
        view_w = canvas->pixel_width - view_x;
    }
    if (view_y + view_h > canvas->pixel_height) {
        view_h = canvas->pixel_height - view_y;
    }
    for (uint64_t y = 0; y < view_h; y++) {
        uint64_t py = y * canvas->height / view_h;
        uint64_t py2 = (y + 1) * canvas->height / view_h;
        uint64_t h = py2 > py ? py2 - py : 1;
        for (uint64_t x = 0; x < view_w; x++) {
            uint64_t px = x * canvas->width / view_w;
            uint64_t px2 = (x + 1) * canvas->width / view_w;
            uint64_t w = px2 > px ? px2 - px : 1;
            gui2_rect(window, canvas->x + (int64_t)px, canvas->y + (int64_t)py,
                w, h, canvas->pixels[(view_y + y) * canvas->pixel_width + view_x + x]);
        }
    }
}

int gui2_canvas_contains(const struct gui2_canvas *canvas, int64_t x, int64_t y) {
    return canvas != 0 && rect_contains(canvas->x, canvas->y,
        canvas->width, canvas->height, x, y);
}

int gui2_canvas_event_pixel(const struct gui2_canvas *canvas,
    const struct gui2_event *event, uint64_t *x, uint64_t *y) {
    uint64_t view_x;
    uint64_t view_y;
    uint64_t view_w;
    uint64_t view_h;
    if (canvas == 0 || event == 0 || x == 0 || y == 0 ||
        canvas->width == 0 || canvas->height == 0 ||
        canvas->pixel_width == 0 || canvas->pixel_height == 0 ||
        !gui2_canvas_contains(canvas, event->x, event->y)) {
        return 0;
    }
    view_x = canvas->view_width != 0 ? canvas->view_x : 0;
    view_y = canvas->view_height != 0 ? canvas->view_y : 0;
    view_w = canvas->view_width != 0 ? canvas->view_width : canvas->pixel_width;
    view_h = canvas->view_height != 0 ? canvas->view_height : canvas->pixel_height;
    if (view_x + view_w > canvas->pixel_width) {
        view_w = canvas->pixel_width - view_x;
    }
    if (view_y + view_h > canvas->pixel_height) {
        view_h = canvas->pixel_height - view_y;
    }
    *x = view_x + (uint64_t)(event->x - canvas->x) * view_w / canvas->width;
    *y = view_y + (uint64_t)(event->y - canvas->y) * view_h / canvas->height;
    if (*x >= canvas->pixel_width) {
        *x = canvas->pixel_width - 1;
    }
    if (*y >= canvas->pixel_height) {
        *y = canvas->pixel_height - 1;
    }
    return 1;
}

int gui2_canvas_event(struct gui2_canvas *canvas, const struct gui2_event *event) {
    uint64_t px = 0;
    uint64_t py = 0;
    int hit;
    if (canvas == 0 || event == 0) {
        return GUI2_WIDGET_NONE;
    }
    hit = gui2_canvas_event_pixel(canvas, event, &px, &py);
    if (hit) {
        canvas->pointer_x = px;
        canvas->pointer_y = py;
    }
    if (event->type == GUI2_EVENT_POINTER_MOVE) {
        int old = canvas->hovered;
        canvas->hovered = hit;
        return old != canvas->hovered ? GUI2_WIDGET_DIRTY : GUI2_WIDGET_NONE;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0) {
        int old_pressed = canvas->pressed;
        canvas->pressed = hit && (event->buttons & 1) != 0;
        if (old_pressed && (event->buttons & 1) == 0 && hit) {
            canvas->clicks++;
            return GUI2_WIDGET_CLICK;
        }
        return old_pressed != canvas->pressed ? GUI2_WIDGET_DIRTY : GUI2_WIDGET_NONE;
    }
    return GUI2_WIDGET_NONE;
}

void gui2_context_init(struct gui2_context *context) {
    if (context != 0) {
        memset(context, 0, sizeof(*context));
    }
}

static int control_contains(const struct gui2_control *control, int64_t x, int64_t y) {
    if (control == 0 || control->ptr == 0) {
        return 0;
    }
    if (control->kind == GUI2_CONTROL_BUTTON) {
        return gui2_button_contains((const struct gui2_button *)control->ptr, x, y);
    }
    if (control->kind == GUI2_CONTROL_TEXTBOX) {
        return gui2_textbox_contains((const struct gui2_textbox *)control->ptr, x, y);
    }
    if (control->kind == GUI2_CONTROL_CANVAS) {
        return gui2_canvas_contains((const struct gui2_canvas *)control->ptr, x, y);
    }
    if (control->kind == GUI2_CONTROL_TEXTAREA) {
        return gui2_textarea_contains((const struct gui2_textarea *)control->ptr, x, y);
    }
    if (control->kind == GUI2_CONTROL_LIST) {
        return gui2_list_contains((const struct gui2_list *)control->ptr, x, y);
    }
    return 0;
}

static void control_set_focus(struct gui2_control *control, int focused) {
    if (control == 0 || control->ptr == 0) {
        return;
    }
    if (control->kind == GUI2_CONTROL_BUTTON) {
        ((struct gui2_button *)control->ptr)->focused = focused;
    } else if (control->kind == GUI2_CONTROL_TEXTBOX) {
        struct gui2_textbox *textbox = (struct gui2_textbox *)control->ptr;
        textbox->focused = focused;
        if (focused && textbox->cursor > textbox->length) {
            textbox->cursor = textbox->length;
        }
    } else if (control->kind == GUI2_CONTROL_TEXTAREA) {
        struct gui2_textarea *textarea = (struct gui2_textarea *)control->ptr;
        textarea->focused = focused;
        if (focused && textarea->cursor > textarea->length) {
            textarea->cursor = textarea->length;
        }
        if (focused) {
            textarea_keep_cursor_visible(textarea);
        }
    } else if (control->kind == GUI2_CONTROL_LIST) {
        struct gui2_list *list = (struct gui2_list *)control->ptr;
        list->focused = focused;
        if (focused) {
            gui2_list_keep_selected_visible(list);
        }
    }
}

static int control_is_tab_focusable(const struct gui2_control *control) {
    if (control == 0 || control->ptr == 0) {
        return 0;
    }
    return control->kind == GUI2_CONTROL_BUTTON ||
        control->kind == GUI2_CONTROL_TEXTBOX ||
        control->kind == GUI2_CONTROL_TEXTAREA ||
        control->kind == GUI2_CONTROL_LIST;
}

static int control_is_same(const struct gui2_control *a, const struct gui2_control *b) {
    return a != 0 && b != 0 && a->kind == b->kind && a->ptr == b->ptr;
}

static int context_focus_step(struct gui2_context *context,
    const struct gui2_control *controls, size_t count, int reverse) {
    size_t current = count;
    if (context == 0 || controls == 0 || count == 0) {
        return GUI2_WIDGET_NONE;
    }
    for (size_t i = 0; i < count; i++) {
        if (control_is_same(&context->focused, &controls[i])) {
            current = i;
            break;
        }
    }
    for (size_t step = 0; step < count; step++) {
        size_t index;
        if (reverse) {
            index = current == count ? count - 1 - step : (current + count - 1 - step) % count;
        } else {
            index = current == count ? step : (current + 1 + step) % count;
        }
        if (!control_is_tab_focusable(&controls[index])) {
            continue;
        }
        if (control_is_same(&context->focused, &controls[index])) {
            return GUI2_WIDGET_NONE;
        }
        control_set_focus(&context->focused, 0);
        context->focused = controls[index];
        control_set_focus(&context->focused, 1);
        return GUI2_WIDGET_DIRTY | GUI2_WIDGET_FOCUS;
    }
    return GUI2_WIDGET_NONE;
}

static int control_event(struct gui2_control *control, const struct gui2_event *event) {
    if (control == 0 || control->ptr == 0 || event == 0) {
        return GUI2_WIDGET_NONE;
    }
    if (control->kind == GUI2_CONTROL_BUTTON) {
        return gui2_button_event((struct gui2_button *)control->ptr, event);
    }
    if (control->kind == GUI2_CONTROL_TEXTBOX) {
        return gui2_textbox_event((struct gui2_textbox *)control->ptr, event);
    }
    if (control->kind == GUI2_CONTROL_CANVAS) {
        return gui2_canvas_event((struct gui2_canvas *)control->ptr, event);
    }
    if (control->kind == GUI2_CONTROL_TEXTAREA) {
        return gui2_textarea_event((struct gui2_textarea *)control->ptr, event);
    }
    if (control->kind == GUI2_CONTROL_LIST) {
        return gui2_list_event((struct gui2_list *)control->ptr, event);
    }
    return GUI2_WIDGET_NONE;
}

int gui2_dispatch_event(struct gui2_context *context, const struct gui2_event *event,
    const struct gui2_control *controls, size_t count) {
    int result = GUI2_WIDGET_NONE;
    if (context == 0 || event == 0 || controls == 0) {
        return result;
    }
    if (event->type == GUI2_EVENT_POINTER_MOVE) {
        for (size_t i = 0; i < count; i++) {
            struct gui2_control control = controls[i];
            result |= control_event(&control, event);
        }
        return result;
    }
    if (event->type == GUI2_EVENT_POINTER_BUTTON && (event->changed_buttons & 1) != 0 &&
        (event->buttons & 1) != 0) {
        struct gui2_control next_focus = {0};
        for (size_t i = 0; i < count; i++) {
            if (control_contains(&controls[i], event->x, event->y)) {
                next_focus = controls[i];
            }
        }
        if (context->focused.ptr != next_focus.ptr || context->focused.kind != next_focus.kind) {
            control_set_focus(&context->focused, 0);
            context->focused = next_focus;
            control_set_focus(&context->focused, 1);
            result |= GUI2_WIDGET_DIRTY | GUI2_WIDGET_FOCUS;
        }
    }
    if (event->type == GUI2_EVENT_KEY_DOWN) {
        if (event->key == '\t') {
            return context_focus_step(context, controls, count, 0);
        }
        if (event->key == GUI_KEY_BACKTAB) {
            return context_focus_step(context, controls, count, 1);
        }
        result |= control_event(&context->focused, event);
        return result;
    }
    for (size_t i = 0; i < count; i++) {
        struct gui2_control control = controls[i];
        result |= control_event(&control, event);
    }
    return result;
}
