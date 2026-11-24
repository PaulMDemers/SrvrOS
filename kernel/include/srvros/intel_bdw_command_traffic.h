#ifndef SRVROS_INTEL_BDW_COMMAND_TRAFFIC_H
#define SRVROS_INTEL_BDW_COMMAND_TRAFFIC_H
#include <srvros/intel_bdw_scanout_mapping.h>
#include <srvros/intel_bdw_ggtt.h>
#include <srvros/vmm.h>

/* Sample the wireless routes before excluding their register pages. Once
 * the VMM guard is live, even the owner cannot remap these pages to read
 * them. The display/HDA owner stays held independently. */
struct bdw_wireless_routes {
    const struct scanout_transfer *transfer;
    const struct vmm_reserved_guard *guard;
    uint64_t transfer_epoch,guard_epoch,root,mmio;
    uint32_t strap,function,well_before,codec_before,control,codec_after,well_after;
};

static inline bool intel_bdw_routes_held(const struct bdw_wireless_routes *r,
                                        const struct scanout_transfer *t)
{
    return r && t && r->transfer==t && t->object==t && t->pinned && t->epoch==r->transfer_epoch &&
        r->guard && r->guard->object==r->guard && r->guard_epoch && r->guard->epoch==r->guard_epoch &&
        r->root && r->mmio==t->manifest.mmio && r->mmio<=UINT64_MAX-0x6e000 &&
        intel_bdw_air_wireless_video_disabled(r->strap) && !(r->function&(1u<<31)) &&
        (r->well_before&r->well_after&(1u<<30)) && r->codec_before==0x80862808 &&
        r->codec_after==r->codec_before && !(r->control&(1u<<30)) &&
        vmm_reserved_guard_excludes_io_page(r->guard,r->root,r->mmio+0x14000) &&
        vmm_reserved_guard_excludes_io_page(r->guard,r->root,r->mmio+0x6e000) &&
        vmm_reserved_guard_excludes_io_page(r->guard,r->root,r->mmio+0x65000);
}

/* Global GO opens all command streamers. Only the blitter may submit.
 * The Air's four other engine pages have sole, read-only UC aliases. */
static inline bool intel_bdw_peer_ports_held(const struct bdw_wireless_routes *r)
{
    if (!r || !r->guard || r->guard->epoch!=r->guard_epoch || r->mmio>UINT64_MAX-0x1c000)
        return false;
    uint64_t pages[4];
    for (unsigned i=0;i<4;i++) pages[i]=r->mmio+intel_bdw_submission_ports[i];
    return vmm_reserved_guard_readonly_registers_held(r->guard,r->root,pages,4);
}

enum bdw_command_phase {
    BDW_CMD_UNUSED, BDW_CMD_ACQUIRING, BDW_CMD_BLOCKED, BDW_CMD_SWITCHING,
    BDW_CMD_EXECUTING, BDW_CMD_RETURNED, BDW_CMD_REFUSED, BDW_CMD_RETAINED
};

/* The device owner controls the request. The driver reads the separate
 * acknowledgements. GPM changes GO for ALL clients. Before permitting the
 * blitter to execute, the original owner must independently exclude WiDi's
 * device and display-proxy producers and hold WIN's clock-gate request.
 * A clock-gate indication alone is not the ownership callback. Keep that
 * exclusion through rollback. The submission owner retains the other FIFOs. */
struct bdw_command_control {
    struct intel_bdw_scanout_traffic traffic;
    enum intel_gt_write (*isolate_widi)(const struct scanout_transfer *,
            const struct scanout_publication *,uint64_t controller_epoch,uint64_t *widi_epoch);
    uint64_t (*widi_epoch)(const struct scanout_transfer *,const struct scanout_publication *);
    enum intel_gt_write (*set_execution)(const struct scanout_transfer *,
            const struct scanout_publication *,uint64_t epoch,bool enable);
};
struct bdw_blitter_mapping;
bool intel_bdw_command_acquire(struct bdw_blitter_mapping *,struct scanout_transfer *,
        const struct scanout_transfer_token *,const struct bdw_wireless_routes *,struct bdw_command_control,
        void *clock_owner,uint64_t (*now)(void *));
bool intel_bdw_command_held(struct bdw_blitter_mapping *);
/* CPU-side authority check after the final callback before a queue store. */
bool intel_bdw_command_submission_owned(struct bdw_command_hold *);
bool intel_bdw_widi_translations_empty(struct bdw_command_hold *);
bool intel_bdw_command_bcs(struct bdw_blitter_mapping *,bool enable);
bool intel_bdw_command_release(struct bdw_blitter_mapping *);

struct bdw_command_hold {
    struct bdw_blitter_mapping *owner;
    struct scanout_transfer *transfer;
    struct scanout_transfer_token grant;
    struct bdw_command_control io;
    struct bdw_wireless_routes routes;
    void *clock_owner;
    uint64_t (*now)(void *);
    uint64_t epoch,widi_epoch,last;
    uint32_t plane,surface,drains;
    enum bdw_command_phase phase;
};
#endif
