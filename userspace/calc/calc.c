#include <srvros/gui2.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define WIDTH 360
#define HEIGHT 300
#define BUTTON_COUNT 25
#define CALC_I64_MIN (-9223372036854775807LL - 1LL)

struct calc_button_def {
    const char *label;
    int value;
};

struct calc_state {
    int64_t current;
    int64_t stored;
    int pending_op;
    int entering;
    char status[48];
};

static const struct calc_button_def button_defs[BUTTON_COUNT] = {
    { "C", 'C' }, { "CE", 'E' }, { "BS", 'B' }, { "+/-", 'N' }, { "/", '/' },
    { "7", 7 }, { "8", 8 }, { "9", 9 }, { "%", '%' }, { "*", '*' },
    { "4", 4 }, { "5", 5 }, { "6", 6 }, { "SQ", 'S' }, { "-", '-' },
    { "1", 1 }, { "2", 2 }, { "3", 3 }, { "ABS", 'A' }, { "+", '+' },
    { "0", 0 }, { "00", 'Z' }, { ".", '.' }, { "ANS", 'M' }, { "=", '=' },
};

static void copy_text(char *to, uint64_t capacity, const char *from) {
    uint64_t i = 0;
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

static void append_char(char *out, uint64_t capacity, uint64_t *length, char c) {
    if (*length + 1 >= capacity) {
        return;
    }
    out[(*length)++] = c;
    out[*length] = '\0';
}

static void append_i64(char *out, uint64_t capacity, uint64_t *length, int64_t value) {
    char digits[21];
    uint64_t count = 0;
    uint64_t magnitude;
    if (value < 0) {
        append_char(out, capacity, length, '-');
        magnitude = (uint64_t)(-(value + 1)) + 1;
    } else {
        magnitude = (uint64_t)value;
    }
    if (magnitude == 0) {
        append_char(out, capacity, length, '0');
        return;
    }
    while (magnitude > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }
    while (count > 0) {
        append_char(out, capacity, length, digits[--count]);
    }
}

static int64_t apply_op(int64_t left, int64_t right, int op, struct calc_state *state) {
    if (op == '+') {
        return left + right;
    }
    if (op == '-') {
        return left - right;
    }
    if (op == '*') {
        return left * right;
    }
    if (op == '/') {
        if (right == 0) {
            copy_text(state->status, sizeof(state->status), "DIVIDE BY ZERO");
            return left;
        }
        return left / right;
    }
    if (op == '%') {
        if (right == 0) {
            copy_text(state->status, sizeof(state->status), "MOD BY ZERO");
            return left;
        }
        return left % right;
    }
    return right;
}

static void format_display(const struct calc_state *state, char *out, uint64_t capacity) {
    uint64_t length = 0;
    out[0] = '\0';
    if (state->pending_op != 0) {
        append_i64(out, capacity, &length, state->stored);
        append_char(out, capacity, &length, ' ');
        append_char(out, capacity, &length, (char)state->pending_op);
        append_char(out, capacity, &length, ' ');
    }
    append_i64(out, capacity, &length, state->current);
}

static void press_value(struct calc_state *state, int value) {
    if (value >= 0 && value <= 9) {
        if (!state->entering) {
            state->current = 0;
            state->entering = 1;
        }
        state->current = state->current * 10 + value;
        copy_text(state->status, sizeof(state->status), "READY");
    } else if (value == 'Z') {
        press_value(state, 0);
        press_value(state, 0);
    } else if (value == '+' || value == '-' || value == '*' || value == '/' || value == '%') {
        if (state->pending_op != 0 && state->entering) {
            state->stored = apply_op(state->stored, state->current, state->pending_op, state);
        } else {
            state->stored = state->current;
        }
        state->current = 0;
        state->pending_op = value;
        state->entering = 0;
    } else if (value == '=') {
        if (state->pending_op != 0) {
            state->current = apply_op(state->stored, state->current, state->pending_op, state);
            state->stored = 0;
            state->pending_op = 0;
            state->entering = 0;
        }
    } else if (value == 'C') {
        state->current = 0;
        state->stored = 0;
        state->pending_op = 0;
        state->entering = 0;
        copy_text(state->status, sizeof(state->status), "READY");
    } else if (value == 'E') {
        state->current = 0;
        state->entering = 0;
        copy_text(state->status, sizeof(state->status), "READY");
    } else if (value == 'B') {
        state->current /= 10;
    } else if (value == 'N') {
        if (state->current == CALC_I64_MIN) {
            copy_text(state->status, sizeof(state->status), "OUT OF RANGE");
        } else {
            state->current = -state->current;
        }
    } else if (value == 'S') {
        state->current = state->current * state->current;
        state->entering = 0;
    } else if (value == 'A') {
        if (state->current < 0) {
            if (state->current == CALC_I64_MIN) {
                copy_text(state->status, sizeof(state->status), "OUT OF RANGE");
            } else {
                state->current = -state->current;
            }
        }
        state->entering = 0;
    } else if (value == 'M') {
        state->current = state->stored;
        state->entering = 1;
    } else if (value == '.') {
        copy_text(state->status, sizeof(state->status), "INTEGER MODE");
    }
}

static void layout_buttons(struct gui2_window *window, struct gui2_button *buttons) {
    const struct gui2_theme *theme = gui2_theme_default();
    uint64_t pad = theme->pad;
    uint64_t gap = theme->gap;
    uint64_t display_h = 58;
    uint64_t top = pad + 18 + gap + display_h + gap;
    uint64_t usable_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    uint64_t usable_h = window->height > top + pad ? window->height - top - pad : 1;
    uint64_t button_w = usable_w > 4 * gap ? (usable_w - 4 * gap) / 5 : 1;
    uint64_t button_h = usable_h > 4 * gap ? (usable_h - 4 * gap) / 5 : 1;
    button_h = button_h < 24 ? 24 : button_h;
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        uint64_t row = i / 5;
        uint64_t col = i % 5;
        buttons[i].x = (int64_t)(pad + col * (button_w + gap));
        buttons[i].y = (int64_t)(top + row * (button_h + gap));
        buttons[i].width = button_w;
        buttons[i].height = button_h;
    }
}

static void draw_calc(struct gui2_window *window, struct gui2_button *buttons,
    const struct calc_state *state) {
    const struct gui2_theme *theme = gui2_theme_default();
    char display[96];
    uint64_t pad = theme->pad;
    uint64_t display_w = window->width > 2 * pad ? window->width - 2 * pad : 1;
    format_display(state, display, sizeof(display));
    layout_buttons(window, buttons);
    gui2_clear(window, theme->canvas);
    gui2_text(window, (int64_t)pad, (int64_t)pad, "CALC", theme->text);
    gui2_text(window, (int64_t)(pad + 70), (int64_t)pad,
        state->status[0] != '\0' ? state->status : "READY", theme->text_muted);
    gui2_panel(window, (int64_t)pad, (int64_t)(pad + 26), display_w, 58, theme->field);
    gui2_text(window, (int64_t)(pad + 12), (int64_t)(pad + 48), display, theme->text);
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        gui2_button_draw(window, &buttons[i]);
    }
}

int main(void) {
    struct gui2_window window;
    struct gui2_context context;
    struct gui2_button buttons[BUTTON_COUNT];
    struct calc_state state = {
        .current = 0,
        .stored = 0,
        .pending_op = 0,
        .entering = 0,
        .status = "READY",
    };
    uint64_t start;

    srv_puts("calc: start\n");
    if (gui2_window_open(&window, WIN, "CALC",
            720, 230, WIDTH, HEIGHT, gui2_theme_default()->canvas) != 0) {
        srv_puts("calc: window open failed\n");
        return 1;
    }
    gui2_context_init(&context);
    for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
        gui2_button_init(&buttons[i], 0, 0, 1, 1, button_defs[i].label);
    }
    draw_calc(&window, buttons, &state);
    gui2_window_present_dirty(&window);

    start = (uint64_t)srv_ticks();
    for (;;) {
        struct gui2_event event;
        int changed = 0;
        int closing = 0;
        struct gui2_control controls[BUTTON_COUNT];
        uint64_t elapsed = (uint64_t)srv_ticks() - start;
        if (elapsed > 260) {
            break;
        }
        for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
            controls[i].kind = GUI2_CONTROL_BUTTON;
            controls[i].ptr = &buttons[i];
        }
        while (gui2_poll_event(&window, &event) > 0) {
            if (event.type == GUI2_EVENT_CONFIGURE) {
                srv_puts("calc: configure ");
                print_u64(event.width);
                srv_puts("x");
                print_u64(event.height);
                srv_puts("\n");
                if (gui2_window_resize(&window, event.width, event.height) != 0) {
                    srv_puts("calc: resize failed\n");
                }
                changed = 1;
            } else if (event.type == GUI2_EVENT_FOCUS) {
                changed = 1;
            } else if (event.type == GUI2_EVENT_CLOSE) {
                srv_puts("calc: close\n");
                closing = 1;
                break;
            }
            changed |= gui2_dispatch_event(&context, &event, controls, BUTTON_COUNT);
            for (uint64_t i = 0; i < BUTTON_COUNT; i++) {
                if (buttons[i].clicks != 0) {
                    buttons[i].clicks = 0;
                    press_value(&state, button_defs[i].value);
                    srv_puts("calc: press ");
                    srv_puts(button_defs[i].label);
                    srv_puts("\n");
                    changed = 1;
                }
            }
        }
        if (closing) {
            break;
        }
        if (changed) {
            draw_calc(&window, buttons, &state);
            gui2_window_present_dirty(&window);
        }
        srv_yield();
    }

    gui2_window_close(&window);
    srv_puts("calc: exited\n");
    return 0;
}
