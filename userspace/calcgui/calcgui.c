#include <srvros/gui.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define DISPLAY 100
#define BTN_BASE 200

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
        to[i] = from[i];
        i++;
    }
    to[i] = '\0';
}

static void append_i64(char *buffer, uint64_t *index, int64_t value) {
    char digits[21];
    uint64_t count = 0;
    uint64_t magnitude;
    if (value < 0) {
        buffer[(*index)++] = '-';
        magnitude = (uint64_t)(-value);
    } else {
        magnitude = (uint64_t)value;
    }
    if (magnitude == 0) {
        buffer[(*index)++] = '0';
        buffer[*index] = '\0';
        return;
    }
    while (magnitude > 0 && count < sizeof(digits)) {
        digits[count++] = (char)('0' + (magnitude % 10));
        magnitude /= 10;
    }
    while (count > 0) {
        buffer[(*index)++] = digits[--count];
    }
    buffer[*index] = '\0';
}

static void send_msg(uint64_t type, uint64_t control, int64_t x, int64_t y,
    uint64_t width, uint64_t height, int64_t value, const char *text) {
    struct gui_message msg = {
        .type = type,
        .window_id = WIN,
        .control_id = control,
        .x = x,
        .y = y,
        .width = width,
        .height = height,
        .value = value,
    };
    copy_text(msg.text, text);
    gui_send(&msg);
}

static void set_display(int64_t current, int64_t result, int pending_op) {
    char line[GUI_TEXT_MAX];
    uint64_t index = 0;
    if (pending_op) {
        append_i64(line, &index, result);
        line[index++] = (char)pending_op;
        line[index] = '\0';
        append_i64(line, &index, current);
    } else {
        append_i64(line, &index, current);
    }
    send_msg(GUI_MSG_SET_TEXT, DISPLAY, 0, 0, 0, 0, 0, line);
}

static int64_t apply_op(int64_t left, int64_t right, int op) {
    if (op == '+') {
        return left + right;
    }
    if (op == '-') {
        return left - right;
    }
    if (op == '*') {
        return left * right;
    }
    if (op == '/' && right != 0) {
        return left / right;
    }
    return right;
}

int main(void) {
    static const char *labels[20] = {
        "7", "8", "9", "/", "C",
        "4", "5", "6", "*", "CE",
        "1", "2", "3", "-", "BS",
        "0", ".", "=", "+", "+/-",
    };
    static const int values[20] = {
        7, 8, 9, '/', 'C',
        4, 5, 6, '*', 'E',
        1, 2, 3, '-', 'B',
        0, '.', '=', '+', 'N',
    };
    int64_t current = 0;
    int64_t result = 0;
    int pending_op = 0;

    srv_puts("calcgui: start\n");
    send_msg(GUI_MSG_CREATE_WINDOW, 0, 168, 72, 284, 224, 0, "CALCULATOR");
    send_msg(GUI_MSG_ADD_LABEL, DISPLAY, 20, 38, 244, 32, 0, "0");
    for (uint64_t i = 0; i < 20; i++) {
        uint64_t row = i / 5;
        uint64_t col = i % 5;
        send_msg(GUI_MSG_ADD_BUTTON, BTN_BASE + i, 20 + (int64_t)(col * 50),
            84 + (int64_t)(row * 30), 44, 24, values[i], labels[i]);
    }
    set_display(current, result, pending_op);

    for (;;) {
        struct gui_message event;
        if (gui_recv(&event) <= 0) {
            srv_yield();
            continue;
        }
        if (event.type == GUI_MSG_EVENT_CLOSE) {
            srv_puts("calcgui: close\n");
            return 0;
        }
        if (event.type != GUI_MSG_EVENT_CLICK ||
            event.control_id < BTN_BASE ||
            event.control_id >= BTN_BASE + 20) {
            continue;
        }

        int value = values[event.control_id - BTN_BASE];
        if (value >= 0 && value <= 9) {
            current = current * 10 + value;
        } else if (value == '+' || value == '-' || value == '*' || value == '/') {
            result = pending_op ? apply_op(result, current, pending_op) : current;
            current = 0;
            pending_op = value;
        } else if (value == '=') {
            if (pending_op) {
                current = apply_op(result, current, pending_op);
                result = 0;
                pending_op = 0;
            }
        } else if (value == 'C') {
            current = 0;
            result = 0;
            pending_op = 0;
        } else if (value == 'E') {
            current = 0;
        } else if (value == 'B') {
            current /= 10;
        } else if (value == 'N') {
            current = -current;
        }
        set_display(current, result, pending_op);
    }
}
