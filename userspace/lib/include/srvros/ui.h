#ifndef SRVROS_USER_UI_H
#define SRVROS_USER_UI_H

#include <stddef.h>
#include <stdint.h>

#define UI_TRANSPARENT 0xff000000u

struct ui_surface {
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    uint32_t *pixels;
};

enum ui_event_type {
    UI_EVENT_NONE = 0,
    UI_EVENT_MOUSE_MOVE,
    UI_EVENT_MOUSE_DOWN,
    UI_EVENT_MOUSE_UP,
    UI_EVENT_KEY_DOWN,
};

struct ui_event {
    enum ui_event_type type;
    int64_t x;
    int64_t y;
    int64_t local_x;
    int64_t local_y;
    int32_t dx;
    int32_t dy;
    uint8_t buttons;
    uint8_t changed_buttons;
    int key;
};

struct ui_element;

typedef void (*ui_draw_fn)(struct ui_element *element);
typedef int (*ui_event_fn)(struct ui_element *element, const struct ui_event *event);

struct ui_element {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    uint32_t background;
    int visible;
    int dirty;
    int64_t dirty_x;
    int64_t dirty_y;
    uint64_t dirty_width;
    uint64_t dirty_height;
    struct ui_surface surface;
    struct ui_element *parent;
    struct ui_element **children;
    size_t child_count;
    size_t child_capacity;
    ui_draw_fn draw;
    ui_event_fn event;
    void *userdata;
};

struct ui_text_entry {
    char *buffer;
    size_t capacity;
    size_t length;
    int focused;
    uint32_t background;
    uint32_t border;
    uint32_t focus_border;
    uint32_t text;
};

struct ui_image {
    const uint32_t *pixels;
    uint64_t width;
    uint64_t height;
    uint64_t stride;
    uint32_t background;
};

void ui_surface_init(struct ui_surface *surface, uint64_t width, uint64_t height,
    uint64_t stride, uint32_t *pixels);
void ui_element_init(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height, struct ui_surface surface);
int ui_element_add(struct ui_element *parent, struct ui_element *child);
int ui_element_bring_to_front(struct ui_element *element);
void ui_set_focus(struct ui_element *element);
struct ui_element *ui_focused_element(void);

void ui_mark_dirty(struct ui_element *element);
void ui_mark_dirty_rect(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height);
int ui_dirty_rect(const struct ui_element *element, int64_t *x, int64_t *y,
    uint64_t *width, uint64_t *height);
void ui_render_tree(struct ui_element *root);
void ui_present(struct ui_element *root);
void ui_present_rect(struct ui_element *root, int64_t x, int64_t y,
    uint64_t width, uint64_t height);

struct ui_element *ui_dispatch_event(struct ui_element *root,
    const struct ui_event *event);
struct ui_element *ui_dispatch_to(struct ui_element *element,
    const struct ui_event *event);

int ui_hit_test(const struct ui_element *element, int64_t x, int64_t y);
void ui_absolute_position(const struct ui_element *element, int64_t *x, int64_t *y);

void ui_clear(struct ui_element *element, uint32_t color);
void ui_draw_pixel(struct ui_element *element, int64_t x, int64_t y, uint32_t color);
void ui_draw_rect(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t color);
void ui_draw_text(struct ui_element *element, int64_t x, int64_t y,
    const char *text, uint32_t color);

void ui_text_entry_init(struct ui_element *element, struct ui_text_entry *state,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    struct ui_surface surface, char *buffer, size_t capacity);
void ui_text_entry_set_text(struct ui_element *element, const char *text);
void ui_image_init(struct ui_element *element, struct ui_image *state,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    struct ui_surface surface, const uint32_t *pixels,
    uint64_t image_width, uint64_t image_height, uint64_t image_stride);
void ui_image_set(struct ui_element *element, const uint32_t *pixels,
    uint64_t image_width, uint64_t image_height, uint64_t image_stride);

#endif
