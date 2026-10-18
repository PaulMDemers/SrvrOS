#ifndef SRVROS_KEY_EVENT_H
#define SRVROS_KEY_EVENT_H
#include <stdint.h>

#define SRV_KEY_EVENT_VERSION 1
/* USB HID keyboard usage IDs, shared by USB and translated PS/2 input. */
enum srv_key_usage {
    SRV_KEY_ENTER=0x28, SRV_KEY_ESCAPE, SRV_KEY_BACKSPACE, SRV_KEY_TAB, SRV_KEY_SPACE,
    SRV_KEY_F1=0x3a, SRV_KEY_F2, SRV_KEY_F3, SRV_KEY_F4, SRV_KEY_F5, SRV_KEY_F6,
    SRV_KEY_F7, SRV_KEY_F8, SRV_KEY_F9, SRV_KEY_F10, SRV_KEY_F11, SRV_KEY_F12,
    SRV_KEY_INSERT=0x49, SRV_KEY_HOME, SRV_KEY_PAGEUP, SRV_KEY_DELETE,
    SRV_KEY_END, SRV_KEY_PAGEDOWN, SRV_KEY_RIGHT, SRV_KEY_LEFT, SRV_KEY_DOWN, SRV_KEY_UP
};
#define SRV_KEY_CTRL 0x11u
#define SRV_KEY_SHIFT 0x22u
#define SRV_KEY_ALT 0x44u
#define SRV_KEY_META 0x88u
#define SRV_KEY_CAPS 0x100u
#define SRV_KEY_DOWN_EVENT 1u
#define SRV_KEY_REPEAT 2u
/* Queue overflow/discontinuity: discard held-key and composition state. */
#define SRV_KEY_CANCEL 4u
struct srv_key_event {
    uint64_t abi_version, struct_size, time_us;
    uint32_t usage, codepoint, modifiers, flags;
};
/* codepoint is a printable Unicode scalar (currently US ASCII layout) or zero.
 * Ctrl/Alt/Meta bindings use usage+modifiers, not a synthesized console byte.
 * Modifier changes have usages 0xe0..0xe7. Releases have codepoint zero.
 * A stream is opt-in and exclusive to the registered GUI server. */
#endif
