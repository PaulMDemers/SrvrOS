#ifndef SRVROS_DESK_CONNECTION_H
#define SRVROS_DESK_CONNECTION_H
#include <srvros/desk_app.h>
/* Local event only: never accepted as a wire packet. Window retirement starts
 * automatically; keep dispatching until CLOSE_ACK, preserving sibling state. */
#define DESK_WINDOW_FAILED 0x80000001u

/* One reader and one UI thread per process. No callbacks or hidden threads.
 * Caller owns each window until dispose succeeds; do not move/free registered
 * records. Do not combine connection_poll with desk_app_poll or desk_receive.
 * IDs never repeat in a connection. A fatal connection error requires process
 * shutdown; disposal cannot assume a still-live server released its surfaces. */
struct desk_connection {
    struct desk_app *windows,*cursor,*opening;
    uint64_t server,next_window;
    int failed,disconnected;
};
void desk_connection_init(struct desk_connection *connection);
/* 0 locally queued; -1 invalid/busy. OPEN_REPLY reports actual admission. */
int desk_window_open(struct desk_connection *connection,struct desk_app *window,
    const char *title,uint32_t width,uint32_t height);
/* 1 targeted app event, 2 internal/discarded progress, 0 idle, -1 fatal.
 * CONFIGURE is delivered only once its canvas exists. OPEN_REPLY contains
 * admission status; CLOSE_ACK means disposal is now eligible. WINDOW_FAILED
 * reports a local window runtime failure without aborting healthy siblings. */
int desk_connection_poll(struct desk_connection *connection,struct desk_app **window,struct desk_packet *event);
/* Stop new frames, retry retirement, continue polling until CLOSE_ACK. */
void desk_window_close(struct desk_app *window);
/* 0 retired/unlinked; -1 ownership still in flight. Does not block. */
int desk_window_dispose(struct desk_app *window);
/* Caller must also exclude its jobs/timers. All windows must be quiescent. */
int desk_connection_wait(struct desk_connection *connection,uint64_t ticks);
#endif
