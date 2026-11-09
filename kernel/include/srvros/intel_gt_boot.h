#ifndef SRVROS_INTEL_GT_BOOT_H
#define SRVROS_INTEL_GT_BOOT_H
#include <srvros/intel_gt_access.h>
/* Explicit cooperative EFI policy, not a proof that opaque SMM is absent.
 * Trusted firmware must yield boot-time requester control after EFI handoff.
 * Grants cover a bounded wake lease only, never engine or memory ownership. */
struct intel_gt_boot_facts {
 bool compiled,recipe,loader,efi64,profile,serialized,excluded,prerequisites,firmware_quiet,requesters_quiet;
};
#define INTEL_GT_BOOT_FACTS 10u
bool intel_gt_boot_completion_recipe(const char *);
bool intel_gt_boot_bcs_reset_recipe(const char *);
bool intel_gt_boot_reset_ready_recipe(const char *);
bool intel_gt_boot_reader_recipe(const char *);
bool intel_gt_boot_hda_lease_recipe(const char *);
bool intel_gt_boot_hda_owner_recipe(const char *);
bool intel_gt_boot_hda_recipe(const char *);
bool intel_gt_boot_blitter_recipe(const char *);
bool intel_gt_boot_engine_recipe(const char *);
bool intel_gt_boot_recipe(const char *);
uint32_t intel_gt_boot_grants(const struct intel_gt_boot_facts *);
bool intel_gt_wake_probe_report(void);
/* Sticky terminal diagnostic state; automatic return must not reset past it. */
bool intel_gt_wake_retained(void);
bool intel_gt_boot_shutdown_recipe(const char *);
bool intel_gt_boot_bcs_shutdown_recipe(const char *);
bool intel_gt_boot_engine_prepare_recipe(const char *);
bool intel_gt_boot_held_requester_recipe(const char *);
bool intel_gt_boot_requester_recipe(const char *);
bool intel_gt_boot_takeover_recipe(const char *);
#endif
