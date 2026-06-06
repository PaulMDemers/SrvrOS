#ifndef SRVROS_USER_GUI2_H
#define SRVROS_USER_GUI2_H

#include <srvros/cli.h>
#include <srvros/gui.h>
#include <srvros/ui.h>

#include <stddef.h>
#include <stdint.h>

#define GUI2_FILE_DIALOG_MAX 48
#define GUI2_FILE_NAME_MAX 64

enum gui2_event_type {
    GUI2_EVENT_NONE = 0,
    GUI2_EVENT_CONFIGURE,
    GUI2_EVENT_FOCUS,
    GUI2_EVENT_POINTER_MOVE,
    GUI2_EVENT_POINTER_BUTTON,
    GUI2_EVENT_KEY_DOWN,
    GUI2_EVENT_CLOSE,
};

enum gui2_widget_result {
    GUI2_WIDGET_NONE = 0,
    GUI2_WIDGET_DIRTY = 1,
    GUI2_WIDGET_CLICK = 2,
    GUI2_WIDGET_FOCUS = 4,
    GUI2_WIDGET_VALUE = 8,
};

enum gui2_control_kind {
    GUI2_CONTROL_BUTTON = 1,
    GUI2_CONTROL_TEXTBOX = 2,
    GUI2_CONTROL_CANVAS = 3,
    GUI2_CONTROL_TEXTAREA = 4,
    GUI2_CONTROL_LIST = 5,
};

enum gui2_list_item_flags {
    GUI2_LIST_ITEM_DIR = 1,
};

struct gui2_theme {
    uint32_t canvas;
    uint32_t panel;
    uint32_t panel_alt;
    uint32_t field;
    uint32_t border;
    uint32_t border_focus;
    uint32_t text;
    uint32_t text_muted;
    uint32_t accent;
    uint32_t accent_hover;
    uint32_t accent_down;
    uint32_t danger;
    uint64_t pad;
    uint64_t gap;
    uint64_t control_h;
    uint64_t toolbar_h;
    uint64_t status_h;
    uint64_t list_row_h;
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

struct gui2_rect {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
};

struct gui2_layout {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t gap;
    uint64_t control_h;
};

struct gui2_button {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    const char *label;
    int hovered;
    int pressed;
    int focused;
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
    size_t cursor;
    int focused;
    const char *placeholder;
};

struct gui2_textarea {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    char *buffer;
    size_t capacity;
    size_t length;
    size_t cursor;
    size_t scroll_line;
    int focused;
    const char *placeholder;
};

struct gui2_list_item {
    const char *label;
    const char *detail;
    uint32_t flags;
};

struct gui2_list_column {
    const char *label;
    uint64_t width;
};

struct gui2_list {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    const struct gui2_list_item *items;
    size_t count;
    const struct gui2_list_column *columns;
    size_t column_count;
    size_t selected;
    size_t scroll;
    uint64_t row_height;
    uint64_t header_height;
    size_t sort_column;
    int sort_desc;
    int focused;
    int hovered;
    uint64_t clicks;
    uint64_t activations;
    uint64_t header_clicks;
    size_t clicked_column;
    const char *empty_text;
};

struct gui2_dialog {
    int active;
    const char *title;
    const char *message;
    const char *progress_text;
    uint64_t progress_value;
    uint64_t progress_max;
    int progress_active;
    struct gui2_button primary;
    struct gui2_button secondary;
    uint64_t primary_clicks;
    uint64_t secondary_clicks;
};

struct gui2_canvas {
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    const uint32_t *pixels;
    uint64_t pixel_width;
    uint64_t pixel_height;
    uint64_t view_x;
    uint64_t view_y;
    uint64_t view_width;
    uint64_t view_height;
    uint32_t background;
    int hovered;
    int pressed;
    uint64_t clicks;
    uint64_t pointer_x;
    uint64_t pointer_y;
};

struct gui2_control {
    enum gui2_control_kind kind;
    void *ptr;
};

struct gui2_context {
    struct gui2_control focused;
};

struct gui2_file_dialog_entry {
    char name[GUI2_FILE_NAME_MAX];
    char path[CLI_PATH_MAX];
    char detail[16];
    int dir;
};

struct gui2_file_dialog {
    int active;
    int save_mode;
    const char *title;
    const char *primary_label;
    char cwd[CLI_PATH_MAX];
    char filename[CLI_PATH_MAX];
    char result_path[CLI_PATH_MAX];
    char status[64];
    struct gui2_file_dialog_entry entries[GUI2_FILE_DIALOG_MAX];
    struct gui2_list_item items[GUI2_FILE_DIALOG_MAX];
    size_t count;
    struct gui2_list list;
    struct gui2_textbox path_box;
    struct gui2_button up;
    struct gui2_button primary;
    struct gui2_button cancel;
    struct gui2_context context;
    uint64_t primary_clicks;
    uint64_t cancel_clicks;
};

uint32_t gui2_rgb(uint8_t r, uint8_t g, uint8_t b);
const struct gui2_theme *gui2_theme_default(void);
int gui2_window_open(struct gui2_window *window, uint64_t window_id,
    const char *title, int64_t x, int64_t y, uint64_t width,
    uint64_t height, uint32_t background);
int gui2_window_resize(struct gui2_window *window, uint64_t width, uint64_t height);
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
void gui2_panel(struct gui2_window *window, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t fill);
void gui2_label(struct gui2_window *window, int64_t x, int64_t y,
    const char *text);
void gui2_app_header(struct gui2_window *window, const char *title,
    const char *subtitle);
void gui2_status_bar(struct gui2_window *window, const char *left,
    const char *right);
void gui2_layout_button_row(struct gui2_button *buttons, size_t count,
    int64_t x, int64_t y, uint64_t width, uint64_t height, uint64_t gap);

void gui2_layout_begin(struct gui2_layout *layout, int64_t x, int64_t y,
    uint64_t width);
struct gui2_rect gui2_layout_next(struct gui2_layout *layout, uint64_t height);

void gui2_button_init(struct gui2_button *button, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const char *label);
void gui2_button_draw(struct gui2_window *window, const struct gui2_button *button);
int gui2_button_event(struct gui2_button *button, const struct gui2_event *event);
int gui2_button_contains(const struct gui2_button *button, int64_t x, int64_t y);

void gui2_textbox_init(struct gui2_textbox *textbox, int64_t x, int64_t y,
    uint64_t width, uint64_t height, char *buffer, size_t capacity);
void gui2_textbox_set_placeholder(struct gui2_textbox *textbox, const char *placeholder);
void gui2_textbox_draw(struct gui2_window *window, const struct gui2_textbox *textbox);
int gui2_textbox_event(struct gui2_textbox *textbox, const struct gui2_event *event);
int gui2_textbox_contains(const struct gui2_textbox *textbox, int64_t x, int64_t y);

void gui2_textarea_init(struct gui2_textarea *textarea, int64_t x, int64_t y,
    uint64_t width, uint64_t height, char *buffer, size_t capacity);
void gui2_textarea_set_placeholder(struct gui2_textarea *textarea, const char *placeholder);
void gui2_textarea_sync(struct gui2_textarea *textarea);
void gui2_textarea_set_cursor(struct gui2_textarea *textarea, size_t cursor);
void gui2_textarea_draw(struct gui2_window *window, const struct gui2_textarea *textarea);
int gui2_textarea_event(struct gui2_textarea *textarea, const struct gui2_event *event);
int gui2_textarea_contains(const struct gui2_textarea *textarea, int64_t x, int64_t y);

void gui2_list_init(struct gui2_list *list, int64_t x, int64_t y,
    uint64_t width, uint64_t height);
void gui2_list_set_items(struct gui2_list *list,
    const struct gui2_list_item *items, size_t count);
void gui2_list_set_columns(struct gui2_list *list,
    const struct gui2_list_column *columns, size_t count);
void gui2_list_draw(struct gui2_window *window, const struct gui2_list *list);
int gui2_list_event(struct gui2_list *list, const struct gui2_event *event);
int gui2_list_contains(const struct gui2_list *list, int64_t x, int64_t y);
void gui2_list_keep_selected_visible(struct gui2_list *list);

void gui2_dialog_init(struct gui2_dialog *dialog, const char *title,
    const char *message, const char *primary, const char *secondary);
void gui2_dialog_open(struct gui2_dialog *dialog, const char *title,
    const char *message);
void gui2_dialog_set_progress(struct gui2_dialog *dialog, uint64_t value,
    uint64_t max, const char *text);
void gui2_dialog_close(struct gui2_dialog *dialog);
void gui2_dialog_draw(struct gui2_window *window, struct gui2_dialog *dialog);
int gui2_dialog_event(struct gui2_dialog *dialog, const struct gui2_event *event);

void gui2_file_dialog_init(struct gui2_file_dialog *dialog, const char *title,
    const char *primary_label, int save_mode);
void gui2_file_dialog_open(struct gui2_file_dialog *dialog, const char *cwd,
    const char *filename);
void gui2_file_dialog_close(struct gui2_file_dialog *dialog);
void gui2_file_dialog_draw(struct gui2_window *window,
    struct gui2_file_dialog *dialog);
int gui2_file_dialog_event(struct gui2_file_dialog *dialog,
    const struct gui2_event *event);

void gui2_canvas_init(struct gui2_canvas *canvas, int64_t x, int64_t y,
    uint64_t width, uint64_t height, const uint32_t *pixels,
    uint64_t pixel_width, uint64_t pixel_height, uint32_t background);
void gui2_canvas_set_view(struct gui2_canvas *canvas, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height);
void gui2_canvas_draw(struct gui2_window *window, const struct gui2_canvas *canvas);
int gui2_canvas_event(struct gui2_canvas *canvas, const struct gui2_event *event);
int gui2_canvas_contains(const struct gui2_canvas *canvas, int64_t x, int64_t y);
int gui2_canvas_event_pixel(const struct gui2_canvas *canvas,
    const struct gui2_event *event, uint64_t *x, uint64_t *y);

void gui2_context_init(struct gui2_context *context);
int gui2_dispatch_event(struct gui2_context *context, const struct gui2_event *event,
    const struct gui2_control *controls, size_t count);

#endif
