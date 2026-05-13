#ifndef SRVROS_WORKQUEUE_H
#define SRVROS_WORKQUEUE_H

#include <stdbool.h>
#include <stdint.h>

typedef void (*workqueue_fn)(void *arg);

void workqueue_init(void);
bool workqueue_schedule(workqueue_fn fn, void *arg);
void workqueue_print_status(void);

#endif
