#ifndef SRVROS_USER_GUI_H
#define SRVROS_USER_GUI_H

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
    GUI_MSG_V2_CREATE_SURFACE_WINDOW = 20,
    GUI_MSG_V2_DAMAGE_SURFACE = 21,
    GUI_MSG_V2_DESTROY_SURFACE = 22,
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

int gui_register_server(void);
int gui_send(const struct gui_message *message);
int gui_recv(struct gui_message *message);
int gui_surface_create(uint64_t width, uint64_t height, uint64_t flags, uint64_t *surface_id_out);
int gui_surface_destroy(uint64_t surface_id);
int gui_surface_blit(uint64_t surface_id, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height, const uint32_t *pixels, uint64_t stride);
int gui_surface_copy(uint64_t surface_id, uint64_t x, uint64_t y,
    uint64_t width, uint64_t height, uint32_t *pixels, uint64_t stride);
long spawn_bg(const char *path);

#endif
