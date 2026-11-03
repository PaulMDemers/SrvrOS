#ifndef SRVROS_MOUSE_FLAGS_H
#define SRVROS_MOUSE_FLAGS_H
/* Absolute events reuse dx/dy as normalized desktop coordinates. Map each
 * endpoint to [0, extent-1] in the consumer's coordinate space. The struct size
 * is unchanged; relative sources continue to report signed deltas. */
#define MOUSE_ABSOLUTE 0x40
#define MOUSE_ABSOLUTE_MAX 65535
#define MOUSE_CANCEL 0x80
#endif
