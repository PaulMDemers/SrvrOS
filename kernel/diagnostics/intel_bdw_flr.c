#include <srvros/intel_bdw_flr.h>
#include <srvros/intel_bdw_traffic.h>

#define IGD_ID 0x16268086u
#define IGD_CLASS 0x03000009u
#define AF_HEADER 0x03060013u
#define PCI_INTX_DISABLE 0x400u
#define PCI_MEMORY 2u
#define PCI_IO 1u
#define PCI_CAP_LIST (1u << 20)
#define PCI_MSI_ENABLE (1u << 16)
#define FLR_QUIET_US 100000u
#define FLR_LIMIT_US 5000000u

struct attempt {
    const struct bdw_flr_io *io;
    struct bdw_flr_report *report;
    const struct bdw_flr_pci_image *image;
    const struct bdw_flr_drain *drain;
    uint64_t issued, last, traffic_begin;
    bool l3_select_uncertain;
};

static bool check(struct attempt *a)
{
    if (a->drain->attempt != a->report || !a->io->held(a->io->context) ||
        !a->io->admitted(a->io->context, a->image, a->drain) ||
        (a->report->block_issued && !a->io->block_held(a->io->context, a->report)))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now == UINT64_MAX || now < a->last)
        return false;
    a->last = now;
    if (a->traffic_begin && now - a->traffic_begin >= BDW_TRAFFIC_BLOCK_US)
        return false;
    if (!a->issued)
        return true;
    a->report->elapsed_us = now - a->issued;
    return a->report->elapsed_us < FLR_LIMIT_US;
}

static bool load(struct attempt *a, uint8_t offset, uint32_t *word)
{
    return check(a) && a->io->read(a->io->context, offset, word) && check(a);
}

static bool expect(struct attempt *a, uint8_t offset, uint32_t mask, uint32_t expected)
{
    uint32_t word;
    return load(a, offset, &word) && word != UINT32_MAX && (word & mask) == expected;
}

static bool profile(struct attempt *a, uint32_t command)
{
    return expect(a, 0, UINT32_MAX, IGD_ID) && expect(a, 8, UINT32_MAX, IGD_CLASS) &&
           expect(a, 4, PCI_CAP_LIST | 0xffffu, PCI_CAP_LIST | command) &&
           expect(a, 0x34, 0xffu, 0x90u) &&
           expect(a, 0x90, PCI_MSI_ENABLE | 0xffffu, 0xd005u) &&
           expect(a, 0xd0, 0x7ffffu, 0x2a401u) &&
           expect(a, 0xd4, 3u, 0) && expect(a, 0xa4, UINT32_MAX, AF_HEADER) &&
           expect(a, 0xa8, UINT32_MAX, 0);
}

static bool saved_resources(struct attempt *a, const struct bdw_flr_pci_image *saved)
{
    if (!expect(a, 0x2c, UINT32_MAX, saved->subsystem))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!expect(a, (uint8_t)(0x10 + 4*i), UINT32_MAX, saved->bar[i]))
            return false;
    return true;
}

/* Broadwell PRM 2c pp. 587-592, 805-814, 859, 994, 1034. Read these
 * only while decode is open under the exclusive block provider. */
static bool gt_word(struct attempt *a, uint32_t offset, uint32_t *word)
{
    *word = UINT32_MAX;
    return check(a) && a->io->gt_read(a->io->context, offset, word) &&
           *word != UINT32_MAX && check(a);
}

static bool held_gt_word(void *ctx,uint32_t offset,uint32_t *word)
{
    return gt_word(ctx,offset,word);
}

static bool select_l3(void *context, uint32_t from, uint32_t to)
{
    struct attempt *a = context;
    if (!check(a)) return false;
    enum intel_gt_write status = a->io->select_l3(a->io->context,from,to);
    if (status == GT_WRITE_NONE) return false;
    ++a->report->l3_selector_writes;
    uint32_t observed;
    if (status != GT_WRITE_DONE || !gt_word(a,0xfdc,&observed) || observed != to) {
        a->l3_select_uncertain = true;
        return false;
    }
    return true;
}

static bool l3_policy(struct attempt *a, struct bdw_traffic_sample *sample)
{
    uint32_t original, current;
    if (!gt_word(a,0xfdc,&original)) return false;
    unsigned writes = a->report->l3_selector_writes;
    bool covered = intel_bdw_traffic_l3_policy(sample,select_l3);
    bool restored = !a->l3_select_uncertain && gt_word(a,0xfdc,&current) && current == original;
    if (!restored && a->report->l3_selector_writes != writes) {
        a->report->l3_selector_retained = true;
        a->report->retained = true;
    }
    if (!covered || !restored) return false;
    ++a->report->l3_policy_checks;
    return true;
}

static bool block_traffic(struct attempt *a)
{
    struct bdw_traffic_sample sample = {.ctx=a, .read=held_gt_word};
    a->traffic_begin = a->last;
    if (!l3_policy(a,&sample) || !intel_bdw_traffic_open(&sample) || !check(a)) return false;
    enum intel_gt_write issued = a->io->block(a->io->context, a->report);
    if (issued != GT_WRITE_NONE) {
        a->report->block_issued = true;
        a->report->retained = true;
    }
    if (issued != GT_WRITE_DONE || !check(a))
        return false;

    for (unsigned i = 0; i < GT_ACCESS_POLL_CAP; ++i) {
        bool drained;
        ++a->report->traffic_polls;
        bool sampled = intel_bdw_traffic_poll(&sample,&drained);
        a->report->engine_acks = sample.engine_ack;
        a->report->l3_ack = sample.l3_ack;
        if (!sampled) return false;
        if (drained) {
            if (!profile(a, PCI_INTX_DISABLE | PCI_MEMORY | PCI_IO) || !l3_policy(a,&sample) ||
                !intel_bdw_traffic_still_blocked(a,held_gt_word)) return false;
            if (a->last <= a->traffic_begin)
                return false;
            a->report->traffic_blocked = true;
            return true;
        }
        uint64_t before = a->last;
        a->io->delay(a->io->context, 1);
        if (!check(a) || a->last <= before)
            return false;
    }
    return false;
}

static bool store(struct attempt *a, uint8_t offset, unsigned bytes, uint32_t value)
{
    if (!check(a))
        return false;
    enum intel_gt_write status = a->io->write(a->io->context, offset, bytes, value);
    if (status != GT_WRITE_NONE)
        ++a->report->stores;
    if (offset == 0xa8 && status != GT_WRITE_NONE) {
        a->report->reset_issued = true;
        a->report->retained = true;
        a->issued = a->last;
    }
    return status == GT_WRITE_DONE && check(a);
}

static bool wait_ready(struct attempt *a)
{
    /* Start the quiet interval after the configuration write returned. */
    uint64_t begin = a->last;
    a->io->delay(a->io->context, FLR_QUIET_US);
    if (!check(a) || a->last - begin < FLR_QUIET_US)
        return false;
    for (;;) {
        uint32_t id;
        if (!load(a, 0, &id))
            return false;
        ++a->report->ready_polls;
        if (id != UINT32_MAX)
            return id == IGD_ID;
        uint64_t before = a->last;
        a->io->delay(a->io->context, 1000);
        if (!check(a) || a->last - before < 1000)
            return false;
    }
}

bool intel_bdw_flr(const struct bdw_flr_io *io, const struct bdw_flr_pci_image *image,
                   struct bdw_flr_drain *drain, struct bdw_flr_report *report)
{
    if (!report || !drain || drain->attempt)
        return false;
    *report = (struct bdw_flr_report){0};
    if (!io || !image || !drain->source || !drain->result || !drain->retire_alias ||
        !io->held || !io->admitted || !io->read || !io->write ||
        !io->now || !io->delay || !io->gt_read || !io->block || !io->block_held || !io->select_l3 ||
        (image->bar[0] & 0xffffffu) != 4 ||
        (image->bar[2] & 0xfffffffu) != 12 || image->subsystem != 0x011b106bu ||
        !image->bar[0] || !image->bar[2] || image->bar[1] || image->bar[3] ||
        image->bar[0] == 4 || image->bar[2] == 12)
        return false;
    const struct bdw_flr_pci_image saved = *image;
    drain->attempt = report;
    const struct bdw_flr_drain child = *drain;
    struct attempt a = {.io = io, .report = report, .image = &saved, .drain = drain};
    if (child.source->consumer || child.result->object || !check(&a) ||
        !profile(&a, PCI_INTX_DISABLE | PCI_MEMORY | PCI_IO) || !saved_resources(&a, &saved) ||
        !block_traffic(&a) ||
        !store(&a, 4, 2, PCI_INTX_DISABLE))
        return false;
    a.traffic_begin = 0;
    if (!profile(&a, PCI_INTX_DISABLE) || !saved_resources(&a, &saved))
        return false;
    /* A new drain belongs to this admission only. A completed object from
     * an earlier hold cannot execute twice. Keep decode and BME off until
     * all covered non-posted and remapped memory work has completed. */
    if (!check(&a))
        return false;
    if (!intel_vtd_drain_run(child.result, child.source, child.io))
        return false;
    if (child.result->p.begin < a.last || !check(&a) || a.last < child.result->end ||
        !child.retire_alias(child.io.ctx, child.source, child.result)) {
        report->retained = true;
        return false;
    }
    if (!check(&a) || !saved_resources(&a, &saved) ||
        !profile(&a, PCI_INTX_DISABLE) ||
        !store(&a, 0xa8, 1, 1))
        return false;
    report->stage = BDW_FLR_WAIT;
    if (!wait_ready(&a))
        return false;
    report->stage = BDW_FLR_RESOURCES;
    uint32_t command, subsystem;
    if (!load(&a, 4, &command) || (command & 0xffffu & ~PCI_INTX_DISABLE) ||
        !profile(&a, command & PCI_INTX_DISABLE) || !load(&a, 0x2c, &subsystem) ||
        (subsystem && subsystem != saved.subsystem))
        return false;
    const uint32_t defaults[] = {4, 0, 12, 0};
    uint32_t bars[4];
    for (unsigned i = 0; i < 4; ++i)
        if (!load(&a, (uint8_t)(0x10 + 4*i), &bars[i]) ||
            (bars[i] != defaults[i] && bars[i] != saved.bar[i]))
            return false;
    if (!subsystem && !store(&a, 0x2c, 4, saved.subsystem))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (bars[i] != saved.bar[i] && !store(&a, (uint8_t)(0x10 + 4*i), 4, saved.bar[i]))
            return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!expect(&a, (uint8_t)(0x10 + 4*i), UINT32_MAX, saved.bar[i]))
            return false;
    report->stage = BDW_FLR_REOPEN;
    if (!expect(&a, 0x2c, UINT32_MAX, saved.subsystem) ||
        !store(&a, 4, 2, PCI_INTX_DISABLE | PCI_MEMORY) ||
        !profile(&a, PCI_INTX_DISABLE | PCI_MEMORY))
        return false;
    report->stage = BDW_FLR_COMPLETE;
    report->retained = false;
    return true;
}
