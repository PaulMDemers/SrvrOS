#ifndef SRVROS_APPLE_TRACKPAD_H
#define SRVROS_APPLE_TRACKPAD_H

#include <stddef.h>
#include <stdint.h>

#define APPLE_TRACKPAD_CONTACTS 16
#define APPLE_TRACKPAD_REPORT_MAX (38 + 28 * APPLE_TRACKPAD_CONTACTS)

/* Raw contacts, deliberately separate from pointer acceleration/gesture policy.
 * Coordinates keep the device's signed native units and orientation. */
struct apple_trackpad_contact {
    int16_t x, y;
    uint16_t origin;
    int16_t touch_major, touch_minor, tool_major, tool_minor;
};
struct apple_trackpad_frame {
    uint8_t button, count;
    struct apple_trackpad_contact contacts[APPLE_TRACKPAD_CONTACTS];
};

/* Wellspring 8 / Type 3 only: e.g. 05ac:0291 interface 2.
 * Returns 1 for a frame, 0 for a two-byte control response, -1 for malformed
 * input. On 0/-1 the caller's previous frame is left intact. */
int apple_trackpad_decode(const uint8_t *report, size_t length,
    struct apple_trackpad_frame *frame);
#endif
