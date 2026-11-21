#ifndef SRVROS_INTEL_BDW_TRAFFIC_H
#define SRVROS_INTEL_BDW_TRAFFIC_H
#include <stdbool.h>
#include <stdint.h>

/* Closing MMIO checks for an already held GPM block with freshly witnessed
 * pipeline and L3 acknowledgements. The reader verifies the live owner around
 * each access. L3's flush ACK may have been consumed by GPM in the meantime. */
static inline bool intel_bdw_traffic_still_blocked(void *ctx, bool (*read)(void *, uint32_t, uint32_t *))
{
    static const struct { uint32_t offset, mask, expected; } checks[] = {
        /* Both video engines' reference and prefetch queues. */
        {0x12870, 0xff0000, 0}, {0x12878, 0xff0000, 0},
        {0x1c870, 0xff0000, 0}, {0x1c878, 0xff0000, 0},
        /* Flush acknowledged and GO clear on all five engines. */
        {0x8000, 0x31, 0x20}, {0x8004, 0x31, 0x20}, {0x8008, 0x31, 0x20},
        {0x800c, 0x31, 0x20}, {0x8010, 0x31, 0x20},
        /* GAM idle, no L3 unblock, no new requests or grants. */
        {0x8040, 1, 1}, {0x8038, 2, 0}, {0x40d0, 0x805f, 0}, {0x8028, 0x5f, 0}
    };
    if (!read) return false;
    for (unsigned i = 0; i < sizeof(checks) / sizeof(checks[0]); ++i) {
        uint32_t word = UINT32_MAX;
        if (!read(ctx, checks[i].offset, &word) || word == UINT32_MAX ||
            (word & checks[i].mask) != checks[i].expected) return false;
    }
    return true;
}
#endif
