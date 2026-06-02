#include <srvros/gui.h>
#include <srvros/gui2.h>

#include <stdlib.h>
#include <string.h>

static const struct gui2_theme default_theme = {
    .canvas = 0x101820,
    .panel = 0x182632,
    .panel_alt = 0x20313b,
    .field = 0x0f1720,
    .border = 0x6a8695,
    .border_focus = 0xf5b84b,
    .text = 0xffffff,
    .text_muted = 0x8fa9b6,
    .accent = 0x3d63df,
    .accent_hover = 0x4f74b8,
    .accent_down = 0x2f4b74,
    .danger = 0x8a3f48,
    .pad = 12,
    .gap = 8,
    .control_h = 30,
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
    gui2_rect(window, button->x, button->y, button->width, 1, border);
    gui2_rect(window, button->x, button->y + (int64_t)button->height - 1,
        button->width, 1, border);
    gui2_rect(window, button->x, button->y, 1, button->height, border);
    gui2_rect(window, button->x + (int64_t)button->width - 1, button->y,
        1, button->height, border);
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
    gui2_rect(window, textbox->x, textbox->y, textbox->width, 1, border);
    gui2_rect(window, textbox->x, textbox->y + (int64_t)textbox->height - 1, textbox->width, 1, border);
    gui2_rect(window, textbox->x, textbox->y, 1, textbox->height, border);
    gui2_rect(window, textbox->x + (int64_t)textbox->width - 1, textbox->y, 1, textbox->height, border);
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
    canvas->background = background;
}

void gui2_canvas_draw(struct gui2_window *window, const struct gui2_canvas *canvas) {
    const struct gui2_theme *theme = gui2_theme_default();
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
    for (uint64_t y = 0; y < canvas->pixel_height; y++) {
        uint64_t py = y * canvas->height / canvas->pixel_height;
        uint64_t py2 = (y + 1) * canvas->height / canvas->pixel_height;
        uint64_t h = py2 > py ? py2 - py : 1;
        for (uint64_t x = 0; x < canvas->pixel_width; x++) {
            uint64_t px = x * canvas->width / canvas->pixel_width;
            uint64_t px2 = (x + 1) * canvas->width / canvas->pixel_width;
            uint64_t w = px2 > px ? px2 - px : 1;
            gui2_rect(window, canvas->x + (int64_t)px, canvas->y + (int64_t)py,
                w, h, canvas->pixels[y * canvas->pixel_width + x]);
        }
    }
}

int gui2_canvas_contains(const struct gui2_canvas *canvas, int64_t x, int64_t y) {
    return canvas != 0 && rect_contains(canvas->x, canvas->y,
        canvas->width, canvas->height, x, y);
}

int gui2_canvas_event_pixel(const struct gui2_canvas *canvas,
    const struct gui2_event *event, uint64_t *x, uint64_t *y) {
    if (canvas == 0 || event == 0 || x == 0 || y == 0 ||
        canvas->width == 0 || canvas->height == 0 ||
        canvas->pixel_width == 0 || canvas->pixel_height == 0 ||
        !gui2_canvas_contains(canvas, event->x, event->y)) {
        return 0;
    }
    *x = (uint64_t)(event->x - canvas->x) * canvas->pixel_width / canvas->width;
    *y = (uint64_t)(event->y - canvas->y) * canvas->pixel_height / canvas->height;
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
    }
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
        result |= control_event(&context->focused, event);
        return result;
    }
    for (size_t i = 0; i < count; i++) {
        struct gui2_control control = controls[i];
        result |= control_event(&control, event);
    }
    return result;
}
