#ifndef SRVROS_GUI_H
#define SRVROS_GUI_H

#include <stdint.h>

#define GUI_TEXT_MAX 192

enum gui_message_type {
    GUI_MSG_NONE = 0,
    GUI_MSG_CREATE_WINDOW = 1,
    GUI_MSG_ADD_BUTTON = 2,
    GUI_MSG_ADD_LABEL = 3,
    GUI_MSG_SET_TEXT = 4,
    GUI_MSG_ADD_TEXT_ENTRY = 5,
    GUI_MSG_ADD_IMAGE = 6,
    GUI_MSG_SET_IMAGE_PIXEL = 7,
    GUI_MSG_FILL_IMAGE = 8,
    GUI_MSG_EVENT_CLICK = 100,
    GUI_MSG_EVENT_CLOSE = 101,
    GUI_MSG_EVENT_CHANGE = 102,
    GUI_MSG_EVENT_IMAGE_CLICK = 103,
};

struct gui_message {
    uint64_t abi_version;
    uint64_t struct_size;
    uint64_t type;
    uint64_t source_pid;
    uint64_t target_pid;
    uint64_t window_id;
    uint64_t control_id;
    int64_t x;
    int64_t y;
    uint64_t width;
    uint64_t height;
    int64_t value;
    char text[GUI_TEXT_MAX];
};

int64_t gui_register_server(void);
int64_t gui_send(const struct gui_message *message);
int64_t gui_recv(struct gui_message *message);

#endif
