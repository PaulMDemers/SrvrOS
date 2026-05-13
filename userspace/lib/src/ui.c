#include <srvros/gfx.h>
#include <srvros/sys.h>
#include <srvros/ui.h>

static struct ui_element *focused_element;

static int64_t max_i64(int64_t a, int64_t b) {
    return a > b ? a : b;
}

static int64_t min_i64(int64_t a, int64_t b) {
    return a < b ? a : b;
}

static void set_dirty_rect(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    if (element == 0 || width == 0 || height == 0) {
        return;
    }

    int64_t x0 = max_i64(x, 0);
    int64_t y0 = max_i64(y, 0);
    int64_t x1 = min_i64(x + (int64_t)width, (int64_t)element->width);
    int64_t y1 = min_i64(y + (int64_t)height, (int64_t)element->height);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    if (!element->dirty || element->dirty_width == 0 || element->dirty_height == 0) {
        element->dirty_x = x0;
        element->dirty_y = y0;
        element->dirty_width = (uint64_t)(x1 - x0);
        element->dirty_height = (uint64_t)(y1 - y0);
        return;
    }

    int64_t old_x1 = element->dirty_x + (int64_t)element->dirty_width;
    int64_t old_y1 = element->dirty_y + (int64_t)element->dirty_height;
    int64_t union_x0 = min_i64(element->dirty_x, x0);
    int64_t union_y0 = min_i64(element->dirty_y, y0);
    int64_t union_x1 = max_i64(old_x1, x1);
    int64_t union_y1 = max_i64(old_y1, y1);

    element->dirty_x = union_x0;
    element->dirty_y = union_y0;
    element->dirty_width = (uint64_t)(union_x1 - union_x0);
    element->dirty_height = (uint64_t)(union_y1 - union_y0);
}

void ui_surface_init(struct ui_surface *surface, uint64_t width, uint64_t height,
    uint64_t stride, uint32_t *pixels) {
    if (surface == 0) {
        return;
    }
    surface->width = width;
    surface->height = height;
    surface->stride = stride == 0 ? width : stride;
    surface->pixels = pixels;
}

void ui_element_init(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height, struct ui_surface surface) {
    if (element == 0) {
        return;
    }
    element->x = x;
    element->y = y;
    element->width = width;
    element->height = height;
    element->background = 0x101820;
    element->visible = 1;
    element->dirty = 1;
    element->dirty_x = 0;
    element->dirty_y = 0;
    element->dirty_width = width;
    element->dirty_height = height;
    element->surface = surface;
    element->parent = 0;
    element->children = 0;
    element->child_count = 0;
    element->child_capacity = 0;
    element->draw = 0;
    element->event = 0;
    element->userdata = 0;
}

int ui_element_add(struct ui_element *parent, struct ui_element *child) {
    if (parent == 0 || child == 0 || parent->children == 0 ||
        parent->child_count >= parent->child_capacity) {
        return -1;
    }
    child->parent = parent;
    parent->children[parent->child_count++] = child;
    ui_mark_dirty(parent);
    return 0;
}

int ui_element_bring_to_front(struct ui_element *element) {
    if (element == 0 || element->parent == 0 || element->parent->children == 0) {
        return -1;
    }

    struct ui_element *parent = element->parent;
    size_t index = parent->child_count;
    for (size_t i = 0; i < parent->child_count; i++) {
        if (parent->children[i] == element) {
            index = i;
            break;
        }
    }
    if (index >= parent->child_count) {
        return -1;
    }
    for (size_t i = index; i + 1 < parent->child_count; i++) {
        parent->children[i] = parent->children[i + 1];
    }
    parent->children[parent->child_count - 1] = element;
    ui_mark_dirty_rect(parent, element->x, element->y, element->width, element->height);
    return 0;
}

void ui_set_focus(struct ui_element *element) {
    if (focused_element == element) {
        return;
    }

    struct ui_element *old = focused_element;
    focused_element = element;
    if (old != 0) {
        ui_mark_dirty(old);
    }
    if (focused_element != 0) {
        ui_mark_dirty(focused_element);
    }
}

struct ui_element *ui_focused_element(void) {
    return focused_element;
}

void ui_mark_dirty(struct ui_element *element) {
    if (element != 0) {
        ui_mark_dirty_rect(element, 0, 0, element->width, element->height);
    }
}

void ui_mark_dirty_rect(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    while (element != 0 && width != 0 && height != 0) {
        int64_t x0 = max_i64(x, 0);
        int64_t y0 = max_i64(y, 0);
        int64_t x1 = min_i64(x + (int64_t)width, (int64_t)element->width);
        int64_t y1 = min_i64(y + (int64_t)height, (int64_t)element->height);
        if (x1 <= x0 || y1 <= y0) {
            return;
        }

        element->dirty = 1;
        set_dirty_rect(element, x0, y0, (uint64_t)(x1 - x0), (uint64_t)(y1 - y0));
        x = x0 + element->x;
        y = y0 + element->y;
        width = (uint64_t)(x1 - x0);
        height = (uint64_t)(y1 - y0);
        element = element->parent;
    }
}

int ui_dirty_rect(const struct ui_element *element, int64_t *x, int64_t *y,
    uint64_t *width, uint64_t *height) {
    if (element == 0 || !element->dirty ||
        element->dirty_width == 0 || element->dirty_height == 0) {
        return 0;
    }
    if (x != 0) {
        *x = element->dirty_x;
    }
    if (y != 0) {
        *y = element->dirty_y;
    }
    if (width != 0) {
        *width = element->dirty_width;
    }
    if (height != 0) {
        *height = element->dirty_height;
    }
    return 1;
}

void ui_absolute_position(const struct ui_element *element, int64_t *x, int64_t *y) {
    int64_t ax = 0;
    int64_t ay = 0;
    while (element != 0) {
        ax += element->x;
        ay += element->y;
        element = element->parent;
    }
    if (x != 0) {
        *x = ax;
    }
    if (y != 0) {
        *y = ay;
    }
}

int ui_hit_test(const struct ui_element *element, int64_t x, int64_t y) {
    int64_t ax = 0;
    int64_t ay = 0;
    if (element == 0 || !element->visible) {
        return 0;
    }
    ui_absolute_position(element, &ax, &ay);
    return x >= ax && y >= ay &&
        x < ax + (int64_t)element->width &&
        y < ay + (int64_t)element->height;
}

void ui_draw_pixel(struct ui_element *element, int64_t x, int64_t y, uint32_t color) {
    if (element == 0 || x < 0 || y < 0 ||
        x >= (int64_t)element->width || y >= (int64_t)element->height) {
        return;
    }

    if (element->surface.pixels != 0) {
        if (x < (int64_t)element->surface.width && y < (int64_t)element->surface.height) {
            element->surface.pixels[(uint64_t)y * element->surface.stride + (uint64_t)x] = color;
        }
        return;
    }

    int64_t ax = 0;
    int64_t ay = 0;
    ui_absolute_position(element, &ax, &ay);
    putpixel((uint64_t)(ax + x), (uint64_t)(ay + y), color);
}

void ui_draw_rect(struct ui_element *element, int64_t x, int64_t y,
    uint64_t width, uint64_t height, uint32_t color) {
    if (element == 0 || width == 0 || height == 0) {
        return;
    }

    int64_t x0 = max_i64(x, 0);
    int64_t y0 = max_i64(y, 0);
    int64_t x1 = min_i64(x + (int64_t)width, (int64_t)element->width);
    int64_t y1 = min_i64(y + (int64_t)height, (int64_t)element->height);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    if (element->surface.pixels != 0) {
        for (int64_t py = y0; py < y1; py++) {
            for (int64_t px = x0; px < x1; px++) {
                element->surface.pixels[(uint64_t)py * element->surface.stride + (uint64_t)px] = color;
            }
        }
        return;
    }

    int64_t ax = 0;
    int64_t ay = 0;
    ui_absolute_position(element, &ax, &ay);
    fillrect((uint64_t)(ax + x0), (uint64_t)(ay + y0),
        (uint64_t)(x1 - x0), (uint64_t)(y1 - y0), color);
}

void ui_clear(struct ui_element *element, uint32_t color) {
    if (element == 0) {
        return;
    }
    ui_draw_rect(element, 0, 0, element->width, element->height, color);
}

static void composite_child(struct ui_element *parent, const struct ui_element *child) {
    if (parent == 0 || child == 0 || parent->surface.pixels == 0 ||
        child->surface.pixels == 0 || !child->visible) {
        return;
    }

    int64_t parent_ax = 0;
    int64_t parent_ay = 0;
    int64_t child_ax = 0;
    int64_t child_ay = 0;
    ui_absolute_position(parent, &parent_ax, &parent_ay);
    ui_absolute_position(child, &child_ax, &child_ay);

    for (uint64_t y = 0; y < child->height; y++) {
        for (uint64_t x = 0; x < child->width; x++) {
            uint32_t color = child->surface.pixels[y * child->surface.stride + x];
            if (color == UI_TRANSPARENT) {
                continue;
            }

            int64_t px = child_ax - parent_ax + (int64_t)x;
            int64_t py = child_ay - parent_ay + (int64_t)y;
            ui_draw_pixel(parent, px, py, color);
        }
    }
}

void ui_render_tree(struct ui_element *root) {
    if (root == 0 || !root->visible) {
        return;
    }

    if (!root->dirty) {
        return;
    }

    int redraw_surface = root->surface.pixels != 0;
    if (redraw_surface) {
        if (root->draw != 0) {
            root->draw(root);
        } else {
            ui_clear(root, root->background);
        }
    } else if (root->draw != 0) {
        root->draw(root);
    }

    for (size_t i = 0; i < root->child_count; i++) {
        struct ui_element *child = root->children[i];
        if (child->dirty) {
            ui_render_tree(child);
        }
        if (redraw_surface) {
            composite_child(root, child);
        }
    }

    root->dirty = 0;
    root->dirty_x = 0;
    root->dirty_y = 0;
    root->dirty_width = 0;
    root->dirty_height = 0;
}

static void present_surface_rect(const struct ui_element *element,
    int64_t rx, int64_t ry, int64_t rw, int64_t rh) {
    if (element == 0 || element->surface.pixels == 0 || !element->visible) {
        return;
    }

    int64_t ax = 0;
    int64_t ay = 0;
    ui_absolute_position(element, &ax, &ay);
    int64_t x0 = max_i64(ax, rx);
    int64_t y0 = max_i64(ay, ry);
    int64_t x1 = min_i64(ax + (int64_t)element->width, rx + rw);
    int64_t y1 = min_i64(ay + (int64_t)element->height, ry + rh);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    for (int64_t sy = y0; sy < y1; sy++) {
        int64_t sx = x0;
        while (sx < x1) {
            uint64_t lx = (uint64_t)(sx - ax);
            uint64_t ly = (uint64_t)(sy - ay);
            uint32_t color = element->surface.pixels[ly * element->surface.stride + lx];
            if (color == UI_TRANSPARENT) {
                sx++;
                continue;
            }

            int64_t run_end = sx + 1;
            while (run_end < x1) {
                uint64_t run_lx = (uint64_t)(run_end - ax);
                uint32_t next = element->surface.pixels[ly * element->surface.stride + run_lx];
                if (next != color) {
                    break;
                }
                run_end++;
            }
            fillrect((uint64_t)sx, (uint64_t)sy, (uint64_t)(run_end - sx), 1, color);
            sx = run_end;
        }
    }
}

void ui_present_rect(struct ui_element *root, int64_t x, int64_t y,
    uint64_t width, uint64_t height) {
    if (root == 0 || width == 0 || height == 0) {
        return;
    }

    struct ui_element **children = root->children;
    size_t child_count = root->child_count;

    int64_t x0 = max_i64(x, 0);
    int64_t y0 = max_i64(y, 0);
    int64_t x1 = min_i64(x + (int64_t)width, (int64_t)root->width);
    int64_t y1 = min_i64(y + (int64_t)height, (int64_t)root->height);
    if (x1 <= x0 || y1 <= y0) {
        return;
    }

    if (root->surface.pixels == 0) {
        fillrect((uint64_t)x0, (uint64_t)y0,
            (uint64_t)(x1 - x0), (uint64_t)(y1 - y0), root->background);
    } else {
        present_surface_rect(root, x0, y0, x1 - x0, y1 - y0);
    }

    for (size_t i = 0; i < child_count; i++) {
        present_surface_rect(children[i], x0, y0, x1 - x0, y1 - y0);
    }
}

void ui_present(struct ui_element *root) {
    if (root == 0) {
        return;
    }
    ui_present_rect(root, 0, 0, root->width, root->height);
}

static struct ui_element *dispatch_at(struct ui_element *element,
    const struct ui_event *event, int force) {
    if (element == 0 || event == 0 || !element->visible) {
        return 0;
    }

    if (!force && event->type != UI_EVENT_KEY_DOWN && !ui_hit_test(element, event->x, event->y)) {
        return 0;
    }

    struct ui_event local = *event;
    int64_t ax = 0;
    int64_t ay = 0;
    ui_absolute_position(element, &ax, &ay);
    local.local_x = event->x - ax;
    local.local_y = event->y - ay;

    if (element->event != 0 && element->event(element, &local)) {
        return element;
    }

    for (size_t i = element->child_count; i > 0; i--) {
        struct ui_element *child = element->children[i - 1];
        struct ui_element *consumer = dispatch_at(child, event, event->type == UI_EVENT_KEY_DOWN);
        if (consumer != 0) {
            return consumer;
        }
    }

    return 0;
}

struct ui_element *ui_dispatch_event(struct ui_element *root,
    const struct ui_event *event) {
    return dispatch_at(root, event, 1);
}

struct ui_element *ui_dispatch_to(struct ui_element *element,
    const struct ui_event *event) {
    return dispatch_at(element, event, 1);
}

static uint8_t glyph_row(char c, uint64_t row) {
    static const uint8_t unknown[7] = { 0x0e, 0x11, 0x01, 0x06, 0x04, 0x00, 0x04 };
    static const uint8_t glyphs[][7] = {
        { 0x0e, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0e }, /* 0 */
        { 0x04, 0x0c, 0x04, 0x04, 0x04, 0x04, 0x0e }, /* 1 */
        { 0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f }, /* 2 */
        { 0x1e, 0x01, 0x01, 0x0e, 0x01, 0x01, 0x1e }, /* 3 */
        { 0x02, 0x06, 0x0a, 0x12, 0x1f, 0x02, 0x02 }, /* 4 */
        { 0x1f, 0x10, 0x10, 0x1e, 0x01, 0x01, 0x1e }, /* 5 */
        { 0x0e, 0x10, 0x10, 0x1e, 0x11, 0x11, 0x0e }, /* 6 */
        { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08 }, /* 7 */
        { 0x0e, 0x11, 0x11, 0x0e, 0x11, 0x11, 0x0e }, /* 8 */
        { 0x0e, 0x11, 0x11, 0x0f, 0x01, 0x01, 0x0e }, /* 9 */
        { 0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 }, /* A */
        { 0x1e, 0x11, 0x11, 0x1e, 0x11, 0x11, 0x1e }, /* B */
        { 0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e }, /* C */
        { 0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e }, /* D */
        { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f }, /* E */
        { 0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10 }, /* F */
        { 0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f }, /* G */
        { 0x11, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11 }, /* H */
        { 0x0e, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0e }, /* I */
        { 0x07, 0x02, 0x02, 0x02, 0x12, 0x12, 0x0c }, /* J */
        { 0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11 }, /* K */
        { 0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1f }, /* L */
        { 0x11, 0x1b, 0x15, 0x15, 0x11, 0x11, 0x11 }, /* M */
        { 0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11 }, /* N */
        { 0x0e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e }, /* O */
        { 0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10 }, /* P */
        { 0x0e, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0d }, /* Q */
        { 0x1e, 0x11, 0x11, 0x1e, 0x14, 0x12, 0x11 }, /* R */
        { 0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e }, /* S */
        { 0x1f, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04 }, /* T */
        { 0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0e }, /* U */
        { 0x11, 0x11, 0x11, 0x11, 0x11, 0x0a, 0x04 }, /* V */
        { 0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a }, /* W */
        { 0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11 }, /* X */
        { 0x11, 0x11, 0x0a, 0x04, 0x04, 0x04, 0x04 }, /* Y */
        { 0x1f, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1f }, /* Z */
    };

    if (row >= 7) {
        return 0;
    }
    if (c >= 'a' && c <= 'z') {
        c = (char)(c - 'a' + 'A');
    }
    if (c >= '0' && c <= '9') {
        return glyphs[c - '0'][row];
    }
    if (c >= 'A' && c <= 'Z') {
        return glyphs[10 + c - 'A'][row];
    }
    if (c == ' ') {
        return 0;
    }
    if (c == '.') {
        static const uint8_t dot[7] = { 0, 0, 0, 0, 0, 0x0c, 0x0c };
        return dot[row];
    }
    if (c == ':') {
        static const uint8_t colon[7] = { 0, 0x0c, 0x0c, 0, 0x0c, 0x0c, 0 };
        return colon[row];
    }
    if (c == '-') {
        static const uint8_t dash[7] = { 0, 0, 0, 0x1f, 0, 0, 0 };
        return dash[row];
    }
    if (c == '+') {
        static const uint8_t plus[7] = { 0, 0x04, 0x04, 0x1f, 0x04, 0x04, 0 };
        return plus[row];
    }
    if (c == '=') {
        static const uint8_t equals[7] = { 0, 0, 0x1f, 0, 0x1f, 0, 0 };
        return equals[row];
    }
    return unknown[row];
}

void ui_draw_text(struct ui_element *element, int64_t x, int64_t y,
    const char *text, uint32_t color) {
    if (element == 0 || text == 0) {
        return;
    }

    int64_t pen = x;
    int64_t line_y = y;
    for (size_t i = 0; text[i] != '\0'; i++) {
        if (text[i] == '\n') {
            pen = x;
            line_y += 10;
            continue;
        }
        for (uint64_t row = 0; row < 7; row++) {
            uint8_t bits = glyph_row(text[i], row);
            for (uint64_t col = 0; col < 5; col++) {
                if ((bits & (1u << (4 - col))) != 0) {
                    ui_draw_pixel(element, pen + (int64_t)col, line_y + (int64_t)row, color);
                }
            }
        }
        pen += 6;
    }
}

static size_t text_length(const char *text) {
    size_t length = 0;
    if (text == 0) {
        return 0;
    }
    while (text[length] != '\0') {
        length++;
    }
    return length;
}

static void text_copy(char *to, size_t capacity, const char *from) {
    size_t i = 0;
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

static void draw_text_entry(struct ui_element *element) {
    struct ui_text_entry *state = (struct ui_text_entry *)element->userdata;
    int focused = ui_focused_element() == element;
    uint32_t background = state != 0 ? state->background : 0x101820;
    uint32_t border = state != 0 && focused ? state->focus_border :
        (state != 0 ? state->border : 0x425466);
    const char *text = state != 0 && state->buffer != 0 ? state->buffer : "";

    if (state != 0) {
        state->focused = focused;
    }
    ui_clear(element, background);
    ui_draw_rect(element, 0, 0, element->width, 1, border);
    ui_draw_rect(element, 0, element->height - 1, element->width, 1, border);
    ui_draw_rect(element, 0, 0, 1, element->height, border);
    ui_draw_rect(element, element->width - 1, 0, 1, element->height, border);
    ui_draw_text(element, 6, 8, text, state != 0 ? state->text : 0xffffff);
    if (state != 0 && focused) {
        int64_t cursor_x = 6 + (int64_t)(state->length * 6);
        if (cursor_x < (int64_t)element->width - 4) {
            ui_draw_rect(element, cursor_x, 6, 1, element->height - 12, state->text);
        }
    }
}

static int text_entry_event(struct ui_element *element, const struct ui_event *event) {
    struct ui_text_entry *state = (struct ui_text_entry *)element->userdata;
    if (state == 0 || event == 0) {
        return 0;
    }
    if (event->type == UI_EVENT_MOUSE_DOWN) {
        ui_set_focus(element);
        state->focused = 1;
        ui_mark_dirty(element);
        return 1;
    }
    if (event->type != UI_EVENT_KEY_DOWN || ui_focused_element() != element) {
        return 0;
    }

    state->focused = 1;
    if ((event->key == 8 || event->key == 127) && state->length > 0) {
        state->buffer[--state->length] = '\0';
        ui_mark_dirty(element);
        return 1;
    }
    if (event->key >= 32 && event->key < 127 &&
        state->buffer != 0 && state->capacity > 0 &&
        state->length + 1 < state->capacity) {
        state->buffer[state->length++] = (char)event->key;
        state->buffer[state->length] = '\0';
        ui_mark_dirty(element);
        return 1;
    }
    return 1;
}

void ui_text_entry_init(struct ui_element *element, struct ui_text_entry *state,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    struct ui_surface surface, char *buffer, size_t capacity) {
    if (element == 0 || state == 0) {
        return;
    }

    ui_element_init(element, x, y, width, height, surface);
    state->buffer = buffer;
    state->capacity = capacity;
    state->length = text_length(buffer);
    state->focused = 0;
    state->background = 0x101820;
    state->border = 0x425466;
    state->focus_border = 0x8fd0d4;
    state->text = 0xffffff;
    element->draw = draw_text_entry;
    element->event = text_entry_event;
    element->userdata = state;
}

void ui_text_entry_set_text(struct ui_element *element, const char *text) {
    struct ui_text_entry *state;
    if (element == 0) {
        return;
    }
    state = (struct ui_text_entry *)element->userdata;
    if (state == 0 || state->buffer == 0 || state->capacity == 0) {
        return;
    }
    text_copy(state->buffer, state->capacity, text);
    state->length = text_length(state->buffer);
    ui_mark_dirty(element);
}

static void draw_image(struct ui_element *element) {
    struct ui_image *state = (struct ui_image *)element->userdata;
    if (state == 0 || state->pixels == 0 ||
        state->width == 0 || state->height == 0) {
        ui_clear(element, state != 0 ? state->background : UI_TRANSPARENT);
        return;
    }

    ui_clear(element, state->background);
    for (uint64_t y = 0; y < element->height; y++) {
        uint64_t source_y = (y * state->height) / element->height;
        for (uint64_t x = 0; x < element->width; x++) {
            uint64_t source_x = (x * state->width) / element->width;
            uint32_t color = state->pixels[source_y * state->stride + source_x];
            if (color != UI_TRANSPARENT) {
                ui_draw_pixel(element, (int64_t)x, (int64_t)y, color);
            }
        }
    }
}

void ui_image_init(struct ui_element *element, struct ui_image *state,
    int64_t x, int64_t y, uint64_t width, uint64_t height,
    struct ui_surface surface, const uint32_t *pixels,
    uint64_t image_width, uint64_t image_height, uint64_t image_stride) {
    if (element == 0 || state == 0) {
        return;
    }

    ui_element_init(element, x, y, width, height, surface);
    state->pixels = pixels;
    state->width = image_width;
    state->height = image_height;
    state->stride = image_stride == 0 ? image_width : image_stride;
    state->background = UI_TRANSPARENT;
    element->draw = draw_image;
    element->event = 0;
    element->userdata = state;
}

void ui_image_set(struct ui_element *element, const uint32_t *pixels,
    uint64_t image_width, uint64_t image_height, uint64_t image_stride) {
    struct ui_image *state;
    if (element == 0) {
        return;
    }
    state = (struct ui_image *)element->userdata;
    if (state == 0) {
        return;
    }
    state->pixels = pixels;
    state->width = image_width;
    state->height = image_height;
    state->stride = image_stride == 0 ? image_width : image_stride;
    ui_mark_dirty(element);
}
