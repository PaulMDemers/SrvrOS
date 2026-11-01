#ifndef SRVROS_DESK_APP_H
#define SRVROS_DESK_APP_H
#include <srvros/desktop.h>
#include <srvros/gui_wait.h>
#include <srvros/desktop_preferences.h>
#include <srvros/desktop_menu.h>
#include <srvros/desk_render.h>

/* Per-window canvas and transport state, independent of widgets/app data.
 * open/poll retain the one-window compatibility path. Multiwindow clients use
 * desk_connection.h for admission, one dispatcher and acknowledged disposal. */
struct desk_connection;
struct desk_app {
    struct desk_connection *connection;
    struct desk_app *next;
    struct desk_packet open_request;
    unsigned open_result;
    int opening,open_sent,closing,close_sent;
    uint64_t wait_epoch;
    uint64_t parent_window;
    uint64_t server,window,surface,serial,generation,pending,pending_generation,last,input_serial,send_since,rejected_generation,retry_after;
    struct desk_packet configure;
    struct desk_menu_item menus[DESK_APP_MENU_ITEMS];
    uint64_t menu_revision;
    unsigned menu_count,menu_next;
    int menu_pending;
    uint32_t *pixels,width,height;
    int dirty,focused,closed,failed;
};
int desk_app_open(struct desk_app *app,const char *title,uint32_t width,uint32_t height);
/* Return 1 app event, 0 empty, 2 handled/discarded transport event, -1 failure. CONFIGURE is emitted
 * only after the old sealed frame is released and the new canvas/surface exist.
 * Failed growth retains the old canvas and asks the server to restore geometry.
 * Caller can receive one internal event per call; service in bounded batches. */
int desk_app_poll(struct desk_app *app,struct desk_packet *event);
/* Only a fully published/clean client may wait: 1 message, 0 timeout, -1
 * ineligible/error. Caller must exclude its own jobs/timers and use a short fallback. */
int desk_app_wait(struct desk_app *app,uint64_t ticks);
/* Combined wait uses request.count/fds/deadline_us, fills ready/revents. Return
 * ready flags or negative error/ineligible. Caller still owns job/timer policy. */
int desk_app_wait_io(struct desk_app *app,struct srv_gui_wait *request);
/* 1 caller may draw into canvas; 0 waiting/clean, -1 failure. */
int desk_app_canvas(struct desk_app *app,struct desk_canvas *canvas);
int desk_app_present(struct desk_app *app);
/* Transfer an exact desk_pixels_alloc(width,height) mapping to a fresh window
 * before CONFIGURE. Success owns it through normal resize/disposal; refusal
 * leaves ownership with the caller. Matching first geometry avoids allocation.
 * It is not a surface or a published frame. */
int desk_app_adopt_pixels(struct desk_app *app,uint32_t *pixels,uint32_t width,uint32_t height);
int desk_app_title(struct desk_app *app,const char *title);
int desk_app_cancel_close(struct desk_app *app);
int desk_app_launch(struct desk_app *app,unsigned application,const char *path);
int desk_app_preferences(struct desk_app *app,const struct desk_preferences *preferences,unsigned mask);
int desk_app_text_size(struct desk_app *app,unsigned size);
/* Copy a menu snapshot. Identical snapshots do not restart publication.
 * Call menu_tick once per application turn (at most four nonblocking sends).
 * The runtime filters stale/disabled MENU_ACTION packets before delivery. */
int desk_app_menus(struct desk_app *app,const struct desk_menu_item *items,unsigned count);
void desk_app_menu_tick(struct desk_app *app);
void desk_app_close(struct desk_app *app);
#endif
