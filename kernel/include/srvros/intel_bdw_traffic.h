#ifndef SRVROS_INTEL_BDW_TRAFFIC_H
#define SRVROS_INTEL_BDW_TRAFFIC_H
#include <stdbool.h>
#include <stdint.h>

#define BDW_GAM_CLIENTS 0x5fu
#define BDW_TRAFFIC_BLOCK_US 10000u

/* One original request. The caller guards each read and rejects all-ones
 * register returns. Never carry these edge observations across requests. */
struct bdw_traffic_sample {
    void *ctx;
    bool (*read)(void *,uint32_t,uint32_t *);
    uint32_t request_closed, response_closed, engine_ack, engine_go_closed, engine_blocked;
    bool l3_ack;
};

static inline bool bdw_traffic_gam(struct bdw_traffic_sample *a, uint32_t allowed, bool *matches)
{
    uint32_t request, response;
    if (!a->read(a->ctx, 0x40d0, &request) || !a->read(a->ctx, 0x8028, &response))
        return false;
    *matches = (request & BDW_GAM_CLIENTS) == allowed && (response & BDW_GAM_CLIENTS) == allowed;
    if (!allowed) {
        /* Each sampled client can close asynchronously. Reopening one
         * invalidates the accumulated evidence, even while others are open. */
        /* Bit 15 requests preparation for a PCI FLR, which this owner has
         * not issued. It cannot belong to the pre-reset traffic block. */
        if ((request & (a->request_closed | 0x8000u)) || (response & a->response_closed))
            return false;
        a->request_closed |= ~request & BDW_GAM_CLIENTS;
        a->response_closed |= ~response & BDW_GAM_CLIENTS;
    }
    /* An existing reset reason is foreign to this new transaction. */
    if (allowed && (request & 0xff00u))
        *matches = false;
    return true;
}

static inline bool bdw_traffic_engines(struct bdw_traffic_sample *a, bool blocked, bool *matches)
{
    static const uint32_t offsets[] = {0x8000, 0x8004, 0x8008, 0x800c, 0x8010, 0x8014};
    /* With Go=0, the engine stays busy (bit 0 clear). Before the request
     * its pipeline may be either idle or busy. */
    /* WiDi has its own GO acknowledgement and busy indication. Bit 5
     * is reserved there; only the five command streamers acknowledge flush. */
    *matches = true;
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(offsets[0]); ++i) {
        uint32_t word;
        if (!a->read(a->ctx, offsets[i], &word))
            return false;
        uint32_t flush = offsets[i] == 0x8014 ? 0u : 0x20u;
        /* WiDi must become busy before acknowledging GO=0, and can only
         * report idle again after GO=1. Later polls cannot repair a violation. */
        if (offsets[i] == 0x8014 && (word & 0x11u) == 1u) return false;
        uint32_t mask = 0x10u | flush | (blocked ? 1u : 0u);
        bool ready = (word & mask) == (blocked ? flush : 0x10u);
        if (blocked) {
            uint32_t engine = 1u << i;
            if (((a->engine_ack & engine) && !(word & 0x20u)) ||
                ((a->engine_go_closed & engine) && (word & 0x10u)) ||
                ((a->engine_blocked & engine) && !ready))
                return false;
            if (word & flush) a->engine_ack |= engine;
            if (!(word & 0x10u)) a->engine_go_closed |= engine;
            if (ready) a->engine_blocked |= engine;
        }
        if (!ready) *matches = false;
    }
    return true;
}

static inline bool intel_bdw_traffic_open(struct bdw_traffic_sample *s)
{
    bool arbiter, pipelines;
    uint32_t l3;
    return bdw_traffic_gam(s,BDW_GAM_CLIENTS,&arbiter) && arbiter &&
           bdw_traffic_engines(s,false,&pipelines) && pipelines &&
           s->read(s->ctx,0x8038,&l3) && !(l3 & 3u);
}

static inline bool intel_bdw_traffic_poll(struct bdw_traffic_sample *s,bool *drained)
{
    bool arbiter, pipelines;
    uint32_t l3, eco;
    if (!s->read(s->ctx,0x8038,&l3) || !bdw_traffic_gam(s,0,&arbiter) ||
        !bdw_traffic_engines(s,true,&pipelines) || !s->read(s->ctx,0x8040,&eco) ||
        (l3 & 2u)) return false;
    /* GPM can consume L3's one-shot acknowledgement before the last engine closes. */
    s->l3_ack |= (l3 & 1u) != 0;
    *drained = arbiter && pipelines && s->l3_ack && (eco & 1u);
    return true;
}

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
        {0x8014, 0x11, 0}, /* WiDi GO acknowledged closed, pipe reports busy. */
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
