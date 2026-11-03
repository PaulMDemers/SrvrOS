#ifndef SRVROS_MOUSE_POSITION_H
#define SRVROS_MOUSE_POSITION_H
#include <stdint.h>
#include <srvros/mouse_flags.h>

/* Pass the consumer's coordinate extent (logical canvas size after UI scaling).
 * Absolute endpoints map exactly to 0 and extent-1; intermediate values floor.
 * CANCEL and empty extents preserve position, even when ABS is also present.
 * Consumers with a cursor-footprint limit may clamp the returned position again.
 * Relative deltas keep their existing units; UI scale does not divide them. */
static inline uint64_t mouse_position_axis(uint64_t current, int32_t value,
    uint8_t flags, uint64_t extent) {
    if ((flags & MOUSE_CANCEL) || !extent) return current;
    uint64_t maximum = extent - 1;
    if (flags & MOUSE_ABSOLUTE) {
        uint64_t normalized = value < 0 ? 0 : value > MOUSE_ABSOLUTE_MAX ?
            MOUSE_ABSOLUTE_MAX : (uint64_t)value;
        /* Quotient/remainder avoids overflowing even for UINT64_MAX extents. */
        return (maximum / MOUSE_ABSOLUTE_MAX) * normalized +
            ((maximum % MOUSE_ABSOLUTE_MAX) * normalized) / MOUSE_ABSOLUTE_MAX;
    }
    if (value < 0) {
        uint64_t amount = (uint64_t)(-(int64_t)value);
        uint64_t next = current < amount ? 0 : current - amount;
        return next > maximum ? maximum : next;
    }
    if (current >= maximum || (uint64_t)value > maximum - current) return maximum;
    return current + (uint64_t)value;
}
#endif
