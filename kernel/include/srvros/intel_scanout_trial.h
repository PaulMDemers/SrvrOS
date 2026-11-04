#ifndef SRVROS_INTEL_SCANOUT_TRIAL_H
#define SRVROS_INTEL_SCANOUT_TRIAL_H
#include <srvros/scanout_lifetime.h>
struct limine_framebuffer;
struct intel_scanout_clock {uint32_t frame,line;uint64_t us;};
/* Read-only adapter. These observations are NOT plane-flip completions. */
bool intel_scanout_trial_profile(struct scanout_profile *);
bool intel_scanout_trial_ram(uint64_t physical,uint64_t frames);
bool intel_scanout_trial_clock(struct intel_scanout_clock *);
bool intel_scanout_trial_revalidate(void);
/* Proposed CPU cache-query pages only; no mapping/managed-range authority. */
bool intel_scanout_trial_cache_pages(uint64_t pages[3]);
bool intel_scanout_lifetime_trial(struct limine_framebuffer *,bool fixture);
#endif
