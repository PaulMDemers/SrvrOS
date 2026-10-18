#ifndef SRVROS_DESK_TIME_MODEL_H
#define SRVROS_DESK_TIME_MODEL_H
#include <stdint.h>
#include <stddef.h>
#define DESK_COUNTDOWN_MAX_US UINT64_C(86400000000)
#define DESK_STOPWATCH_MAX_US UINT64_C(360000000000)
/* Caller supplies monotonic microseconds. Clock rollback contributes no time.
 * These models never read a clock, schedule work, draw or emit notifications. */
struct desk_countdown { uint64_t duration,remaining,anchor;int running,finished; };
struct desk_stopwatch { uint64_t elapsed,anchor;int running; };
/* Strict decimal seconds, m:ss or h:mm:ss; range 1 second through 24 hours. */
int desk_duration_parse(const char *text,uint64_t *duration_us);
void desk_duration_format(char *text,size_t capacity,uint64_t duration_us,int centiseconds);
int desk_countdown_set(struct desk_countdown *timer,uint64_t duration_us);
uint64_t desk_countdown_value(const struct desk_countdown *timer,uint64_t now_us);
int desk_countdown_start(struct desk_countdown *timer,uint64_t now_us);
/* update: 1 exactly once when the active countdown finishes; otherwise 0. */
int desk_countdown_update(struct desk_countdown *timer,uint64_t now_us);
/* pause: 0 unchanged, 1 paused, 2 completed at this instant. */
int desk_countdown_pause(struct desk_countdown *timer,uint64_t now_us);
void desk_countdown_reset(struct desk_countdown *timer);
uint64_t desk_stopwatch_value(const struct desk_stopwatch *watch,uint64_t now_us);
int desk_stopwatch_toggle(struct desk_stopwatch *watch,uint64_t now_us);
void desk_stopwatch_reset(struct desk_stopwatch *watch);
/* Stops at the 100-hour bound and returns 1 on that transition. */
int desk_stopwatch_update(struct desk_stopwatch *watch,uint64_t now_us);
#endif
