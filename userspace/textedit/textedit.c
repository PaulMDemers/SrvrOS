#include <srvros/gui.h>
#include <srvros/sys.h>

#include <stdint.h>

#define WIN 1
#define DOC 100
#define ENTRY 101
#define ADD_LINE 200
#define SAVE 201
#define CLEAR 202

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

static void append_line(char *doc, const char *line) {
    uint64_t out = 0;
    while (doc[out] != '\0' && out + 1 < GUI_TEXT_MAX) {
        out++;
    }
    if (out != 0 && out + 1 < GUI_TEXT_MAX) {
        doc[out++] = '\n';
    }
    for (uint64_t i = 0; line[i] != '\0' && out + 1 < GUI_TEXT_MAX; i++) {
        doc[out++] = line[i];
    }
    doc[out] = '\0';
}

static void load_document(const char *path, char *doc) {
    int fd = (int)srv_open(path);
    uint64_t out = 0;
    if (fd < 0) {
        copy_text(doc, "TEXT EDITOR");
        return;
    }
    for (;;) {
        long count = srv_read(fd, doc + out, GUI_TEXT_MAX - out - 1);
        if (count <= 0) {
            break;
        }
        out += (uint64_t)count;
        if (out + 1 >= GUI_TEXT_MAX) {
            break;
        }
    }
    doc[out] = '\0';
    srv_close(fd);
    if (doc[0] == '\0') {
        copy_text(doc, "TEXT EDITOR");
    }
}

int main(int argc, char **argv) {
    char draft[GUI_TEXT_MAX] = "";
    char doc[GUI_TEXT_MAX];
    const char *path = argc > 1 ? argv[1] : "/fat/text.txt";

    srv_puts("textedit: start\n");
    load_document(path, doc);
    send_msg(GUI_MSG_CREATE_WINDOW, 0, 220, 110, 304, 230, 0, "TEXT EDIT");
    send_msg(GUI_MSG_ADD_LABEL, DOC, 18, 36, 268, 108, 0, doc);
    send_msg(GUI_MSG_ADD_TEXT_ENTRY, ENTRY, 18, 152, 268, 28, 0, "");
    send_msg(GUI_MSG_ADD_BUTTON, ADD_LINE, 18, 192, 72, 24, 0, "ADD");
    send_msg(GUI_MSG_ADD_BUTTON, SAVE, 98, 192, 72, 24, 0, "SAVE");
    send_msg(GUI_MSG_ADD_BUTTON, CLEAR, 178, 192, 72, 24, 0, "CLEAR");

    for (;;) {
        struct gui_message event;
        if (gui_recv(&event) <= 0) {
            srv_yield();
            continue;
        }
        if (event.type == GUI_MSG_EVENT_CLOSE) {
            srv_puts("textedit: close\n");
            return 0;
        }
        if (event.type == GUI_MSG_EVENT_CHANGE && event.control_id == ENTRY) {
            copy_text(draft, event.text);
            continue;
        }
        if (event.type != GUI_MSG_EVENT_CLICK) {
            continue;
        }
        if (event.control_id == ADD_LINE && draft[0] != '\0') {
            if (doc[0] == 'T' && doc[1] == 'E' && doc[2] == 'X') {
                doc[0] = '\0';
            }
            append_line(doc, draft);
            draft[0] = '\0';
            set_text(DOC, doc);
            set_text(ENTRY, "");
        } else if (event.control_id == SAVE) {
            if (srv_fs_write(path, doc, srv_strlen(doc)) >= 0) {
                set_text(DOC, "SAVED");
            } else {
                set_text(DOC, "SAVE FAILED");
            }
        } else if (event.control_id == CLEAR) {
            draft[0] = '\0';
            copy_text(doc, "TEXT EDITOR");
            set_text(DOC, doc);
            set_text(ENTRY, "");
        }
    }
}
