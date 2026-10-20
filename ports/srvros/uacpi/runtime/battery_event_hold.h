#ifndef SRVROS_BATTERY_EVENT_HOLD_H
#define SRVROS_BATTERY_EVENT_HOLD_H
#include <stdbool.h>
#include <stdint.h>
/* One explicitly identified foreign GPE byte. The owner must first validate
 * platform identity, a masked/free SCI route, and all other enable registers.
 * Never acknowledges foreign W1C status. No AML handler is installed for it. */
struct battery_event_hold {
    uint16_t status,enable;
    uint8_t mask,original;
    bool active,poisoned;
    uint64_t protected_writes;
};
struct battery_event_ops {
    void *ctx;
    uint8_t (*read)(void *,uint16_t);
    void (*write)(void *,uint16_t,uint8_t);
};
bool battery_event_hold_begin(struct battery_event_hold *,struct battery_event_ops);
bool battery_event_hold_restore(struct battery_event_hold *,struct battery_event_ops);
/* Applied only after ordinary access/width checks. Hides the retained event
 * from the interpreter and removes only its bit from W1C writes. */
uint8_t battery_event_hold_filter(struct battery_event_hold *,uint16_t,bool write,uint8_t);
#endif
