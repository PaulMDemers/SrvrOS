#ifndef SRVROS_INTEL_GFX_H
#define SRVROS_INTEL_GFX_H

#include <stdbool.h>
#include <stdint.h>

#define SRV_GFX_ACCEL_NONE 0
#define SRV_GFX_ACCEL_INTEL_GEN8 1

#define SRV_GFX_FLAG_ACCEL_DEVICE_PRESENT 0x00000001ull
#define SRV_GFX_FLAG_ACCEL_MMIO_MAPPED 0x00000002ull
#define SRV_GFX_FLAG_ACCEL_BLITTER_PLANNED 0x00000004ull
#define SRV_GFX_FLAG_ACCEL_RENDER_PLANNED 0x00000008ull

bool intel_gfx_init(void);
bool intel_gfx_available(void);
uint64_t intel_gfx_flags(void);
uint64_t intel_gfx_backend(void);
void intel_gfx_print_status(void);

#endif
