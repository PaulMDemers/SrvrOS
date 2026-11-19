#include <srvros/intel_bdw_edp.h>
#include <srvros/intel_bdw_edp_source.h>

#include <assert.h>
#include <stdio.h>
#include <string.h>

#define CTL 0x64010u
#define DATA 0x64014u
#define BUSY 0x80000000u
#define DONE 0x40000000u
#define TIMEOUT 0x10000000u
#define RECEIVE 0x02000000u
#define EVENTS (DONE | TIMEOUT | RECEIVE)
#define PP_STATUS 0xc7200u
#define PP_CONTROL 0xc7204u
#define PP_ON_DELAYS 0xc7208u
#define PP_OFF_DELAYS 0xc720cu
#define PP_DIVISOR 0xc7210u

static const uint8_t caps[16] = {
    0x11, 0x0a, 0x81, 0x41, 0, 0, 1, 0xc0, 2, 0, 0, 0, 0x0f, 1, 0, 0
};
static const uint8_t request_sizes[] = {16, 1, 2, 1, 2, 2, 6, 1, 6, 2, 6, 1, 6, 1, 3, 6};
static const uint16_t request_addresses[] = {
    0, 0x600, 0x107, 0x10a, 0x100, 0x102, 0x202, 0x103,
    0x202, 0x102, 0x202, 0x103, 0x202, 0x102, 0x100, 0x202
};
static const uint16_t payloads[] = {
    0, 1, 1, 0, 0x0a81, 0x2100, 0, 1, 0, 0x2201, 0, 0x0e, 0, 0, 0, 0
};

enum injection {
    NO_FAULT, FOREIGN_BUSY, STUCK_BUSY, BAD_CONTROLLER_TIMEOUT, RECEIVE_ERROR,
    MISSING_DONE, EMPTY_REPLY, BIG_REPLY, SHORT_ACK, LONG_ACK, NACK, BAD_REPLY,
    DEFER, UNSUPPORTED_CAP, BAD_FINAL_CONFIG, BAD_FINAL_STATUS, EQ_LOSTS_CLOCK,
    EQ_NO_ALIGNMENT, CR_NO_PROGRESS, EARLY_DELAY, BACKWARD_CLOCK, FROZEN_CLOCK,
    OVERDUE_SCANOUT,
    PANEL_ALREADY_ON, PANEL_ALREADY_LIT, PANEL_CYCLE_STUCK, PANEL_BAD_DELAYS,
    PANEL_BAD_SEQUENCE, PANEL_POWER_STUCK, PANEL_FRAME_NOT_LIVE,
    PANEL_FRAME_LOST, PANEL_POWER_LOST, PANEL_CONTROL_CHANGED, PANEL_EARLY_ON,
};

struct fixture {
    enum injection fault;
    uint64_t time, last_change;
    uint32_t tx[5], control, response;
    uint8_t rx[20];
    uint8_t config[3];
    unsigned busy_reads, attempts, requests, defers, clears, reads, writes, sources;
    unsigned events, fail_at, lost_at, lost_after, clock_after, elapsed_after;
    unsigned aux_setup;
    bool faulted, uncertain, d0, idle, scanout, normal;
    enum bdw_edp_stage stage;
    bool panel, backlight;
    uint32_t pp_control, pp_delay[3];
    unsigned pp_writes, live_checks;
    uint64_t cycle_until, power_requested, first_live;
};

enum source_fault {
    SOURCE_GOOD, SOURCE_ACTIVE, SOURCE_PLL_UNLOCKED, SOURCE_CDCLK_WRONG,
    SOURCE_LANE_CHANGED, SOURCE_STALE_UNDERRUN, SOURCE_MASKED, SOURCE_IRQ_ENABLED,
    SOURCE_BAD_IMAGE, SOURCE_BAD_PHY, SOURCE_CLOCK_MISMATCH, SOURCE_WARMUP_SHORT,
    SOURCE_UNDERRUN_DURING_VIDEO, SOURCE_CLOCK_STOPPED, SOURCE_INACTIVE,
    SOURCE_WRONG_LIVE, SOURCE_UNPROVEN_FETCH, SOURCE_UNDERRUN_AFTER_PROOF,
    SOURCE_CLOCK_LOST_AFTER_PROOF, SOURCE_PANEL_LOST, SOURCE_COUNTER_WRAP,
};

struct register_write {
    uint32_t offset, value;
};

struct source_fixture {
    struct fixture panel;
    struct bdw_edp_source_image image;
    enum source_fault fault;
    uint32_t clock, transport, buffer, transcoder, primary, surface;
    uint32_t stride, plane_offset;
    struct register_write posted;
    uint64_t buffer_enabled_at, normal_at;
    unsigned step, proofs;
};

/* Source write order captured separately from the driver's register plan. */
static const struct register_write source_suffix[] = {
    {0x64040, 0x80040000}, {0x64000, 0x80010010},
    {0x64040, 0x80040000}, {0x64000, 0x84010010},
    {0x64040, 0x80040100}, {0x64000, 0x84010010},
    {0x64040, 0x80040100}, {0x64000, 0x88010010},
    {0x64040, 0x80040200},
    {0x6001c, 0x059f0383}, {0x70030, 0x50},
    {0x6f000, 0x063f059f}, {0x6f004, 0x063f059f}, {0x6f008, 0x05ef05cf},
    {0x6f00c, 0x039d0383}, {0x6f010, 0x039d0383}, {0x6f014, 0x038c0386},
    {0x6f030, 0x7e5ed0e5}, {0x6f034, 0x800000}, {0x6f040, 0x2a23f}, {0x6f044, 0x80000},
    {0x6f410, 1}, {0x45100, 0xe0000}, {0x45270, 144},
    {0x6f400, 0x82200000}, {0x7f008, 0x80000000},
    {0x70188, 6144}, {0x701a4, 0}, {0x70180, 0x98000400}, {0x7019c, 0},
    {0x64040, 0x80040300},
};

static bool event(struct fixture *f)
{
    assert(!f->faulted);
    ++f->events;
    f->time += 2;
    if (f->events == f->clock_after)
        f->time = 0;
    if (f->events == f->elapsed_after)
        f->time += 150000;
    if (f->events != f->fail_at)
        return true;
    f->faulted = true;
    return false;
}

static bool held(void *context)
{
    struct fixture *f = context;
    return (!f->lost_at || f->events < f->lost_at) &&
           (!f->lost_after || f->requests < f->lost_after);
}

static bool panel_powered(const struct fixture *f)
{
    return !f->panel || (f->power_requested && f->time - f->power_requested >= 200000);
}

static bool link_held(void *context)
{
    return held(context) && panel_powered(context);
}

static uint64_t now(void *context)
{
    return ((struct fixture *)context)->time;
}

static void wait(void *context, uint32_t us)
{
    struct fixture *f = context;
    if (f->fault == FROZEN_CLOCK)
        return;
    if (f->fault == BACKWARD_CLOCK) {
        --f->time;
        return;
    }
    f->time += f->fault == EARLY_DELAY ? us / 2 : us;
}

static void response(struct fixture *f, unsigned address, unsigned request,
                     unsigned size, const uint8_t *tx)
{
    memset(f->rx, 0, sizeof(f->rx));
    unsigned length = request == 9 ? 1 + size : 1;
    uint32_t flag = DONE;
    if (f->fault == DEFER && f->defers) {
        --f->defers;
        f->rx[0] = 0x20;
        length = 1;
    } else if (f->fault >= BAD_CONTROLLER_TIMEOUT && f->fault <= BAD_REPLY) {
        switch (f->fault) {
        case BAD_CONTROLLER_TIMEOUT: flag = TIMEOUT; break;
        case RECEIVE_ERROR: flag |= RECEIVE; break;
        case MISSING_DONE: flag = 0; break;
        case EMPTY_REPLY: length = 0; break;
        case BIG_REPLY: length = 21; break;
        case SHORT_ACK: length = size; break;
        case LONG_ACK: length = size + 2; break;
        case NACK: f->rx[0] = 0x10; length = 1; break;
        case BAD_REPLY: f->rx[0] = 0x40; length = 1; break;
        default: assert(0);
        }
    } else {
        unsigned i = f->requests++;
        if (f->fault != CR_NO_PROGRESS && f->fault != EQ_NO_ALIGNMENT) {
            assert(i < sizeof(request_sizes));
            assert(address == request_addresses[i] && size == request_sizes[i]);
            assert(request == ((address == 0 || address == 0x202 || i == 14) ? 9u : 8u));
            if (request == 8) {
                unsigned payload = size == 2 ? (unsigned)tx[4] * 256 + tx[5] : tx[4];
                assert(payload == payloads[i]);
            }
        }
        switch (address) {
        case 0:
            memcpy(f->rx + 1, caps, 16);
            if (f->fault == UNSUPPORTED_CAP)
                f->rx[3] = 0x82;
            break;
        case 0x600:
            assert(f->stage == BDW_EDP_PREPARE_SOURCE);
            f->last_change = f->time;
            f->d0 = true;
            break;
        case 0x107:
            assert(f->d0 && f->time - f->last_change >= 2000);
            break;
        case 0x100:
            if (request == 9) {
                memcpy(f->rx + 1, f->config, sizeof(f->config));
                if (f->fault == BAD_FINAL_CONFIG)
                    f->rx[2] = 1;
            } else {
                memcpy(f->config, tx + 4, size);
            }
            break;
        case 0x102:
            if (!tx[4])
                assert(f->idle && f->scanout && !f->normal);
            else
                assert(f->stage == (tx[4] == 0x21 ? BDW_EDP_CLOCK_RECOVERY : BDW_EDP_EQUALIZE));
            f->config[2] = tx[4];
            f->last_change = f->time;
            break;
        case 0x103:
            f->last_change = f->time;
            break;
        case 0x202:
            assert(f->time - f->last_change >= (f->stage == BDW_EDP_CLOCK_RECOVERY ? 100u : 400u));
            if (f->fault == CR_NO_PROGRESS)
                break;
            f->rx[1] = (i == 6 || (i == 10 && f->fault == EQ_LOSTS_CLOCK))
                ? 0 : (i == 8 || i == 10 ? 1 : 7);
            f->rx[3] = f->fault != EQ_NO_ALIGNMENT && i >= 12 ? 1 : 0;
            f->rx[5] = i < 10 ? 1 : 7; /* swing 3, emphasis 1 -> swing 2 */
            if (i == 15 && f->fault == BAD_FINAL_STATUS)
                f->rx[3] = 0;
            break;
        default: assert(address == 0x10a);
        }
    }
    f->response = f->aux_setup | flag | (length << 20);
}

static bool read(void *context, uint32_t offset, uint32_t *word)
{
    struct fixture *f = context;
    ++f->reads;
    if (!event(f))
        return false;
    if (offset == PP_STATUS) {
        assert(f->panel);
        if (f->fault == PANEL_ALREADY_ON)
            *word = 0x80000008u;
        else if (f->fault == PANEL_CYCLE_STUCK || f->time < f->cycle_until)
            *word = 0x0800000fu;
        else if (!f->power_requested)
            *word = 0;
        else if (f->fault == PANEL_BAD_SEQUENCE)
            *word = 0x20000009u;
        else if (f->fault == PANEL_EARLY_ON)
            *word = 0x80000008u;
        else if (f->fault == PANEL_POWER_STUCK || f->time - f->power_requested < 202000)
            *word = 0x90000009u;
        else if (f->fault == PANEL_POWER_LOST && f->live_checks == 2)
            *word = 0;
        else
            *word = 0x80000008u;
    } else if (offset == PP_CONTROL) {
        assert(f->panel);
        *word = f->fault == PANEL_CONTROL_CHANGED && f->live_checks == 2 ? 0 : f->pp_control;
    } else if (offset >= PP_ON_DELAYS && offset <= PP_DIVISOR) {
        assert(f->panel && (offset - PP_ON_DELAYS) % 4 == 0);
        *word = f->fault == PANEL_BAD_DELAYS ? 0 : f->pp_delay[(offset - PP_ON_DELAYS) / 4];
    } else if (offset == CTL) {
        assert(panel_powered(f) && !f->backlight);
        if (f->control & BUSY && f->fault != FOREIGN_BUSY && f->fault != STUCK_BUSY &&
            --f->busy_reads == 0)
            f->control = f->response;
        *word = f->control;
    } else {
        assert(offset >= DATA && offset <= DATA + 16 && (offset - DATA) % 4 == 0);
        assert((f->control & (BUSY | EVENTS)) == DONE);
        unsigned i = offset - DATA;
        *word = 0;
        for (unsigned j = 0; j < 4; ++j)
            *word |= (uint32_t)f->rx[i + j] << (24 - 8 * j);
    }
    return true;
}

static enum intel_gt_write write(void *context, uint32_t offset, uint32_t word)
{
    struct fixture *f = context;
    ++f->writes;
    if (!event(f))
        return f->uncertain ? GT_WRITE_UNCERTAIN : GT_WRITE_NONE;
    if (offset == PP_CONTROL) {
        assert(f->panel && f->time >= f->cycle_until);
        if (word == 3) {
            assert(!f->pp_control && f->pp_writes == 3 && !f->requests);
            f->power_requested = f->time;
        } else {
            assert(word == 7 && f->pp_control == 3 && !f->backlight && f->normal);
            assert(f->live_checks == 2 && f->time - f->first_live >= 210000);
            f->backlight = true;
        }
        f->pp_control = word;
        ++f->pp_writes;
        return GT_WRITE_DONE;
    }
    if (offset >= PP_ON_DELAYS && offset <= PP_DIVISOR) {
        const uint32_t expected[] = {0x07d00001u, 0x02e40001u, 0x0004af06u};
        assert(f->panel && !f->pp_control && f->time >= f->cycle_until);
        assert(f->pp_writes < 3 && offset == PP_ON_DELAYS + f->pp_writes * 4);
        assert(word == expected[f->pp_writes]);
        f->pp_delay[f->pp_writes++] = word;
        return GT_WRITE_DONE;
    }
    assert(panel_powered(f) && !f->backlight);
    assert(!(f->control & BUSY));
    if (offset != CTL) {
        assert(offset >= DATA && offset <= DATA + 16 && (offset - DATA) % 4 == 0);
        f->tx[(offset - DATA) / 4] = word;
        return GT_WRITE_DONE;
    }

    assert((word & ~0x01f00000u & ~(EVENTS | BUSY)) == f->aux_setup);
    assert((word & EVENTS) == EVENTS);
    if (word & BUSY) {
        ++f->attempts;
        f->busy_reads = 2;
        unsigned length = (word >> 20) & 31u;
        assert(length >= 4 && length <= 20);
        uint8_t bytes[20];
        for (unsigned i = 0; i < length; ++i)
            bytes[i] = (uint8_t)(f->tx[i / 4] >> (24 - 8 * (i % 4)));
        unsigned request = bytes[0] >> 4;
        unsigned address = (bytes[0] & 15u) * 65536u + bytes[1] * 256u + bytes[2];
        unsigned size = bytes[3] + 1u;
        assert(request == 8 || request == 9);
        assert(length == (request == 9 ? 4 : 4 + size));
        response(f, address, request, size, bytes);
        f->control = word & ~EVENTS;
    } else {
        ++f->clears;
        f->control &= ~EVENTS;
    }
    return GT_WRITE_DONE;
}

static enum intel_gt_write source(void *context, enum bdw_edp_stage stage,
                                  unsigned swing, unsigned emphasis)
{
    struct fixture *f = context;
    ++f->sources;
    if (!event(f))
        return f->uncertain ? GT_WRITE_UNCERTAIN : GT_WRITE_NONE;
    assert(stage >= f->stage && stage >= BDW_EDP_PREPARE_SOURCE && stage <= BDW_EDP_NORMAL);
    assert(swing <= 2 && swing + emphasis <= 3);
    if (stage == BDW_EDP_IDLE) {
        assert(f->stage == BDW_EDP_EQUALIZE);
        f->idle = true;
    }
    if (stage == BDW_EDP_SCANOUT) {
        assert(f->idle && !f->normal && f->config[2] == 0x22);
        f->scanout = true;
        if (f->fault == OVERDUE_SCANOUT)
            f->time += 150000;
    }
    if (stage == BDW_EDP_NORMAL) {
        assert(f->scanout && f->config[2] == 0);
        f->normal = true;
        f->last_change = f->time;
    }
    f->stage = stage;
    return GT_WRITE_DONE;
}

static bool live(void *context)
{
    struct fixture *f = context;
    if (!event(f))
        return false;
    assert(f->panel && f->normal && !f->backlight && f->pp_control == 3);
    assert(f->requests == sizeof(request_sizes));
    ++f->live_checks;
    if (f->live_checks == 1)
        f->first_live = f->time;
    else
        assert(f->live_checks == 2 && f->time - f->first_live >= 210000);
    return f->fault != PANEL_FRAME_NOT_LIVE &&
           (f->fault != PANEL_FRAME_LOST || f->live_checks != 2);
}

static bool panel_register(uint32_t offset)
{
    return (offset >= PP_STATUS && offset <= PP_DIVISOR) ||
           (offset >= CTL && offset <= DATA + 16);
}

static bool source_held(void *context, const struct bdw_edp_source_image *image)
{
    struct source_fixture *s = context;
    assert(image->backing == &s->panel && image->epoch == s->image.epoch &&
           image->mapped_bytes == s->image.mapped_bytes &&
           image->cdclk_khz == s->image.cdclk_khz &&
           image->ddi_lane_bits == s->image.ddi_lane_bits &&
           memcmp(image->phy, s->image.phy, sizeof(image->phy)) == 0);
    return held(&s->panel);
}

static bool source_read(void *context, uint32_t offset, uint32_t *word)
{
    struct source_fixture *s = context;
    struct fixture *f = &s->panel;
    if (panel_register(offset)) {
        if (!read(f, offset, word))
            return false;
        if (offset == PP_STATUS && s->fault == SOURCE_PANEL_LOST && f->stage == BDW_EDP_EQUALIZE)
            *word = 0;
        return true;
    }
    if (!event(f))
        return false;
    switch (offset) {
    case 0x130040:
        *word = (s->fault == SOURCE_PLL_UNLOCKED ||
                 (s->fault == SOURCE_CLOCK_LOST_AFTER_PROOF && s->proofs == 2)) ? 0 : 0x40000000u;
        break;
    case 0x42014: *word = 0; break;
    case 0x46200: *word = s->fault == SOURCE_CDCLK_WRONG ? 449u + 1 : 449u; break;
    case 0x46100: *word = s->clock; break;
    case 0x64000:
        *word = s->buffer;
        if (!s->buffer_enabled_at || f->time - s->buffer_enabled_at < 518)
            *word |= 0x80u;
        if (s->fault == SOURCE_LANE_CHANGED)
            *word ^= 0x10u;
        break;
    case 0x64040: *word = s->transport; break;
    case 0x6f400:
        *word = s->fault == SOURCE_ACTIVE ? 0x82200000u : 0;
        if (s->posted.offset == offset)
            *word = s->posted.value;
        break;
    case 0x7f008:
        *word = s->transcoder;
        if (s->normal_at && s->fault != SOURCE_INACTIVE)
            *word |= 0x40000000u;
        break;
    case 0x70180: *word = s->primary; break;
    case 0x70188: *word = s->stride; break;
    case 0x701a4: *word = s->plane_offset; break;
    case 0x7019c: *word = s->surface; break;
    case 0x701ac: *word = s->fault == SOURCE_WRONG_LIVE ? 0x40000u : 0; break;
    case 0x70040:
        assert(f->normal && f->scanout);
        *word = s->fault == SOURCE_CLOCK_STOPPED ? 0 : (uint32_t)((f->time - s->normal_at) / 16667);
        if (s->fault == SOURCE_COUNTER_WRAP)
            *word += UINT32_MAX - 1;
        break;
    case 0x44404: *word = s->fault == SOURCE_MASKED ? 0x80000000u : 0; break;
    case 0x4440c: *word = s->fault == SOURCE_IRQ_ENABLED ? 0x80000000u : 0; break;
    case 0x44408:
        *word = (s->fault == SOURCE_STALE_UNDERRUN ||
                 (s->fault == SOURCE_UNDERRUN_DURING_VIDEO && s->normal_at) ||
                 (s->fault == SOURCE_UNDERRUN_AFTER_PROOF && s->proofs == 2)) ? 0x80000000u : 0;
        break;
    default:
        assert(s->posted.offset == offset);
        *word = s->posted.value;
    }
    return true;
}

static enum intel_gt_write source_write(void *context, uint32_t offset, uint32_t word)
{
    struct source_fixture *s = context;
    struct fixture *f = &s->panel;
    if (panel_register(offset)) {
        if (offset == CTL && (word & BUSY) && f->stage == BDW_EDP_CLOCK_RECOVERY)
            assert(f->time - s->buffer_enabled_at >= 519);
        return write(f, offset, word);
    }
    if (!event(f))
        return f->uncertain ? GT_WRITE_UNCERTAIN : GT_WRITE_NONE;
    assert(panel_powered(f) && !f->backlight);
    unsigned step = s->step++;
    if (step == 0) {
        assert(offset == 0x46100 && word == 0x20000000 && !s->transport);
        s->clock = word;
    } else if (step <= BDW_EDP_PHY_ENTRIES * 2) {
        assert(!s->transport && !(s->buffer & 0x80000000u));
        assert(offset == 0x64e00u + (step - 1) * 4);
        assert(word == s->image.phy[(step - 1) / 2][(step - 1) % 2]);
        if (step == BDW_EDP_PHY_ENTRIES * 2)
            f->stage = BDW_EDP_PREPARE_SOURCE;
    } else {
        unsigned i = step - 1 - BDW_EDP_PHY_ENTRIES * 2;
        assert(i < sizeof(source_suffix) / sizeof(source_suffix[0]));
        assert(source_suffix[i].offset == offset && source_suffix[i].value == word);
        switch (offset) {
        case 0x64000:
            if (!s->buffer_enabled_at)
                s->buffer_enabled_at = f->time;
            s->buffer = word;
            break;
        case 0x64040:
            s->transport = word;
            if (word == 0x80040000)
                f->stage = BDW_EDP_CLOCK_RECOVERY;
            else if (word == 0x80040100)
                f->stage = BDW_EDP_EQUALIZE;
            else if (word == 0x80040200) {
                f->idle = true;
                f->stage = BDW_EDP_IDLE;
            } else {
                assert(f->scanout && !f->config[2]);
                f->stage = BDW_EDP_NORMAL;
                f->normal = true;
                s->normal_at = f->last_change = f->time;
            }
            break;
        case 0x7f008: s->transcoder = word; break;
        case 0x70180: s->primary = word; break;
        case 0x70188: s->stride = word; break;
        case 0x701a4: s->plane_offset = word; break;
        case 0x7019c:
            assert(f->idle && f->config[2] == 0x22 && s->transcoder && s->primary);
            s->surface = word;
            f->scanout = true;
            f->stage = BDW_EDP_SCANOUT;
            break;
        }
    }
    s->posted.offset = offset;
    s->posted.value = word;
    return GT_WRITE_DONE;
}

static uint64_t source_now(void *context)
{
    return now(&((struct source_fixture *)context)->panel);
}

static void source_delay(void *context, uint32_t us)
{
    struct source_fixture *s = context;
    wait(&s->panel, us == 520 && s->fault == SOURCE_WARMUP_SHORT ? 1 : us);
}

static bool source_frame_matches(void *context, const struct bdw_edp_source_image *image,
                                 uint32_t first, uint32_t last)
{
    struct source_fixture *s = context;
    assert(image->backing == &s->panel && last - first >= 2 && last - first <= 4);
    ++s->proofs;
    return live(&s->panel) && s->fault != SOURCE_UNPROVEN_FETCH;
}

static enum bdw_edp_result run_source(struct source_fixture *s, struct bdw_edp_source_report *report)
{
    struct fixture *f = &s->panel;
    f->panel = true;
    f->time = 1;
    f->cycle_until = 4000;
    f->aux_setup = 0x240300e1u;
    s->clock = 0xe0000000u;
    s->buffer = 0x10010u;
    s->image = (struct bdw_edp_source_image){
        .backing = f, .epoch = 41, .mapped_bytes = 904ull * 6144 + 128ull * 4096,
        .cdclk_khz = 450000, .ddi_lane_bits = 0x10010,
    };
    for (unsigned i = 0; i < BDW_EDP_PHY_ENTRIES; ++i) {
        s->image.phy[i][0] = 0x123456u + i;
        s->image.phy[i][1] = 0x7000eu + i;
    }
    if (s->fault == SOURCE_BAD_IMAGE)
        s->image.mapped_bytes -= 4096;
    if (s->fault == SOURCE_BAD_PHY)
        s->image.phy[8][1] |= 1u << 12;
    if (s->fault == SOURCE_CLOCK_MISMATCH)
        s->image.cdclk_khz = 337500;
    const struct bdw_edp_source_io io = {
        s, source_held, source_read, source_write, source_now, source_delay, source_frame_matches,
    };
    return intel_bdw_edp_source_restore(&io, &s->image, report);
}

static unsigned test_source(void)
{
    struct source_fixture good = {0};
    struct bdw_edp_source_report report;
    assert(run_source(&good, &report) == BDW_EDP_OK);
    assert(report.panel.panel_stage == BDW_EDP_PANEL_READY && good.panel.backlight);
    assert(good.proofs == 2 && report.verified_frames == 4);
    assert(good.step == 1 + 2 * BDW_EDP_PHY_ENTRIES + sizeof(source_suffix) / sizeof(source_suffix[0]));
    assert(report.source_writes == good.step);

    const enum bdw_edp_source_error errors[] = {
        BDW_EDP_SOURCE_RESET_STATE, BDW_EDP_SOURCE_CLOCK, BDW_EDP_SOURCE_CLOCK,
        BDW_EDP_SOURCE_RESET_STATE, BDW_EDP_SOURCE_UNDERRUN, BDW_EDP_SOURCE_UNDERRUN,
        BDW_EDP_SOURCE_UNDERRUN, BDW_EDP_SOURCE_IMAGE, BDW_EDP_SOURCE_IMAGE,
        BDW_EDP_SOURCE_CLOCK, BDW_EDP_SOURCE_CLOCK, BDW_EDP_SOURCE_UNDERRUN,
        BDW_EDP_SOURCE_CLOCK, BDW_EDP_SOURCE_FRAME, BDW_EDP_SOURCE_FRAME,
        BDW_EDP_SOURCE_FRAME, BDW_EDP_SOURCE_UNDERRUN, BDW_EDP_SOURCE_CLOCK, BDW_EDP_SOURCE_PANEL,
    };
    for (unsigned i = 0; i < sizeof(errors) / sizeof(errors[0]); ++i) {
        struct source_fixture f = {.fault = SOURCE_ACTIVE + i};
        assert(run_source(&f, &report) != BDW_EDP_OK);
        assert(report.source_error == errors[i] && !f.panel.backlight);
        if (f.fault <= SOURCE_CLOCK_MISMATCH)
            assert(f.step == 0 && f.panel.writes == 0 && !report.panel.effects_possible);
    }
    for (unsigned i = 1; i <= good.panel.events; ++i) {
        struct source_fixture failed = {.panel.fail_at = i, .panel.uncertain = true};
        assert(run_source(&failed, &report) != BDW_EDP_OK && failed.panel.events == i);
        struct source_fixture lost = {.panel.lost_at = i};
        assert(run_source(&lost, &report) == BDW_EDP_OWNERSHIP && lost.panel.events == i);
        assert(report.source_error == BDW_EDP_SOURCE_OWNER);
        if (i < good.panel.events - 1)
            assert(!failed.panel.backlight && !lost.panel.backlight);
    }
    struct source_fixture wrap = {.fault = SOURCE_COUNTER_WRAP};
    assert(run_source(&wrap, &report) == BDW_EDP_OK);
    assert(report.verified_frames == 4 && wrap.panel.backlight);
    return 2 + good.panel.events * 2 + sizeof(errors) / sizeof(errors[0]);
}

static enum bdw_edp_result run(struct fixture *f, struct bdw_edp_report *report, uint32_t cdclk)
{
    f->time = 1;
    f->aux_setup = 0x24030000u | (cdclk + 1000) / 2000;
    if (f->fault == FOREIGN_BUSY)
        f->control = BUSY;
    const struct bdw_edp_io io = {f, link_held, read, write, now, wait, source};
    if (f->panel) {
        f->cycle_until = 4000;
        if (f->fault == PANEL_ALREADY_LIT)
            f->pp_control = 7;
        const struct bdw_edp_panel_io panel = {io, held, live};
        return intel_bdw_edp_panel_restore(&panel, cdclk, report);
    }
    return intel_bdw_edp_recover(&io, cdclk, report);
}

static unsigned test_panel(void)
{
    struct bdw_edp_report report;
    struct fixture good = {.panel = true};
    assert(run(&good, &report, 450000) == BDW_EDP_OK);
    assert(good.backlight && good.live_checks == 2 && good.pp_writes == 5);
    assert(report.panel_stage == BDW_EDP_PANEL_READY && report.panel_control == 7);
    for (unsigned i = 1; i <= good.events; ++i) {
        struct fixture failed = {.panel = true, .fail_at = i, .uncertain = true};
        assert(run(&failed, &report, 450000) != BDW_EDP_OK);
        assert(failed.events == i && report.panel_stage != BDW_EDP_PANEL_READY);
        if (i < good.events - 1)
            assert(!failed.backlight);
        struct fixture lost = {.panel = true, .lost_at = i};
        assert(run(&lost, &report, 450000) == BDW_EDP_OWNERSHIP);
        assert(lost.events == i && report.panel_stage != BDW_EDP_PANEL_READY);
    }
    const enum bdw_edp_result expected[] = {
        BDW_EDP_PANEL_STATE, BDW_EDP_PANEL_STATE, BDW_EDP_CLOCK, BDW_EDP_PANEL_STATE,
        BDW_EDP_PANEL_STATE, BDW_EDP_CLOCK, BDW_EDP_SCANOUT_FAILED,
        BDW_EDP_SCANOUT_FAILED, BDW_EDP_PANEL_STATE, BDW_EDP_PANEL_STATE,
    };
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        struct fixture f = {.panel = true, .fault = PANEL_ALREADY_ON + i};
        assert(run(&f, &report, 450000) == expected[i]);
        assert(!f.backlight && report.panel_stage != BDW_EDP_PANEL_READY);
        if (i < 3)
            assert(!f.writes && !report.effects_possible);
    }
    struct fixture early = {.panel = true, .fault = PANEL_EARLY_ON};
    assert(run(&early, &report, 450000) == BDW_EDP_OK);
    assert(early.backlight && early.live_checks == 2);
    return 2 + good.events * 2 + sizeof(expected) / sizeof(expected[0]);
}

int main(void)
{
    struct bdw_edp_report report;
    const uint32_t clocks[] = {337500, 450000, 540000, 675000};
    struct fixture good = {0};
    for (unsigned i = 0; i < sizeof(clocks) / sizeof(clocks[0]); ++i) {
        good = (struct fixture){0};
        assert(run(&good, &report, clocks[i]) == BDW_EDP_OK);
        assert(report.stage == BDW_EDP_LINK_READY && report.effects_possible);
        assert(good.requests == sizeof(request_sizes) && good.attempts == good.clears);
        assert(good.idle && good.scanout && good.normal && report.swing == 2 && report.emphasis == 1);
        assert(memcmp(report.caps, caps, sizeof(caps)) == 0);
    }
    struct fixture invalid = {0};
    assert(run(&invalid, &report, 123000) == BDW_EDP_INVALID);
    assert(invalid.events == 0 && !report.effects_possible);

    for (unsigned i = 1; i <= good.events; ++i) {
        for (unsigned uncertain = 0; uncertain <= 1; ++uncertain) {
            struct fixture f = {.fail_at = i, .uncertain = uncertain};
            assert(run(&f, &report, 450000) == BDW_EDP_IO);
            assert(f.events == i && (i > 2 || report.effects_possible == (i == 2 && uncertain)));
        }
        struct fixture lost = {.lost_at = i};
        assert(run(&lost, &report, 450000) == BDW_EDP_OWNERSHIP);
        assert(lost.events == i);
        struct fixture bad_clock = {.clock_after = i};
        assert(run(&bad_clock, &report, 450000) == BDW_EDP_CLOCK);
        assert(bad_clock.events == i);
        struct fixture expired = {.elapsed_after = i};
        assert(run(&expired, &report, 450000) == BDW_EDP_CLOCK);
        assert(expired.events == i);
    }

    const enum bdw_edp_result expected[] = {
        [FOREIGN_BUSY] = BDW_EDP_AUX_BUSY, [STUCK_BUSY] = BDW_EDP_AUX_TIMEOUT,
        [BAD_CONTROLLER_TIMEOUT] = BDW_EDP_AUX_TIMEOUT,
        [RECEIVE_ERROR] = BDW_EDP_AUX_REPLY, [MISSING_DONE] = BDW_EDP_AUX_REPLY,
        [EMPTY_REPLY] = BDW_EDP_AUX_REPLY, [BIG_REPLY] = BDW_EDP_AUX_REPLY,
        [SHORT_ACK] = BDW_EDP_AUX_REPLY, [LONG_ACK] = BDW_EDP_AUX_REPLY,
        [NACK] = BDW_EDP_AUX_REPLY, [BAD_REPLY] = BDW_EDP_AUX_REPLY,
        [DEFER] = BDW_EDP_AUX_DEFER, [UNSUPPORTED_CAP] = BDW_EDP_CAPS,
        [BAD_FINAL_CONFIG] = BDW_EDP_TRAINING, [BAD_FINAL_STATUS] = BDW_EDP_TRAINING,
        [EQ_LOSTS_CLOCK] = BDW_EDP_TRAINING, [EQ_NO_ALIGNMENT] = BDW_EDP_TRAINING,
        [CR_NO_PROGRESS] = BDW_EDP_TRAINING, [EARLY_DELAY] = BDW_EDP_CLOCK,
        [BACKWARD_CLOCK] = BDW_EDP_CLOCK, [FROZEN_CLOCK] = BDW_EDP_CLOCK,
        [OVERDUE_SCANOUT] = BDW_EDP_CLOCK,
    };
    for (unsigned i = FOREIGN_BUSY; i <= OVERDUE_SCANOUT; ++i) {
        struct fixture f = {.fault = i, .defers = 7};
        assert(run(&f, &report, 450000) == expected[i]);
        assert(report.stage != BDW_EDP_LINK_READY);
        if (i == FOREIGN_BUSY)
            assert(!report.effects_possible && !f.writes);
        if (i == STUCK_BUSY)
            assert(report.effects_possible && f.attempts == 1 && f.clears == 0);
        if (i == DEFER)
            assert(f.attempts == 7 && f.requests == 0);
        if (i == CR_NO_PROGRESS || i == EQ_NO_ALIGNMENT || i == EQ_LOSTS_CLOCK)
            assert(!f.scanout);
        if (i == OVERDUE_SCANOUT)
            assert(f.scanout && !f.normal);
    }
    struct fixture deferred = {.fault = DEFER, .defers = 6};
    assert(run(&deferred, &report, 450000) == BDW_EDP_OK);
    assert(deferred.attempts == deferred.requests + 6);
    struct fixture lost_after_send = {.lost_after = 1};
    assert(run(&lost_after_send, &report, 450000) == BDW_EDP_OWNERSHIP);
    assert(lost_after_send.attempts == 1 && !lost_after_send.clears && report.effects_possible);
    printf("Broadwell eDP recovery: %zu cases (%u link injection points)\n",
           sizeof(clocks) / sizeof(clocks[0]) + good.events * 5 +
           sizeof(expected) / sizeof(expected[0]) + 2 + test_panel() + test_source(), good.events);
}
