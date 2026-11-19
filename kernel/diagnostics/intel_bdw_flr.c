#include <srvros/intel_bdw_flr.h>

#define IGD_ID 0x16268086u
#define IGD_CLASS 0x03000009u
#define AF_HEADER 0x03060013u
#define PCI_INTX_DISABLE 0x400u
#define PCI_MEMORY 2u
#define PCI_CAP_LIST (1u << 20)
#define PCI_MSI_ENABLE (1u << 16)
#define FLR_QUIET_US 100000u
#define FLR_LIMIT_US 5000000u

struct attempt {
    const struct bdw_flr_io *io;
    struct bdw_flr_report *report;
    uint64_t issued, last;
};

static bool check(struct attempt *a)
{
    if (!a->io->held(a->io->context))
        return false;
    uint64_t now = a->io->now(a->io->context);
    if (!now || now < a->last)
        return false;
    a->last = now;
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
    a->io->delay(a->io->context, FLR_QUIET_US);
    if (!check(a) || a->report->elapsed_us < FLR_QUIET_US)
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
                   struct bdw_flr_report *report)
{
    if (!report)
        return false;
    *report = (struct bdw_flr_report){0};
    if (!io || !image || !io->held || !io->admitted || !io->read || !io->write ||
        !io->now || !io->delay || (image->bar[0] & 0xffffffu) != 4 ||
        (image->bar[2] & 0xfffffffu) != 12 || image->subsystem != 0x011b106bu ||
        !image->bar[0] || !image->bar[2] || image->bar[1] || image->bar[3] ||
        image->bar[0] == 4 || image->bar[2] == 12)
        return false;
    const struct bdw_flr_pci_image saved = *image;
    struct attempt a = {io, report, 0, 0};
    if (!check(&a) || !io->admitted(io->context, &saved) || !profile(&a, PCI_INTX_DISABLE) ||
        !expect(&a, 0x2c, UINT32_MAX, saved.subsystem))
        return false;
    for (unsigned i = 0; i < 4; ++i)
        if (!expect(&a, (uint8_t)(0x10 + 4*i), UINT32_MAX, saved.bar[i]))
            return false;
    if (!io->admitted(io->context, &saved) ||
        !expect(&a, 4, 0xffffu, PCI_INTX_DISABLE) ||
        !expect(&a, 0xa8, UINT32_MAX, 0) || !store(&a, 0xa8, 1, 1))
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
        !io->admitted(io->context, &saved) || !store(&a, 4, 2, PCI_INTX_DISABLE | PCI_MEMORY) ||
        !profile(&a, PCI_INTX_DISABLE | PCI_MEMORY))
        return false;
    report->stage = BDW_FLR_COMPLETE;
    report->retained = false;
    return true;
}
