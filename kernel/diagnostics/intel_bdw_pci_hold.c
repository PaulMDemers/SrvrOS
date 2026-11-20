#include <srvros/intel_bdw_pci_hold.h>

#define AF_TP 0x100u

struct hold_attempt {
    const struct bdw_pci_hold_io *io;
    struct bdw_pci_hold_report *report;
    uint64_t begin, last;
    uint32_t status;
};

static bool held(struct hold_attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    if (!a->begin)
        a->begin = now;
    a->last = now;
    a->report->elapsed_us = now - a->begin;
    return a->report->elapsed_us < BDW_PCI_HOLD_LIMIT_US;
}

static bool sample(struct hold_attempt *a, uint16_t command, struct bdw_pci_hold_sample *s)
{
    return held(a) && a->io->sample(a->io->context, s) && held(a) &&
           s->command == (a->status | command) && !(s->af & ~AF_TP);
}

static void record(struct hold_attempt *a, const struct bdw_pci_hold_sample *s)
{
    a->report->sample[a->report->samples++] = *s;
}

static bool store(struct hold_attempt *a, uint16_t before, uint16_t after)
{
    if (!held(a))
        return false;
    enum intel_gt_write written = a->io->command(a->io->context, before, after);
    if (written != GT_WRITE_NONE) {
        ++a->report->writes;
        a->report->retained = true;
    }
    return written == GT_WRITE_DONE && held(a);
}

static bool wait_pending(struct hold_attempt *a)
{
    uint64_t begin = a->last;
    for (unsigned i = 0; i < BDW_PCI_HOLD_TP_LIMIT_US / BDW_PCI_HOLD_SETTLE_US; ++i) {
        uint64_t before = a->last;
        struct bdw_pci_hold_sample s;
        a->io->delay(a->io->context, BDW_PCI_HOLD_SETTLE_US);
        if (!sample(a, 0x403, &s) || a->last - before < BDW_PCI_HOLD_SETTLE_US ||
            a->last - begin >= BDW_PCI_HOLD_TP_LIMIT_US)
            return false;
        ++a->report->polls;
        a->report->settle_us = a->last - begin;
        if (a->report->first_clear_us && !s.af) {
            record(a, &s);
            return true;
        }
        a->report->first_clear_us = s.af ? 0 : a->report->settle_us;
    }
    return false;
}

static bool observe_dark_rc6(struct hold_attempt *a)
{
    struct bdw_pci_hold_report *r = a->report;
    for (unsigned i = 0; i < 2; ++i) {
        struct bdw_pci_hold_sample pci;
        if (i)
            a->io->delay(a->io->context, BDW_PCI_RC6_INTERVAL_US);
        if (!sample(a, 0x403, &pci) || pci.af)
            return false;
        struct bdw_pci_rc6_sample point = {.begin_us = r->elapsed_us};
        if ((i && point.begin_us - r->rc6[0].end_us < BDW_PCI_RC6_INTERVAL_US) ||
            !a->io->read_rc6(a->io->context, &point) ||
            !sample(a, 0x403, &pci) || pci.af)
            return false;
        point.end_us = r->elapsed_us;
        r->rc6[r->rc6_count++] = point;
    }
    return true;
}

bool intel_bdw_pci_hold(const struct bdw_pci_hold_io *io, struct bdw_pci_hold_report *report)
{
    if (!report)
        return false;
    *report = (struct bdw_pci_hold_report){0};
    if (!io || !io->held || !io->sample || !io->command || !io->now ||
        !io->delay || !io->read_rc6 || !io->cpu_copy || !io->backing)
        return false;
    struct hold_attempt a = {.io = io, .report = report};
    struct bdw_pci_hold_sample s;
    if (!held(&a) || !io->sample(io->context, &s) || !held(&a) ||
        (s.command & 0x10ffffu) != 0x100007u || (s.af & ~AF_TP))
        return false;
    a.status = s.command & 0xffff0000u;
    record(&a, &s);
    if (!store(&a, 7, 0x403) || !wait_pending(&a) || !observe_dark_rc6(&a) ||
        !store(&a, 0x403, 0x400) || !sample(&a, 0x400, &s) || s.af)
        return false;
    record(&a, &s);
    if (!io->cpu_copy(io->context) || !sample(&a, 0x400, &s) || s.af)
        return false;
    report->cpu_copy = true;
    if (!store(&a, 0x400, 0x402) || !sample(&a, 0x402, &s) || s.af)
        return false;
    record(&a, &s);
    if (!io->backing(io->context) || !sample(&a, 0x402, &s) || s.af)
        return false;
    report->backing = true;
    if (!store(&a, 0x402, 7) || !sample(&a, 7, &s) || s.af)
        return false;
    record(&a, &s);
    report->retained = false;
    report->complete = true;
    return true;
}
