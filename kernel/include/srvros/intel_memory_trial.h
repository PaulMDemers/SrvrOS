#ifndef SRVROS_INTEL_MEMORY_TRIAL_H
#define SRVROS_INTEL_MEMORY_TRIAL_H
#include <stdbool.h>
struct limine_framebuffer;
bool intel_memory_trial(struct limine_framebuffer *,const char *cmdline);
bool intel_memory_retained(void);
#endif
