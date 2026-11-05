#include <srvros/intel_handoff.h>
#include <srvros/intel_boot_claim.h>
#include <srvros/intel_gfx.h>
#include <srvros/console.h>
#include <srvros/display_present.h>
#include <srvros/backlight.h>
static bool attempted;
static struct scanout_handoff transaction;
static struct scanout_space_pool pool;
static bool revalidate(void *ctx,const struct scanout_handoff_snapshot *s) {
    (void)ctx;
    /* Native hardware assurances remain unavailable: do not accept a forged
     * positive proof, even if the geometry/readback still matches. */
    return !s->assurances && intel_window_handoff_revalidate(s);
}
bool intel_handoff_probe_report(void) {
    if(attempted)return false;
    attempted=true;
    console_write("intel-handoff: pausing CPU clients; validating inherited backing and candidate entries; hardware grant remains unavailable\n");
    struct display_access_token t={0};struct scanout_handoff_snapshot s={0};
    bool selected=intel_boot_policy()==INTEL_BOOT_OBSERVER && intel_boot_claimed_client()==INTEL_BOOT_OBSERVER;
    /* Drain visible progress and any preempted text flush before a nonwaiting
     * lease attempt. No provider/owner is forcibly cancelled. */
    bool acquired=selected && console_sync() && display_access_acquire(0x494e54454cull,&t);
    bool idle=acquired && display_present_handoff_idle(),snapshot=false,preserved=false,released=false;
    unsigned blocked=0;uint32_t missing=HANDOFF_ALL|HANDOFF_EXCLUSION;
    if(idle) {
        uint32_t pixel=0x102030;
        if(!console_put_pixel(0,0,pixel))blocked|=1;
        if(!console_fill_rectangle(0,0,1,1,pixel))blocked|=2;
        if(!console_blit_rgb(0,0,1,1,&pixel,1))blocked|=4;
        if(!console_blit_rgb_scaled(0,0,1,1,&pixel,1,2))blocked|=8;
        if(backlight_percent(10)==-1)blocked|=16;
        if(!display_present_begin(1))blocked|=32;
        if(!intel_gfx_init())blocked|=64;
        console_write("intel-handoff: text queued while framebuffer clients excluded\n");
        snapshot=intel_window_handoff_snapshot(&s);
        preserved=snapshot && intel_window_handoff_revalidate(&s);
        if(preserved)missing=scanout_handoff_open(&transaction,&pool,&t,&s,(struct scanout_handoff_io){NULL,revalidate});
    }
    if(acquired)released=display_access_release(&t,intel_window_handoff_clean() && !transaction.active);
    if(released)console_flush();
    bool complete=idle && snapshot && preserved && blocked==127 && missing==HANDOFF_ALL && !transaction.active && released;
    console_printf("intel-handoff-summary: v=1 selected=%u acquired=%u idle=%u blocked=%u snapshot=%u preserved=%u missing=%u managed_pages=0 pte_writes=0 aperture_mappings=0 released=%u complete=%u hardware_ownership=0\n",
        (uint64_t)selected,(uint64_t)acquired,(uint64_t)idle,(uint64_t)blocked,(uint64_t)snapshot,(uint64_t)preserved,
        (uint64_t)missing,(uint64_t)released,(uint64_t)complete);
    return complete;
}
