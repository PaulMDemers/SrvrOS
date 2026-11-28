#ifndef SRVROS_INTEL_HSW_ENGINE_HOLD_H
#define SRVROS_INTEL_HSW_ENGINE_HOLD_H

#include <stdbool.h>
#include <stdint.h>

enum hsw_hold_state { HSW_ENGINE_HOLD_NEW, HSW_ENGINE_HOLD_REFUSED,
                      HSW_ENGINE_HOLD_RETAINED, HSW_ENGINE_HOLD_RETURNED };
enum hsw_hold_op { HSW_ENGINE_STOP, HSW_ENGINE_FLUSH, HSW_ENGINE_REOPEN };
enum hsw_hold_change { HSW_ENGINE_UNCHANGED, HSW_ENGINE_CHANGED, HSW_ENGINE_UNCERTAIN };

struct hsw_hold_sample {
    uint32_t mi_mode, instpm, sync_status;
};

/* A trusted blitter provider must own the exact device and boot epoch,
 * exclude competing commands and firmware, and pin Pipe A and the status
 * destination throughout. It must exclude pending prior sync completions.
 * Read reports sync status from that owned blitter destination. Keep
 * its ticket valid through reopen verification, including retained returns. */
struct hsw_hold_io {
    void *ctx;
    bool (*valid)(void *, uint64_t ticket);
    uint64_t (*now)(void *);
    bool (*read)(void *, uint64_t ticket, struct hsw_hold_sample *);
    enum hsw_hold_change (*change)(void *, uint64_t ticket, enum hsw_hold_op);
};

struct hsw_engine_hold {
    enum hsw_hold_state state;
    const char *reason;
    uint64_t begin, last, completed_at, ticket;
    unsigned polls, attempted;
};

/* Serialized on the boot CPU. The caller owns the zero-initialized result
 * and the ticket. For RETAINED, leave the provider and its pins reserved. */
bool intel_hsw_engine_hold(struct hsw_engine_hold *, const struct hsw_hold_io *, uint64_t ticket);

#endif
