#ifndef SRVROS_DESK_WINDOW_SET_H
#define SRVROS_DESK_WINDOW_SET_H
#include <stddef.h>
#include <srvros/desk_application.h>
#include <srvros/desk_runtime.h>

/* Allocating application layer, separate from pure widgets and the allocation-
 * free dispatcher. One stable model and main window per owner, with an optional
 * owned dialog. Applications retain job/timer/model-specific cleanup ownership. */
struct desk_window_set;
struct desk_window_owner {
    struct desk_window_owner *next;
    struct desk_window_set *set;
    struct desk_app *window;
    struct desk_dialog_window dialog;
    size_t mapping_bytes,model_bytes;
    uint64_t requester;
    int attached,ready,retired,disposed;
    _Alignas(16) unsigned char model[];
};
struct desk_window_set {
    struct desk_connection connection;
    struct desk_runtime runtime;
    struct desk_application_lifecycle lifecycle;
    struct desk_window_owner *windows;
    uint64_t admitted,retired,refused;
    int failed;
};
typedef int (*desk_window_event)(void *,struct desk_window_owner *,struct desk_app *,struct desk_packet);
void desk_window_set_init(struct desk_window_set *);
/* End-of-turn operations only. Models are zeroed and aligned to 16 bytes. No
 * fixed window count; existing memory and compositor admission can refuse. */
struct desk_window_owner *desk_window_owner_new(struct desk_window_set *,size_t model_bytes);
/* The main GUI record must be embedded in this owner's model. Failure retains
 * the unopened allocation for caller cleanup/release; admission is asynchronous. */
int desk_window_owner_open(struct desk_window_owner *,struct desk_app *,uint64_t requester,
                           const char *title,uint32_t width,uint32_t height);
/* Dispatch updates main-window admission/retirement and routes dialog events to
 * the same owner. A fatal transport/callback failure retains all allocations. */
int desk_window_set_dispatch(struct desk_window_set *,desk_window_event,void *);
struct desk_window_owner *desk_window_set_next_close(struct desk_window_set *);
/* Dispose only after CLOSE_ACK and dialog disposal. Release is a separate,
 * explicit assertion that accepted jobs, timers and borrowed model data drained.
 * Release refuses while dispatching, before GUI disposal, or after set failure.
 * Unopened owners can also be released. Neither operation kills a worker. */
int desk_window_owner_dispose(struct desk_window_owner *);
int desk_window_owner_release(struct desk_window_owner *,int work_drained);
#endif
