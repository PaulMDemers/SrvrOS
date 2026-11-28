#include <assert.h>
#include <string.h>
#include <srvros/intel_hsw_completion_page.h>

struct model {
    struct hsw_completion_binding expected;
    struct hsw_completion_sample regs;
    unsigned char bytes[4096];
    bool pinned, firmware, zero, visible, retire, fail_pga, lose_gate, slow_write;
    unsigned reads, writes;
    uint64_t clock;
};

static bool valid(void *ctx, uint64_t ticket, const struct hsw_completion_binding *b) {
    const struct model *m = ctx;
    return ticket == 4 && m->pinned && m->firmware && b->boot_epoch == m->expected.boot_epoch &&
           b->gpu_offset == m->expected.gpu_offset && b->dma_address == m->expected.dma_address;
}

static uint64_t now(void *ctx) {
    struct model *m = ctx;
    return ++m->clock;
}

static bool read_sample(void *ctx, const struct hsw_completion_binding *b, struct hsw_completion_sample *sample) {
    struct model *m = ctx;
    assert(valid(ctx, 4, b));
    *sample = m->regs;
    m->reads++;
    return true;
}

static bool zero_and_verify(void *ctx, const struct hsw_completion_binding *b) {
    struct model *m = ctx;
    assert(valid(ctx, 4, b) && !m->regs.pte && !m->regs.pga);
    if (!m->zero) return false;
    memset(m->bytes, 0, sizeof(m->bytes));
    for (unsigned i = 0; i < sizeof(m->bytes); i++) assert(m->bytes[i] == 0);
    return true;
}

static bool write_pte(void *ctx, const struct hsw_completion_binding *b, uint32_t word) {
    struct model *m = ctx;
    assert(valid(ctx, 4, b) && !m->regs.pga && (!word || word == (b->dma_address | 1)));
    assert(word || m->retire);
    m->regs.pte = word;
    m->writes++;
    if (m->lose_gate) m->firmware = false;
    if (m->slow_write) m->clock += 4001;
    return true;
}

static bool write_pga(void *ctx, const struct hsw_completion_binding *b, uint32_t word) {
    struct model *m = ctx;
    assert(valid(ctx, 4, b) && m->visible && m->regs.pte == (b->dma_address | 1));
    assert(!word || word == b->gpu_offset);
    m->regs.pga = word;
    m->writes++;
    return !m->fail_pga;
}

static bool translation_visible(void *ctx, const struct hsw_completion_binding *b, uint32_t word) {
    const struct model *m = ctx;
    assert(valid(ctx, 4, b) && word == m->regs.pte && !m->regs.pga);
    return m->visible;
}

static bool readers_retired(void *ctx, const struct hsw_completion_binding *b) {
    const struct model *m = ctx;
    assert(valid(ctx, 4, b) && m->regs.pte && !m->regs.pga);
    return m->retire;
}

void test_hsw_completion_page(const struct hwb_result *backing) {
    const struct hsw_completion_binding binding = {.boot_epoch=12, .dma_address=0x100000, .gpu_offset=0x6000000};
    const struct model baseline = {.expected=binding, .regs={.hwstam=UINT32_MAX,.imr=UINT32_MAX},
                                   .pinned=true,.firmware=true,.zero=true,.visible=true,.retire=true};
    struct model m;
    const struct hsw_completion_io io = {&m, valid, now, read_sample, zero_and_verify,
                                         write_pte, write_pga, translation_visible, readers_retired};
    const uint32_t offsets[] = {HSW_CANDIDATE_GPU, HSW_BOOT_SLOT_GPU, 0x1000};
    for (unsigned i = 0; i < sizeof(offsets) / sizeof(*offsets); i++) {
        m = baseline;
        m.expected.gpu_offset = offsets[i];
        struct hsw_completion_page result = {0};
        assert(!intel_hsw_completion_page_run(&result, backing, &m.expected, &io, 4));
        assert(result.state == HSW_COMPLETION_REFUSED && !m.reads && !m.writes);
    }
    enum { CLEAN, NO_FIRMWARE, WRONG_EPOCH, UNPINNED, OCCUPIED, LIVE_PGA, OPEN_MASK,
           ZERO_FAILED, NO_TRANSLATION, PGA_UNCERTAIN, READERS_LIVE, LOST_GATE, DEADLINE, COUNT };
    for (unsigned test = CLEAN; test < COUNT; test++) {
        m = baseline;
        struct hsw_completion_page result = {0};
        memset(m.bytes, 0xaa, sizeof(m.bytes));
        if (test == NO_FIRMWARE) m.firmware = false;
        if (test == WRONG_EPOCH) m.expected.boot_epoch++;
        if (test == UNPINNED) m.pinned = false;
        if (test == OCCUPIED) m.regs.pte = 1;
        if (test == LIVE_PGA) m.regs.pga = 0x2000;
        if (test == OPEN_MASK) m.regs.imr = UINT32_MAX ^ (1u << 24);
        if (test == ZERO_FAILED) m.zero = false;
        if (test == NO_TRANSLATION) m.visible = false;
        if (test == PGA_UNCERTAIN) m.fail_pga = true;
        if (test == READERS_LIVE) m.retire = false;
        if (test == LOST_GATE) m.lose_gate = true;
        if (test == DEADLINE) m.slow_write = true;
        assert(intel_hsw_completion_page_run(&result, backing, &binding, &io, 4) == (test == CLEAN));
        if (test == CLEAN) {
            assert(result.state == HSW_COMPLETION_RETURNED && m.writes == 4);
            assert(result.zero_attempts == 1 && result.pte_attempts == 2 && result.pga_attempts == 2);
            assert(!m.regs.pte && !m.regs.pga && m.bytes[0] == 0);
        } else if (test <= ZERO_FAILED) {
            assert(result.state == HSW_COMPLETION_REFUSED && !m.writes && m.bytes[0] == 0xaa);
            if (test <= UNPINNED) assert(!m.reads && !result.zero_attempts);
        } else {
            assert(result.state == HSW_COMPLETION_RETAINED && m.regs.pte == (binding.dma_address | 1));
            assert(result.pte_attempts == 1);
            if (test == DEADLINE) assert(!result.pga_attempts);
        }
        assert(!intel_hsw_completion_page_run(&result, backing, &binding, &io, 4));
    }
}
