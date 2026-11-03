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
bool intel_scanout_lifetime_trial(struct limine_framebuffer *,bool fixture);
#endif
