#ifndef SRVROS_USER_GUI2_H
#define SRVROS_USER_GUI2_H

#include <srvros/ui.h>

#include <stddef.h>
#include <stdint.h>

enum gui2_event_type {
    GUI2_EVENT_NONE = 0,
    GUI2_EVENT_CONFIGURE,
    GUI2_EVENT_FOCUS,
    GUI2_EVENT_POINTER_MOVE,
    GUI2_EVENT_POINTER_BUTTON,
    GUI2_EVENT_KEY_DOWN,
};

struct gui2_event {
    enum gui2_event_type type;
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    int64_t value;
    int key;
    uint8_t buttons;
    uint8_t changed_buttons;
    int focused;
};

struct gui2_window {
    uint64_t window_id;
    uint64_t surface_id;
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    uint32_t background;
    uint32_t *pixels;
    uint8_t buttons;
    int focused;
    int opened;
    int dirty;
    int64_t dirty_x;
    int64_t dirty_y;
    uint64_t dirty_width;
    uint64_t dirty_height;
    struct ui_surface surface;
    struct ui_element root;
};

struct gui2_button {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    const char *label;
    int hovered;
    int pressed;
    uint64_t clicks;
};

struct gui2_textbox {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    char *buffer;
    size_t capacity;
    size_t length;
    int focused;
};

uint32_t gui2_rgb(uint8_t r, uint8_t g, uint8_t b);
int gui2_window_open(struct gui2_window *window, uint64_t window_id,
    const char *title, int64_t x, int64_t y, uint64_t width,
    uint64_t height, uint32_t background);
void gui2_window_close(struct gui2_window *window);
void gui2_window_mark_dirty(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height);
int gui2_window_present(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height);
int gui2_window_present_dirty(struct gui2_window *window);
int gui2_poll_event(struct gui2_window *window, struct gui2_event *event);

void gui2_clear(struct gui2_window *window, uint32_t color);
void gui2_rect(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t color);
void gui2_text(struct gui2_window *window, int64_t x, int64_t y,
    const char *text, uint32_t color);

void gui2_button_init(struct gui2_button *button, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const char *label);
void gui2_button_draw(struct gui2_window *window, const struct gui2_button *button);
int gui2_button_event(struct gui2_button *button, const struct gui2_event *event);

void gui2_textbox_init(struct gui2_textbox *textbox, int64_t x, int64_t y,
    uint64_t width, uint64_t height, char *buffer, size_t capacity);
void gui2_textbox_draw(struct gui2_window *window, const struct gui2_textbox *textbox);
int gui2_textbox_event(struct gui2_textbox *textbox, const struct gui2_event *event);

#endif
