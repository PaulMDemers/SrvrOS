#ifndef SRVROS_DESKTOP_PROTOCOL_H
#define SRVROS_DESKTOP_PROTOCOL_H
#include <stdint.h>

/* Fixed-size v1 packet carried in gui_message.text. The kernel supplies the
 * envelope source PID. Unknown versions/types are rejected, never guessed. */
#define DESK_PROTOCOL_VERSION 1
#define DESK_TRANSPORT_MESSAGE 300
#define DESK_SURFACE_STREAM 1
#define DESK_MAX_WIDTH 960
#define DESK_MAX_HEIGHT 640
enum desk_packet_type {
    DESK_OPEN = 1, DESK_CONFIGURE, DESK_COMMIT, DESK_RELEASE,
    DESK_CLOSE, DESK_TEXT, DESK_FOCUS, DESK_SCROLL
};
struct desk_packet {
    uint32_t version, size, type, reserved;
    uint64_t serial, generation, surface;
    uint32_t width, height;
    int32_t x, y;
    uint32_t value, reserved2;
    char text[96];
};
_Static_assert(sizeof(struct desk_packet) == 160, "desktop wire layout");
/* COMMIT publishes the entire surface and seals it against writes/destruction.
 * RELEASE acknowledges a private compositor snapshot, not scanout completion.
 * CONFIGURE generations prevent an old-sized frame replacing a newer resize.
 * SCROLL uses signed x/y viewport-pixel deltas (positive right/down); only the
 * server may send it, targeting the window under the pointer without focus change.
 * TEXT currently carries one decoded console byte; raw keys are a later ABI. */
#endif
