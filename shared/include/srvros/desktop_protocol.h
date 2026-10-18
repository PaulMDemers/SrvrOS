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
    DESK_CLOSE, DESK_TEXT, DESK_FOCUS, DESK_SCROLL, DESK_POINTER
};
#define DESK_POINTER_LEFT 1u
#define DESK_POINTER_PRESS (1u << 8)
#define DESK_POINTER_CANCEL (1u << 9)
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
 * POINTER uses signed content-local x/y (possibly outside the window), the
 * configured generation, and value bits above. PRESS preserves the initial
 * grab position; held motion may coalesce, release/cancel may not. A content
 * press captures through release; focus/resize/device cancellation ends capture.
 * TEXT currently carries one decoded console byte; raw keys are a later ABI. */
#endif
