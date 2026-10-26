#ifndef SRVROS_DESK_RUNTIME_H
#define SRVROS_DESK_RUNTIME_H
#include <srvros/desk_connection.h>
/* Single UI-thread orchestration above the transport and pure widgets. No
 * allocation, hidden receiver or automatic resource cleanup. Registered timers
 * and the connection must remain at stable addresses. */
#define DESK_RUNTIME_BATCH 32u
#define DESK_RUNTIME_HEARTBEAT_US 100000u
struct desk_runtime;
struct desk_runtime_timer {
    struct desk_runtime *owner;
    struct desk_runtime_timer *next;
    uint64_t deadline_us;
    void (*fire)(void *context,uint64_t now_us);
    void *context;
};
struct desk_runtime {
    struct desk_connection *connection;
    struct desk_runtime_timer *timers;
    struct srv_gui_wait wait;
    int retry,dispatching,failed;
};
/* Event callback: 0 continue, 1 stop this batch, -1 app failure. Internal
 * transport progress counts toward the same quantum. Jobs/rendering run after
 * dispatch even when messages remain, before one application-wide idle. */
typedef int (*desk_runtime_event)(void *,struct desk_app *,struct desk_packet);
void desk_runtime_init(struct desk_runtime *,struct desk_connection *);
int desk_runtime_dispatch(struct desk_runtime *,desk_runtime_event,void *);
void desk_runtime_retry(struct desk_runtime *);
void desk_runtime_deadline(struct desk_runtime *,uint64_t absolute_us);
/* Nonblocking pipe/PTY interests only. Duplicate FDs merge masks; invalid/full
 * sets force short retry, never a sleep that silently omits accepted work. */
int desk_runtime_watch(struct desk_runtime *,int fd,unsigned events);
/* Busy/unfinished publication uses one-tick retry; otherwise one combined wait
 * honors the earliest app/timer deadline and the bounded service heartbeat. */
void desk_runtime_idle(struct desk_runtime *,int busy);
/* One-shot, zero-initialized/canceled timers. No periodic catch-up loop: a
 * callback may explicitly rearm itself, but only 32 callbacks run per turn.
 * Callback runs detached and may cancel/free another registered timer. */
int desk_runtime_timer_arm(struct desk_runtime *,struct desk_runtime_timer *,uint64_t,
                           void (*)(void *,uint64_t),void *);
void desk_runtime_timer_cancel(struct desk_runtime_timer *);
void desk_runtime_timers_clear(struct desk_runtime *);
struct desk_file_pool;struct desk_file_job;struct desk_image_job;
void desk_runtime_file_pool(struct desk_runtime *,const struct desk_file_pool *);
void desk_runtime_file_job(struct desk_runtime *,const struct desk_file_job *);
void desk_runtime_image_job(struct desk_runtime *,const struct desk_image_job *);
#endif
