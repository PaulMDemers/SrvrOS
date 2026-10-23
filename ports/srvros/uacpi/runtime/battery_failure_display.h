#ifndef SRVROS_BATTERY_FAILURE_DISPLAY_H
#define SRVROS_BATTERY_FAILURE_DISPLAY_H
#include <stdbool.h>
#include <stdint.h>
/* Terminal CPU0 context only: no scheduling, storage or EC work. Each delay
 * has its own stopped-clock iteration bound; stop at the first failed slice. */
static inline unsigned battery_failure_display_wait(bool (*delay)(uint64_t)) {
    unsigned slices=0;
    while(slices<300 && delay(100000))slices++;
    return slices;
}
#endif
