#ifndef SRVROS_WORKQUEUE_H
#define SRVROS_WORKQUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*workqueue_fn)(void *arg);

void workqueue_init(void);
bool workqueue_schedule(workqueue_fn fn, void *arg);
/* Only callbacks obeying scheduler_spawn_io_service's restrictions. */
bool workqueue_schedule_io_safe(workqueue_fn fn, void *arg);
void workqueue_print_status(void);

#endif
