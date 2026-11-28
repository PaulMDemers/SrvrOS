#include <srvros/intel_bdw_edp.h>
#include <srvros/intel_bdw_edp_source.h>
#include <srvros/intel_bdw_edp_platform.h>
#include <srvros/intel_bdw_edp_baseline.h>
#include <srvros/intel_bdw_display_takeover.h>
#include <srvros/intel_dma_observe.h>
#include <srvros/intel_bdw_traffic.h>
#include <srvros/intel_bdw_flr.h>
#include <srvros/intel_bdw_pci_hold.h>

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
    bool panel, backlight, keep_timings;
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
    SOURCE_NO_CRC, SOURCE_CRC_RETAINED, SOURCE_BACKING_CHANGED, SOURCE_SECOND_CRC_WRONG,
    SOURCE_DDI_LOST_DURING_CRC, SOURCE_NO_REFERENCE, SOURCE_REFERENCE_CHANGED,
};

enum flr_admission_fault {
    DRAIN_WRITE_UNCERTAIN = 26, DRAIN_NO_GLOBAL_ACK, DRAIN_OWNER_LOST,
    DRAIN_NEW_TRANSACTION, DRAIN_STALE_EPOCH, DRAIN_SOURCE_USED,
    DRAIN_BME_ENABLED, DRAIN_BAR_CHANGED, DRAIN_MSI_ENABLED,
    FLR_SHORT_QUIET, DRAIN_ALIAS_RETAINED,
    GT_STALE_L3, GT_FOREIGN_RESET, GT_STALE_ENGINE, GT_BLOCK_UNCERTAIN, GT_REQUEST_OPEN, GT_RESPONSE_OPEN,
    GT_ENGINE_OPEN, GT_ENGINE_IDLE, GT_ENGINE_UNFLUSHED, GT_NO_L3, GT_L3_UNBLOCK,
    GT_ARBITER_BUSY, GT_MEDIA_BUSY, GT_REOPENED, GT_LATE_BLOCK, GT_FLOAT,
    GT_LOST_AT_RESET, GT_CLOSE_UNCERTAIN, GT_BLOCK_CLOCK_FROZEN, GT_AIR_MEDIA_TRACE, GT_READ_FAILED, GT_BLOCK_REFUSED,
    GT_REQUEST_REOPENS_ONCE, GT_RESPONSE_REOPENS_ONCE, GT_ENGINE_LOSES_ACK, GT_ENGINE_RESUMES, GT_ENGINE_GO_REOPEN,
    GT_FOREIGN_FLR_AFTER_BLOCK, GT_OWNER_NOT_REGISTERED, GT_OWNER_REPLACED, GT_OWNER_LOST_AT_DRAIN, GT_REOPEN_AT_CONFIG, GT_PROGRESSIVE_BLOCK,
    DRAIN_IO_REPLACED, DRAIN_SEQUENCE_CHANGED, DRAIN_COMPLETION_LOST, DRAIN_RESULT_OWNER_CHANGED,
};

#define TEST_REFERENCE_CRC 0x5d3928e4u
#define TEST_BACKING_HASH 0x012970c1aee06011ull
#define TEST_PWM_DATA 0x0ad90184u

struct register_write {
    uint32_t offset, value;
};

enum l3_policy_fault { L3_POLICY_OK, L3_POLICY_NARROW, L3_POLICY_OFF, L3_SELECTOR_UNCERTAIN,
                       L3_SELECTOR_UNPOSTED, L3_SELECTOR_DEADLINE };

struct source_fixture {
    struct fixture panel;
    struct bdw_edp_source_image image;
    enum source_fault fault;
    uint32_t clock, transport, buffer, ddi, transcoder, primary, surface;
    uint32_t stride, plane_offset;
    uint32_t crc_ctl, irq_mask, crc_frame;
    bool crc_pending, crc_retired;
    struct register_write posted;
    uint64_t buffer_enabled_at, normal_at;
    unsigned step, proofs;
    uint64_t backlight_off_at, plane_off_at, trans_off_at, panel_off_at;
    uint32_t panel_off_delay_us;
    unsigned flr_fault, flr_writes;
    uint64_t flr_issued;
    uint32_t flr_config[64];
    struct bdw_flr_report flr;
    const struct bdw_flr_report *active_flr;
    struct bdw_flr_drain flr_drain;
    struct intel_vtd_drain drain;
    struct intel_vtd_drain_source drain_source;
    unsigned drain_stores, drain_reads_after_store;
    unsigned block_calls, decode_closes, media_reads, l3_reads, gam_reads;
    unsigned l3_selector, l3_banks[2], l3_fault_bank;
    enum l3_policy_fault l3_fault;
    bool l3_fault_held;
    bool lost_block_owner, reopened_at_config;
    const struct bdw_flr_report *block_owner;
    uint64_t block_epoch;
    uint32_t gt_fault_offset;
    bool drain_alias_retired;
    struct bdw_edp_reopen_report reopen;
    struct bdw_edp_pps_report pps;
    struct bdw_edp_pwm_report pwm;
    struct bdw_edp_pwm_image live_pwm;
    uint32_t display_cdclk, display_gate, well_request;
    uint64_t well_ready_at;
    bool exercise_flr, function_reset, preserve_platform;
    unsigned disable_step, fail_disable_write, mask_writes;
    bool cycle, dark, lose_dark_backing, fail_mask_close, uncertain_mask_open;
};

static const struct register_write disable_steps[] = {
    {PP_CONTROL, 3}, {0x70180, 0x58000400}, {0x7019c, 0}, {0x7f008, 0},
    {0x6f400, 0x00200000}, {0x64000, 0x00010010}, {0x64040, 0x00040340},
    {PP_CONTROL, 0}, {0x46100, 0xe0000000},
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
            assert(!f->pp_control && f->pp_writes == (f->keep_timings ? 0u : 3u) && !f->requests);
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
           image->backing_physical == s->image.backing_physical &&
           image->backing_hash == s->image.backing_hash &&
           image->reference_crc == s->image.reference_crc &&
           image->cdclk_khz == s->image.cdclk_khz &&
           image->ddi_lane_bits == s->image.ddi_lane_bits &&
           image->pch_pwm_data == s->image.pch_pwm_data &&
           memcmp(image->phy, s->image.phy, sizeof(image->phy)) == 0);
    return !s->panel.faulted && held(&s->panel);
}

static bool source_read(void *context, uint32_t offset, uint32_t *word)
{
    struct source_fixture *s = context;
    struct fixture *f = &s->panel;
    if (panel_register(offset)) {
        if (s->cycle && !s->dark && (offset == PP_CONTROL || offset == PP_STATUS)) {
            if (!event(f))
                return false;
            *word = f->pp_control;
            if (offset == PP_STATUS)
                *word = !s->panel_off_at ? 0x80000008u :
                        f->time - s->panel_off_at < s->panel_off_delay_us ? 0xa000000bu : 0x08000001u;
            return true;
        }
        if (!read(f, offset, word))
            return false;
        if (offset == PP_STATUS && s->fault == SOURCE_PANEL_LOST && f->stage == BDW_EDP_EQUALIZE)
            *word = 0;
        return true;
    }
    if (!event(f))
        return false;
    if (offset >= 0x64e00u && offset < 0x64e00u + 8 * BDW_EDP_PHY_ENTRIES) {
        unsigned word_index = (offset - 0x64e00u) / 4;
        unsigned writes = 1 + word_index;
        assert(s->step >= writes + 1);
        *word = s->image.phy[word_index / 2][word_index % 2];
        return true;
    }
    switch (offset) {
    case BDW_EDP_LP_CLOCK_GATE: *word = s->display_gate; break;
    case 0xc2004: *word = 0x24; break;
    case 0x48250: *word = 0; break;
    case BDW_EDP_PWM_CONTROL: *word = s->live_pwm.control; break;
    case BDW_EDP_PWM_DATA:
        *word = s->live_pwm.data;
        if (s->flr_fault == 13 && s->proofs == 2 && s->crc_retired)
            *word ^= 1u;
        break;
    case 0x45400: case BDW_EDP_WELL_DRIVER: case 0x45408: case 0x4540c: {
        bool powered = !s->function_reset || s->preserve_platform ||
                       (s->well_ready_at && f->time >= s->well_ready_at);
        if (s->flr_fault == 25 && s->proofs == 2 && s->crc_retired)
            powered = false;
        *word = powered ? BDW_EDP_WELL_ON : 0;
        if (offset == BDW_EDP_WELL_DRIVER)
            *word |= s->well_request;
        if (offset == 0x45400 && (!s->function_reset || s->preserve_platform))
            *word |= BDW_EDP_WELL_REQUEST;
        if (offset == 0x45408 && s->flr_fault == 17)
            *word |= BDW_EDP_WELL_REQUEST;
        break;
    }
    case 0x130040:
        *word = (s->fault == SOURCE_PLL_UNLOCKED ||
                 (s->fault == SOURCE_CLOCK_LOST_AFTER_PROOF && s->proofs == 2 && s->crc_retired)) ? 0 :
                 (s->exercise_flr && !(s->function_reset && s->flr_fault == 9) ? 0x4c000000u : 0x40000000u);
        break;
    case 0x42014: *word = s->exercise_flr ? 0x20100042u : 0; break;
    case 0x46200:
        *word = s->fault == SOURCE_CDCLK_WRONG ? 450u : s->display_cdclk;
        break;
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
        *word = s->fault == SOURCE_ACTIVE ? 0x82200000u : s->ddi;
        if (s->fault == SOURCE_DDI_LOST_DURING_CRC && s->crc_retired)
            *word = 0;
        break;
    case 0x7f008:
        *word = s->transcoder;
        if ((s->normal_at || (s->cycle && !s->dark && s->trans_off_at &&
                             f->time - s->trans_off_at < 16000)) && s->fault != SOURCE_INACTIVE)
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
    case BDW_EDP_CRC_CTL: *word = s->crc_ctl; break;
    case BDW_EDP_CRC_RES:
        assert(s->crc_pending);
        *word = TEST_REFERENCE_CRC;
        if (s->fault == SOURCE_UNPROVEN_FETCH || (s->fault == SOURCE_SECOND_CRC_WRONG && s->proofs == 2))
            *word ^= 1;
        break;
    case 0x44404: *word = s->irq_mask | (s->fault == SOURCE_MASKED ? 0x80000000u : 0); break;
    case 0x4440c:
        *word = s->fault == SOURCE_IRQ_ENABLED ? 0x80000000u :
                (s->function_reset && s->flr_fault == 8 ? BDW_EDP_CRC_DONE : 0);
        break;
    case 0x44408: {
        uint32_t frame = (uint32_t)((f->time - s->normal_at) / 16667);
        if (s->crc_ctl && frame != s->crc_frame && s->fault != SOURCE_NO_CRC) {
            s->crc_pending = true;
            s->crc_frame = frame;
        }
        *word = (s->fault == SOURCE_STALE_UNDERRUN ||
                 (s->fault == SOURCE_UNDERRUN_DURING_VIDEO && s->normal_at) ||
                 (s->fault == SOURCE_UNDERRUN_AFTER_PROOF && s->proofs == 2 && s->crc_retired))
                ? 0x80000000u : 0;
        if (s->crc_pending)
            *word |= BDW_EDP_CRC_DONE;
        break;
    }
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
    if (offset == BDW_EDP_PIPE_IMR && s->cycle &&
        (s->irq_mask ^ word) == BDW_EDP_FIFO_UNDERRUN) {
        assert(!s->crc_pending && !s->crc_ctl);
        ++s->mask_writes;
        if (s->mask_writes == 2 && s->fail_mask_close)
            return GT_WRITE_UNCERTAIN;
        s->irq_mask = word;
        if (s->mask_writes == 1 && s->uncertain_mask_open)
            return GT_WRITE_UNCERTAIN;
        return GT_WRITE_DONE;
    }
    if (s->cycle && !s->dark) {
        assert(s->disable_step < sizeof(disable_steps) / sizeof(disable_steps[0]));
        if (!event(f) || s->disable_step + 1 == s->fail_disable_write)
            return GT_WRITE_UNCERTAIN;
        const struct register_write *expected = &disable_steps[s->disable_step++];
        assert(offset == expected->offset && word == expected->value);
        switch (offset) {
        case PP_CONTROL:
            f->pp_control = word;
            if (word) {
                f->backlight = false;
                s->backlight_off_at = f->time;
            } else {
                assert(!(s->buffer & 0x80000000u) && !(s->transport & 0x80000000u));
                s->panel_off_at = f->time;
            }
            break;
        case 0x70180:
            assert(f->time - s->backlight_off_at >= 250000);
            s->primary = word;
            break;
        case 0x7019c: s->plane_off_at = f->time; break;
        case 0x7f008:
            assert(f->time / 16667 - s->plane_off_at / 16667 >= 2);
            s->transcoder = 0;
            s->normal_at = 0;
            f->normal = false;
            f->scanout = false;
            s->trans_off_at = f->time;
            break;
        case 0x6f400:
            assert(f->time - s->trans_off_at >= 16000);
            s->ddi = word;
            break;
        case 0x64000:
            s->buffer = word;
            s->buffer_enabled_at = 0;
            break;
        case 0x64040: s->transport = word; break;
        case 0x46100:
            assert(f->time - s->panel_off_at >= s->panel_off_delay_us);
            s->clock = word;
            f->power_requested = 0;
            f->cycle_until = f->time + 500000;
            s->dark = true;
            break;
        default: assert(0);
        }
        return GT_WRITE_DONE;
    }
    if (panel_register(offset)) {
        if (offset == CTL && (word & BUSY) && f->stage == BDW_EDP_CLOCK_RECOVERY)
            assert(f->time - s->buffer_enabled_at >= 519);
        return write(f, offset, word);
    }
    if (!event(f))
        return f->uncertain ? GT_WRITE_UNCERTAIN : GT_WRITE_NONE;
    assert(panel_powered(f) && !f->backlight);
    if (offset == BDW_EDP_PIPE_IMR) {
        assert(!s->crc_ctl && ((s->irq_mask ^ word) & ~BDW_EDP_CRC_DONE) == 0);
        s->irq_mask = word;
        s->crc_retired = (word & BDW_EDP_CRC_DONE) != 0;
        return GT_WRITE_DONE;
    }
    if (offset == BDW_EDP_CRC_CTL) {
        assert(f->normal && f->scanout && !(s->irq_mask & (BDW_EDP_CRC_DONE | BDW_EDP_FIFO_UNDERRUN)) &&
               (s->irq_mask & BDW_EDP_CRC_ERROR));
        assert(word == 0 || word == BDW_EDP_CRC_ENABLE);
        if (word) {
            ++s->proofs;
            ++f->live_checks;
            if (s->proofs == 1)
                f->first_live = f->time;
            else
                assert(s->proofs == 2 && f->time - f->first_live >= 210000);
            s->crc_frame = (uint32_t)((f->time - s->normal_at) / 16667);
        }
        if (!word && s->fault == SOURCE_CRC_RETAINED)
            return GT_WRITE_UNCERTAIN;
        s->crc_ctl = word;
        return GT_WRITE_DONE;
    }
    if (offset == BDW_EDP_PIPE_IIR) {
        assert(word == BDW_EDP_CRC_DONE && s->crc_pending);
        s->crc_pending = false;
        return GT_WRITE_DONE;
    }
    unsigned step = s->step++;
    if (step == 0) {
        assert(offset == 0x46100 && word == 0x20000000 && !(s->transport & 0x80000000u));
        s->clock = word;
    } else if (step <= BDW_EDP_PHY_ENTRIES * 2) {
        assert(!(s->transport & 0x80000000u) && !(s->buffer & 0x80000000u));
        assert(offset == 0x64e00u + (step - 1) * 4);
        assert(word == s->image.phy[(step - 1) / 2][(step - 1) % 2]);
        if (step == BDW_EDP_PHY_ENTRIES * 2)
            f->stage = BDW_EDP_PREPARE_SOURCE;
    } else {
        unsigned i = step - 1 - BDW_EDP_PHY_ENTRIES * 2;
        assert(i < sizeof(source_suffix) / sizeof(source_suffix[0]));
        uint32_t expected = source_suffix[i].value;
        if (s->cycle && !s->function_reset && offset == 0x70180)
            expected |= 1u << 30;
        assert(source_suffix[i].offset == offset && expected == word);
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
        case 0x6f400: s->ddi = word; break;
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
    bool short_warmup = us == 520 && s->fault == SOURCE_WARMUP_SHORT;
    bool short_well_pause = us == 10 && s->flr_fault == 23;
    if (s->flr_fault == FLR_SHORT_QUIET && us == 100000u) us -= 5000u;
    if (s->flr_fault == GT_BLOCK_CLOCK_FROZEN && s->block_calls && us == 1)
        return;
    wait(&s->panel, short_warmup || short_well_pause ? 1 : us);
}

static bool source_backing_hash(void *context, const struct bdw_edp_source_image *image,
                                uint64_t *hash)
{
    struct source_fixture *s = context;
    assert(image->backing == &s->panel);
    assert(s->cycle || (s->panel.normal && !s->panel.backlight));
    *hash = TEST_BACKING_HASH;
    if (s->fault == SOURCE_BACKING_CHANGED && s->crc_retired)
        *hash ^= 1;
    if ((s->lose_dark_backing && s->dark && !s->panel.power_requested) ||
        (s->function_reset && s->flr_fault == 5))
        *hash ^= 1;
    return event(&s->panel);
}

static struct bdw_edp_baseline_report setup_source(struct source_fixture *s)
{
    struct fixture *f = &s->panel;
    f->panel = true;
    f->time = 1;
    f->cycle_until = 4000;
    f->aux_setup = 0x240300e1u;
    s->display_cdclk = s->exercise_flr ? 674u : 449u;
    s->display_gate = 0x1000u;
    s->clock = 0xe0000000u;
    s->buffer = 0x10010u;
    s->irq_mask = BDW_EDP_CRC_IRQS;
    s->image = (struct bdw_edp_source_image){
        .backing = f, .epoch = 41, .mapped_bytes = 904ull * 6144 + 128ull * 4096,
        .backing_physical = 0x8c001000u,
        .backing_hash = TEST_BACKING_HASH, .reference_crc = TEST_REFERENCE_CRC,
        .cdclk_khz = s->exercise_flr ? 675000u : 450000u, .ddi_lane_bits = 0x10010,
        .pch_pwm_data = s->exercise_flr && s->flr_fault != 14 ? TEST_PWM_DATA : 0,
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
    if (s->fault == SOURCE_NO_REFERENCE)
        s->image.reference_crc = 0;
    struct bdw_edp_baseline_report reference = {
        .status = BDW_EDP_BASELINE_OK,
        .sample = {.port = 0x20000000u, .buffer = 0x80010010u, .transport = 0x80040340u,
                   .backing_hash = TEST_BACKING_HASH},
        .frame = {100, 101, 102, 103},
        .crc = {0, 0, TEST_REFERENCE_CRC, TEST_REFERENCE_CRC},
        .poll_iir = BDW_EDP_CRC_DONE, .events = 4, .writes = 8, .restored = true,
    };
    memcpy(reference.sample.phy, s->image.phy, sizeof(reference.sample.phy));
    if (s->fault == SOURCE_REFERENCE_CHANGED)
        reference.sample.backing_hash ^= 1;
    return reference;
}

static struct bdw_edp_source_io source_io(struct source_fixture *s)
{
    return (struct bdw_edp_source_io){
        s, source_held, source_read, source_write, source_now, source_delay, source_backing_hash,
    };
}

static enum bdw_edp_result run_source(struct source_fixture *s, struct bdw_edp_source_report *report)
{
    struct bdw_edp_baseline_report reference = setup_source(s);
    const struct bdw_edp_source_io io = source_io(s);
    return intel_bdw_edp_source_restore(&io, &s->image,
        s->fault == SOURCE_NO_REFERENCE ? NULL : &reference, report);
}

static bool flr_held(void *context)
{
    struct source_fixture *s = context;
    return s->dark && held(&s->panel) && !s->panel.backlight;
}

static bool flr_admitted(void *context, const struct bdw_flr_pci_image *image,
                         const struct bdw_flr_drain *child)
{
    struct source_fixture *s = context;
    assert(image->bar[0] == 0xc0000004 && image->bar[2] == 0xb000000c);
    return child == &s->flr_drain && child->attempt == s->active_flr &&
           !s->lost_block_owner && s->flr_fault != 1 && child->result == &s->drain && child->source == &s->drain_source &&
           child->source->binding.lifetime == &s->image && child->source->binding.epoch == s->image.epoch &&
           (s->flr_fault != DRAIN_OWNER_LOST || !s->drain.complete);
}

/* Modeled GPM transaction. L3 first reads low under closure, then pulses
 * and clears before the five engines finish the handshake. */
static bool flr_gt_read(void *context, uint32_t offset, uint32_t *word)
{
    struct source_fixture *s = context;
    assert(flr_held(s) && s->flr_config[1] == 0x100403 && !s->decode_closes);
    bool block = s->block_calls != 0;
    bool ready = block && s->l3_reads >= 3;
    unsigned bank = (s->l3_selector >> 24) & 7u;
    bool l3_fault = block == s->l3_fault_held && bank == s->l3_fault_bank;
    if (block && offset == 0x40d0) ++s->gam_reads;
    if (block && s->flr_fault == GT_READ_FAILED && offset == 0x8038) return false;
    /* Replay the observed pre/held response words from Air release
     * 8e5b856650ecf306. The request register was not in that recording.
     * Leave it modeled, even granting the hypothetical request success. */
    if (s->flr_fault == GT_AIR_MEDIA_TRACE) {
        switch (offset) {
        case 0x8028: *word = 0x7f; return true;
        case 0x8000: *word = 0xc00010; return true;
        case 0x8004: case 0x8008: *word = 0x10; return true;
        case 0x800c: case 0x8010: *word = 0x1f; return true;
        case 0x8038: *word = 0; return true;
        case 0x8040: *word = 1; return true;
        }
    }
    switch (offset) {
    case 0x9120: *word = 3u << 25; break;
    case 0x138064: *word = l3_fault && s->l3_fault == L3_POLICY_OFF ? 1u : 3u; break;
    case 0xfdc: *word = s->l3_selector; break;
    case 0xb11c:
        s->l3_banks[block] |= 1u << bank;
        *word = l3_fault && s->l3_fault == L3_POLICY_NARROW ? 0x11u : 1u;
        break;
    case 0x8108: *word = 0; break;
    case 0x8014:
        *word = !block || (s->flr_fault == GT_PROGRESSIVE_BLOCK && s->gam_reads == 1) ? 0x10 : 0;
        break;
    case 0x40d0: case 0x8028:
        *word = block ? 0x20 : 0x7f;
        if (!block && offset == 0x40d0 && s->flr_fault == GT_FOREIGN_RESET) *word |= 0x8000;
        if (block && ((offset == 0x40d0 && s->flr_fault == GT_REQUEST_OPEN) ||
            (offset == 0x8028 && (s->flr_fault == GT_RESPONSE_OPEN ||
            (s->flr_fault == GT_REOPENED && s->media_reads == 4))))) *word |= 0x40;
        if (block && s->l3_reads == 1 &&
            ((s->flr_fault == GT_REQUEST_REOPENS_ONCE && offset == 0x40d0) ||
             (s->flr_fault == GT_RESPONSE_REOPENS_ONCE && offset == 0x8028)))
            *word |= s->gt_fault_offset;
        if (block && s->flr_fault == GT_PROGRESSIVE_BLOCK && s->l3_reads == 0)
            *word |= offset == 0x40d0 ? 1 : 0x41;
        if (s->reopened_at_config && offset == 0x8028) *word |= 1;
        if (block && s->flr_fault == GT_FOREIGN_FLR_AFTER_BLOCK && offset == 0x40d0)
            *word |= 0x8000u;
        break;
    case 0x8000: case 0x8004: case 0x8008: case 0x800c: case 0x8010:
        *word = ready ? 0x20 : 0x10;
        if (!block && s->flr_fault == GT_STALE_ENGINE) *word |= 0x20;
        if (block && offset == s->gt_fault_offset &&
            (s->flr_fault == GT_ENGINE_LOSES_ACK || s->flr_fault == GT_ENGINE_RESUMES)) {
            *word = s->l3_reads == 2 ? (s->flr_fault == GT_ENGINE_LOSES_ACK ? 0x10 : 0x21) : 0x20;
        }
        if (block && s->flr_fault == GT_ENGINE_GO_REOPEN && offset == s->gt_fault_offset)
            *word = s->l3_reads == 1 ? 0 : s->l3_reads == 2 ? 0x10 : 0x20;
        if (block && s->flr_fault == GT_PROGRESSIVE_BLOCK && offset == 0x8004) *word = 0x20;
        if (ready && offset == s->gt_fault_offset) {
            if (s->flr_fault == GT_ENGINE_OPEN) *word |= 0x10;
            if (s->flr_fault == GT_ENGINE_IDLE) *word |= 1;
            if (s->flr_fault == GT_ENGINE_UNFLUSHED) *word &= ~0x20u;
        }
        break;
    case 0x8038:
        *word = block ? (++s->l3_reads == 2 && s->flr_fault != GT_NO_L3) : s->flr_fault == GT_STALE_L3;
        if (block && s->flr_fault == GT_PROGRESSIVE_BLOCK)
            *word = s->l3_reads == 3;
        if (block && s->flr_fault == GT_OWNER_REPLACED && s->l3_reads == 2)
            ++s->block_epoch;
        if (block && s->flr_fault == GT_L3_UNBLOCK && s->media_reads == 4) *word = 2;
        break;
    case 0x8040:
        *word = s->flr_fault != GT_ARBITER_BUSY;
        break;
    case 0x12870: case 0x12878: case 0x1c870: case 0x1c878:
        assert(ready);
        ++s->media_reads;
        *word = 0x40001001u;
        if (s->flr_fault == GT_MEDIA_BUSY && offset == s->gt_fault_offset) *word |= 1u << 16;
        break;
    default: assert(false); return false;
    }
    if (block && s->flr_fault == GT_FLOAT) *word = UINT32_MAX;
    return true;
}

static enum intel_gt_write flr_select_l3(void *context, uint32_t expected, uint32_t target)
{
    struct source_fixture *s = context;
    assert(flr_held(s) && s->flr_config[1] == 0x100403 && !s->decode_closes);
    assert(expected == s->l3_selector && ((expected ^ target) & 0xf0ffffffu) == 0);
    bool fault = (s->block_calls != 0) == s->l3_fault_held && ((target >> 24) & 7u) == s->l3_fault_bank;
    if (fault && s->l3_fault == L3_SELECTOR_UNPOSTED) return GT_WRITE_DONE;
    s->l3_selector = target;
    if (fault && s->l3_fault == L3_SELECTOR_DEADLINE) s->panel.time += BDW_TRAFFIC_BLOCK_US;
    return fault && s->l3_fault == L3_SELECTOR_UNCERTAIN ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static bool flr_block_held(void *context, const struct bdw_flr_report *request)
{
    struct source_fixture *s = context;
    return flr_held(s) && request == s->block_owner && s->block_epoch == s->image.epoch &&
           request == s->flr_drain.attempt && !s->lost_block_owner;
}

static enum intel_gt_write flr_gt_block(void *context, const struct bdw_flr_report *request)
{
    struct source_fixture *s = context;
    assert(flr_held(s) && s->flr_config[1] == 0x100403 && !s->block_calls);
    ++s->block_calls;
    assert(request == s->flr_drain.attempt && !s->block_owner);
    if (s->flr_fault != GT_OWNER_NOT_REGISTERED && s->flr_fault != GT_BLOCK_REFUSED) {
        s->block_owner = request;
        s->block_epoch = s->image.epoch;
    }
    if (s->flr_fault == GT_LATE_BLOCK) s->panel.time += 10000;
    if (s->flr_fault == DRAIN_IO_REPLACED) s->flr_drain.io.pause = NULL;
    if (s->flr_fault == GT_BLOCK_REFUSED) return GT_WRITE_NONE;
    return s->flr_fault == GT_BLOCK_UNCERTAIN ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static bool drain_held(void *context, const struct intel_vtd_drain_source *source)
{
    struct source_fixture *s = context;
    return flr_block_held(s, s->active_flr) && !s->drain_alias_retired && source == &s->drain_source && source->consumer == &s->drain &&
           source->binding.lifetime == &s->image && source->binding.epoch == s->image.epoch &&
           s->active_flr->traffic_blocked && !s->lost_block_owner && s->decode_closes == 1 &&
           !s->flr_writes && s->flr_config[1] == 0x100400;
}

static bool drain_read(void *context, uint32_t offset, uint32_t *word)
{
    struct source_fixture *s = context;
    assert(drain_held(s, &s->drain_source));
    switch (offset) {
    case 0:
        *word = 0x10;
        if (s->flr_fault == GT_OWNER_LOST_AT_DRAIN) s->block_owner = NULL;
        break;
    case 8: *word = (uint32_t)INTEL_VTD_DRAIN_CAP; break;
    case 12: *word = INTEL_VTD_DRAIN_CAP >> 32; break;
    case 16: *word = (uint32_t)INTEL_VTD_DRAIN_ECAP; break;
    case 20: *word = INTEL_VTD_DRAIN_ECAP >> 32; break;
    case 44: *word = 0x08000000; break;
    case 28: case 32: case 36: case 40: case 52: case 0x508: *word = 0; break;
    case 0x50c:
        *word = 0x02000000;
        if (s->drain_stores) {
            ++s->drain_reads_after_store;
            *word = s->drain_reads_after_store == 1 ? INTEL_VTD_DRAIN_REQUEST : 0x12030000;
            if (s->flr_fault == DRAIN_NO_GLOBAL_ACK) *word = 0x10030000; /* no global acknowledgement */
            if (s->flr_fault == DRAIN_NEW_TRANSACTION) s->flr_config[0xa8 / 4] = 0x100;
            if (s->flr_fault == DRAIN_BAR_CHANGED) s->flr_config[0x10 / 4] = 0xd0000004;
            if (s->flr_fault == DRAIN_MSI_ENABLED) s->flr_config[0x90 / 4] |= 1u << 16;
            if (s->flr_fault == DRAIN_BME_ENABLED) s->flr_config[1] |= 4; /* requester escapes its hold */
        }
        break;
    default: assert(false); return false;
    }
    return true;
}

static enum intel_gt_write drain_write(void *context, uint32_t offset, uint32_t word)
{
    struct source_fixture *s = context;
    assert(drain_held(s, &s->drain_source) && !s->drain_stores);
    assert(offset == 0x50c && word == INTEL_VTD_DRAIN_REQUEST);
    ++s->drain_stores;
    return s->flr_fault == DRAIN_WRITE_UNCERTAIN ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static void drain_pause(void *context)
{
    source_delay(context, 1);
}

static bool drain_retire_alias(void *context, const struct intel_vtd_drain_source *source,
                               const struct intel_vtd_drain *result)
{
    struct source_fixture *s = context;
    assert(drain_held(s, source) && result == &s->drain && result->source == source);
    assert(result->complete && !result->retained && result->p.final == 0x1203000000000000ull);
    s->drain_alias_retired = s->flr_fault != DRAIN_ALIAS_RETAINED;
    if (s->flr_fault == DRAIN_SEQUENCE_CHANGED) ++s->drain_source.binding.sequence;
    if (s->flr_fault == DRAIN_COMPLETION_LOST) s->drain.complete = false;
    if (s->flr_fault == DRAIN_RESULT_OWNER_CHANGED) s->drain.binding.root += 4096;
    return s->drain_alias_retired;
}

static bool flr_read(void *context, uint8_t offset, uint32_t *word)
{
    struct source_fixture *s = context;
    if (s->flr_issued && (s->flr_fault == 2 || s->panel.time - s->flr_issued < 102000u)) {
        assert(offset == 0 && s->panel.time - s->flr_issued >= 100000u);
        *word = UINT32_MAX;
    } else {
        *word = s->flr_config[offset / 4];
        if (offset == 0xa8 && s->l3_reads >= 2 && !s->decode_closes && s->flr_fault == GT_REOPEN_AT_CONFIG)
            s->reopened_at_config = true;
        if (offset == 0xa8 && s->drain_alias_retired && !s->flr_issued && s->flr_fault == GT_LOST_AT_RESET)
            s->lost_block_owner = true;
    }
    return true;
}

static enum intel_gt_write flr_write(void *context, uint8_t offset, unsigned bytes, uint32_t word)
{
    struct source_fixture *s = context;
    if (offset == 4 && word == 0x400) {
        assert(s->active_flr->traffic_blocked && s->active_flr->engine_acks == 31u &&
               s->active_flr->l3_ack && s->block_calls == 1 && s->media_reads == 4);
        assert(s->active_flr->l3_policy_checks == 2 && !s->active_flr->l3_selector_retained &&
               s->l3_selector == 0x10000000 && s->l3_banks[0] == 0xff && s->l3_banks[1] == 0xff);
        assert(s->flr_config[1] == 0x100403 && bytes == 2 && !s->decode_closes);
        ++s->decode_closes;
        s->flr_config[1] = 0x100400;
        return s->flr_fault == GT_CLOSE_UNCERTAIN ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
    }
    static const unsigned sequence[] = {0xa8, 0x2c, 0x10, 0x18, 4};
    assert(s->flr_writes < sizeof(sequence) / sizeof(sequence[0]));
    assert(offset == sequence[s->flr_writes++]);
    if (offset == 0xa8) {
        assert(word == 1 && bytes == 1 && !s->flr_issued);
        assert(s->drain_alias_retired && s->drain_stores == 1 && s->drain.complete && !s->drain.retained &&
               s->drain.end <= s->panel.time && s->drain.source == &s->drain_source);
        if (s->flr_fault == FLR_SHORT_QUIET) s->panel.time += 5000u;
        s->flr_issued = s->panel.time;
        s->flr_config[1] = 0x00100000;
        s->flr_config[0x2c / 4] = 0;
        s->flr_config[0x10 / 4] = 4;
        s->flr_config[0x18 / 4] = s->flr_fault == 3 ? 0xa000000c : 12;
        s->panel.pp_delay[0] = 0;
        s->panel.pp_delay[1] = 0;
        s->panel.cycle_until = s->panel.time + 500000;
        s->live_pwm = (struct bdw_edp_pwm_image){0};
        s->well_request = 0;
        if (!s->preserve_platform) {
            s->display_cdclk = s->flr_fault == 20 ? 539u : 449u;
            s->display_gate = s->flr_fault == 22 ? 0x100u : 0;
        }
        return s->flr_fault == 4 ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
    }
    assert(s->panel.time - s->flr_issued >= 102000);
    assert(!(s->flr_config[1] & 7) && bytes == (offset == 4 ? 2u : 4u));
    if (offset == 4) {
        assert(word == 0x402u);
        word |= 0x00100000u;
    }
    s->flr_config[offset / 4] = word;
    return GT_WRITE_DONE;
}

static bool resources_held(void *context)
{
    struct source_fixture *s = context;
    return flr_held(s) && s->function_reset && (s->flr_config[1] & 0xffffu) == 0x0402 &&
           (s->flr_fault != 24 || !s->well_request);
}

static bool powered_resources_held(void *context)
{
    const struct source_fixture *s = context;
    return resources_held(context) && s->reopen.phase == BDW_EDP_REOPEN_READY && s->well_request &&
           (s->preserve_platform || s->panel.time >= s->well_ready_at);
}

static enum intel_gt_write resource_write(void *context, uint32_t offset, uint32_t word)
{
    struct source_fixture *s = context;
    struct fixture *f = &s->panel;
    assert(resources_held(s) && !f->pp_control && !f->power_requested);
    switch (offset) {
    case BDW_EDP_WELL_DRIVER:
        assert(word == BDW_EDP_WELL_REQUEST && !s->well_request);
        s->well_request = word;
        s->well_ready_at = f->time + (s->flr_fault == 18 ? 2000 : 40);
        return s->flr_fault == 19 ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
    case BDW_EDP_CDCLK_FREQ:
        assert(s->well_request && word == 674u && s->display_cdclk == 449u);
        s->display_cdclk = s->flr_fault == 21 ? 449u : word;
        break;
    case BDW_EDP_LP_CLOCK_GATE:
        assert(s->display_cdclk == 674u && word == 0x1000u && !s->display_gate);
        s->display_gate = s->flr_fault == 12 ? 0 : word;
        break;
    case PP_ON_DELAYS: case PP_OFF_DELAYS: case PP_DIVISOR:
        assert(s->reopen.phase == BDW_EDP_REOPEN_READY);
        assert(f->time >= f->cycle_until);
        f->pp_delay[(offset - PP_ON_DELAYS) / 4] = word ^ (s->flr_fault == 10);
        break;
    case BDW_EDP_PWM_CONTROL:
        assert(s->pps.complete && !s->pps.retained);
        s->live_pwm.control = word;
        break;
    case BDW_EDP_PWM_DATA:
        assert(s->pps.complete && !s->pps.retained);
        s->live_pwm.data = word ^ (s->flr_fault == 11);
        break;
    default:
        assert(false);
    }
    return GT_WRITE_DONE;
}

static void setup_reset(struct source_fixture *s, const struct bdw_edp_source_image *image,
                       const struct bdw_flr_report *report, struct bdw_display_takeover_reset *out)
{
    assert(image->backing_hash == TEST_BACKING_HASH && s->dark);
    s->active_flr = report;
    s->l3_selector = 0x10000000;
    const struct bdw_flr_pci_image pci = {{0xc0000004, 0, 0xb000000c, 0}, 0x011b106b};
    s->flr_config[0] = 0x16268086;
    s->flr_config[1] = 0x00100403;
    s->flr_config[2] = 0x03000009;
    s->flr_config[0x2c / 4] = pci.subsystem;
    s->flr_config[0x34 / 4] = 0x90;
    s->flr_config[0x90 / 4] = 0xd005;
    s->flr_config[0xa4 / 4] = 0x03060013;
    if (s->flr_fault == 6)
        s->flr_config[0xa8 / 4] = 0x100;
    s->flr_config[0xd0 / 4] = 0x2a401;
    for (unsigned i = 0; i < 4; ++i)
        s->flr_config[4 + i] = pci.bar[i];
    const struct bdw_flr_io io = {
        s, flr_held, flr_admitted, flr_read, flr_write, source_now, source_delay, flr_gt_read, flr_gt_block, flr_block_held,
        flr_select_l3,
    };
    /* The fixture registers the child to the same display epoch and requester
     * before handing off. The actual remapper core issues and polls its store. */
    s->drain_source = (struct intel_vtd_drain_source){
        .object = &s->drain_source,
        .binding = {.owner = s, .provider = s, .lifetime = image, .root = 0x1000,
                    .epoch = image->epoch, .sequence = 1, .hash = INTEL_VTD_DRAIN_HASH,
                    .unit = INTEL_VTD_DRAIN_UNIT, .vendor = 0x8086, .device = 0x1626,
                    .prepare_begin = s->panel.time, .gt_begin = s->panel.time,
                    .hda_begin = s->panel.time, .outer_begin = s->panel.time},
    };
    if (s->flr_fault == DRAIN_STALE_EPOCH) ++s->drain_source.binding.epoch;
    if (s->flr_fault == DRAIN_SOURCE_USED) s->drain_source.consumer = &s->drain;
    s->flr_drain = (struct bdw_flr_drain){
        .source = &s->drain_source, .result = &s->drain,
        .io = {s, drain_held, drain_read, drain_write, source_now, drain_pause}, .retire_alias = drain_retire_alias
    };
    *out = (struct bdw_display_takeover_reset){.io=io,.pci=pci,.drain=&s->flr_drain};
}

static bool recover_reset(struct source_fixture *s, struct bdw_edp_pps_image timings,
                          struct bdw_edp_pwm_image pwm)
{
    s->function_reset = true;
    s->irq_mask = s->flr_fault == 7 ? 0x7fffffffu : UINT32_MAX;
    if (s->flr_fault == 15)
        return true; /* Provider incorrectly claims platform readiness. */
    const struct bdw_edp_dark_io reopen_io = {s, resources_held, source_read, resource_write, source_now, source_delay};
    const struct bdw_edp_dark_io pps_io = {s, powered_resources_held, source_read, resource_write, source_now, source_delay};
    const struct bdw_edp_pwm_io pwm_io = {s, powered_resources_held, source_read, resource_write, source_now};
    if (!intel_bdw_edp_reopen_resources(&reopen_io, &s->reopen) ||
        !intel_bdw_edp_pps_restore(&pps_io, timings, false, &s->pps) ||
        !intel_bdw_edp_pwm_restore(&pwm_io, pwm, false, &s->pwm)) return false;
    s->panel.aux_setup = 0x24030152u;
    return true;
}

static enum bdw_edp_dark_result source_reset(void *context, const struct bdw_edp_source_image *image)
{
    struct source_fixture *s = context;
    const struct bdw_edp_pps_image timings = {s->panel.pp_delay[0], s->panel.pp_delay[1], s->panel.pp_delay[2]};
    const struct bdw_edp_pwm_image pwm = s->live_pwm;
    struct bdw_display_takeover_reset reset;
    setup_reset(s,image,&s->flr,&reset);
    const struct bdw_flr_io io = reset.io;
    const struct bdw_flr_pci_image pci = reset.pci;
    if (!intel_bdw_flr(&io, &pci, &s->flr_drain, &s->flr)) {
        if (s->flr_fault == GT_BLOCK_UNCERTAIN) {
            struct bdw_flr_report second = {0};
            assert(!intel_bdw_flr(&io, &pci, &s->flr_drain, &s->flr));
            assert(!intel_bdw_flr(&io, &pci, &s->flr_drain, &second));
            assert(s->flr.retained && !second.block_issued && s->block_calls == 1);
            struct bdw_flr_drain copy = s->flr_drain;
            copy.attempt = NULL;
            assert(!intel_bdw_flr(&io, &pci, &copy, &second) && s->block_calls == 1);
        }
        return BDW_EDP_DARK_REFUSED;
    }
    return recover_reset(s,timings,pwm) ? BDW_EDP_DARK_RESET_RECOVERED : BDW_EDP_DARK_REFUSED;
}

static struct bdw_edp_baseline_report setup_cycle(struct source_fixture *s)
{
    const struct bdw_edp_baseline_report reference = setup_source(s);
    s->cycle = true;
    if (!s->panel_off_delay_us)
        s->panel_off_delay_us = 324000u;
    s->panel.keep_timings = true;
    s->panel.pp_delay[0] = BDW_EDP_PPS_ON_AIR72;
    s->panel.pp_delay[1] = BDW_EDP_PPS_OFF_AIR72;
    s->panel.pp_delay[2] = BDW_EDP_PPS_DIV_AIR72;
    s->live_pwm = (struct bdw_edp_pwm_image){0xc0000000u, TEST_PWM_DATA};
    s->well_request = BDW_EDP_WELL_REQUEST;
    if (s->flr_fault == 16)
        s->live_pwm.data ^= 1u;
    s->irq_mask = UINT32_MAX;
    s->panel.time = 300000;
    s->panel.power_requested = 1;
    s->panel.normal = s->panel.scanout = s->panel.backlight = true;
    s->panel.pp_control = 7;
    s->clock = reference.sample.port;
    s->buffer = reference.sample.buffer;
    s->transport = reference.sample.transport;
    s->buffer_enabled_at = s->normal_at = 1;
    s->ddi = 0x82200000u;
    s->transcoder = 0x80000000u;
    s->primary = 0xd8000400u;
    s->stride = 6144;
    return reference;
}

static bool run_cycle(struct source_fixture *s, struct bdw_edp_cycle_report *report)
{
    const struct bdw_edp_baseline_report reference = setup_cycle(s);
    const struct bdw_edp_source_io io = source_io(s);
    return intel_bdw_edp_source_cycle(&io, &s->image, &reference,
                                      s->exercise_flr ? source_reset : NULL, s, report);
}

enum { TAKEOVER_SLOT = 16385, TAKEOVER_PTE = 0x800000 + 8 * TAKEOVER_SLOT };
static const uint64_t takeover_scratch = 0x2100000, takeover_private = 0x2200000;
static const uint64_t takeover_mmio = 0xc0000000, takeover_aperture = 0xb0000000;
static const uint64_t takeover_bytes = 0x10000000, takeover_fence = 0x2200000;
static const uint64_t takeover_root = 0x1000;
static const uint64_t scratch_marker[2] = {0x53524348, 0x48161834};
static const uint64_t private_marker[2] = {0x53525652, 0x37189634};
static struct bdw_display_takeover takeover;

struct takeover_fixture {
    struct source_fixture display;
    struct display_access_token lease;
    struct scanout_interval occupied;
    struct bdw_edp_pps_image timings;
    struct bdw_edp_pwm_image pwm;
    const char *mode;
    uint64_t epoch, translation;
    volatile uint64_t pte;
    unsigned grants, dark_reads, stores, invalidates, apertures;
    bool recovery_loan_tested;
};

static bool takeover_is(const struct takeover_fixture *f, const char *mode)
{
    return !strcmp(f->mode,mode);
}

static bool takeover_reset(void *context, const struct bdw_edp_source_image *image,
                           const struct bdw_flr_report *report, struct bdw_display_takeover_reset *reset)
{
    struct takeover_fixture *f = context;
    assert(!display_access_release(&f->lease,true));
    if (takeover_is(f,"takeover-reset-declined")) return false;
    struct source_fixture *s = &f->display;
    f->timings = (struct bdw_edp_pps_image){s->panel.pp_delay[0],s->panel.pp_delay[1],s->panel.pp_delay[2]};
    f->pwm = s->live_pwm;
    setup_reset(s,image,report,reset);
    return true;
}

static bool takeover_recover(void *context, const struct bdw_edp_source_image *image,
                             const struct bdw_flr_report *report)
{
    struct takeover_fixture *f = context;
    assert(f->display.active_flr == report && report->reset_issued && report->traffic_blocked &&
           report->l3_ack && f->display.drain_alias_retired && display_access_held(&f->lease));
    assert(image == &f->display.image);
    return recover_reset(&f->display,f->timings,f->pwm);
}

static bool takeover_range(void *context, const struct bdw_edp_source_image *image,
                           const struct bdw_flr_report *report, struct bdw_display_takeover_grant *out)
{
    struct takeover_fixture *f = context;
    assert(f->display.dark && f->display.function_reset && !f->display.panel.backlight &&
           !f->display.proofs && display_access_held(&f->lease));
    assert(report == f->display.active_flr && report->stage == BDW_FLR_COMPLETE &&
           f->display.drain.complete && f->display.drain_alias_retired);
    ++f->grants;
    if (takeover_is(f,"takeover-no-grant")) return false;
    f->epoch = image->epoch + 1;
    *out = (struct bdw_display_takeover_grant){.scratch=takeover_scratch,
        .binding={.driver=f,.epoch=f->epoch,.root=takeover_root,.mmio=takeover_mmio,.aperture=takeover_aperture,
            .aperture_bytes=takeover_bytes,.fence_end=takeover_fence,.display=f->lease,
            .index=TAKEOVER_SLOT,.plane=0x98000400u,.backing=takeover_scratch,
            .marker={scratch_marker[0],scratch_marker[1]}},.private_backing=takeover_private,
        .private_marker={private_marker[0],private_marker[1]}};
    if (takeover_is(f,"takeover-overlap")) out->binding.index = f->occupied.pages.first;
    if (takeover_is(f,"takeover-foreign-lease")) ++out->binding.display.epoch;
    if (takeover_is(f,"takeover-epoch-replay")) out->binding.epoch = image->epoch;
    if (takeover_is(f,"takeover-root-drift")) out->binding.root += 4096;
    if (takeover_is(f,"takeover-bar-drift")) out->binding.mmio += 0x1000000;
    if (takeover_is(f,"takeover-rmrr-scratch")) out->binding.backing = out->scratch = 0x8f005000u;
    if (takeover_is(f,"takeover-rmrr-private")) out->private_backing = INTEL_DMA_RMRR_END - 4096;
    if (takeover_is(f,"takeover-backing-scratch")) out->binding.backing = out->scratch = image->backing_physical;
    if (takeover_is(f,"takeover-backing-private"))
        out->private_backing = image->backing_physical + image->mapped_bytes - 4096;
    return true;
}

static bool takeover_read_display(void *context, uint32_t offset, uint32_t *word)
{
    struct takeover_fixture *f = context;
    if (takeover_is(f,"takeover-range-protected") && f->grants && !f->recovery_loan_tested) {
        struct scanout_interval competing = {0};
        const struct scanout_space_range slot = {TAKEOVER_SLOT,1};
        struct scanout_interval *range = &takeover.driver_range;
        assert(!f->display.proofs && !f->stores && !range->sealed);
        scanout_interval_release(range);
        assert(scanout_interval_held(range,takeover_mmio,takeover_aperture,takeover_bytes,takeover_fence,slot));
        assert(!scanout_interval_borrow_driver(&competing,range,f,slot));
        f->recovery_loan_tested = true;
    }
    return source_read(context,offset,word);
}

static uint64_t takeover_now(void *context)
{
    struct takeover_fixture *f = context;
    return ++f->display.panel.time;
}

static bool takeover_prove(void *context, const struct bdw_boot_page_source *source,
                           struct bdw_boot_page_proof *proof)
{
    struct takeover_fixture *f = context;
    struct source_fixture *s = &f->display;
    assert(s->function_reset && s->panel.backlight && s->proofs == 2 && s->panel.scanout &&
           display_access_held(&f->lease) && !display_access_release(&f->lease,true));
    uint32_t rights = BDW_PAGE_REQUIRED;
    if (takeover_is(f,"takeover-stale-epoch") && !f->stores) rights &= ~BDW_PAGE_PRIOR_ACCESS_RETIRED;
    assert(scanout_interval_held(&takeover.recovery_loan,takeover_mmio,takeover_aperture,takeover_bytes,
                                 takeover_fence,(struct scanout_space_range){TAKEOVER_SLOT,1}));
    *proof = (struct bdw_boot_page_proof){.source=source,.driver=f,.driver_range=&takeover.driver_range,
        .scratch=takeover_scratch,.epoch=f->epoch,.root=takeover_root,.at=s->panel.time,.index=TAKEOVER_SLOT,
        .rights=rights,.plane=s->primary,.programmed=0,.live=0};
    return true;
}

static bool takeover_read(void *context, uint32_t offset, uint64_t *word)
{
    struct takeover_fixture *f = context;
    assert(offset == TAKEOVER_PTE);
    if (!f->display.proofs) {
        assert(f->display.dark && !f->display.panel.backlight && !f->stores && !f->dark_reads++);
        if (takeover_is(f,"takeover-dark-read-error")) return false;
    } else if (!f->stores && takeover_is(f,"takeover-late-framebuffer-alias")) {
        f->pte = 0x8c001001u;
    }
    *word = f->pte;
    return true;
}

static enum intel_gt_write takeover_exchange(void *context, uint32_t offset, uint64_t expected, uint64_t replacement)
{
    struct takeover_fixture *f = context;
    const uint64_t ptes[] = {0,takeover_scratch|1,takeover_private|1,takeover_scratch|1};
    assert(offset == TAKEOVER_PTE && f->stores < 3 && f->stores == f->invalidates && takeover.driver_range.sealed);
    assert(expected == ptes[f->stores] && replacement == ptes[f->stores+1]);
    ++f->stores;
    return intel_bdw_ggtt_exchange(&f->pte,expected,replacement);
}

static bool takeover_invalidate(void *context)
{
    struct takeover_fixture *f = context;
    assert(++f->invalidates == f->stores);
    if (f->stores != 3 || !takeover_is(f,"takeover-stale-rollback")) f->translation = f->pte;
    return true;
}

static bool takeover_aperture_read(void *context, uint64_t address, uint64_t words[2])
{
    struct takeover_fixture *f = context;
    assert(address == takeover_aperture + 4096ull * TAKEOVER_SLOT && f->stores == f->invalidates);
    const uint64_t *marker = f->translation == (takeover_scratch | 1) ? scratch_marker : private_marker;
    assert(f->translation == (takeover_scratch | 1) || f->translation == (takeover_private | 1));
    words[0] = marker[0];
    words[1] = marker[1];
    ++f->apertures;
    return true;
}

static int test_takeover(const char *mode)
{
    struct takeover_fixture f = {.display={.exercise_flr=true},.mode=mode};
    if (takeover_is(&f,"takeover-framebuffer-alias")) f.pte = 0x8c001001u;
    if (takeover_is(&f,"takeover-traffic-open")) f.display.flr_fault = GT_AIR_MEDIA_TRACE;
    if (takeover_is(&f,"takeover-drain-io")) f.display.flr_fault = DRAIN_IO_REPLACED;
    if (takeover_is(&f,"takeover-drain-sequence")) f.display.flr_fault = DRAIN_SEQUENCE_CHANGED;
    if (takeover_is(&f,"takeover-drain-completion")) f.display.flr_fault = DRAIN_COMPLETION_LOST;
    if (takeover_is(&f,"takeover-drain-result-owner")) f.display.flr_fault = DRAIN_RESULT_OWNER_CHANGED;
    bool drain_fault = f.display.flr_fault >= DRAIN_IO_REPLACED;
    if (takeover_is(&f,"takeover-bad-crc")) f.display.fault = SOURCE_SECOND_CRC_WRONG;
    bool l3_before = takeover_is(&f,"takeover-l3-before"), l3_held = takeover_is(&f,"takeover-l3-held");
    bool l3_selector = takeover_is(&f,"takeover-l3-selector");
    if (l3_before || l3_held || l3_selector) {
        f.display.l3_fault = l3_selector ? L3_SELECTOR_UNPOSTED : L3_POLICY_NARROW;
        f.display.l3_fault_bank = 6;
        f.display.l3_fault_held = l3_held;
    }
    const struct bdw_edp_baseline_report reference = setup_cycle(&f.display);
    f.display.panel.aux_setup = 0x24030152u;
    struct bdw_display_takeover_io io = {.display=source_io(&f.display),.prepare_reset=takeover_reset,
        .recover=takeover_recover,.range=takeover_range,
        .page={&f,takeover_prove,takeover_now,takeover_read,takeover_exchange,takeover_invalidate,takeover_aperture_read}};
    io.display.read = takeover_read_display;
    assert(display_access_acquire(42,&f.lease));
    assert(scanout_interval_protect_boot_framebuffer(takeover_mmio,takeover_aperture,takeover_bytes,takeover_fence));
    assert(scanout_interval_import_firmware(&f.occupied,takeover_mmio,takeover_aperture,takeover_bytes,takeover_fence,
                                            (struct scanout_space_range){12292,1}));
    bool ok = intel_bdw_display_takeover(&takeover,&f.lease,&io,&f.display.image,&reference);
    assert(!intel_bdw_display_takeover(&takeover,&f.lease,&io,&f.display.image,&reference));
    bool stale = takeover_is(&f,"takeover-stale-rollback");
    bool retained = stale || drain_fault || l3_held || l3_selector || takeover_is(&f,"takeover-traffic-open") ||
                    takeover_is(&f,"takeover-bad-crc");
    assert(ok == (takeover_is(&f,"takeover-roundtrip") || takeover_is(&f,"takeover-range-protected")));
    assert(f.recovery_loan_tested == takeover_is(&f,"takeover-range-protected"));
    if (takeover_is(&f,"takeover-framebuffer-alias") || takeover_is(&f,"takeover-late-framebuffer-alias") ||
        takeover_is(&f,"takeover-dark-read-error")) {
        assert(f.dark_reads == 1 && f.display.proofs == 2 && !f.stores);
        assert(f.pte == (takeover_is(&f,"takeover-dark-read-error") ? 0 : 0x8c001001u));
    }
    if (takeover_is(&f,"takeover-rmrr-scratch") || takeover_is(&f,"takeover-rmrr-private") ||
        takeover_is(&f,"takeover-backing-scratch") || takeover_is(&f,"takeover-backing-private")) {
        assert(takeover.cycle.stage == BDW_EDP_CYCLE_COMPLETE && f.display.proofs == 2 &&
               !takeover.seed_source.object && !takeover.private_source.object && !takeover.recovery_loan.lender && !f.dark_reads);
    }
    assert(takeover.state == (ok ? BDW_TAKEOVER_READY : retained ? BDW_TAKEOVER_RETAINED : BDW_TAKEOVER_REFUSED));
    assert(display_access_release(&f.lease,true) == !retained);
    assert(scanout_interval_held(&f.occupied,takeover_mmio,takeover_aperture,takeover_bytes,takeover_fence,f.occupied.pages));
    assert(takeover.reset.reset_issued == (!takeover_is(&f,"takeover-reset-declined") &&
                                         !takeover_is(&f,"takeover-traffic-open") && !drain_fault &&
                                         !l3_before && !l3_held && !l3_selector));
    if (drain_fault) {
        bool remapper_ran = f.display.flr_fault != DRAIN_IO_REPLACED;
        assert(f.display.block_calls == 1 && !f.grants && !f.display.flr_writes &&
               f.display.drain_stores == (unsigned)remapper_ran && f.display.drain_alias_retired == remapper_ran);
    }
    if (l3_held) assert(takeover.reset.engine_acks == 31u && takeover.reset.l3_ack &&
                        takeover.reset.l3_policy_checks == 1 && !f.display.decode_closes);
    if (l3_selector) assert(takeover.reset.l3_selector_retained && !takeover.reset.block_issued);
    if (ok || stale) {
        assert(takeover.seed.state == BDW_PAGE_SEEDED && f.grants == 1 && f.dark_reads == 1 &&
               takeover.driver_range.sealed && f.stores == 3 &&
               f.invalidates == 3 && f.apertures == 4 && f.pte == (takeover_scratch | 1));
        assert(f.translation == ((stale ? takeover_private : takeover_scratch) | 1));
    } else {
        assert(!f.stores && !f.invalidates && !f.apertures && !takeover.driver_range.sealed);
    }
    const struct scanout_space_range slot = {TAKEOVER_SLOT, 1};
    bool crc_failed = takeover_is(&f,"takeover-bad-crc");
    assert(scanout_interval_held(&takeover.recovery_loan,takeover_mmio,takeover_aperture,takeover_bytes,
                                 takeover_fence,slot) == (crc_failed || stale));
    assert(scanout_interval_held(&takeover.driver_range,takeover_mmio,takeover_aperture,takeover_bytes,
                                 takeover_fence,slot) == (ok || crc_failed || stale));
    assert(f.display.panel.backlight == (stale || !retained));
    printf("PASS %s\n",mode);
    return 0;
}

static unsigned test_cycle(void)
{
    struct bdw_edp_cycle_report r;
    struct source_fixture f = {0};
    const unsigned writes = sizeof(disable_steps) / sizeof(disable_steps[0]);
    assert(run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_COMPLETE && !r.retained);
    assert(r.shutdown.source_writes == writes && f.dark && f.panel.backlight);
    assert(r.restore.crc_checks == 2 && r.restore.captured_crc[1] == TEST_REFERENCE_CRC);
    assert(r.mask_writes == 2 && r.mask_restored && f.mask_writes == 2 && f.irq_mask == UINT32_MAX);
    assert(r.mask_before == UINT32_MAX && r.mask_live == 0x7fffffffu && r.mask_after == UINT32_MAX);
    assert(r.dark_hash == TEST_BACKING_HASH && f.panel.pp_writes == 2 && f.primary == 0xd8000400u);
    assert(r.panel_off.outcome == BDW_EDP_PANEL_OFF_COMPLETE && r.panel_off.samples > 1);
    assert(r.panel_off.first == 0xa000000bu && r.panel_off.last == 0x08000001u);
    assert(r.panel_off.elapsed_us >= 323900u && r.panel_off.elapsed_us < 350000u);
    for (unsigned i = 1; i <= writes; ++i) {
        f = (struct source_fixture){.fail_disable_write = i};
        assert(!run_cycle(&f, &r) && r.retained && r.shutdown.source_writes == i);
        assert(!r.restore.source_writes && !f.proofs && !f.dark);
        assert(r.mask_restored && f.irq_mask == UINT32_MAX);
    }
    f = (struct source_fixture){.fault = SOURCE_MASKED};
    assert(!run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_ADMISSION);
    assert(!r.retained && f.panel.backlight && !r.shutdown.source_writes);
    assert(r.mask_writes == 1 && r.mask_restored);
    f = (struct source_fixture){.fault = SOURCE_STALE_UNDERRUN};
    assert(!run_cycle(&f, &r) && !r.retained && !r.mask_writes && f.panel.backlight);
    f = (struct source_fixture){.fault = SOURCE_IRQ_ENABLED};
    assert(!run_cycle(&f, &r) && !r.retained && !r.mask_writes && f.panel.backlight);
    f = (struct source_fixture){.uncertain_mask_open = true};
    assert(!run_cycle(&f, &r) && !r.retained && r.mask_restored && r.mask_writes == 2);
    assert(f.panel.backlight && !r.shutdown.source_writes && f.irq_mask == UINT32_MAX);
    f = (struct source_fixture){.fail_mask_close = true};
    assert(!run_cycle(&f, &r) && r.retained && !r.mask_restored && r.mask_writes == 2);
    assert(r.stage == BDW_EDP_CYCLE_RESTORE && f.panel.backlight);
    assert(!r.restore.source_error && r.mask_error == BDW_EDP_SOURCE_UNDERRUN);
    f = (struct source_fixture){.panel_off_delay_us = 351000u};
    assert(!run_cycle(&f, &r) && r.retained && r.stage == BDW_EDP_CYCLE_PANEL);
    assert(r.shutdown.source_error == BDW_EDP_SOURCE_CLOCK && r.shutdown.offset == PP_STATUS);
    assert(r.shutdown.word == 0xa000000bu && r.panel_off.last == r.shutdown.word);
    assert(r.panel_off.outcome == BDW_EDP_PANEL_OFF_TIMEOUT && r.panel_off.elapsed_us >= 350000u);
    assert(r.mask_restored && !r.mask_error && !r.restore.source_writes && !f.dark);
    f = (struct source_fixture){.fault = SOURCE_CLOCK_STOPPED};
    assert(!run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_PRIMARY && r.retained);
    assert(!f.panel.backlight && !r.restore.source_writes);
    f = (struct source_fixture){.lose_dark_backing = true};
    assert(!run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_DARK && r.retained);
    assert(f.dark && !f.panel.backlight && !r.restore.source_writes);
    f = (struct source_fixture){.fault = SOURCE_SECOND_CRC_WRONG};
    assert(!run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_RESTORE && r.retained);
    assert(f.dark && f.proofs == 2 && !f.panel.backlight);
    f = (struct source_fixture){.exercise_flr = true};
    assert(run_cycle(&f, &r) && r.stage == BDW_EDP_CYCLE_COMPLETE && !r.retained);
    assert(f.flr.reset_issued && f.flr.ready_polls == 3 && f.flr_writes == 5);
    assert(f.flr.block_issued && f.flr.traffic_blocked && f.flr.l3_ack && f.flr.traffic_polls == 3);
    assert(f.flr.stores == 6 && f.media_reads == 4 && f.decode_closes == 1);
    assert(f.flr.stage == BDW_FLR_COMPLETE && !f.flr.retained);
    assert(f.reopen.phase == BDW_EDP_REOPEN_READY && f.reopen.writes == 3 && !f.reopen.retained);
    assert(!f.reopen.driver_before && f.reopen.driver_after == 0xc0000000u);
    assert(f.reopen.cdclk_before == 449u && !f.reopen.gate_before);
    assert(f.primary == 0x98000400u && f.panel.pp_writes == 2 && f.proofs == 2);
    assert(f.pps.complete && f.pps.writes == 3 && f.pps.wait_us >= 390000 && !f.pps.retained);
    assert(f.pwm.complete && f.pwm.writes == 3 && !f.pwm.retained);
    assert(r.mask_writes == 3 && r.mask_restored && f.irq_mask == UINT32_MAX);
    f = (struct source_fixture){.exercise_flr = true, .preserve_platform = true};
    assert(run_cycle(&f, &r) && !r.retained && f.panel.backlight);
    assert(f.reopen.writes == 1 && f.reopen.driver_before == 0x40000000u);
    assert(f.reopen.cdclk_before == 674u && f.reopen.gate_before == 0x1000u);
    for (unsigned fault = 1; fault <= 9; ++fault) {
        f = (struct source_fixture){.exercise_flr = true, .flr_fault = fault};
        assert(!run_cycle(&f, &r) && r.retained);
        assert(r.stage == ((fault == 5 || fault == 7 || fault == 8) ? BDW_EDP_CYCLE_RESTORE : BDW_EDP_CYCLE_DARK));
        assert(!r.restore.source_writes && !f.panel.backlight && !r.mask_restored && r.mask_writes == 1);
        assert(f.flr.reset_issued == (fault != 1 && fault != 6));
        assert(f.flr.retained == (fault == 2 || fault == 3 || fault == 4));
    }
    for (unsigned fault = 10; fault <= 15; ++fault) {
        f = (struct source_fixture){.exercise_flr = true, .flr_fault = fault};
        assert(!run_cycle(&f, &r) && r.retained && !f.panel.backlight);
        assert(r.stage == (fault <= 12 ? BDW_EDP_CYCLE_DARK : BDW_EDP_CYCLE_RESTORE));
        if (fault <= 12 || fault >= 14)
            assert(!f.panel.power_requested);
        if (fault == 10)
            assert(f.pps.retained && f.pps.writes == 1 && !f.pwm.writes);
        if (fault == 11)
            assert(f.pps.complete && f.pwm.retained && f.pwm.writes == 1);
        if (fault == 12)
            assert(f.reopen.retained && f.reopen.writes == 3 && !f.pps.writes && !f.pwm.writes);
        if (fault == 13)
            assert(f.proofs == 2 && r.restore.offset == BDW_EDP_PWM_DATA);
        if (fault == 15)
            assert(!r.restore.source_writes);
    }
    f = (struct source_fixture){.exercise_flr = true, .flr_fault = 16};
    assert(!run_cycle(&f, &r) && !r.retained && f.panel.backlight && !f.flr_writes);
    assert(r.stage == BDW_EDP_CYCLE_ADMISSION && r.shutdown.offset == BDW_EDP_PWM_DATA);
    const unsigned expected[] = {0, 1, 1, 1, 2, 2, 1, 1};
    for (unsigned i = 0; i < sizeof(expected) / sizeof(expected[0]); ++i) {
        f = (struct source_fixture){.exercise_flr = true, .flr_fault = 17 + i};
        assert(!run_cycle(&f, &r) && r.retained && r.stage == BDW_EDP_CYCLE_DARK);
        assert(f.reopen.writes == expected[i] && f.reopen.retained == !!expected[i]);
        assert(!f.pps.writes && !f.pwm.writes && !f.panel.power_requested && !f.panel.backlight);
    }
    f = (struct source_fixture){.exercise_flr = true, .flr_fault = 25};
    assert(!run_cycle(&f, &r) && r.retained && !f.panel.backlight && f.proofs == 2);
    assert(r.restore.offset == BDW_EDP_WELL_DRIVER);
    for (unsigned fault = DRAIN_WRITE_UNCERTAIN; fault <= DRAIN_MSI_ENABLED; ++fault) {
        f = (struct source_fixture){.exercise_flr = true, .flr_fault = fault};
        assert(!run_cycle(&f, &r) && r.retained && !f.panel.backlight);
        assert(!f.flr.reset_issued && !f.flr_writes && r.stage == BDW_EDP_CYCLE_DARK);
        assert(f.drain_stores == (fault != DRAIN_STALE_EPOCH && fault != DRAIN_SOURCE_USED));
        assert(f.flr.retained == (fault != DRAIN_STALE_EPOCH && fault != DRAIN_SOURCE_USED));
        assert(f.drain.complete == (fault == DRAIN_OWNER_LOST || fault == DRAIN_NEW_TRANSACTION ||
                                    fault == DRAIN_BAR_CHANGED || fault == DRAIN_MSI_ENABLED));
    }
    f = (struct source_fixture){.exercise_flr = true, .flr_fault = FLR_SHORT_QUIET};
    assert(!run_cycle(&f, &r) && r.retained && !f.panel.backlight);
    assert(f.flr.reset_issued && f.flr.retained && !f.flr.ready_polls && f.flr_writes == 1);
    f = (struct source_fixture){.exercise_flr = true, .flr_fault = DRAIN_ALIAS_RETAINED};
    assert(!run_cycle(&f, &r) && r.retained && !f.flr.reset_issued && !f.flr_writes);
    assert(f.drain.complete && !f.drain_alias_retired && f.flr.retained && !f.panel.backlight);
    const uint32_t engines[] = {0x8000, 0x8004, 0x8008, 0x800c, 0x8010};
    const uint32_t queues[] = {0x12870, 0x12878, 0x1c870, 0x1c878};
    unsigned traffic_cases = 0;
    for (unsigned fault = GT_STALE_L3; fault <= GT_BLOCK_REFUSED; ++fault) {
        bool engine = fault >= GT_ENGINE_OPEN && fault <= GT_ENGINE_UNFLUSHED;
        unsigned count = engine ? 5 : fault == GT_MEDIA_BUSY ? 4 : 1;
        for (unsigned i = 0; i < count; ++i) {
            f = (struct source_fixture){.exercise_flr = true, .flr_fault = fault};
            if (engine) f.gt_fault_offset = engines[i];
            if (fault == GT_MEDIA_BUSY) f.gt_fault_offset = queues[i];
            assert(!run_cycle(&f, &r) && r.retained && !f.panel.backlight);
            assert(!f.flr.reset_issued && !f.flr_writes);
            assert(f.decode_closes == (fault == GT_LOST_AT_RESET || fault == GT_CLOSE_UNCERTAIN));
            assert(f.drain_stores == (fault == GT_LOST_AT_RESET));
            assert(f.flr.retained == (fault > GT_STALE_ENGINE && fault != GT_BLOCK_REFUSED));
            if (fault == GT_AIR_MEDIA_TRACE)
                assert(f.block_calls == 1 && !f.flr.traffic_blocked && !f.flr.l3_ack && !f.flr.engine_acks);
            ++traffic_cases;
        }
    }
    const uint32_t clients[] = {1, 2, 4, 8, 16, 64};
    for (unsigned fault = GT_REQUEST_REOPENS_ONCE; fault <= GT_REOPEN_AT_CONFIG; ++fault) {
        unsigned count = fault <= GT_RESPONSE_REOPENS_ONCE ? 6 : fault <= GT_ENGINE_GO_REOPEN ? 5 : 1;
        for (unsigned i = 0; i < count; ++i) {
            f = (struct source_fixture){.exercise_flr = true, .flr_fault = fault};
            f.gt_fault_offset = fault <= GT_RESPONSE_REOPENS_ONCE ? clients[i] : engines[i];
            assert(!run_cycle(&f, &r) && r.retained && f.flr.retained);
            assert(!f.flr.reset_issued && !f.flr_writes && !f.drain_stores);
            assert(f.decode_closes == (fault == GT_OWNER_LOST_AT_DRAIN));
            ++traffic_cases;
        }
    }
    f = (struct source_fixture){.exercise_flr = true, .flr_fault = GT_PROGRESSIVE_BLOCK};
    assert(run_cycle(&f, &r) && f.flr.block_issued && f.flr.traffic_polls == 3);
    assert(f.flr.l3_ack && f.flr.traffic_blocked && f.flr.reset_issued && f.drain_stores == 1);
    return writes + 49 + traffic_cases;
}

static unsigned test_flr_l3_policy(void)
{
    struct bdw_edp_cycle_report r;
    for (unsigned phase = 0; phase < 2; ++phase) {
        for (unsigned bank = 0; bank < 8; ++bank) {
            struct source_fixture f = {.exercise_flr=true,.l3_fault=L3_POLICY_NARROW,
                .l3_fault_bank=bank,.l3_fault_held=phase != 0};
            assert(!run_cycle(&f,&r) && !f.flr.reset_issued && !f.decode_closes);
            assert(f.flr.block_issued == (phase != 0) && f.flr.l3_ack == (phase != 0) &&
                   f.flr.l3_policy_checks == phase && !f.flr.l3_selector_retained &&
                   f.l3_selector == 0x10000000);
        }
    }
    for (enum l3_policy_fault fault = L3_POLICY_OFF; fault <= L3_SELECTOR_DEADLINE; ++fault) {
        struct source_fixture f = {.exercise_flr=true,.l3_fault=fault,.l3_fault_bank=3};
        assert(!run_cycle(&f,&r) && !f.flr.block_issued && !f.flr.reset_issued && !f.decode_closes);
        assert(f.flr.l3_selector_retained == (fault != L3_POLICY_OFF));
        if (fault == L3_POLICY_OFF || fault == L3_SELECTOR_UNPOSTED) {
            /* Unposted steering can leave the original word readable. */
            assert(f.l3_selector == (fault == L3_POLICY_OFF ? 0x10000000u : 0x12000000u));
        }
    }
    return 20;
}

struct pci_hold_fixture {
    uint64_t time;
    unsigned command, stores, polls, fault;
    bool copy, backing;
};

static bool hold_active(void *ctx)
{
    struct pci_hold_fixture *f = ctx;
    return f->fault != 1;
}

static bool hold_sample(void *ctx, struct bdw_pci_hold_sample *sample)
{
    struct pci_hold_fixture *f = ctx;
    *sample = (struct bdw_pci_hold_sample){.command = 0x900000u | f->command};
    if (f->command == 0x403 && (++f->polls <= 3 || f->fault == 2))
        sample->af = 0x100;
    if (f->command == 0x400 && f->fault == 3)
        sample->command |= 2;
    if (f->command == 0x402 && f->fault == 4)
        sample->af = 0x100;
    return true;
}

static enum intel_gt_write hold_store(void *ctx, uint16_t before, uint16_t after)
{
    struct pci_hold_fixture *f = ctx;
    static const uint16_t order[] = {7, 0x403, 0x400, 0x402, 7};
    assert(f->stores < 4 && before == f->command &&
           before == order[f->stores] && after == order[f->stores+1]);
    assert(after != 7 || (f->copy && f->backing));
    f->command = after;
    ++f->stores;
    return f->fault == 5 && after == 0x400 ? GT_WRITE_UNCERTAIN : GT_WRITE_DONE;
}

static uint64_t hold_now(void *ctx)
{
    return ((struct pci_hold_fixture *)ctx)->time;
}

static void hold_delay(void *ctx, uint32_t us)
{
    struct pci_hold_fixture *f = ctx;
    f->time += f->fault == 6 ? 0 : us;
}

static bool hold_read_rc6(void *ctx, struct bdw_pci_rc6_sample *s)
{
    struct pci_hold_fixture *f = ctx;
    assert(f->command == 0x403 && f->stores == 1 && f->polls >= 6);
    if (f->fault == 10)
        return false;
    if (f->fault == 11)
        f->time += BDW_PCI_HOLD_LIMIT_US;
    if (f->fault == 12)
        f->command = 7;
    s->residency = (uint32_t)f->time;
    return true;
}

static bool hold_copy(void *ctx)
{
    struct pci_hold_fixture *f = ctx;
    assert(f->command == 0x400);
    f->copy = f->fault != 7;
    return f->copy;
}

static bool hold_backing(void *ctx)
{
    struct pci_hold_fixture *f = ctx;
    assert(f->command == 0x402 && f->copy);
    f->time += f->fault == 8 ? BDW_PCI_HOLD_LIMIT_US : 200000;
    f->backing = f->fault != 9;
    return f->backing;
}

static unsigned test_pci_hold(void)
{
    struct bdw_pci_hold_report report;
    static const unsigned writes[] = {4, 0, 1, 2, 3, 2, 1, 2, 3, 3, 1, 1, 1};
    for (unsigned fault = 0; fault < sizeof(writes) / sizeof(writes[0]); ++fault) {
        struct pci_hold_fixture f = {.command = 7, .time = 1, .fault = fault};
        const struct bdw_pci_hold_io io = {
            &f, hold_active, hold_sample, hold_store, hold_now, hold_delay, hold_read_rc6, hold_copy, hold_backing,
        };
        assert(intel_bdw_pci_hold(&io, &report) == !fault);
        assert(report.writes == f.stores && f.stores == writes[fault]);
        assert(report.retained == (fault > 1));
        if (!fault)
            assert(report.complete && report.samples == 5 && report.polls == 5 &&
                   report.rc6_count == 2 && report.rc6[1].residency - report.rc6[0].residency == BDW_PCI_RC6_INTERVAL_US &&
                   report.first_clear_us == 400 && report.settle_us == 500 &&
                   report.cpu_copy && report.backing && f.command == 7);
    }
    return sizeof(writes) / sizeof(writes[0]);
}

static unsigned test_source(void)
{
    struct source_fixture good = {0};
    struct bdw_edp_source_report report;
    assert(run_source(&good, &report) == BDW_EDP_OK);
    assert(report.panel.panel_stage == BDW_EDP_PANEL_READY && good.panel.backlight);
    assert(good.proofs == 2 && report.verified_frames == 4);
    assert(report.crc_checks == 2 && !report.crc_retained && good.crc_retired);
    assert(report.captured_crc[0] == TEST_REFERENCE_CRC && report.captured_crc[1] == TEST_REFERENCE_CRC);
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
    for (enum source_fault fault = SOURCE_NO_CRC; fault <= SOURCE_REFERENCE_CHANGED; ++fault) {
        struct source_fixture f = {.fault = fault};
        assert(run_source(&f, &report) != BDW_EDP_OK && !f.panel.backlight);
        if (fault == SOURCE_NO_REFERENCE || fault == SOURCE_REFERENCE_CHANGED)
            assert(!report.crc_checks && !f.panel.writes);
        else
            assert(report.crc_checks == (fault == SOURCE_SECOND_CRC_WRONG ? 2u : 1u));
        assert(report.crc_retained == (fault == SOURCE_CRC_RETAINED || fault == SOURCE_BACKING_CHANGED ||
                                       fault == SOURCE_DDI_LOST_DURING_CRC));
    }
    return 9 + good.panel.events * 2 + sizeof(errors) / sizeof(errors[0]);
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
        const struct bdw_edp_panel_io panel = {.link = io, .panel_held = held, .scanout_live = live};
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

int main(int argc, char **argv)
{
    if (argc == 2) return test_takeover(argv[1]);
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
           sizeof(expected) / sizeof(expected[0]) + 2 + test_panel() + test_source() + test_cycle() +
           test_flr_l3_policy() + test_pci_hold(), good.events);
    return 0;
}
