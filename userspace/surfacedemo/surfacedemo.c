#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define SURFACE_W 240
#define SURFACE_H 140

struct demo_state {
    int64_t pointer_x;
    int64_t pointer_y;
    int focused;
    int last_key;
};

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

static void put_pixel(struct gui2_window *window, int64_t x, int64_t y, uint32_t color) {
    if (window == 0 || window->pixels == 0 ||
        x < 0 || y < 0 || x >= (int64_t)window->width || y >= (int64_t)window->height) {
        return;
    }
    window->pixels[(uint64_t)y * window->width + (uint64_t)x] = color;
}

static void draw_demo(struct gui2_window *window, uint64_t tick, const struct demo_state *state) {
    for (uint64_t y = 0; y < window->height; y++) {
        for (uint64_t x = 0; x < window->width; x++) {
            uint8_t r = (uint8_t)((x + tick * 3) & 0xff);
            uint8_t g = (uint8_t)((y * 2 + tick * 5) & 0xff);
            uint8_t b = (uint8_t)(((x ^ y) + tick * 2 + state->last_key) & 0xff);
            window->pixels[y * window->width + x] = gui2_rgb(r, g, b);
        }
    }
    for (uint64_t y = 22; y < 72; y++) {
        for (uint64_t x = 32; x < 112; x++) {
            window->pixels[y * window->width + x] = gui2_rgb(0x11, 0x1b, 0x24);
        }
    }
    for (uint64_t y = 28; y < 66; y++) {
        for (uint64_t x = 126; x < 210; x++) {
            window->pixels[y * window->width + x] = gui2_rgb(0xf5, 0xb8, 0x4b);
        }
    }
    if (state->focused) {
        for (uint64_t x = 0; x < window->width; x++) {
            window->pixels[x] = gui2_rgb(0xff, 0xff, 0xff);
            window->pixels[(window->height - 1) * window->width + x] = gui2_rgb(0xff, 0xff, 0xff);
        }
        for (uint64_t y = 0; y < window->height; y++) {
            window->pixels[y * window->width] = gui2_rgb(0xff, 0xff, 0xff);
            window->pixels[y * window->width + window->width - 1] = gui2_rgb(0xff, 0xff, 0xff);
        }
    }
    if (state->pointer_x >= 0 && state->pointer_y >= 0) {
        uint32_t color = gui2_rgb(0xff, 0xff, 0xff);
        for (int64_t d = -8; d <= 8; d++) {
            put_pixel(window, state->pointer_x + d, state->pointer_y, color);
            put_pixel(window, state->pointer_x, state->pointer_y + d, color);
        }
    }
    gui2_window_mark_dirty(window, 0, 0, window->width, window->height);
}

static int handle_event(struct demo_state *state, const struct gui2_event *event) {
    if (event->type == GUI2_EVENT_CONFIGURE) {
        srv_puts("surfacedemo: configure ");
        print_u64(event->width);
        srv_puts("x");
        print_u64(event->height);
        srv_puts("\n");
        return 1;
    }
    if (event->type == GUI2_EVENT_FOCUS) {
        state->focused = event->focused;
        srv_puts("surfacedemo: focus ");
        print_u64(state->focused ? 1 : 0);
        srv_puts("\n");
        return 1;
    }
    if (event->type == GUI2_EVENT_POINTER_MOVE ||
        event->type == GUI2_EVENT_POINTER_BUTTON) {
        state->pointer_x = event->x;
        state->pointer_y = event->y;
        return 1;
    }
    if (event->type == GUI2_EVENT_KEY_DOWN) {
        state->last_key = event->key;
        return 1;
    }
    return 0;
}

int main(void) {
    struct demo_state state = {
        .pointer_x = -1,
        .pointer_y = -1,
        .focused = 0,
        .last_key = 0,
    };
    struct gui2_window window;
    uint64_t start;

    srv_puts("surfacedemo: start\n");
    if (gui2_window_open(&window, WIN, "SURFACE DEMO",
            340, 190, SURFACE_W, SURFACE_H, gui2_rgb(0x11, 0x1b, 0x24)) != 0) {
        srv_puts("surfacedemo: window open failed\n");
        return 1;
    }

    draw_demo(&window, 0, &state);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 140) {
            break;
        }
        while (gui2_poll_event(&window, &event) > 0) {
            changed |= handle_event(&state, &event);
        }
        if (changed || (elapsed % 20) == 0) {
            draw_demo(&window, elapsed, &state);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("surfacedemo: exited\n");
    return 0;
}
