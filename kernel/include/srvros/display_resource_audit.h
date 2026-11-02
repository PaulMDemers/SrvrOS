#pragma once
#include <stdint.h>
#include <stdbool.h>
/* Exact Haswell controls only. Linux v7.0 intel_display_regs.h,
 * i9xx_plane_regs.h, intel_sprite_regs.h, intel_cursor_regs.h,
 * intel_pfit_regs.h, intel_vga_regs.h + Haswell offsets in intel_display_device.c.
 * No status acknowledgement, backing memory, AUX, clock, forcewake or writes.
 * All dependent reads require the existing well enabled, checked on both sides.
 */
#define DISPLAY_RESOURCE_COUNT 26u
#define DISPLAY_RESOURCE_PAGES 12u
static const unsigned display_resource_offsets[DISPLAY_RESOURCE_COUNT]={
    0x70008,0x71008,0x72008,0x7f008,0x60400,0x61400,0x62400,0x6f400,0x70180,0x71180,0x72180,0x70280,0x71280,0x72280,0x70080,0x71080,0x72080,0x68080,0x68880,0x69080,0x64000,0x64100,0x64200,0x64300,0x64400,0x41000
};
static const unsigned display_resource_pages[DISPLAY_RESOURCE_PAGES]={
    0x41000,0x60000,0x61000,0x62000,0x64000,0x68000,0x69000,0x6f000,0x70000,0x71000,0x72000,0x7f000
};
struct display_resource_snapshot {
    uint64_t begin,end;
    uint32_t before[4],after[4],raw[DISPLAY_RESOURCE_COUNT];
    unsigned state,read_count; /* 1 observed; 2 off/unavailable; 3 invalid */
};
static inline bool display_resource_well_on(const uint32_t values[4]) {
    for(unsigned i=0;i<4;i++)
        if(values[i]==0xffffffffu || !(values[i]&(1u<<30)))return false;
    return true;
}
