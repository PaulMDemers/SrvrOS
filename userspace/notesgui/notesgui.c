#include <srvros/gui.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define TEXT 100
#define ENTRY 101
#define ADD 200
#define CLEAR 201

static void copy_text(char *to, const char *from) {
    uint64_t i = 0;
    while (from[i] != '\0' && i + 1 < GUI_TEXT_MAX) {
        to[i] = from[i];
        i++;
    }
    to[i] = '\0';
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

static void set_text(uint64_t control, const char *text) {
    send_msg(GUI_MSG_SET_TEXT, control, 0, 0, 0, 0, 0, text);
}

static void append_line(char *list, const char *line) {
    uint64_t out = 0;
    while (list[out] != '\0' && out + 1 < GUI_TEXT_MAX) {
        out++;
    }
    if (out == 0) {
        copy_text(list, line);
        return;
    }
    if (out + 1 < GUI_TEXT_MAX) {
        list[out++] = '\n';
    }
    for (uint64_t i = 0; line[i] != '\0' && out + 1 < GUI_TEXT_MAX; i++) {
        list[out++] = line[i];
    }
    list[out] = '\0';
}

int main(void) {
    char draft[GUI_TEXT_MAX] = "";
    char list[GUI_TEXT_MAX] = "TYPE A TODO";

    srv_puts("notesgui: start\n");
    send_msg(GUI_MSG_CREATE_WINDOW, 0, 430, 92, 292, 218, 0, "TODO");
    send_msg(GUI_MSG_ADD_LABEL, TEXT, 20, 38, 252, 92, 0, list);
    send_msg(GUI_MSG_ADD_TEXT_ENTRY, ENTRY, 20, 140, 252, 28, 0, "");
    send_msg(GUI_MSG_ADD_BUTTON, ADD, 20, 180, 72, 24, 0, "ADD");
    send_msg(GUI_MSG_ADD_BUTTON, CLEAR, 100, 180, 72, 24, 0, "CLEAR");

    for (;;) {
        struct gui_message event;
        if (gui_recv(&event) <= 0) {
            srv_yield();
            continue;
        }
        if (event.type == GUI_MSG_EVENT_CLOSE) {
            srv_puts("notesgui: close\n");
            return 0;
        }
        if (event.type == GUI_MSG_EVENT_CHANGE && event.control_id == ENTRY) {
            copy_text(draft, event.text);
            continue;
        }
        if (event.type != GUI_MSG_EVENT_CLICK) {
            continue;
        }
        if (event.control_id == ADD && draft[0] != '\0') {
            if (list[0] == 'T' && list[1] == 'Y') {
                list[0] = '\0';
            }
            append_line(list, draft);
            draft[0] = '\0';
            set_text(TEXT, list);
            set_text(ENTRY, "");
        } else if (event.control_id == CLEAR) {
            draft[0] = '\0';
            copy_text(list, "TYPE A TODO");
            set_text(TEXT, list);
            set_text(ENTRY, "");
        }
    }
}
