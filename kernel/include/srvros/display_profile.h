#ifndef SRVROS_DISPLAY_PROFILE_H
#define SRVROS_DISPLAY_PROFILE_H

#include <stdint.h>
#include <stdbool.h>

struct limine_memmap_response;
struct limine_smbios_response;

void display_profile_init(const struct limine_smbios_response *smbios,
                          const struct limine_memmap_response *memmap, uint64_t hhdm);
unsigned display_profile_scale(uint64_t width, uint64_t height);
bool display_profile_is_air61(void);
bool display_profile_is_pro81(void);

#endif
