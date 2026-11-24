#ifndef SRVROS_INTEL_BDW_TRAFFIC_H
#define SRVROS_INTEL_BDW_TRAFFIC_H
#include <stdbool.h>
#include <stdint.h>

#define BDW_GAM_CLIENTS 0x5fu
#define BDW_RSTFCTLMSG 0x8108u
#define BDW_PREP_RST_WIDI_ACK (1u << 10)
#define BDW_TRAFFIC_BLOCK_US 10000u

/* One original request. The caller guards each read and rejects all-ones
 * register returns. Never carry these edge observations across requests. */
struct bdw_traffic_sample {
    void *ctx;
    bool (*read)(void *,uint32_t,uint32_t *);
    uint32_t request_closed, response_closed, engine_ack, engine_go_closed, engine_blocked;
    bool l3_high, l3_ack;
};

static inline bool bdw_traffic_gam(struct bdw_traffic_sample *a, uint32_t allowed, bool *matches)
{
    /* RSTFCTLMSG is independent of WiDi GO. An acknowledged WiDi
     * reset can change the reader underneath an otherwise stable GAM sample. */
    uint32_t request, response, reset_flow;
    if (!a->read(a->ctx, 0x40d0, &request) || !a->read(a->ctx, 0x8028, &response) ||
        !a->read(a->ctx, BDW_RSTFCTLMSG, &reset_flow) || (reset_flow & BDW_PREP_RST_WIDI_ACK))
        return false;
    *matches = (request & BDW_GAM_CLIENTS) == allowed && (response & BDW_GAM_CLIENTS) == allowed;
    if (!allowed) {
        /* Each sampled client can close asynchronously. Reopening one
         * invalidates the accumulated evidence, even while others are open. */
        /* Bit 15 requests preparation for a PCI FLR, which this owner has
         * not issued. It cannot belong to the pre-reset traffic block. */
        /* A reply sampled closed while its request was still sampled open
         * cannot establish a fresh acknowledgement for that client. */
        if ((request & ~response & BDW_GAM_CLIENTS) ||
            (request & (a->request_closed | 0x8000u)) || (response & a->response_closed))
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
    /* The message and GAM orders differ for BCS/VECS. WiDi uses GAM bit 6. */
    static const struct { uint32_t offset, request_bit; } clients[] = {
        {0x8000, 0}, {0x8004, 1}, {0x8008, 2}, {0x800c, 4}, {0x8010, 3}, {0x8014, 6}
    };
    /* With Go=0, the engine stays busy (bit 0 clear). Before the request
     * its pipeline may be either idle or busy. */
    /* WiDi has its own GO acknowledgement and busy indication. Bit 5
     * is reserved there; only the five command streamers acknowledge flush. */
    *matches = true;
    for (unsigned i = 0; i < sizeof(clients) / sizeof(clients[0]); ++i) {
        uint32_t word;
        if (!a->read(a->ctx, clients[i].offset, &word))
            return false;
        uint32_t flush = clients[i].offset == 0x8014 ? 0u : 0x20u;
        /* Every client must be busy before it acknowledges GO=0. An
         * out-of-order acknowledgement invalidates the current request. */
        if ((word & 0x11u) == 1u) return false;
        uint32_t mask = 0x10u | flush | (blocked ? 1u : 0u);
        bool ready = (word & mask) == (blocked ? flush : 0x10u);
        if (blocked) {
            uint32_t engine = 1u << i;
            if (!(a->request_closed & (1u << clients[i].request_bit)) &&
                ((word & flush) || !(word & 0x10u))) return false;
            if (((a->engine_ack & engine) && !(word & 0x20u)) ||
                ((a->engine_go_closed & engine) && (word & 0x10u)) ||
                ((a->engine_blocked & engine) && !ready))
                return false;
            /* A high flush bit sampled while that client's GO remains open
             * or its arbiter reply is outstanding cannot retire its reader. */
            if (word & flush) {
                if ((word & 0x10u) || !(a->response_closed & (1u << clients[i].request_bit)))
                    return false;
                a->engine_ack |= engine;
            }
            if (!(word & 0x10u)) a->engine_go_closed |= engine;
            if (ready) a->engine_blocked |= engine;
        }
        if (!ready) *matches = false;
    }
    return true;
}

static inline bool bdw_traffic_slices_powered(struct bdw_traffic_sample *s)
{
    uint32_t status;
    return s->read(s->ctx, 0x138064, &status) && (status & 7u) == 3u;
}

static inline bool bdw_traffic_steer(struct bdw_traffic_sample *s, bool (*select)(void *,uint32_t,uint32_t), uint32_t from, uint32_t to)
{
    uint32_t observed;
    if (!s->read(s->ctx, 0xfdc, &observed) || observed != from) return false;
    if (from == to) return true;
    return select(s->ctx, from, to) &&
           s->read(s->ctx, 0xfdc, &observed) && observed == to;
}

static inline bool intel_bdw_traffic_l3_policy(struct bdw_traffic_sample *s, bool (*select)(void *,uint32_t,uint32_t))
{
    uint32_t fuse, original;
    if (!s->read(s->ctx, 0x9120, &fuse) || ((fuse >> 25) & 7u) != 3u ||
        !bdw_traffic_slices_powered(s) || !s->read(s->ctx, 0xfdc, &original)) return false;

    uint32_t current = original;
    bool complete = true;
    for (unsigned slice = 0; complete && slice < 2; ++slice) {
        for (unsigned bank = 0; bank < 4; ++bank) {
            uint32_t target = (original & ~(15u << 24)) | (slice << 26) | (bank << 24);
            if (!bdw_traffic_steer(s,select, current, target)) return false;
            current = target;
            uint32_t policy, selector;
            /* A powered-down bank can read as zero, which by itself would
             * look like permission to flush both coherent and noncoherent lines. */
            if (!bdw_traffic_slices_powered(s) || !s->read(s->ctx, 0xb11c, &policy) ||
                !s->read(s->ctx, 0xfdc, &selector) || selector != target ||
                !bdw_traffic_slices_powered(s) || (policy & (1u << 4))) {
                complete = false;
                break;
            }
        }
    }
    /* Refuse a foreign selector or expired owner even on the cleanup path. */
    return bdw_traffic_steer(s,select, current, original) && complete;
}

/* A release may be in progress. Distinguish a valid partial sample from
 * unreadable registers, and require both L3 messages to settle. */
static inline bool bdw_traffic_reopened(struct bdw_traffic_sample *s, bool *open)
{
    bool arbiter, pipelines, closing;
    uint32_t l3;
    if (!bdw_traffic_gam(s,BDW_GAM_CLIENTS,&arbiter) ||
        !bdw_traffic_engines(s,false,&pipelines) || !s->read(s->ctx,0x8038,&l3) ||
        !bdw_traffic_gam(s,BDW_GAM_CLIENTS,&closing))
        return false;
    *open = arbiter && pipelines && !(l3 & 3u) && closing;
    return true;
}

static inline bool intel_bdw_traffic_open(struct bdw_traffic_sample *s)
{
    bool arbiter, pipelines;
    uint32_t l3;
    return bdw_traffic_gam(s,BDW_GAM_CLIENTS,&arbiter) && arbiter &&
           bdw_traffic_engines(s,false,&pipelines) && pipelines &&
           s->read(s->ctx,0x8038,&l3) && !(l3 & 3u) &&
           bdw_traffic_gam(s,BDW_GAM_CLIENTS,&arbiter) && arbiter;
}

static inline bool intel_bdw_traffic_poll(struct bdw_traffic_sample *s,bool *drained)
{
    bool arbiter, pipelines, closing;
    uint32_t l3, eco;
    /* Read the request before the acknowledgements. A pulse observed while
     * every request line is still open cannot qualify a later closure. */
    if (!bdw_traffic_gam(s,0,&arbiter) || !s->read(s->ctx,0x8038,&l3) ||
        !bdw_traffic_engines(s,true,&pipelines) || !s->read(s->ctx,0x8040,&eco) ||
        (l3 & 2u) || ((l3 & 1u) && !s->request_closed)) return false;
    if (!bdw_traffic_gam(s,0,&closing)) return false;
    /* An ACK first seen while any client could still issue memory traffic
     * cannot cover that client, even if the bit stays high through closure.
     * GPM may consume a valid pulse before the last engine reports ready. */
    bool high = (l3 & 1u) != 0;
    if (arbiter && closing && high && !s->l3_high) s->l3_ack = true;
    s->l3_high = high;
    *drained = arbiter && closing && pipelines && s->l3_ack && (eco & 1u);
    return true;
}

/* Closing MMIO checks for an already held GPM block with freshly witnessed
 * pipeline and L3 acknowledgements. The reader verifies the live owner around
 * each access. L3's flush ACK may have been consumed by GPM in the meantime. */
static inline bool intel_bdw_traffic_still_blocked(void *ctx, bool (*read)(void *, uint32_t, uint32_t *))
{
    static const struct { uint32_t offset, mask, expected; } checks[] = {
        /* Bracket the queue and flush reads with a closed arbiter. A late
         * closure cannot authenticate acknowledgements sampled before it. */
        {0x40d0, 0x805f, 0}, {0x8028, 0x5f, 0}, {BDW_RSTFCTLMSG, BDW_PREP_RST_WIDI_ACK, 0},
        /* Both video engines' reference and prefetch queues. */
        {0x12870, 0xff0000, 0}, {0x12878, 0xff0000, 0},
        {0x1c870, 0xff0000, 0}, {0x1c878, 0xff0000, 0},
        /* Flush acknowledged and GO clear on all five engines. */
        {0x8000, 0x31, 0x20}, {0x8004, 0x31, 0x20}, {0x8008, 0x31, 0x20},
        {0x800c, 0x31, 0x20}, {0x8010, 0x31, 0x20},
        {0x8014, 0x11, 0}, /* WiDi GO acknowledged closed, pipe reports busy. */
        /* GAM idle, no L3 unblock, no new requests or grants. */
        {0x8040, 1, 1}, {0x8038, 2, 0}, {0x40d0, 0x805f, 0}, {0x8028, 0x5f, 0},
        {BDW_RSTFCTLMSG, BDW_PREP_RST_WIDI_ACK, 0}
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
