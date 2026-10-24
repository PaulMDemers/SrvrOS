#ifndef SRVROS_DESKTOP_PROTOCOL_H
#define SRVROS_DESKTOP_PROTOCOL_H
#include <stdint.h>

/* Fixed-size v12 packet carried in gui_message.text. The kernel supplies the
 * envelope source PID. Unknown versions/types are rejected, never guessed. */
#define DESK_PROTOCOL_VERSION 12
#define DESK_TRANSPORT_MESSAGE 300
#define DESK_SURFACE_STREAM 1
/* Bounded surfaces cover the initial 1440x900 laptop target. */
#define DESK_MAX_WIDTH 1440
#define DESK_MAX_HEIGHT 900
#define DESK_CLIPBOARD_BYTES 65536
#define DESK_MAX_WINDOWS 16
enum desk_packet_type {
    DESK_OPEN = 1, DESK_CONFIGURE, DESK_COMMIT, DESK_RELEASE,
    DESK_CLOSE, DESK_TEXT, DESK_FOCUS, DESK_SCROLL, DESK_POINTER,
    DESK_KEY, DESK_CLOSE_REQUEST, DESK_CANCEL_CLOSE, DESK_TITLE, DESK_LAUNCH,
    DESK_PREFERENCES_SET, DESK_PREFERENCES,
    DESK_CLIPBOARD_SET, DESK_CLIPBOARD_GET, DESK_CLIPBOARD_ACK, DESK_CLIPBOARD_DATA, DESK_MENU_SET, DESK_MENU_ACTION, DESK_NOTIFY, DESK_NOTIFY_ACK, DESK_CONTEXT, DESK_BRIGHTNESS, DESK_AUDIO, DESK_RESIZE_REFUSED
};
enum desk_application { DESK_APP_FILES=1, DESK_APP_TEXT, DESK_APP_CALCULATOR,
    DESK_APP_SYSTEM, DESK_APP_SETTINGS, DESK_APP_TERMINAL, DESK_APP_PREVIEW, DESK_APP_HELP, DESK_APP_CLOCK, DESK_APP_SKETCH, DESK_APP_NETWORK, DESK_APP_FIND, DESK_APP_COUNT=12 };
#define DESK_POINTER_LEFT 1u
#define DESK_POINTER_PRESS (1u << 8)
#define DESK_POINTER_CANCEL (1u << 9)
#define DESK_POINTER_HOVER (1u << 10)
struct desk_packet {
    uint32_t version, size, type, reserved;
    uint64_t serial, generation, surface;
    uint32_t width, height;
    int32_t x, y;
    uint32_t value, reserved2;
    uint64_t input_serial;
    char text[88];
};
_Static_assert(sizeof(struct desk_packet) == 160, "desktop wire layout");
/* COMMIT publishes the entire surface and seals it against writes/destruction.
 * RELEASE acknowledges a private compositor snapshot, not scanout completion.
 * RESIZE_REFUSED carries the failed CONFIGURE generation and retained canvas
 * width/height. The server may restore the last published geometry with a fresh
 * generation; stale refusals cannot undo a later resize.
 * CONFIGURE generations prevent an old-sized frame replacing a newer resize.
 * SCROLL uses signed x/y viewport-pixel deltas (positive right/down); only the
 * server may send it, targeting the window under the pointer without focus change.
 * POINTER uses signed content-local x/y (possibly outside the window), the
 * configured generation, and value bits above. PRESS preserves the initial
 * grab position; held motion may coalesce, release/cancel may not. A content
 * press captures through release; focus/resize/device cancellation ends capture.
 * CONTEXT is server-originated: generation guards the current surface, x/y
 * are content coordinates, value=0 for secondary/Control-click or 1 for a
 * keyboard request (the client chooses its focused control anchor).
 * POINTER_HOVER is unpressed motion; it may coalesce, never with a release.
 * TEXT carries one decoded byte for legacy study clients.
 * Its optional input_serial is echoed cumulatively in COMMIT after processing
 * that input. It acknowledges application state, not physical display scanout.
 * KEY stores a srv_key_event in text, FOCUS.value is 0/1. CLOSE_REQUEST asks
 * an app to close; it may show an unsaved dialog and send CANCEL_CLOSE to veto.
 * CLOSE remains final client retirement or server-forced shutdown. TITLE is a
 * client-originated NUL-terminated title in text. LAUNCH is a whitelisted app
 * request: value=application, serial=path offset, height=chunk bytes, width=final.
 * Up to 160 bytes including the final NUL, assembled per client. Offset zero
 * starts/replaces a request; nothing launches until a valid final chunk.
 * BRIGHTNESS: client serial identifies a request; value=0 query or 5..100 set.
 * Only focused Settings may set. Server echoes serial and returns value=0..100
 * or UINT32_MAX unavailable/error. Replies are retried; requests do not persist.
 * AUDIO: serial identifies request, value=SRV_AUDIO_* operation, width=argument.
 * Only focused Settings may mutate audio; any registered client may query.
 * Reply echoes serial, value=0 accepted or 1 rejected, text=srv_audio_status.
 * Acceptance queues work; query state/pending to observe completion. No PCM or
 * codec commands are accepted from an application through this protocol.
 * PREFERENCES_SET: value=DESK_PREF_* field mask, text=desk_preferences.
 * Only focused Settings may update. PREFERENCES broadcasts the complete value
 * in text, value=persistence status, serial=revision, including new windows.
 * CLIPBOARD_SET/DATA: serial=request, generation=byte offset, width=total,
 * height=chunk bytes, value=final (DATA may use 2 for cancellation). SET is
 * committed only after a complete transfer; interrupted copies retain the old
 * clipboard. GET carries serial=request. ACK value=0 success, 1 failure.
 * Only a focused window may begin copy/paste. No data is exposed on partial
 * paste, and a clipboard revision change cancels an in-flight paste.
 * MENU_SET publishes one desk_menu_item per packet: serial=revision,
 * generation=item index, width=total items (0..32), value=final (0/1).
 * Index zero starts a replacement; the old snapshot remains until the entire
 * validated new snapshot arrives. A zero-item final packet clears it.
 * MENU_ACTION: serial=snapshot revision, value=action ID. Server-originated,
 * focused app only. Clients reject stale revisions or disabled/unknown IDs.
 * NOTIFY: serial=nonzero per-client increasing request, value=tone (0..2),
 * text=desk_notification (32-byte title,56-byte message; NUL-terminated UTF-8).
 * Other payload fields are zero. Only registered session clients may send;
 * server supplies app identity. Duplicate requests do not create new records.
 * NOTIFY_ACK: serial=request, value=0 accepted / 1 rejected. Acceptance does not
 * imply the user has read it. History is bounded and session-local. */
#endif
